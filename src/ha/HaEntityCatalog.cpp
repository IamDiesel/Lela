// ==============================================================================
// HaEntityCatalog - Implementierung (siehe Header)
// ==============================================================================

#include "HaEntityCatalog.h"
#include "HaWebsocketLogic.h"
#include "HaPsramAllocator.h"
#include <ArduinoJson.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ------------------------------------------------------------------------------
// Parameter
// ------------------------------------------------------------------------------
// Nach dieser Zeit wird die Liste bei der naechsten Anforderung neu geladen
// (neue Geraete in HA tauchen so ohne Neustart auf).
static const uint32_t MAX_AGE_MS      = 5UL * 60UL * 1000UL;

// Grosse Installationen brauchen serverseitig etwas, bis das Template steht.
static const uint32_t LOAD_TIMEOUT_MS = 15000;

// Serverseitiges Template. Es liefert ein JSON-Array mit einem kompakten
// Eintrag je Entitaet:
//   i  = entity_id         n  = Anzeigename        ic = Icon (falls gesetzt)
//   o  = Optionen (select/input_select) bzw. HVAC-Modi (climate)
//   mn/mx/st = Grenzen und Schrittweite (number, text, climate)
//
// Die Domains entsprechen der Typauswahl im Hinzufuegen-Dialog. Die Liste
// ist auf 1500 Eintraege begrenzt; HA kappt Template-Ausgaben ohnehin bei
// 256 KB, und 1500 Eintraege bleiben sicher darunter.
static const char CATALOG_TEMPLATE[] = R"JINJA(
{%- set d = ['light','switch','input_boolean','media_player','vacuum','select','input_select','number','input_number','text','input_text','cover','climate','fan','input_button','button','scene','script','automation','sensor','binary_sensor','weather','person','device_tracker'] -%}
[{%- for s in (states | selectattr('domain','in',d) | list)[:1500] -%}
{"i":{{ s.entity_id | to_json }},"n":{{ s.name | to_json }}
{%- if s.attributes.icon is defined %},"ic":{{ s.attributes.icon | to_json }}{% endif -%}
{%- if s.domain in ['select','input_select'] %},"o":{{ s.attributes.options | default([]) | list | to_json }}
{%- elif s.domain in ['number','input_number','text','input_text'] %},"mn":{{ s.attributes.min | default(0) | to_json }},"mx":{{ s.attributes.max | default(100) | to_json }},"st":{{ s.attributes.step | default(1) | to_json }}
{%- elif s.domain == 'climate' %},"o":{{ s.attributes.hvac_modes | default([]) | list | to_json }},"mn":{{ s.attributes.min_temp | default(7) | to_json }},"mx":{{ s.attributes.max_temp | default(35) | to_json }},"st":{{ s.attributes.target_temp_step | default(0.5) | to_json }}
{%- endif %}}{{ ',' if not loop.last }}
{%- endfor -%}]
)JINJA";

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
static SemaphoreHandle_t s_mutex = nullptr;

// Die Liste selbst (ganzes Antwortdokument, Eintraege ueber s_list). Nur unter s_mutex anfassen.
static std::unique_ptr<JsonDocument> s_catalog;
static JsonArrayConst s_list;   // die Eintraege innerhalb von s_catalog
static uint32_t s_loadedAt = 0;

// Anforderung aus der GUI und laufende Anfrage
static volatile bool     s_requested = false;
static volatile bool     s_loading   = false;
static volatile uint32_t s_version   = 0;

