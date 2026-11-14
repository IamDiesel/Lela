// ==============================================================================
// HaTrackHistory - Implementierung (siehe Header)
// ==============================================================================

#include "HaTrackHistory.h"
#include "HaWebsocketLogic.h"
#include "HaPsramAllocator.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <map>
#include <memory>
#include <algorithm>
#include <math.h>
#include <time.h>

// ------------------------------------------------------------------------------
// Parameter: Laden in Abschnitten
// ------------------------------------------------------------------------------
// HA liefert jeden gespeicherten Zustand mit ALLEN Attributen. Manche Entitaeten
// melden sehr oft mit grossen Attributen: 3 h ergaben eine Antwort von 37 MB
// in EINEM Websocket-Frame. ArduinoWebsockets legt jeden Frame am Stueck an
// (readData) - das scheitert (bad_alloc) und bricht ab. Deshalb wird je
// Entitaet von jetzt aus rueckwaerts in Zeitabschnitten geladen, deren Laenge
// sich an der gemessenen Datenrate ausrichtet.

// Angestrebte Groesse einer Antwort.
static const uint32_t TARGET_RESPONSE_BYTES = 256UL * 1024UL;
// Erster Abschnitt je Entitaet: misst die Datenrate.
static const uint32_t PROBE_SLICE_S = 60;
static const uint32_t MIN_SLICE_S   = 30;
// Harte Obergrenze gegen Ratenspruenge (Tracker wechselt in den Live-Modus):
// bei der hoechsten bisher gemessenen Rate (~12 KB/s) bleibt eine Antwort
// unter 4 MB, auch wenn der vorige Abschnitt ruhig war.
static const uint32_t MAX_SLICE_S   = 300;
// Ein Abschnitt waechst je Schritt hoechstens um diesen Faktor.
static const uint32_t MAX_SLICE_GROWTH = 4;
// Datenmenge je Entitaet und Ladevorgang. Darueber wird der Weg gekuerzt
// (der neuere Teil bleibt) - lieber kurzer Weg als minutenlange Last.
static const uint32_t ENTITY_BUDGET_BYTES = 4UL * 1024UL * 1024UL;
// Eine Antwort liegt beim Empfang mehrfach im Speicher (Frame der Bibliothek,
// String, Parsen). Groesster freier PSRAM-Block / dieser Faktor begrenzt die
// erwartete Antwortgroesse.
static const uint32_t RESPONSE_COPIES = 4;
// Darunter wird nicht weitergeladen (Video, Kacheln haben Vorrang).
static const size_t   MIN_FREE_PSRAM_BLOCK = 6UL * 1024UL * 1024UL;
// Antworten ab dieser Groesse einzeln loggen (Fehlersuche bei Datenmengen).
static const uint32_t LOG_LARGE_RESPONSE_BYTES = 1024UL * 1024UL;
// Ab dieser Groesse je Zustand Hinweis im Log (Diagnose-Attribute o. ae.).
static const uint32_t LARGE_STATE_BYTES = 4096;

// Pause nach dem Aufbau der HA-App (SetHold), bevor geladen wird, und
// Hoechstdauer einer Pause.
static const uint32_t HOLD_GRACE_MS = 1500;
static const uint32_t HOLD_MAX_MS = 15000;

static const uint32_t SLICE_TIMEOUT_MS = 15000;
// Pause zwischen zwei Abschnitten: HA und WLAN nicht dauerhaft belegen.
static const uint32_t SLICE_GAP_MS = 50;

// Fruehestens gueltige Uhrzeit (Nov. 2023) - davor ist NTP noch nicht durch.
static const time_t MIN_VALID_EPOCH = 1700000000;

// ------------------------------------------------------------------------------
// Parameter: Reduktion
// ------------------------------------------------------------------------------
// Neuer Punkt erst ab halber GPS-Genauigkeit Abstand, begrenzt auf diesen
// Bereich. Ohne Genauigkeit gilt MIN_STEP_M.
static const float  MIN_STEP_M = 2.0f;
static const float  MAX_STEP_M = 25.0f;
// Beim Mischen (Abschnitte, Live) gelten Punkte darunter als derselbe.
static const double MERGE_SAME_M = 1.0;

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
using Track = std::vector<HaTrackPoint, HaPsramStdAllocator<HaTrackPoint>>;

