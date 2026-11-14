// ==============================================================================
// HaEntityCache - Implementierung (siehe Header)
// ==============================================================================

#include "HaEntityCache.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "HaPsramAllocator.h"
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <esp_heap_caps.h>
#include <algorithm>

// Letzter Stand im Flash (siehe Header).
static const char* const SNAPSHOT_PATH = "/ha_states.json";
static const char* const SNAPSHOT_TMP_PATH = "/ha_states.tmp";
static const char* const FS_PARTITION_LABEL = "spiffs";
static const int SNAPSHOT_FORMAT = 1;
// Aenderungszaehler je Entitaet im Abbild (Wert von cacheVersion bei der
// letzten Aenderung). Die Oberflaeche aktualisiert damit nur Widgets, deren
// Entitaet sich geaendert hat - nicht alle bei jeder Aenderung im Haus.
static const char* const VERSION_KEY = "#";
// Markierung im Abbild: stammt nicht aus der laufenden Verbindung.
// ApplyFull leert das Abbild und entfernt sie damit.
static const char* const STALE_KEY = "~";
// Groessere Dateien gelten als defekt (zum Vergleich: ~200 B je Entitaet).
static const size_t SNAPSHOT_MAX_BYTES = 512UL * 1024UL;
// Hoechstens so oft sichern (Flash schonen; Schreiben dauert 0,4-0,7 s).
// Ein paar Minuten alter Stand reicht: er wird nur bis zu den Live-Daten
// gezeigt. Das erste Sichern nach dem Booten ist immer erlaubt.
static const uint32_t SNAPSHOT_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL;
static uint32_t s_lastSnapshotMs = 0;
static bool     s_snapshotSavedThisBoot = false;

volatile uint32_t HaEntityCache::cacheVersion = 0;

SemaphoreHandle_t HaEntityCache::mutex = nullptr;
std::vector<String> HaEntityCache::trackedEntities;
std::map<String, JsonDocument> HaEntityCache::entities;
uint32_t HaEntityCache::savedVersion = 0;

// ------------------------------------------------------------------------------
// Verwaltung
// ------------------------------------------------------------------------------
void HaEntityCache::Init()
{
    // Nur einmal anlegen: Init() laeuft bei jedem Betreten des HA-Screens.
    if (mutex == nullptr)
        mutex = xSemaphoreCreateMutex();
}

// ------------------------------------------------------------------------------
// Letzter Stand
// ------------------------------------------------------------------------------
void HaEntityCache::MarkAllStale()
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
    for (auto &kv : entities) kv.second[STALE_KEY] = true;
    xSemaphoreGive(mutex);
}

bool HaEntityCache::IsLive(String entity_id)
{
    return read<bool>(entity_id, false, [](JsonObjectConst e, JsonObjectConst) {
        return e[STALE_KEY].isNull();
    });
}

void HaEntityCache::DropIfStale(const char *entity_id)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
    auto it = entities.find(entity_id);
    if (it != entities.end() && !it->second[STALE_KEY].isNull())
    {
        entities.erase(it);
        touch();
    }
    xSemaphoreGive(mutex);
}