// Nur HA_WS_Task
static uint32_t s_requestId = 0;   // ID des laufenden render_template
static uint32_t s_unsubId   = 0;   // ID der Abmeldung danach
static uint32_t s_sentAt    = 0;

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
namespace {

// RAII-Sperre fuer die Liste.
class Lock {
public:
    Lock()  { ok_ = s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE; }
    ~Lock() { if (ok_) xSemaphoreGive(s_mutex); }
    bool ok() const { return ok_; }
private:
    bool ok_;
};

// Teilstring ohne Gross-/Kleinschreibung (ASCII). Vermeidet je Eintrag zwei
// String-Kopien wie bei toLowerCase() + indexOf().
bool containsIgnoreCase(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return false;
    for (; *hay; hay++) {
        const char* h = hay;
        const char* n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return true;
    }
    return false;
}

// Sucht einen Eintrag. Nur unter Lock aufrufen.
JsonObjectConst findEntry(const String& entityId) {
    if (!s_catalog) return JsonObjectConst();
    for (JsonObjectConst e : s_list) {
        const char* id = e["i"];
        if (id && entityId.equals(id)) return e;
    }
    return JsonObjectConst();
}

void sendUnsubscribe(uint32_t subscriptionId) {
    JsonDocument req(HaPsramAlloc());
    req["type"] = "unsubscribe_events";
    req["subscription"] = subscriptionId;
    s_unsubId = HaWebsocketLogic_SendRequest(req);
}

void finishRequest() {
    s_requestId = 0;
    s_loading = false;
}

// Uebernimmt das Ergebnis aus dem Template-Ereignis.
void storeResult(const String& payload) {
    const uint32_t t0 = millis();

    // Das Parsen einer grossen Liste dauert einige hundert Millisekunden. Der
    // HA_WS_Task laeuft mit hoeherer Prioritaet auf demselben Kern wie die
    // Oberflaeche - fuer diese Zeit auf deren Prioritaet absenken, damit sich
    // beide die Rechenzeit teilen, statt dass die Oberflaeche stehen bleibt.
    const UBaseType_t ownPrio = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, 1);

    JsonDocument filter;
    filter["event"]["result"] = true;
    filter["event"]["error"] = true;

    // Direkt ins endgueltige Dokument parsen. Frueher wurde die Liste danach
    // noch einmal komplett kopiert - das kostete die Haelfte der Zeit.
    std::unique_ptr<JsonDocument> doc(new JsonDocument(HaPsramAlloc()));
    DeserializationError err = deserializeJson(*doc, payload, DeserializationOption::Filter(filter));
    bool nested = true;   // Liste liegt unter event.result

    if (!err && (*doc)["event"]["result"].is<const char*>()) {
        // HA liefert das Ergebnis als Text, wenn es ihn nicht selbst als JSON
        // erkannt hat - dann hier parsen.
        std::unique_ptr<JsonDocument> list(new JsonDocument(HaPsramAlloc()));
        err = deserializeJson(*list, (*doc)["event"]["result"].as<const char*>());
        doc = std::move(list);
        nested = false;
    }

    vTaskPrioritySet(NULL, ownPrio);

    if (err) {
        Serial.printf("[HA-Katalog] Antwort nicht lesbar: %s (%u Bytes)\n", err.c_str(), (unsigned)payload.length());
        return;
    }
    if (doc->overflowed()) {
        Serial.println("[HA-Katalog] Kein Speicher fuer die Liste.");
        return;
    }

    JsonArrayConst list = nested ? (*doc)["event"]["result"].as<JsonArrayConst>()
                                 : doc->as<JsonArrayConst>();
    if (list.isNull()) {
        const char* e = (*doc)["event"]["error"] | "kein Ergebnis";
        Serial.printf("[HA-Katalog] Template-Fehler: %s\n", e);
        return;
    }

    const size_t count = list.size();
    {
        Lock lock;
        if (!lock.ok()) return;
        s_catalog = std::move(doc);
        s_list = list;   // zeigt in s_catalog, bleibt gueltig solange es lebt
        s_loadedAt = millis();
    }
    s_version = s_version + 1;
    Serial.printf("[HA-Katalog] %u Entitaeten geladen (%u Bytes, %lu ms Verarbeitung).\n",
                  (unsigned)count, (unsigned)payload.length(), (unsigned long)(millis() - t0));
}

} // namespace

// ------------------------------------------------------------------------------
// GUI-Seite
// ------------------------------------------------------------------------------
void HaEntityCatalog::Init() {
    if (s_mutex == nullptr) s_mutex = xSemaphoreCreateMutex();
}

void HaEntityCatalog::RequestLoad() {
    if (s_loading || s_requested) return;
    {
        Lock lock;
        if (lock.ok() && s_catalog && millis() - s_loadedAt < MAX_AGE_MS) return;
    }
    s_requested = true;
}

bool HaEntityCatalog::IsReady() {
    Lock lock;
    return lock.ok() && s_catalog != nullptr;
}

bool HaEntityCatalog::IsLoading() {
    return s_loading || s_requested;
}

uint32_t HaEntityCatalog::GetVersion() {
    return s_version;
}

