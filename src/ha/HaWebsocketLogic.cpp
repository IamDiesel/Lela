// ==============================================================================
// HaWebsocketLogic - Verbindung zu Home Assistant (Transport und Routing)
// ==============================================================================
// Aufgaben:
//   - Verbindung im eigenen Task (HA_WS_Task) aufbauen, halten und ueberwachen
//   - Anmeldung (auth)
//   - Senden mit streng steigenden Nachrichten-IDs (SendRequest)
//   - Eingehende Nachrichten an die Zustaendigen verteilen:
//       HaEntitySync    - Live-Zustaende der platzierten Entitaeten
//       HaEntityCatalog - Liste aller Entitaeten (Suche)
//       Postfach        - Antworten fuer Medien-Browser und Lovelace-Import,
//                         ausgewertet im loopTask (siehe ProcessPendingResponses)
//
// Verbindungsziel: IP/Port aus dem Web-Setup haben Vorrang, secrets.h ist der
// Rueckfall - sowohl wenn im Setup nichts Gueltiges steht als auch, wenn das
// Setup-Ziel mehrfach hintereinander nicht erreichbar ist.
// ==============================================================================

#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "HaEntitySync.h"
#include "HaEntityCatalog.h"
#include "HaPsramAllocator.h"
#include "HeapMonitor.h"
#include "HaConfigLogic.h"
#include "SharedData.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// ------------------------------------------------------------------------------
// Parameter
// ------------------------------------------------------------------------------
// Wiederverbinden: Wartezeit verdoppelt sich je Fehlschlag bis zum Maximum.
static const uint32_t RECONNECT_MIN_MS      = 2000;
static const uint32_t RECONNECT_MAX_MS      = 30000;

// Falsches Token: deutlich laengere Pausen. Ohne sie verbindet sich das Geraet
// im Sekundentakt neu, flutet das HA-Log und riskiert eine IP-Sperre (ip_ban).
static const uint32_t AUTH_RETRY_MIN_MS     = 60000;
static const uint32_t AUTH_RETRY_MAX_MS     = 600000;

// Lebenszeichen: Kommt so lange nichts von HA, wird ein Ping gesendet. Bleibt
// auch die Antwort aus, gilt die Verbindung als tot und wird neu aufgebaut.
// Ohne diese Pruefung blieben nach einem WLAN-Aussetzer alte Werte stehen.
static const uint32_t PING_AFTER_SILENCE_MS = 20000;
static const uint32_t PONG_TIMEOUT_MS       = 8000;

// Nach so vielen Fehlversuchen in Folge wird zwischen Setup-Ziel und
// secrets.h abgewechselt (nur wenn beide verschieden sind).
static const uint8_t  HOST_FAILOVER_AFTER   = 3;

// ------------------------------------------------------------------------------
// Verbindungszustand
// ------------------------------------------------------------------------------
static WebsocketsClient  haClient;
static volatile bool     isHaConnected = false;
static volatile bool     isHaAuthenticated = false;
static SemaphoreHandle_t haClientMutex = NULL;

// Nur unter haClientMutex veraendern (siehe SendRequest).
static uint32_t messageIdCounter = 1;

// Task-Lebenszyklus
static volatile TaskHandle_t haTaskHandle = NULL;
static volatile bool         haShouldRun = false;
static volatile bool         haTaskExiting = false;
static portMUX_TYPE          haTaskMux = portMUX_INITIALIZER_UNLOCKED;

// Nur HA_WS_Task
static uint32_t s_nextConnectAt = 0;
static uint32_t s_reconnectDelay = RECONNECT_MIN_MS;
static uint32_t s_authRetryDelay = AUTH_RETRY_MIN_MS;
static uint8_t  s_failCount = 0;
static bool     s_useFallbackHost = false;
static volatile uint32_t s_lastRxAt = 0;
static uint32_t s_pingSentAt = 0;
static volatile bool s_pingPending = false;
static bool     s_authRejected = false;   // auth_invalid empfangen, nach poll() behandeln