void HaEntityCache::SaveSnapshot()
{
    if (!mutex || !esp_littlefs_mounted(FS_PARTITION_LABEL)) return;
    if (s_snapshotSavedThisBoot && millis() - s_lastSnapshotMs < SNAPSHOT_MIN_INTERVAL_MS) return;
    const uint32_t t0 = millis();

    // Unter dem Mutex nur kopieren, geschrieben wird ohne ihn.
    JsonDocument doc(HaPsramAlloc());
    uint32_t version;
    size_t count = 0;
    {
        if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
        version = cacheVersion;
        if (version == savedVersion)
        {
            xSemaphoreGive(mutex);
            return;   // nichts geaendert
        }
        doc["v"] = SNAPSHOT_FORMAT;
        JsonObject e = doc["e"].to<JsonObject>();
        for (const auto &kv : entities)
        {
            // Nur platzierte Entitaeten (Prune laeuft erst beim naechsten Abo).
            if (std::find(trackedEntities.begin(), trackedEntities.end(), kv.first) == trackedEntities.end()) continue;
            e[kv.first.c_str()] = kv.second.as<JsonObjectConst>();
            count++;
        }
        xSemaphoreGive(mutex);
    }
    if (doc.overflowed())
    {
        Serial.println("[HA-Cache] Sichern abgebrochen: kein Speicher.");
        return;
    }

    File f = LittleFS.open(SNAPSHOT_TMP_PATH, "w");
    if (!f)
    {
        Serial.println("[HA-Cache] Sichern: Datei nicht anlegbar.");
        return;
    }
    const size_t expected = measureJson(doc);
    const size_t written = serializeJson(doc, f);
    f.close();
    if (written != expected)
    {
        LittleFS.remove(SNAPSHOT_TMP_PATH);
        Serial.println("[HA-Cache] Sichern: Flash voll?");
        return;
    }
    LittleFS.remove(SNAPSHOT_PATH);
    if (!LittleFS.rename(SNAPSHOT_TMP_PATH, SNAPSHOT_PATH))
    {
        Serial.println("[HA-Cache] Sichern: Umbenennen fehlgeschlagen.");
        return;
    }
    savedVersion = version;
    s_lastSnapshotMs = millis();
    s_snapshotSavedThisBoot = true;
    Serial.printf("[HA-Cache] Letzter Stand gesichert: %u Entitaeten, %u Bytes, %lu ms.\n",
                  (unsigned)count, (unsigned)written, (unsigned long)(millis() - t0));
}

void HaEntityCache::LoadSnapshot()
{
    Init();
    if (!esp_littlefs_mounted(FS_PARTITION_LABEL) || !LittleFS.exists(SNAPSHOT_PATH)) return;
    const uint32_t t0 = millis();

    // Am Stueck in den PSRAM lesen und dann parsen (schneller als Byte fuer
    // Byte aus der Datei).
    File f = LittleFS.open(SNAPSHOT_PATH, "r");
    if (!f) return;
    const size_t size = f.size();
    if (size == 0 || size > SNAPSHOT_MAX_BYTES)
    {
        f.close();
        Serial.printf("[HA-Cache] Letzter Stand ignoriert (%u Bytes).\n", (unsigned)size);
        return;
    }
    char *buf = (char *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf)
    {
        f.close();
        return;
    }
    const size_t got = f.read((uint8_t *)buf, size);
    f.close();

    JsonDocument doc(HaPsramAlloc());
    const DeserializationError err = deserializeJson(doc, buf, got);
    heap_caps_free(buf);
    if (err || (doc["v"] | 0) != SNAPSHOT_FORMAT)
    {
        Serial.printf("[HA-Cache] Letzter Stand nicht lesbar (%s) - wird ignoriert.\n", err ? err.c_str() : "Format");
        return;
    }

    size_t count = 0;
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
    for (JsonPairConst kv : doc["e"].as<JsonObjectConst>())
    {
        if (entities.find(kv.key().c_str()) != entities.end()) continue;   // schon live
        JsonDocument &snap = snapshotLocked(kv.key().c_str());
        snap.set(kv.value());
        snap[STALE_KEY] = true;
        snap[VERSION_KEY] = (uint32_t)(cacheVersion + 1);   // = Wert nach touch() unten
        count++;
    }
    touch();
    savedVersion = cacheVersion;   // geladener Stand ist gesichert
    xSemaphoreGive(mutex);

    Serial.printf("[HA-Cache] Letzter Stand geladen: %u Entitaeten, %u Bytes, %lu ms.\n",
                  (unsigned)count, (unsigned)got, (unsigned long)(millis() - t0));
}

// Sammelt alle Entitaeten eines Widgets: eigene, Bedingungen, Ordnerkinder.
static void recursivelyAddTrackedEntities(const HAWidgetDef &w, std::vector<String> &tracked)
{
    if (w.entity_id.length() > 0 && std::find(tracked.begin(), tracked.end(), w.entity_id) == tracked.end())
    {
        tracked.push_back(w.entity_id);
    }
    for (const auto &c : w.conditions)
    {
        if (c.entity.length() > 0 && std::find(tracked.begin(), tracked.end(), c.entity) == tracked.end())
        {
            tracked.push_back(c.entity);
        }
    }
    for (const auto &child : w.children)
    {
        recursivelyAddTrackedEntities(child, tracked);
    }
    // Karte: alle dargestellten Tracker und Zonen
    for (const auto &id : w.map_entities)
    {
        if (id.length() > 0 && std::find(tracked.begin(), tracked.end(), id) == tracked.end())
        {
            tracked.push_back(id);
        }
    }
}