std::vector<String> HaEntityCatalog::Search(const String& query, size_t maxResults) {
    std::vector<String> hits;
    String q = query;
    q.trim();
    if (q.length() < 2) return hits;

    Lock lock;
    if (!lock.ok() || !s_catalog) return hits;

    for (JsonObjectConst e : s_list) {
        const char* id   = e["i"];
        const char* name = e["n"];
        if (containsIgnoreCase(id, q.c_str()) || containsIgnoreCase(name, q.c_str())) {
            hits.push_back(String(id));
            if (hits.size() >= maxResults) break;
        }
    }
    return hits;
}

bool HaEntityCatalog::Contains(const String& entityId) {
    Lock lock;
    return lock.ok() && !findEntry(entityId).isNull();
}

String HaEntityCatalog::GetName(const String& entityId) {
    Lock lock;
    if (!lock.ok()) return "";
    return String(findEntry(entityId)["n"] | "");
}

String HaEntityCatalog::GetIcon(const String& entityId) {
    Lock lock;
    if (!lock.ok()) return "";
    return String(findEntry(entityId)["ic"] | "");
}

std::vector<String> HaEntityCatalog::GetOptions(const String& entityId) {
    std::vector<String> opts;
    Lock lock;
    if (!lock.ok()) return opts;
    for (JsonVariantConst v : findEntry(entityId)["o"].as<JsonArrayConst>()) {
        opts.push_back(v.as<String>());
    }
    return opts;
}

bool HaEntityCatalog::GetLimits(const String& entityId, float& minVal, float& maxVal, float& stepVal) {
    Lock lock;
    if (!lock.ok()) return false;
    JsonObjectConst e = findEntry(entityId);
    if (e.isNull() || e["mn"].isNull() || e["mx"].isNull()) return false;
    minVal  = e["mn"].as<float>();
    maxVal  = e["mx"].as<float>();
    stepVal = e["st"] | 1.0f;
    return true;
}

// ------------------------------------------------------------------------------
// HA_WS_Task
// ------------------------------------------------------------------------------
void HaEntityCatalog::OnAuthenticated() {
    // Eine auf der alten Verbindung laufende Anfrage ist verloren - erneut
    // anfordern, falls gerade jemand auf die Liste wartet.
    const bool wasLoading = s_loading;
    s_requestId = 0;
    s_unsubId = 0;
    s_loading = false;
    if (wasLoading) s_requested = true;
}

void HaEntityCatalog::Tick() {
    if (s_requested && !s_loading) {
        s_requested = false;

        JsonDocument req(HaPsramAlloc());
        req["type"] = "render_template";
        req["template"] = CATALOG_TEMPLATE;

        // Antworten kommen nur in diesem Task an - ID nach dem Senden merken ist sicher.
        s_requestId = HaWebsocketLogic_SendRequest(req);
        if (s_requestId == 0) {
            s_requested = true;   // nicht gesendet: beim naechsten Durchlauf erneut
            return;
        }
        s_loading = true;
        s_sentAt = millis();
        return;
    }

    if (s_loading && millis() - s_sentAt > LOAD_TIMEOUT_MS) {
        Serial.println("[HA-Katalog] Keine Antwort, Abbruch.");
        if (s_requestId) sendUnsubscribe(s_requestId);
        finishRequest();
    }
}

bool HaEntityCatalog::HandleMessage(const String& type, uint32_t id, bool success, const String& payload) {
    if (id == 0) return false;

    // Antwort auf die eigene Abmeldung
    if (id == s_unsubId && type == "result") {
        s_unsubId = 0;
        return true;
    }

    if (id != s_requestId) return false;

    if (type == "result") {
        if (!success) {
            Serial.println("[HA-Katalog] HA hat die Anfrage abgelehnt.");
            finishRequest();
        }
        return true;   // Erfolg: das Ergebnis folgt als Ereignis
    }

    if (type == "event") {
        storeResult(payload);
        // Einmalabfrage: Template sofort abmelden, sonst rendert HA es bei
        // jeder Zustandsaenderung im Haus erneut (gedrosselt, aber unnoetig).
        sendUnsubscribe(id);
        finishRequest();
        return true;
    }
    return false;
}

void HaEntityCatalog::Clear() {
    {
        Lock lock;
        if (lock.ok()) {
            s_list = JsonArrayConst();
            s_catalog.reset();
        }
    }
    s_requested = false;
    s_loading = false;
    s_requestId = 0;
    s_unsubId = 0;
}
