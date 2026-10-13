// ==============================================================================
// HaEntitySync - Implementierung (siehe Header fuer das Protokoll)
// ==============================================================================

#include "HaEntitySync.h"
#include "HaEntityCache.h"
#include "HaWebsocketLogic.h"
#include "HaPsramAllocator.h"
#include "HeapMonitor.h"
#include <ArduinoJson.h>

#include <vector>
#include <algorithm>

// ------------------------------------------------------------------------------
// Parameter
// ------------------------------------------------------------------------------
// Entitaeten je Abo. Klein genug fuer schnellen ersten Bildaufbau und moderate
// Nachrichtengroesse (die Rohnachricht enthaelt ALLE Attribute, gefiltert wird
// erst beim Parsen), gross genug fuer wenige Round-Trips.
static const size_t   ENTITIES_PER_SUBSCRIPTION = 16;

// Wartezeit auf die erste Antwort eines Abos, danach wird der Block uebersprungen.
static const uint32_t SUBSCRIBE_TIMEOUT_MS = 5000;

// Entprellung: Speichern und anschliessender Neuaufbau fordern direkt
// hintereinander an - daraus wird ein einziger Neuaufbau.
static const uint32_t RESYNC_DEBOUNCE_MS = 300;

// Attribute, die die Oberflaeche auswertet (HaEntityCache::ProcessEntity).
// Alles andere wird schon beim Parsen verworfen.
static const char* const ATTRIBUTE_WHITELIST[] = {
    "friendly_name", "icon", "unit_of_measurement",
    "battery_level", "fan_speed", "fan_speed_list",
    "options", "min", "max", "step",
    "brightness", "color_temp", "color_temp_kelvin", "supported_color_modes",
    "rgb_color", "rgbw_color",
    "media_title", "media_artist", "volume_level", "source", "source_list",
    "current_position",
    "current_temperature", "temperature", "hvac_modes",
    "min_temp", "max_temp", "target_temp_step",
};

// ------------------------------------------------------------------------------
// Zustand (nur HA_WS_Task, ausser den beiden volatile-Feldern)
// ------------------------------------------------------------------------------
enum class Phase { Idle, Subscribing, Live };

static volatile bool     s_resyncRequested   = false;
static volatile uint32_t s_resyncRequestedAt = 0;
static bool              s_resyncImmediate   = false;

static Phase                 s_phase = Phase::Idle;
static std::vector<String>   s_plan;              // Entitaeten des laufenden Aufbaus
static size_t                s_planIndex = 0;     // erster noch nicht abonnierter Eintrag
static size_t                s_chunkLen  = 0;     // Groesse des gerade angefragten Blocks
static uint32_t              s_currentSubId = 0;  // ID des gerade angefragten Blocks
static bool                  s_waiting   = false; // wartet auf dessen erste Antwort
static uint32_t              s_sentAt    = 0;
static uint32_t              s_resyncStartedAt = 0;

static std::vector<uint32_t> s_activeSubs;        // IDs aller eigenen, laufenden Abos
static std::vector<uint32_t> s_pendingUnsubs;     // IDs eigener Abmelde-Anfragen


// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
template <typename T>
static bool eraseValue(std::vector<T>& v, const T& value) {
    auto it = std::find(v.begin(), v.end(), value);
    if (it == v.end()) return false;
    v.erase(it);
    return true;
}