// Weg einer Entitaet samt Abdeckung: [covFrom, covTo] liegt vollstaendig vor.
// covLive: seit dem Laden laufen Live-Punkte ein - die Abdeckung reicht bis
// jetzt, bis die Verbindung endet (dann covTo = letzter Verbindungszeitpunkt).
struct EntityTrack {
    Track  pts;
    bool   pendingBreak = false;   // naechster Punkt beginnt neuen Abschnitt
    time_t covFrom = 0;
    time_t covTo = 0;
    bool   covLive = false;
};

// Anmeldung eines Widgets
struct Owner {
    const void*         key;
    std::vector<String> ids;
    uint32_t            seconds;
};

static SemaphoreHandle_t             s_mutex = nullptr;
static std::map<String, EntityTrack> s_tracks;          // unter s_mutex
static std::vector<Owner>            s_owners;          // unter s_mutex
static std::vector<String>           s_wantedIds;       // unter s_mutex (aus s_owners)
static uint32_t                      s_wantedSeconds = 0;

static volatile bool     s_requested = false;   // GUI -> HA_WS_Task: (nach)laden
static volatile uint32_t s_version   = 0;
static volatile bool     s_hold = false;        // GUI baut auf (SetHold)
static volatile uint32_t s_holdChangedAt = 0;

// Ladeplan - nur HA_WS_Task.
struct LoadPlan {
    bool                active = false;
    std::vector<String> ids;          // Entitaeten in Ladereihenfolge
    size_t              index = 0;    // aktuelle Entitaet
    uint32_t            seconds = 0;  // Zeitfenster

    // Aktuelle Entitaet
    time_t   loadEnd = 0;       // "jetzt" beim Start dieser Entitaet
    time_t   windowStart = 0;   // aelteste gewuenschte Zeit
    time_t   cursor = 0;        // Ende des naechsten Abschnitts (wandert rueckwaerts)
    time_t   sliceStart = 0;    // Beginn des laufenden Abschnitts
    uint32_t sliceS = 0;        // Laenge des naechsten Abschnitts
    bool     hasCov = false;    // schon vorliegender Zeitraum, der uebersprungen wird
    time_t   covFrom = 0;
    time_t   covEnd = 0;