void HaEntityCache::UpdateTrackedEntities()
{
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY))
    {
        trackedEntities.clear();
        for (const auto &tab : HaConfigLogic::dashboards)
        {
            for (const auto &w : tab.widgets)
            {
                recursivelyAddTrackedEntities(w, trackedEntities);
            }
        }
        xSemaphoreGive(mutex);
    }
    // Das Neu-Abonnieren stoesst der Aufrufer an (HaWebsocketLogic_UpdateTrackedEntities
    // -> HaEntitySync::RequestResync). Der Cache selbst kennt die Verbindung nicht.
}

std::vector<String> HaEntityCache::GetTrackedEntities()
{
    // Kopie unter dem Mutex: der loopTask baut die Liste beim Speichern neu auf,
    // waehrend der HA_WS_Task sie liest.
    std::vector<String> copy;
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY))
    {
        copy = trackedEntities;
        xSemaphoreGive(mutex);
    }
    return copy;
}

uint32_t HaEntityCache::GetCacheVersion() { return cacheVersion; }

void HaEntityCache::touch()
{
    cacheVersion = cacheVersion + 1;   // volatile: kein ++ (C++20-Warnung)
}

void HaEntityCache::touchEntity(JsonDocument &snap)
{
    touch();
    snap[VERSION_KEY] = (uint32_t)cacheVersion;
}

uint32_t HaEntityCache::GetEntityVersion(String entity_id)
{
    return read<uint32_t>(entity_id, 0, [](JsonObjectConst e, JsonObjectConst) {
        return e[VERSION_KEY] | (uint32_t)0;
    });
}

// ------------------------------------------------------------------------------
// Schreiben (HA_WS_Task)
// ------------------------------------------------------------------------------
JsonDocument &HaEntityCache::snapshotLocked(const char *entity_id)
{
    auto it = entities.find(entity_id);
    if (it == entities.end())
    {
        it = entities.emplace(String(entity_id), JsonDocument(HaPsramAlloc())).first;
    }
    return it->second;
}

void HaEntityCache::ApplyFull(const char *entity_id, JsonObjectConst entity)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    snap.clear();
    snap["s"] = entity["s"];
    if (!entity["lc"].isNull()) snap["lc"] = entity["lc"];
    if (!entity["lu"].isNull()) snap["lu"] = entity["lu"];
    if (entity["a"].is<JsonObjectConst>()) snap["a"] = entity["a"];
    else                                   snap["a"].to<JsonObject>();
    touchEntity(snap);

    xSemaphoreGive(mutex);
}

void HaEntityCache::ApplyDiff(const char *entity_id, JsonObjectConst change)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    JsonObject attrs = snap["a"].is<JsonObject>() ? snap["a"].as<JsonObject>()
                                                  : snap["a"].to<JsonObject>();

    // "+": geaenderte Werte (nur die geaenderten Attribute!)
    JsonObjectConst plus = change["+"];
    if (!plus.isNull())
    {
        if (!plus["s"].isNull()) snap["s"] = plus["s"];
        // Zeitstempel: "lu" fehlt, wenn er gleich "lc" ist (HA-Kompaktformat).
        if (!plus["lc"].isNull()) snap["lc"] = plus["lc"];
        if (!plus["lu"].isNull())      snap["lu"] = plus["lu"];
        else if (!plus["lc"].isNull()) snap.remove("lu");
        for (JsonPairConst kv : plus["a"].as<JsonObjectConst>())
        {
            attrs[kv.key()] = kv.value();
        }
    }

    // "-": entfallene Attribute (z. B. brightness, wenn ein Licht ausgeht)
    for (JsonVariantConst key : change["-"]["a"].as<JsonArrayConst>())
    {
        attrs.remove(key.as<const char *>());
    }
    touchEntity(snap);

    xSemaphoreGive(mutex);
}