// Formal gueltige Entity-ID ("domain.objekt", Kleinbuchstaben, Ziffern, '_').
// HA lehnt ein Abo mit einer einzigen ungueltigen ID komplett ab - ohne diese
// Pruefung fielen alle Entitaeten desselben Blocks aus.
static bool isValidEntityId(const String& id) {
    const int dot = id.indexOf('.');
    if (dot <= 0 || dot >= (int)id.length() - 1) return false;
    for (size_t i = 0; i < id.length(); i++) {
        const char c = id[i];
        if ((int)i == dot) continue;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// Filter fuer eingehende Abo-Ereignisse, einmalig aufgebaut.
static JsonDocument& eventFilter() {
    static JsonDocument filter(HaPsramAlloc());
    static bool built = false;
    if (!built) {
        JsonObject ev = filter["event"].to<JsonObject>();

        // Vollstaendiger Zustand
        JsonObject full = ev["a"]["*"].to<JsonObject>();
        full["s"] = true;
        JsonObject fullAttr = full["a"].to<JsonObject>();

        // Aenderung
        JsonObject diff = ev["c"]["*"].to<JsonObject>();
        JsonObject plus = diff["+"].to<JsonObject>();
        plus["s"] = true;
        JsonObject plusAttr = plus["a"].to<JsonObject>();
        diff["-"]["a"] = true;

        for (const char* name : ATTRIBUTE_WHITELIST) {
            fullAttr[name] = true;
            plusAttr[name] = true;
        }

        // Entfernte Entitaeten
        ev["r"] = true;
        built = true;
    }
    return filter;
}

static void applyEvent(const String& payload) {
    JsonDocument doc(HaPsramAlloc());
    DeserializationError err = deserializeJson(doc, payload,
                                               DeserializationOption::Filter(eventFilter()),
                                               DeserializationOption::NestingLimit(20));
    if (err) {
        Serial.printf("[HA-Sync] Ereignis nicht lesbar: %s (%u Bytes)\n", err.c_str(), (unsigned)payload.length());
        return;
    }

    JsonObjectConst ev = doc["event"];
    for (JsonPairConst kv : ev["a"].as<JsonObjectConst>()) {
        HaEntityCache::ApplyFull(kv.key().c_str(), kv.value().as<JsonObjectConst>());
    }
    for (JsonPairConst kv : ev["c"].as<JsonObjectConst>()) {
        HaEntityCache::ApplyDiff(kv.key().c_str(), kv.value().as<JsonObjectConst>());
    }
    for (JsonVariantConst v : ev["r"].as<JsonArrayConst>()) {
        HaEntityCache::ApplyRemoved(v.as<const char*>());
    }
}

static void sendUnsubscribe(uint32_t subscriptionId) {
    JsonDocument req(HaPsramAlloc());
    req["type"] = "unsubscribe_events";
    req["subscription"] = subscriptionId;

    // Antworten werden nur in diesem Task empfangen - die ID nach dem Senden
    // zu merken ist daher sicher.
    const uint32_t id = HaWebsocketLogic_SendRequest(req);
    if (id) s_pendingUnsubs.push_back(id);
}

static void startResync() {
    // Bisherige Abos abmelden. Deren Antworten werden in HandleMessage
    // stillschweigend verbraucht; Ereignisse, die bis dahin noch eintreffen,
    // gehoeren zu keinem aktiven Abo mehr und werden verworfen.
    for (uint32_t sub : s_activeSubs) sendUnsubscribe(sub);
    s_activeSubs.clear();

    s_plan.clear();
    for (const String& id : HaEntityCache::GetTrackedEntities()) {
        if (isValidEntityId(id)) s_plan.push_back(id);
        else Serial.printf("[HA-Sync] Ungueltige Entity-ID uebersprungen: '%s'\n", id.c_str());
    }
    HaEntityCache::Prune(s_plan);

    s_planIndex = 0;
    s_waiting = false;
    s_resyncStartedAt = millis();
    s_phase = s_plan.empty() ? Phase::Live : Phase::Subscribing;
}

static void advance() {
    s_planIndex += s_chunkLen;
    s_chunkLen = 0;
    s_waiting = false;
}

static void sendNextChunk() {
    if (s_planIndex >= s_plan.size()) {
        s_phase = Phase::Live;
        Serial.printf("[HA-Sync] %u Entitaeten in %u Abos geladen (%lu ms).\n",
                      (unsigned)s_plan.size(), (unsigned)s_activeSubs.size(),
                      (unsigned long)(millis() - s_resyncStartedAt));
        HeapMonitor_Log("nach HA-Sync");
        return;
    }

    JsonDocument req(HaPsramAlloc());
    req["type"] = "subscribe_entities";
    JsonArray ids = req["entity_ids"].to<JsonArray>();

    s_chunkLen = 0;
    for (size_t i = s_planIndex; i < s_plan.size() && s_chunkLen < ENTITIES_PER_SUBSCRIPTION; i++) {
        ids.add(s_plan[i]);
        s_chunkLen++;
    }

    // Bei 0 (nicht gesendet) greift die Zeitueberschreitung und der Block
    // wird uebersprungen.
    const uint32_t id = HaWebsocketLogic_SendRequest(req);
    if (id) s_activeSubs.push_back(id);
    s_currentSubId = id;
    s_waiting = true;
    s_sentAt = millis();
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void HaEntitySync::RequestResync() {
    // Zeitpunkt zuerst, dann das Flag: der WS-Task wertet beides in dieser
    // Reihenfolge aus.
    s_resyncRequestedAt = millis();
    s_resyncRequested = true;
}

void HaEntitySync::OnAuthenticated() {
    // Neue Verbindung: alte Abos sind serverseitig bereits beendet.
    s_activeSubs.clear();
    s_pendingUnsubs.clear();
    s_phase = Phase::Idle;
    s_waiting = false;

    s_resyncImmediate = true;
    RequestResync();
}

void HaEntitySync::Tick() {
    const uint32_t now = millis();

    if (s_resyncRequested &&
        (s_resyncImmediate || now - s_resyncRequestedAt >= RESYNC_DEBOUNCE_MS)) {
        s_resyncRequested = false;
        s_resyncImmediate = false;
        startResync();
    }

    if (s_phase != Phase::Subscribing) return;

    if (s_waiting && now - s_sentAt > SUBSCRIBE_TIMEOUT_MS) {
        Serial.printf("[HA-Sync] Abo %lu ohne Antwort, Block wird uebersprungen.\n", (unsigned long)s_currentSubId);
        advance();
    }
    if (!s_waiting) sendNextChunk();
}

bool HaEntitySync::HandleMessage(const String& type, uint32_t id, bool success, const String& payload) {
    if (id == 0) return false;

    if (type == "result") {
        // Antwort auf eine eigene Abmeldung
        if (eraseValue(s_pendingUnsubs, id)) return true;

        // Antwort auf ein eigenes Abo
        if (std::find(s_activeSubs.begin(), s_activeSubs.end(), id) != s_activeSubs.end()) {
            if (!success) {
                Serial.printf("[HA-Sync] HA hat Abo %lu abgelehnt.\n", (unsigned long)id);
                eraseValue(s_activeSubs, id);
                if (id == s_currentSubId && s_waiting) advance();
            }
            return true;
        }
        return false;
    }

    if (type == "event") {
        if (std::find(s_activeSubs.begin(), s_activeSubs.end(), id) == s_activeSubs.end()) return false;

        applyEvent(payload);
        if (id == s_currentSubId && s_waiting) advance();
        return true;
    }

    return false;
}

void HaEntitySync::Reset() {
    s_resyncRequested = false;
    s_resyncImmediate = false;
    s_phase = Phase::Idle;
    s_plan.clear();
    s_planIndex = 0;
    s_chunkLen = 0;
    s_currentSubId = 0;
    s_waiting = false;
    s_activeSubs.clear();
    s_pendingUnsubs.clear();
}