    // Statistik der aktuellen Entitaet (Log)
    uint32_t slices = 0;
    uint32_t states = 0;
    uint32_t bytes = 0;
    uint32_t maxRate = 0;       // Bytes/s, hoechster gemessener Wert
    uint32_t startedMs = 0;
};
static LoadPlan s_plan;
static uint32_t s_requestId = 0;      // laufender Abschnitt (0 = keiner)
static uint32_t s_sentAt = 0;
static uint32_t s_lastResultMs = 0;
static time_t   s_lastAlive = 0;      // letzter Tick mit Verbindung (Ende der Live-Abdeckung)

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
namespace {

class Lock {
public:
    Lock()  { ok_ = s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE; }
    ~Lock() { if (ok_) xSemaphoreGive(s_mutex); }
    bool ok() const { return ok_; }
private:
    bool ok_;
};

bool nowValid(time_t& now) {
    now = time(nullptr);
    return now >= MIN_VALID_EPOCH;
}

void touch() {
    s_version = s_version + 1;   // volatile: kein ++ (C++20-Warnung)
}

// Abstand in Metern (flache Naeherung, genau genug fuer wenige Kilometer).
double distanceM(double lat1, double lon1, double lat2, double lon2) {
    const double dy = (lat2 - lat1) * 110540.0;
    const double dx = (lon2 - lon1) * 111320.0 * cos(lat1 * M_PI / 180.0);
    return sqrt(dx * dx + dy * dy);
}

float stepFor(float accuracyM) {
    float step = accuracyM * 0.5f;
    if (step < MIN_STEP_M) step = MIN_STEP_M;
    if (step > MAX_STEP_M) step = MAX_STEP_M;
    return step;
}

bool isWantedLocked(const String& id) {
    return std::find(s_wantedIds.begin(), s_wantedIds.end(), id) != s_wantedIds.end();
}

// Wanted-Liste aus den Anmeldungen neu bilden. true, wenn etwas dazukam
// (neue Entitaet oder laengeres Fenster) - dann muss geladen werden.
bool recomputeWantedLocked() {
    std::vector<String> ids;
    uint32_t seconds = 0;
    for (const Owner& o : s_owners) {
        for (const String& id : o.ids) {
            if (id.startsWith("zone.")) continue;
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        }
        if (o.seconds > seconds) seconds = o.seconds;
    }
    bool grew = seconds > s_wantedSeconds;
    for (const String& id : ids) {
        if (!isWantedLocked(id)) { grew = true; break; }
    }
    s_wantedIds = std::move(ids);
    s_wantedSeconds = seconds;
    return grew;
}

// Verwirft Punkte ausserhalb des Fensters und ueber der Obergrenze. Die
// Abdeckung schrumpft mit. Unter Lock.
void trimLocked(EntityTrack& e, time_t now) {
    if (s_wantedSeconds == 0) return;   // keiner angemeldet: nichts verwerfen
    Track& t = e.pts;
    const time_t oldestTime = now - (time_t)s_wantedSeconds;
    const uint32_t oldest = (uint32_t)oldestTime;
    size_t drop = 0;
    while (drop < t.size() && t[drop].ts < oldest) drop++;
    // Den letzten Punkt vor dem Fenster behalten: er ist der Startpunkt des
    // sichtbaren Weges (Position zu Beginn des Fensters).
    if (drop > 0) drop--;
    if (t.size() - drop > HaTrackHistory::MAX_POINTS) {
        drop = t.size() - HaTrackHistory::MAX_POINTS;
        // Punkte innerhalb des Fensters fallen weg: Abdeckung ab dem aeltesten verbleibenden.
        if (e.covFrom < (time_t)t[drop].ts) e.covFrom = (time_t)t[drop].ts;
    }
    if (drop > 0) t.erase(t.begin(), t.begin() + drop);
    if (e.covFrom < oldestTime) e.covFrom = oldestTime;
}

// Mischt geladene Punkte in einen Weg: nach Zeit sortieren, nahe Punkte in
// Folge zusammenfassen (erste und letzte Meldung bleiben), kuerzen.
// Abschnitte kommen rueckwaerts an, Live-Punkte jederzeit - die Reihenfolge
// ergibt sich erst hier. Unter Lock.
void mergeLocked(EntityTrack& e, const Track& add, time_t now) {
    Track all;
    all.reserve(e.pts.size() + add.size());
    all.insert(all.end(), e.pts.begin(), e.pts.end());
    all.insert(all.end(), add.begin(), add.end());
    std::stable_sort(all.begin(), all.end(),
                     [](const HaTrackPoint& a, const HaTrackPoint& b) { return a.ts < b.ts; });

    Track out;
    out.reserve(all.size());
    for (const HaTrackPoint& p : all) {
        if (!out.empty() && !p.brk && distanceM(out.back().lat, out.back().lon, p.lat, p.lon) < MERGE_SAME_M) {
            if (p.tsLast > out.back().tsLast) out.back().tsLast = p.tsLast;
            continue;
        }
        out.push_back(p);
    }
    e.pts = std::move(out);
    trimLocked(e, now);
}

// Live-Abdeckung beenden: vollstaendig bis zum letzten Tick mit Verbindung.
void closeLiveCoverage() {
    time_t now;
    nowValid(now);
    const time_t end = s_lastAlive > 0 ? s_lastAlive : now;
    Lock lock;
    if (!lock.ok()) return;
    for (auto& kv : s_tracks) {
        if (!kv.second.covLive) continue;
        kv.second.covLive = false;
        kv.second.covTo = end;
    }
}

// "2026-11-13T15:04:05+00:00"
String isoUtc(time_t t) {
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
    return String(buf);
}

// "3 h 05 min", "12 min", "40 s"
String formatSpan(uint32_t s) {
    char buf[24];
    if (s >= 3600)    snprintf(buf, sizeof(buf), "%u h %02u min", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
    else if (s >= 60) snprintf(buf, sizeof(buf), "%u min", (unsigned)(s / 60));
    else              snprintf(buf, sizeof(buf), "%u s", (unsigned)s);
    return String(buf);
}

// Liest einen Abschnitt (komprimiertes Format von history_during_period):
//   {"result": {"device_tracker.x": [{"s":..,"a":{..},"lu":1731500000.1}, ...]}}
// "lu" = last_updated (immer vorhanden), "lc" = last_changed (nur wenn abweichend).
// Mischt die Positionen in den Weg der Entitaet. Zustaende ohne Koordinaten
// (ausser Reichweite, unavailable) unterbrechen den Weg. Rueckgabe: Anzahl
// der Zustaende, -1 = nicht lesbar. 'full': Weg hat die Punkte-Obergrenze
// erreicht (aeltere Abschnitte fielen sofort wieder heraus).
int storeSlice(const String& entityId, const String& payload, bool& full) {
    full = false;

    JsonDocument filter;
    JsonObject entry = filter["result"]["*"][0].to<JsonObject>();
    entry["a"]["latitude"] = true;
    entry["a"]["longitude"] = true;
    entry["a"]["gps_accuracy"] = true;
    entry["lu"] = true;

    // Parsen dauert bei grossen Antworten spuerbar (bis ~1 MB). Prioritaet
    // UNTER die der Oberflaeche (loopTask, 1) auf demselben Kern: die
    // Bedienung geht vor, geparst wird in ihren Pausen.
    const UBaseType_t ownPrio = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
    JsonDocument doc(HaPsramAlloc());
    const DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    vTaskPrioritySet(NULL, ownPrio);

    if (err || doc.overflowed()) {
        Serial.printf("[HA-Verlauf] %s: Antwort nicht lesbar: %s (%u Bytes)\n", entityId.c_str(),
                      err ? err.c_str() : "kein Speicher", (unsigned)payload.length());
        return -1;
    }

    const JsonArrayConst states = doc["result"][entityId].as<JsonArrayConst>();
    Track loaded;
    bool breakNext = false;
    uint32_t trailingGapTs = 0;   // Zustand ohne Position am Ende des Abschnitts
    for (JsonObjectConst st : states) {
        const uint32_t ts = (uint32_t)(st["lu"] | 0.0);
        if (!st["a"]["latitude"].is<double>() || !st["a"]["longitude"].is<double>()) {
            breakNext = true;
            trailingGapTs = ts;
            continue;
        }
        const double lat = st["a"]["latitude"].as<double>();
        const double lon = st["a"]["longitude"].as<double>();
        const float  acc = st["a"]["gps_accuracy"] | 0.0f;
        trailingGapTs = 0;

        if (!breakNext && !loaded.empty() &&
            distanceM(loaded.back().lat, loaded.back().lon, lat, lon) < stepFor(acc)) {
            loaded.back().tsLast = ts;   // im Stand: nur die letzte Meldung merken
            continue;
        }
        loaded.push_back({ (float)lat, (float)lon, ts, ts, breakNext });
        breakNext = false;
    }

    time_t now;
    nowValid(now);
    Lock lock;
    if (lock.ok()) {
        EntityTrack& e = s_tracks[entityId];
        if (!loaded.empty()) mergeLocked(e, loaded, now);
        // Endet der Abschnitt ohne Position, liegt die Unterbrechung vor dem
        // ersten Punkt eines neueren Abschnitts (schon geladen - rueckwaerts).
        if (trailingGapTs > 0) {
            for (HaTrackPoint& p : e.pts) {
                if (p.ts > trailingGapTs) { p.brk = true; break; }
            }
        }
        full = e.pts.size() >= HaTrackHistory::MAX_POINTS;
    }
    return (int)states.size();
}

// Schon vorliegenden Zeitraum ueberspringen, sobald der Cursor ihn erreicht.
void skipCovered() {
    if (s_plan.hasCov && s_plan.cursor <= s_plan.covEnd && s_plan.cursor > s_plan.covFrom) {
        s_plan.cursor = s_plan.covFrom;
        s_plan.hasCov = false;
    }
}

// Setzt den Plan auf die Entitaet s_plan.index (Zeitfenster ab jetzt).
void beginEntity() {
    time_t now;
    nowValid(now);
    s_plan.loadEnd = now;
    s_plan.windowStart = now - (time_t)s_plan.seconds;
    s_plan.cursor = now;
    s_plan.sliceStart = now;
    s_plan.sliceS = PROBE_SLICE_S;
    s_plan.slices = 0;
    s_plan.states = 0;
    s_plan.bytes = 0;
    s_plan.maxRate = 0;
    s_plan.startedMs = millis();

    // Abdeckung: diesen Zeitraum nicht noch einmal laden.
    s_plan.hasCov = false;
    {
        Lock lock;
        if (lock.ok()) {
            auto it = s_tracks.find(s_plan.ids[s_plan.index]);
            if (it != s_tracks.end()) {
                const EntityTrack& e = it->second;
                const time_t end = e.covLive ? now : e.covTo;
                if (end > e.covFrom) {
                    s_plan.hasCov = true;
                    s_plan.covFrom = e.covFrom;
                    s_plan.covEnd = end;
                }
            }
        }
    }
    skipCovered();
}

// Weiter zur naechsten Entitaet (oder Plan fertig).
void nextEntity() {
    s_plan.index++;
    if (s_plan.index < s_plan.ids.size()) {
        beginEntity();
    } else {
        s_plan.active = false;
    }
}

// Schliesst die aktuelle Entitaet ab (Log, Abdeckung) und geht zur naechsten.
// 'reason' != nullptr: vorzeitig beendet, der Weg ist kuerzer als gewuenscht.
// 'acceptShorter': die Kuerzung ist endgueltig (Datenmenge, Punkte-Obergrenze)
// - die Abdeckung gilt dann fuer das ganze Fenster, sonst wuerde jedes
// Oeffnen der HA-App den aelteren Teil erneut (und wieder vergeblich) laden.
void finishEntity(const char* reason, bool acceptShorter = false) {
    if (!s_plan.active || s_plan.index >= s_plan.ids.size()) return;
    const String& id = s_plan.ids[s_plan.index];

    size_t points = 0;
    {
        Lock lock;
        if (lock.ok()) {
            EntityTrack& e = s_tracks[id];
            // Vollstaendig liegt jetzt [cursor, loadEnd] vor (zusammenhaengend,
            // auch ueber den uebersprungenen Teil). Ohne einen einzigen
            // geladenen Abschnitt bei Abbruch: alte Abdeckung behalten.
            if (s_plan.slices > 0 || reason == nullptr) {
                e.covFrom = acceptShorter ? s_plan.windowStart : s_plan.cursor;
                e.covTo = s_plan.loadEnd;
                e.covLive = true;   // ab jetzt laufen Live-Punkte ein
            }
            points = e.pts.size();
        }
    }

    // Nur loggen, wenn wirklich geladen wurde (Luecke leer = still).
    if (s_plan.slices > 0 || reason) {
        const uint32_t loadedS = (uint32_t)(s_plan.loadEnd - s_plan.cursor);
        Serial.printf("[HA-Verlauf] %s: %s abgedeckt, %u Abschnitte, %u Zustaende, %u KB "
                      "(%u B/Zustand, max %u B/s), %u Punkte, %lu ms\n",
                      id.c_str(), formatSpan(loadedS).c_str(), (unsigned)s_plan.slices,
                      (unsigned)s_plan.states, (unsigned)(s_plan.bytes / 1024),
                      (unsigned)(s_plan.states ? s_plan.bytes / s_plan.states : 0),
                      (unsigned)s_plan.maxRate, (unsigned)points,
                      (unsigned long)(millis() - s_plan.startedMs));
        if (reason) {
            Serial.printf("[HA-Verlauf] %s: abgebrochen (%s) - Weg nur ueber die letzten %s statt %s.\n",
                          id.c_str(), reason, formatSpan(loadedS).c_str(), formatSpan(s_plan.seconds).c_str());
        }
        if (s_plan.states >= 10 && s_plan.bytes / s_plan.states > LARGE_STATE_BYTES) {
            if (id.startsWith("device_tracker.") || id.startsWith("person.")) {
                Serial.printf("[HA-Verlauf] Hinweis: %s liefert grosse Attribute (%u B je Zustand) - "
                              "lange Wege kosten entsprechend Daten.\n",
                              id.c_str(), (unsigned)(s_plan.bytes / s_plan.states));
            } else {
                Serial.printf("[HA-Verlauf] Tipp: %s liefert grosse Attribute (%u B je Zustand). "
                              "Fuer die Karte besser den zugehoerigen device_tracker verwenden.\n",
                              id.c_str(), (unsigned)(s_plan.bytes / s_plan.states));
            }
        }
        touch();   // je Entitaet einmal neu zeichnen, nicht je Abschnitt
    }
    nextEntity();
}

// Laenge des naechsten Abschnitts aus der hoechsten gemessenen Rate.
void planNextSlice(uint32_t lastSliceS) {
    uint32_t next = s_plan.maxRate > 0 ? TARGET_RESPONSE_BYTES / s_plan.maxRate : MAX_SLICE_S;
    if (next > lastSliceS * MAX_SLICE_GROWTH) next = lastSliceS * MAX_SLICE_GROWTH;
    if (next > MAX_SLICE_S) next = MAX_SLICE_S;
    if (next < MIN_SLICE_S) next = MIN_SLICE_S;
    s_plan.sliceS = next;
}

// Neuen Ladevorgang aus den angemeldeten Entitaeten/Fenster aufsetzen.
// false: noch keine Uhrzeit oder nichts zu tun.
bool startPlan() {
    time_t now;
    if (!nowValid(now)) return false;   // ohne Uhrzeit kein Zeitfenster

    // Vor dem Kopieren zuruecksetzen: eine Anmeldung, die waehrenddessen
    // kommt, loest danach einen weiteren Durchlauf aus (statt verloren zu gehen).
    s_requested = false;
    {
        Lock lock;
        if (!lock.ok()) return false;
        s_plan.ids = s_wantedIds;
        s_plan.seconds = s_wantedSeconds;
    }
    s_plan.index = 0;
    s_plan.active = !s_plan.ids.empty() && s_plan.seconds > 0;
    if (!s_plan.active) return false;

    beginEntity();
    return true;
}

// Fordert den naechsten Abschnitt der aktuellen Entitaet an.
void sendSlice() {
    const String& id = s_plan.ids[s_plan.index];

    // Speicher: ohne Reserve nicht weiterladen (Video und Kacheln gehen vor).
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (largest < MIN_FREE_PSRAM_BLOCK) {
        Serial.printf("[HA-Verlauf] Speicher knapp (groesster PSRAM-Block %u KB) - Laden beendet.\n",
                      (unsigned)(largest / 1024));
        // Alle restlichen Entitaeten mit dem bisher Geladenen abschliessen.
        while (s_plan.active) finishEntity("Speicher knapp");
        return;
    }

    uint32_t len = s_plan.sliceS;
    if (s_plan.maxRate > 0) {
        // Erwartete Antwort muss mehrfach in den groessten Block passen.
        const uint32_t byMemory = (uint32_t)(largest / RESPONSE_COPIES / s_plan.maxRate);
        if (len > byMemory) len = byMemory < MIN_SLICE_S ? MIN_SLICE_S : byMemory;
    }

    time_t start = s_plan.cursor - (time_t)len;
    // Nicht in einen schon vorliegenden Zeitraum hineinladen.
    if (s_plan.hasCov && s_plan.cursor > s_plan.covEnd && start < s_plan.covEnd) start = s_plan.covEnd;
    bool oldest = false;
    if (start <= s_plan.windowStart) {
        start = s_plan.windowStart;
        oldest = true;
    }
    s_plan.sliceStart = start;

    JsonDocument req(HaPsramAlloc());
    req["type"] = "history/history_during_period";
    req["start_time"] = isoUtc(start);
    req["end_time"] = isoUtc(s_plan.cursor);
    req["entity_ids"].to<JsonArray>().add(id);
    // Position zu Beginn des Fensters - nur beim aeltesten Abschnitt, sonst
    // kaeme sie bei jedem Abschnitt doppelt.
    req["include_start_time_state"] = oldest;
    req["significant_changes_only"] = false;  // reine Attributaenderungen (Position) mitnehmen
    req["minimal_response"] = false;
    req["no_attributes"] = false;

    s_requestId = HaWebsocketLogic_SendRequest(req);
    if (s_requestId != 0) s_sentAt = millis();
    // Nicht gesendet: beim naechsten Tick erneut.
}

} // namespace

// ------------------------------------------------------------------------------
// GUI-Seite
// ------------------------------------------------------------------------------
void HaTrackHistory::Init() {
    if (s_mutex == nullptr) s_mutex = xSemaphoreCreateMutex();
}

void HaTrackHistory::Register(const void* owner, const std::vector<String>& entityIds, uint32_t seconds) {
    if (seconds == 0) {
        Unregister(owner);
        return;
    }
    if (seconds > MAX_WINDOW_S) seconds = MAX_WINDOW_S;

    bool grew;
    {
        Lock lock;
        if (!lock.ok()) return;
        auto it = std::find_if(s_owners.begin(), s_owners.end(), [&](const Owner& o) { return o.key == owner; });
        if (it == s_owners.end()) {
            s_owners.push_back({ owner, entityIds, seconds });
        } else {
            it->ids = entityIds;
            it->seconds = seconds;
        }
        grew = recomputeWantedLocked();
    }
    if (grew) s_requested = true;
}

void HaTrackHistory::Unregister(const void* owner) {
    Lock lock;
    if (!lock.ok()) return;
    s_owners.erase(std::remove_if(s_owners.begin(), s_owners.end(),
                                  [&](const Owner& o) { return o.key == owner; }),
                   s_owners.end());
    recomputeWantedLocked();   // schrumpft nur - nichts zu laden
}

void HaTrackHistory::Append(const String& entityId, double lat, double lon, float accuracyM, uint32_t ts) {
    time_t now;
    if (!nowValid(now)) return;
    if (ts == 0 || ts > (uint32_t)now) ts = (uint32_t)now;

    Lock lock;
    if (!lock.ok()) return;
    if (!isWantedLocked(entityId)) return;

    EntityTrack& e = s_tracks[entityId];
    Track& t = e.pts;
    if (!t.empty() && ts < t.back().ts) return;   // aelter als der Weg (kommt aus dem Verlauf)
    if (!e.pendingBreak && !t.empty() && distanceM(t.back().lat, t.back().lon, lat, lon) < stepFor(accuracyM)) {
        if (ts > t.back().tsLast) t.back().tsLast = ts;   // im Stand: kein neuer Punkt
        return;
    }
    t.push_back({ (float)lat, (float)lon, ts, ts, e.pendingBreak });
    e.pendingBreak = false;
    trimLocked(e, now);
    touch();
}

void HaTrackHistory::MarkGap(const String& entityId) {
    Lock lock;
    if (!lock.ok() || !isWantedLocked(entityId)) return;
    auto it = s_tracks.find(entityId);
    if (it != s_tracks.end() && !it->second.pts.empty()) it->second.pendingBreak = true;
}

std::vector<HaTrackPoint> HaTrackHistory::Get(const String& entityId, uint32_t seconds, uint32_t gapSeconds) {
    std::vector<HaTrackPoint> out;
    time_t now;
    if (!nowValid(now) || seconds == 0) return out;

    Lock lock;
    if (!lock.ok()) return out;
    auto it = s_tracks.find(entityId);
    if (it == s_tracks.end()) return out;

    const uint32_t oldest = (uint32_t)now - seconds;
    const Track& t = it->second.pts;
    // Ab dem letzten Punkt vor dem Fenster (Startposition des Weges).
    size_t first = 0;
    while (first + 1 < t.size() && t[first + 1].ts < oldest) first++;
    out.assign(t.begin() + first, t.end());

    // Funkstille: zwischen der letzten Meldung an einem Punkt und der ersten
    // am naechsten lag zu viel Zeit - nicht verbinden.
    for (size_t i = 1; i < out.size(); i++) {
        if (gapSeconds > 0 && out[i].ts > out[i - 1].tsLast + gapSeconds) out[i].brk = true;
    }
    if (!out.empty()) out.front().brk = false;
    return out;
}

bool HaTrackHistory::GetLast(const String& entityId, HaTrackPoint& out) {
    Lock lock;
    if (!lock.ok()) return false;
    auto it = s_tracks.find(entityId);
    if (it == s_tracks.end() || it->second.pts.empty()) return false;
    out = it->second.pts.back();
    return true;
}

uint32_t HaTrackHistory::GetVersion() {
    return s_version;
}

void HaTrackHistory::SetHold(bool hold) {
    if (hold == s_hold) return;
    s_holdChangedAt = millis();   // zuerst die Zeit, dann der Zustand
    s_hold = hold;
}

// ------------------------------------------------------------------------------
// HA_WS_Task
// ------------------------------------------------------------------------------
void HaTrackHistory::OnAuthenticated() {
    // Neue Verbindung: Waehrend der Trennung kamen keine Live-Punkte. Die
    // Abdeckung endet beim letzten Tick der alten Verbindung; nur die Luecke
    // seitdem wird nachgeladen. Ein laufender Abschnitt ist verloren.
    closeLiveCoverage();
    s_requestId = 0;
    s_plan.active = false;
    Lock lock;
    if (lock.ok() && !s_wantedIds.empty()) s_requested = true;
}

void HaTrackHistory::Tick() {
    time_t now;
    if (nowValid(now)) s_lastAlive = now;   // nur mit angemeldeter Verbindung aufgerufen

    // Abschnitt unterwegs: nur auf Zeitueberschreitung achten.
    if (s_requestId != 0) {
        if (millis() - s_sentAt > SLICE_TIMEOUT_MS) {
            s_requestId = 0;
            finishEntity("keine Antwort");
        }
        return;
    }

    // Neue Anmeldung (Entitaet, laengeres Fenster) oder neue Verbindung:
    // Plan neu aufsetzen. Vorliegende Zeitraeume werden dabei uebersprungen.
    if (s_requested) startPlan();

    if (!s_plan.active) return;
    if (millis() - s_lastResultMs < SLICE_GAP_MS) return;

    // Oberflaeche baut auf: noch nicht laden (Antworten auf laufende
    // Abschnitte werden weiter angenommen).
    const uint32_t sinceHoldChange = millis() - s_holdChangedAt;
    if (s_hold ? sinceHoldChange < HOLD_MAX_MS : sinceHoldChange < HOLD_GRACE_MS) return;

    // Inzwischen von keiner Karte mehr angezeigt: nicht laden.
    bool wanted;
    {
        Lock lock;
        wanted = lock.ok() && isWantedLocked(s_plan.ids[s_plan.index]);
    }
    if (!wanted) {
        nextEntity();   // still uebergehen, Abdeckung bleibt
        return;
    }
    if (s_plan.cursor <= s_plan.windowStart) {
        finishEntity(nullptr);   // nichts (mehr) zu laden
        return;
    }
    sendSlice();
}

bool HaTrackHistory::HandleMessage(const String& type, uint32_t id, bool success, const String& payload) {
    if (id == 0 || id != s_requestId || type != "result") return false;
    s_requestId = 0;
    s_lastResultMs = millis();
    if (!s_plan.active || s_plan.index >= s_plan.ids.size()) return true;

    if (!success) {
        finishEntity("von HA abgelehnt");
        return true;
    }

    const String& entityId = s_plan.ids[s_plan.index];
    const uint32_t sliceS = s_plan.cursor > s_plan.sliceStart ? (uint32_t)(s_plan.cursor - s_plan.sliceStart) : 1;

    bool full = false;
    const int states = storeSlice(entityId, payload, full);
    if (states < 0) {
        finishEntity("Antwort nicht lesbar");
        return true;
    }

    // Statistik und Datenrate
    const uint32_t bytes = (uint32_t)payload.length();
    const uint32_t rate = bytes / sliceS;
    s_plan.slices++;
    s_plan.states += (uint32_t)states;
    s_plan.bytes += bytes;
    if (rate > s_plan.maxRate) s_plan.maxRate = rate;
    if (bytes >= LOG_LARGE_RESPONSE_BYTES) {
        Serial.printf("[HA-Verlauf] %s: grosse Antwort %u KB fuer %u s (%d Zustaende)\n",
                      entityId.c_str(), (unsigned)(bytes / 1024), (unsigned)sliceS, states);
    }

    // Weiter rueckwaerts, vorliegenden Zeitraum ueberspringen.
    s_plan.cursor = s_plan.sliceStart;
    skipCovered();
    if (s_plan.cursor <= s_plan.windowStart) {
        finishEntity(nullptr);                       // ganzes Fenster liegt vor
    } else if (s_plan.bytes >= ENTITY_BUDGET_BYTES) {
        finishEntity("Datenmenge", true);
    } else if (full) {
        finishEntity("Punkte-Obergrenze", true);
    } else {
        planNextSlice(sliceS);
    }
    return true;
}

void HaTrackHistory::OnDisconnected() {
    closeLiveCoverage();
    s_requested = false;
    s_requestId = 0;
    s_plan.active = false;
    s_plan.ids.clear();
}