// ------------------------------------------------------------------------------
// Import- und Medien-Zustand (von der GUI gelesen)
// ------------------------------------------------------------------------------
std::vector<String> availableDashboardUrls;
std::vector<String> availableDashboardTitles;
volatile bool pendingDashboardList = false;
volatile bool pendingImportError = false;
String importErrorMessage = "";
std::vector<String> availableViewTitles;
volatile bool pendingViewList = false;
String currentImportUrl = "";
volatile bool isImporting = false;
// Medien-Ordner (nur loopTask): komplette, gefilterte Antwort im PSRAM.
static JsonDocument* s_mediaFolder = nullptr;
volatile bool pendingMediaBrowserUpdate = false;
volatile bool pendingMediaBrowserError = false;

volatile uint32_t lastLovelaceReqId = 0;
volatile uint32_t lastLovelaceDashboardsReqId = 0;
volatile uint32_t lastMediaBrowseReqId = 0;
bool requestingViewsOnly = false;
int targetViewIndex = 0;
int currentImportTab = -1;

// ------------------------------------------------------------------------------
// Postfach: Antworten, die im loopTask ausgewertet werden
// ------------------------------------------------------------------------------
// Medien-Browser und Lovelace-Import veraendern Daten, die die Oberflaeche
// liest (Medien-Ordner, HaConfigLogic::dashboards) und speichern in den
// Flash. Das geschah frueher direkt im HA_WS_Task - nebenlaeufig zur GUI und
// mit blockiertem Empfang. Jetzt legt der WS-Task die Antwort nur ab.
struct PendingResponse {
    bool     present = false;
    bool     success = false;
    uint32_t id = 0;
    String   payload;
};
static PendingResponse   s_pendingMedia;
static PendingResponse   s_pendingImport;
static SemaphoreHandle_t s_mailboxMutex = NULL;