void HaEntityCache::ApplyRemoved(const char *entity_id)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    snap.clear();
    snap["s"] = "unavailable";
    snap["a"].to<JsonObject>();
    touchEntity(snap);

    xSemaphoreGive(mutex);
}

void HaEntityCache::Prune(const std::vector<String> &keep)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    for (auto it = entities.begin(); it != entities.end();)
    {
        if (std::find(keep.begin(), keep.end(), it->first) == keep.end()) it = entities.erase(it);
        else ++it;
    }

    xSemaphoreGive(mutex);
}

// ------------------------------------------------------------------------------
// Lesen (GUI)
// ------------------------------------------------------------------------------
JsonObjectConst HaEntityCache::findLocked(const String &entity_id)
{
    auto it = entities.find(entity_id);
    return (it == entities.end()) ? JsonObjectConst() : it->second.as<JsonObjectConst>();
}

template <typename T, typename F>
T HaEntityCache::read(const String &entity_id, T fallback, F fn)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return fallback;
    JsonObjectConst e = findLocked(entity_id);
    T res = e.isNull() ? fallback : fn(e, e["a"].as<JsonObjectConst>());
    xSemaphoreGive(mutex);
    return res;
}

// Liest ein JSON-Array von Zeichenketten.
static std::vector<String> toStringList(JsonVariantConst v)
{
    std::vector<String> out;
    for (JsonVariantConst item : v.as<JsonArrayConst>()) out.push_back(item.as<String>());
    return out;
}

// Prueft, ob supported_color_modes einen der Modi enthaelt.
static bool hasColorMode(JsonObjectConst a, std::initializer_list<const char *> modes)
{
    for (JsonVariantConst v : a["supported_color_modes"].as<JsonArrayConst>())
    {
        const char *m = v.as<const char *>();
        if (!m) continue;
        for (const char *wanted : modes)
        {
            if (strcmp(m, wanted) == 0) return true;
        }
    }
    return false;
}

static bool isClimate(const String &id) { return id.startsWith("climate."); }

// Ganzzahl aus einem Zahlwert, egal ob HA ihn als 80 oder 80.0 liefert.
static int intOr(JsonVariantConst v, int fallback)
{
    return v.is<float>() ? (int)lroundf(v.as<float>()) : fallback;
}

String HaEntityCache::GetState(String id)
{
    return read<String>(id, "", [](JsonObjectConst e, JsonObjectConst) { return String(e["s"] | ""); });
}

String HaEntityCache::GetCachedIcon(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["icon"] | ""); });
}

String HaEntityCache::GetEntityName(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["friendly_name"] | ""); });
}

String HaEntityCache::GetUnit(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["unit_of_measurement"] | ""); });
}

bool HaEntityCache::EntityExists(String id)
{
    // true, sobald HA fuer diese (abonnierte) Entitaet einen Zustand geliefert hat.
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst) { return true; });
}

// --- Licht ---
int HaEntityCache::GetBrightness(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["brightness"], -1); });
}

uint32_t HaEntityCache::GetRGB(String id)
{
    return read<uint32_t>(id, 0xFFFFFFFF, [](JsonObjectConst, JsonObjectConst a) -> uint32_t {
        JsonArrayConst c = a["rgb_color"];
        if (c.isNull() || c.size() < 3) return 0xFFFFFFFF;
        return ((uint32_t)(uint8_t)(c[0] | 0) << 16) | ((uint32_t)(uint8_t)(c[1] | 0) << 8) | (uint8_t)(c[2] | 0);
    });
}

int HaEntityCache::GetWhite(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) -> int {
        JsonArrayConst c = a["rgbw_color"];
        return (c.isNull() || c.size() < 4) ? -1 : (int)(c[3] | -1);
    });
}

bool HaEntityCache::IsRGBW(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) { return hasColorMode(a, {"rgbw", "rgbww"}); });
}

bool HaEntityCache::SupportsBrightness(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) {
        return hasColorMode(a, {"brightness", "color_temp", "hs", "rgb", "xy", "rgbw", "rgbww"});
    });
}