static void storeResponse(PendingResponse& slot, uint32_t id, bool success, String& payload) {
    if (!s_mailboxMutex || xSemaphoreTake(s_mailboxMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    slot.present = true;
    slot.success = success;
    slot.id = id;
    slot.payload = std::move(payload);
    xSemaphoreGive(s_mailboxMutex);
}

static bool takeResponse(PendingResponse& slot, PendingResponse& out) {
    if (!s_mailboxMutex || xSemaphoreTake(s_mailboxMutex, 0) != pdTRUE) return false;
    const bool had = slot.present;
    if (had) {
        out.present = true;
        out.success = slot.success;
        out.id = slot.id;
        out.payload = std::move(slot.payload);
        slot.present = false;
        slot.payload = String();
    }
    xSemaphoreGive(s_mailboxMutex);
    return had;
}

// ------------------------------------------------------------------------------
// Senden
// ------------------------------------------------------------------------------
bool HaWebsocketLogic_IsConnected() {
    return isHaConnected && isHaAuthenticated;
}

// Rohdaten ohne ID (nur fuer die Anmeldung, die keine ID traegt).
static void sendRaw(const String& payload) {
    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

uint32_t HaWebsocketLogic_SendRequest(JsonDocument& doc, volatile uint32_t* idOut) {
    if (haClientMutex == NULL || xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return 0;
    }

    uint32_t id = 0;
    // Nur auf einer angemeldeten Verbindung: vor auth_ok trennt HA sonst.
    if (isHaAuthenticated) {
        // ID-Vergabe und Senden unter derselben Sperre: HA verlangt streng
        // steigende IDs und verwirft kleinere stumm (id_reuse). Frueher wurde
        // die ID ausserhalb der Sperre vergeben - zwei Tasks konnten sich
        // ueberholen, und ein Schaltbefehl ging verloren.
        id = messageIdCounter++;
        doc["id"] = id;
        if (idOut) *idOut = id;   // vor dem Senden: die Antwort kann sofort kommen

        String payload;
        serializeJson(doc, payload);
        if (!haClient.send(payload)) {
            id = 0;
            if (idOut) *idOut = 0;
        }
    }

    xSemaphoreGiveRecursive(haClientMutex);
    return id;
}

// ------------------------------------------------------------------------------
// Verbindungsziel
// ------------------------------------------------------------------------------
static bool isValidPort(int port) { return port > 0 && port <= 65535; }

// Liefert Host und Port fuer den naechsten Verbindungsversuch.
static String currentHost(uint16_t& port, const char*& source) {
    String setupHost = haIP;
    setupHost.trim();
    const bool setupValid = setupHost.length() > 0 && isValidPort(haPort);

    const String secretHost = SECRET_HA_IP;
    const bool   differs = setupValid && (setupHost != secretHost || haPort != SECRET_HA_PORT);

    if (setupValid && !(differs && s_useFallbackHost)) {
        port = (uint16_t)haPort;
        source = "Web-Setup";
        return setupHost;
    }
    port = SECRET_HA_PORT;
    source = setupValid ? "secrets.h (Rueckfall)" : "secrets.h";
    return secretHost;
}

static void scheduleReconnect(uint32_t delayMs) {
    s_nextConnectAt = millis() + delayMs;
}

// Beendet die Verbindung bewusst (WLAN weg, keine Antwort, Token falsch).
// Nur im HA_WS_Task aufrufen.
static void closeConnection(const char* reason, uint32_t retryDelayMs) {
    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        haClient.close();
        xSemaphoreGiveRecursive(haClientMutex);
    }
    isHaConnected = false;
    isHaAuthenticated = false;
    s_pingPending = false;
    scheduleReconnect(retryDelayMs);
    Serial.printf("[HA] Verbindung beendet: %s. Neuer Versuch in %lu s.\n", reason, (unsigned long)(retryDelayMs / 1000));
}

// ------------------------------------------------------------------------------
// Empfang
// ------------------------------------------------------------------------------
void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    String payload = message.data();

    // Jede Nachricht ist ein Lebenszeichen.
    s_lastRxAt = millis();
    s_pingPending = false;

    JsonDocument headerFilter;
    headerFilter["type"] = true;
    headerFilter["id"] = true;
    headerFilter["success"] = true;

    JsonDocument headerDoc;
    if (deserializeJson(headerDoc, payload, DeserializationOption::Filter(headerFilter), DeserializationOption::NestingLimit(200))) {
        return;
    }

    const String   type = headerDoc["type"] | "";
    const uint32_t msgId = headerDoc["id"] | 0;
    const bool     success = headerDoc["success"] | false;

    // --- Abos und Entitaetskatalog verwalten ihre IDs selbst ---
    if (HaEntitySync::HandleMessage(type, msgId, success, payload)) return;
    if (HaEntityCatalog::HandleMessage(type, msgId, success, payload)) return;

    // Ereignisse ohne Empfaenger, z. B. eines gerade abgemeldeten Abos.
    if (type == "event" || type == "pong") return;

    // --- Antworten fuer den loopTask ins Postfach ---
    if (msgId != 0) {
        if (msgId == lastMediaBrowseReqId) {
            storeResponse(s_pendingMedia, msgId, success, payload);
            return;
        }
        if (msgId == lastLovelaceReqId || msgId == lastLovelaceDashboardsReqId) {
            storeResponse(s_pendingImport, msgId, success, payload);
            return;
        }
    }

    // --- Anmeldung ---
    if (type == "auth_required") {
        JsonDocument authDoc;
        authDoc["type"] = "auth";
        authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; serializeJson(authDoc, authPayload);
        sendRaw(authPayload);
    }
    else if (type == "auth_ok") {
        isHaAuthenticated = true;
        s_failCount = 0;
        s_reconnectDelay = RECONNECT_MIN_MS;
        s_authRetryDelay = AUTH_RETRY_MIN_MS;
        Serial.println("[HA] Angemeldet.");

        // Neue Verbindung (Start oder Reconnect): alle Abos neu anlegen.
        HaEntitySync::OnAuthenticated();
        HaEntityCatalog::OnAuthenticated();
    }
    else if (type == "auth_invalid") {
        // Nicht hier schliessen: wir stecken mitten in haClient.poll().
        s_authRejected = true;
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        isHaConnected = true;
        s_lastRxAt = millis();
        s_pingPending = false;
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        isHaConnected = false;
        isHaAuthenticated = false;
    }
}

// ------------------------------------------------------------------------------
// HA_WS_Task
// ------------------------------------------------------------------------------
// Ein Durchlauf bei bestehender WLAN-Verbindung.
static void serviceConnection() {
    if (haClientMutex == NULL || xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

    if (!haClient.available()) {
        isHaConnected = false;
        isHaAuthenticated = false;

        if ((int32_t)(millis() - s_nextConnectAt) < 0) {
            xSemaphoreGiveRecursive(haClientMutex);
            return;   // Wartezeit laeuft noch
        }

        uint16_t port = 0;
        const char* source = "";
        const String host = currentHost(port, source);
        Serial.printf("[HA] Verbinde mit %s:%u (%s) ...\n", host.c_str(), (unsigned)port, source);

        // Blockiert bis zum TCP-Timeout. Stop()/Start() warten darauf nicht
        // mehr (siehe HaWebsocketLogic_Start).
        const bool ok = haClient.connect(host, port, "/api/websocket");
        xSemaphoreGiveRecursive(haClientMutex);

        if (!ok) {
            s_failCount++;
            if (s_failCount >= HOST_FAILOVER_AFTER) s_useFallbackHost = !s_useFallbackHost;
            Serial.printf("[HA] Verbindung fehlgeschlagen (%u in Folge). Neuer Versuch in %lu s.\n",
                          (unsigned)s_failCount, (unsigned long)(s_reconnectDelay / 1000));
            scheduleReconnect(s_reconnectDelay);
            s_reconnectDelay = min(s_reconnectDelay * 2, RECONNECT_MAX_MS);
        }
        return;
    }

    haClient.poll();
    xSemaphoreGiveRecursive(haClientMutex);

    if (s_authRejected) {
        // Wiederholen hilft bei falschem Token nicht - lange, wachsende Pause.
        s_authRejected = false;
        const uint32_t delayMs = s_authRetryDelay;
        s_authRetryDelay = min(s_authRetryDelay * 2, AUTH_RETRY_MAX_MS);
        closeConnection("Token ungueltig (secrets.h pruefen)", delayMs);
        return;
    }

    // Lebenszeichen pruefen (nur angemeldet; vorher antwortet HA nicht auf Pings).
    if (!isHaAuthenticated) return;
    const uint32_t now = millis();

    if (s_pingPending) {
        if (now - s_pingSentAt > PONG_TIMEOUT_MS) {
            closeConnection("keine Antwort auf Ping", RECONNECT_MIN_MS);
        }
    } else if (now - s_lastRxAt > PING_AFTER_SILENCE_MS) {
        JsonDocument ping;
        ping["type"] = "ping";
        if (HaWebsocketLogic_SendRequest(ping) != 0) {
            s_pingSentAt = now;
            s_pingPending = true;
        }
    }
}

static void haWsTask(void *pvParameters) {
    for (;;) {
        while (haShouldRun) {
            if (WiFi.status() == WL_CONNECTED) {
                // Abos und Katalog-Anfragen nur auf einer angemeldeten Verbindung.
                if (isHaAuthenticated) {
                    HaEntitySync::Tick();
                    HaEntityCatalog::Tick();
                }
                serviceConnection();
                vTaskDelay(pdMS_TO_TICKS(2));
            } else {
                // WLAN weg: Verbindung sofort als getrennt behandeln, statt
                // mit alten Werten weiterzulaufen.
                if (isHaConnected || isHaAuthenticated) {
                    closeConnection("WLAN getrennt", RECONNECT_MIN_MS);
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }

        // Ende nur, wenn nicht inzwischen wieder gestartet wurde. Die
        // Entscheidung faellt unter derselben Sperre wie in Start().
        portENTER_CRITICAL(&haTaskMux);
        if (haShouldRun) {
            portEXIT_CRITICAL(&haTaskMux);
            continue;
        }
        haTaskExiting = true;
        portEXIT_CRITICAL(&haTaskMux);
        break;
    }

    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, portMAX_DELAY) == pdTRUE) {
        haClient.close();
        xSemaphoreGiveRecursive(haClientMutex);
    }
    isHaConnected = false;
    isHaAuthenticated = false;

    // Abbilder und Katalog freigeben (PSRAM). Hier im Task selbst, weil nur
    // er diese Daten anfasst.
    HaEntitySync::Reset();
    HaEntityCatalog::Clear();

    portENTER_CRITICAL(&haTaskMux);
    haTaskHandle = NULL;
    haTaskExiting = false;
    portEXIT_CRITICAL(&haTaskMux);
    vTaskDelete(NULL);
}

// ------------------------------------------------------------------------------
// Start / Stop
// ------------------------------------------------------------------------------
void HaWebsocketLogic_Start() {
    if (haClientMutex == NULL) haClientMutex = xSemaphoreCreateRecursiveMutex();
    if (s_mailboxMutex == NULL) s_mailboxMutex = xSemaphoreCreateMutex();
    HaEntityCache::Init();
    HaEntityCatalog::Init();

    // Laeuft der Task noch (auch wenn Stop() ihn gerade beenden wollte), wird
    // er einfach weiterverwendet. Frueher wartete Start() aktiv im loopTask,
    // bis der alte Task beendet war - der konnte in connect() haengen, und
    // die Oberflaeche fror beim schnellen Wechsel HA -> X -> HA ein.
    portENTER_CRITICAL(&haTaskMux);
    const bool reuse = (haTaskHandle != NULL) && !haTaskExiting;
    if (reuse) haShouldRun = true;
    portEXIT_CRITICAL(&haTaskMux);

    if (reuse) {
        // Stop() hat den Cache geleert - vollen Stand neu anfordern.
        HaEntitySync::RequestResync();
        return;
    }

    // Der Task raeumt gerade auf (Schliessen der Verbindung, Freigeben) -
    // das dauert nur Millisekunden.
    while (haTaskHandle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    HaEntitySync::Reset();   // kein Task aktiv, kein Nebenlaeufer
    s_nextConnectAt = 0;
    s_reconnectDelay = RECONNECT_MIN_MS;
    haShouldRun = true;

    haClient.onMessage(onMessageCallback);
    haClient.onEvent(onEventsCallback);

    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 5, (TaskHandle_t*)&haTaskHandle, 1);
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false;
    HaEntityCache::ClearAll();
}

// ------------------------------------------------------------------------------
// Postfach auswerten (loopTask)
// ------------------------------------------------------------------------------
static void processMediaResponse(const PendingResponse& r) {
    // Antwort auf eine inzwischen ueberholte Anfrage (anderer Player, Ordner
    // gewechselt, Browser geschlossen) verwerfen.
    if (r.id != lastMediaBrowseReqId) return;
    lastMediaBrowseReqId = 0;

    if (!r.success) {
        pendingMediaBrowserError = true;
        return;
    }

    JsonDocument filter;
    filter["result"]["children"][0]["title"] = true;
    filter["result"]["children"][0]["media_content_type"] = true;
    filter["result"]["children"][0]["media_content_id"] = true;
    filter["result"]["children"][0]["can_expand"] = true;
    filter["result"]["children"][0]["can_play"] = true;

    JsonDocument* doc = new JsonDocument(HaPsramAlloc());
    if (deserializeJson(*doc, r.payload, DeserializationOption::Filter(filter)) || doc->overflowed()) {
        delete doc;
        pendingMediaBrowserError = true;
        return;
    }

    HaWebsocketLogic_ClearMediaFolder();
    s_mediaFolder = doc;
    pendingMediaBrowserUpdate = true;

    Serial.printf("[HA-Medien] Ordner mit %u Eintraegen (%u Bytes).\n",
                  (unsigned)HaWebsocketLogic_MediaFolderCount(), (unsigned)r.payload.length());
    HeapMonitor_Log("Medien-Ordner");
}

static JsonArrayConst mediaChildren() {
    return s_mediaFolder ? (*s_mediaFolder)["result"]["children"].as<JsonArrayConst>() : JsonArrayConst();
}

size_t HaWebsocketLogic_MediaFolderCount() {
    return mediaChildren().size();
}

bool HaWebsocketLogic_GetMediaItem(size_t index, MediaBrowserItem& out) {
    JsonArrayConst children = mediaChildren();
    if (index >= children.size()) return false;
    JsonObjectConst child = children[index];
    out.title = child["title"] | "Unbekannt";
    out.media_content_type = child["media_content_type"] | "";
    out.media_content_id = child["media_content_id"] | "";
    out.can_expand = child["can_expand"] | false;
    out.can_play = child["can_play"] | false;
    return true;
}

void HaWebsocketLogic_ClearMediaFolder() {
    delete s_mediaFolder;
    s_mediaFolder = nullptr;
}

static void processImportResponse(const PendingResponse& r) {
    if (r.id == lastLovelaceDashboardsReqId) {
        lastLovelaceDashboardsReqId = 0;
        if (r.success) {
            HaLovelaceParser::parseDashboardList(r.payload);
        } else {
            importErrorMessage = "Home Assistant hat die\nDashboard-Liste verweigert.";
            pendingImportError = true;
        }
        return;
    }

    if (r.id == lastLovelaceReqId) {
        lastLovelaceReqId = 0;
        if (!r.success) {
            importErrorMessage = "Import fehlgeschlagen!\nHome Assistant hat die Anfrage verweigert.";
            pendingImportError = true;
            return;
        }
        if (requestingViewsOnly) {
            HaLovelaceParser::parseViewsList(r.payload);
            return;
        }
        // Ziel-Tab pruefen: er kann seit der Anfrage geloescht worden sein.
        if (currentImportTab < 0 || currentImportTab >= (int)HaConfigLogic::dashboards.size()) {
            importErrorMessage = "Ziel-Tab existiert nicht mehr.";
            pendingImportError = true;
            return;
        }
        isImporting = true;
        HaLovelaceParser::parseCards(r.payload, targetViewIndex, currentImportTab);
    }
    // Sonst: Antwort auf eine abgebrochene Anfrage - verwerfen.
}

void HaWebsocketLogic_ProcessPendingResponses() {
    PendingResponse r;
    if (takeResponse(s_pendingMedia, r)) processMediaResponse(r);

    PendingResponse i;
    if (takeResponse(s_pendingImport, i)) processImportResponse(i);
}

void HaWebsocketLogic_CancelImport() {
    // Spaeter eintreffende Antworten passen auf keine ID mehr und werden
    // verworfen; ein bereits abgelegtes Ergebnis ebenso.
    lastLovelaceReqId = 0;
    lastLovelaceDashboardsReqId = 0;
    PendingResponse dropped;
    takeResponse(s_pendingImport, dropped);
}

void HaWebsocketLogic_CancelMediaBrowse() {
    lastMediaBrowseReqId = 0;
    PendingResponse dropped;
    takeResponse(s_pendingMedia, dropped);
    pendingMediaBrowserUpdate = false;
    pendingMediaBrowserError = false;
}