bool HaEntityCache::SupportsColor(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) {
        return hasColorMode(a, {"hs", "rgb", "xy", "rgbw", "rgbww"});
    });
}

bool HaEntityCache::SupportsColorTemp(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) { return hasColorMode(a, {"color_temp"}); });
}

int HaEntityCache::GetColorTemp(String id)
{
    // Farbtemperatur in Mired. Neuere HA-Versionen liefern nur noch
    // color_temp_kelvin - dann wird umgerechnet (Mired = 1e6 / K).
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) -> int {
        const int mired = a["color_temp"] | -1;
        if (mired > 0) return mired;
        const int kelvin = a["color_temp_kelvin"] | 0;
        return (kelvin > 0) ? (1000000 / kelvin) : -1;
    });
}

// --- Medien ---
String HaEntityCache::GetMediaTitle(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["media_title"] | ""); });
}

String HaEntityCache::GetMediaArtist(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["media_artist"] | ""); });
}

float HaEntityCache::GetMediaVolume(String id)
{
    return read<float>(id, -1.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["volume_level"] | -1.0f); });
}

std::vector<String> HaEntityCache::GetMediaSourceList(String id)
{
    return read<std::vector<String>>(id, {}, [](JsonObjectConst, JsonObjectConst a) { return toStringList(a["source_list"]); });
}

String HaEntityCache::GetMediaSource(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["source"] | ""); });
}

// --- Sauger / Rollladen ---
int HaEntityCache::GetBattery(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["battery_level"], -1); });
}

String HaEntityCache::GetFanSpeed(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["fan_speed"] | ""); });
}

std::vector<String> HaEntityCache::GetFanSpeedList(String id)
{
    return read<std::vector<String>>(id, {}, [](JsonObjectConst, JsonObjectConst a) { return toStringList(a["fan_speed_list"]); });
}

bool HaEntityCache::GetLocation(String id, double &lat, double &lon, float &accuracyM)
{
    struct Loc { bool ok; double lat; double lon; float acc; };
    const Loc res = read<Loc>(id, Loc{false, 0, 0, 0}, [](JsonObjectConst, JsonObjectConst a) {
        if (!a["latitude"].is<double>() || !a["longitude"].is<double>()) return Loc{false, 0, 0, 0};
        return Loc{true, a["latitude"].as<double>(), a["longitude"].as<double>(), (float)(a["gps_accuracy"] | 0.0f)};
    });
    if (!res.ok) return false;
    lat = res.lat;
    lon = res.lon;
    accuracyM = res.acc;
    return true;
}

uint32_t HaEntityCache::GetLastUpdated(String id)
{
    return read<uint32_t>(id, 0, [](JsonObjectConst e, JsonObjectConst) {
        const double t = !e["lu"].isNull() ? (e["lu"] | 0.0) : (e["lc"] | 0.0);
        return (uint32_t)t;
    });
}

float HaEntityCache::GetZoneRadius(String id)
{
    return read<float>(id, 0.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["radius"] | 0.0f); });
}

int HaEntityCache::GetPosition(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["current_position"], -1); });
}

// --- Klima ---
float HaEntityCache::GetCurrentTemperature(String id)
{
    return read<float>(id, -99.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["current_temperature"] | -99.0f); });
}

float HaEntityCache::GetTargetTemperature(String id)
{
    return read<float>(id, -99.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["temperature"] | -99.0f); });
}

// --- Optionen und Grenzen ---
std::vector<String> HaEntityCache::GetOptions(String id)
{
    const char *key = isClimate(id) ? "hvac_modes" : "options";
    return read<std::vector<String>>(id, {}, [key](JsonObjectConst, JsonObjectConst a) { return toStringList(a[key]); });
}

float HaEntityCache::GetMin(String id)
{
    const char *key = isClimate(id) ? "min_temp" : "min";
    return read<float>(id, 0.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 0.0f); });
}

float HaEntityCache::GetMax(String id)
{
    const char *key = isClimate(id) ? "max_temp" : "max";
    return read<float>(id, 100.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 100.0f); });
}

float HaEntityCache::GetStep(String id)
{
    const char *key = isClimate(id) ? "target_temp_step" : "step";
    return read<float>(id, 1.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 1.0f); });
}
