#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h> // WICHTIG FUER DEN ZERO-COPY RAM!

using namespace websockets;

static WebsocketsClient haClient;
static bool isHaConnected = false;
static bool isHaAuthenticated = false;
static SemaphoreHandle_t haClientMutex = NULL;

static uint32_t lastPingTime = 0;
static uint32_t messageIdCounter = 1;
static uint32_t getStatesReqId = 0; 
static volatile TaskHandle_t haTaskHandle = NULL;
static volatile bool haShouldRun = false;

// --- PROFILING VARIABLEN ---
static uint32_t prof_start_connect = 0;
static uint32_t prof_connected = 0;
static uint32_t prof_auth = 0;
static uint32_t prof_req_sent = 0;
static uint32_t prof_req_recv = 0;

// Globale Import Variablen
std::vector<String> availableDashboardUrls;
std::vector<String> availableDashboardTitles;
volatile bool pendingDashboardList = false;
volatile bool pendingImportError = false;
String importErrorMessage = ""; 
std::vector<String> availableViewTitles;
volatile bool pendingViewList = false;
String currentImportUrl = "";
volatile bool isImporting = false;
std::vector<MediaBrowserItem> currentMediaFolder;
volatile bool pendingMediaBrowserUpdate = false;
volatile bool pendingMediaBrowserError = false;

uint32_t lastLovelaceReqId = 0;
uint32_t lastLovelaceDashboardsReqId = 0;
uint32_t lastMediaBrowseReqId = 0; 
bool requestingViewsOnly = false; 
int targetViewIndex = 0;
int currentImportTab = -1;

bool HaWebsocketLogic_IsConnected() { 
    return isHaConnected && isHaAuthenticated; 
}

uint32_t HaWebsocketLogic_GetNextMessageId() { 
    return messageIdCounter++; 
}

void HaWebsocketLogic_SendPayload(const String& payload) {
    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    
    String payload = message.data();

    // 1. Live State Updates
    if (payload.indexOf("\"type\":\"event\"") != -1 || payload.indexOf("\"type\": \"event\"") != -1) {
        if (payload.indexOf("\"state_changed\"") != -1) {
            JsonDocument filter; 
            filter["event"]["data"]["entity_id"] = true; 
            filter["event"]["data"]["new_state"] = true;
            
            JsonDocument doc; 
            if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
                HaEntityCache::ProcessParsedEntity(doc["event"]["data"]["new_state"]);
            }
        }
        return; 
    }

    JsonDocument headerFilter;
    headerFilter["type"] = true; 
    headerFilter["id"] = true; 
    headerFilter["success"] = true;
    
    JsonDocument headerDoc;
    if (deserializeJson(headerDoc, payload, DeserializationOption::Filter(headerFilter), DeserializationOption::NestingLimit(200))) {
        return;
    }

    String type = headerDoc["type"] | "";
    uint32_t msgId = headerDoc["id"] | 0;
    bool success = headerDoc["success"] | false;

    // 2. Das dicke 'get_states' Paket (ZERO-COPY PARSING)
    if (msgId == getStatesReqId && getStatesReqId > 0 && success) {
        prof_req_recv = millis();
        Serial.printf("[PROFILING] 4. 'get_states' Antwort da! Server-Latenz: %d ms\n", prof_req_recv - prof_req_sent);
        Serial.printf("[PROFILING] 4a. Paketgroesse: %d Bytes\n", payload.length());

        uint32_t parse_start = millis();

        // --- DER ZERO-COPY FIX ---
        // Wir reservieren einen bearbeitbaren Puffer im PSRAM und kopieren den Text hinein.
        char* mut_buf = (char*)heap_caps_malloc(payload.length() + 1, MALLOC_CAP_SPIRAM);
        if (!mut_buf) {
            Serial.println("[PROFILING] ERROR: Nicht genug PSRAM fuer Zero-Copy Puffer!");
            return;
        }
        strcpy(mut_buf, payload.c_str()); // Kopiert den String 1x hart in den Puffer

        // --- DER FILTER FIX ---
        JsonDocument filter;
        filter["result"][0]["entity_id"] = true;
        filter["result"][0]["state"] = true;
        filter["result"][0]["attributes"] = true;

        JsonDocument doc;
        // Durch die Übergabe von mut_buf (char*) macht ArduinoJson KEINE Stringkopien mehr!
        DeserializationError err = deserializeJson(doc, mut_buf, DeserializationOption::Filter(filter));

        uint32_t parse_end = millis();
        Serial.printf("[PROFILING] 5. JSON Zero-Copy Parse dauerte: %d ms\n", parse_end - parse_start);

        if (!err) {
            uint32_t process_start = millis();
            
            JsonArray arr = doc["result"].as<JsonArray>();
            int count = 0;
            
            for (JsonObject entity : arr) {
                HaEntityCache::ProcessParsedEntity(entity);
                count++;
                
                // Watchdog atmen lassen
                if (count % 25 == 0) vTaskDelay(pdMS_TO_TICKS(5));
            }
            
            uint32_t process_end = millis();
            Serial.printf("[PROFILING] 6. ProcessParsedEntity Schleife dauerte: %d ms fuer %d Entities\n", process_end - process_start, count);
            Serial.printf("[PROFILING] === GESAMTZEIT SEIT CONNECT: %d ms ===\n\n", process_end - prof_start_connect);
        } else {
            Serial.printf("[PROFILING] JSON ERROR: %s\n", err.c_str());
        }

        // Puffer wieder freigeben, nachdem wir alle Daten verarbeitet haben!
        heap_caps_free(mut_buf);
        return;
    }

    // Media Browser Antwort
    if (msgId == lastMediaBrowseReqId && lastMediaBrowseReqId > 0) {
        if (!success) { pendingMediaBrowserError = true; return; }
        JsonDocument browseFilter;
        browseFilter["result"]["children"] = true; 
        JsonDocument doc;
        deserializeJson(doc, payload, DeserializationOption::Filter(browseFilter));
        currentMediaFolder.clear();
        JsonArray children = doc["result"]["children"];
        if (!children.isNull()) {
            for (JsonObject child : children) {
                MediaBrowserItem item;
                item.title = child["title"] | "Unbekannt"; 
                item.media_content_type = child["media_content_type"] | "";
                item.media_content_id = child["media_content_id"] | ""; 
                item.can_expand = child["can_expand"] | false;
                item.can_play = child["can_play"] | false;
                currentMediaFolder.push_back(item);
                if (currentMediaFolder.size() > 100) break; 
            }
        }
        pendingMediaBrowserUpdate = true;
        return;
    }

    // Lovelace Import
    if (msgId == lastLovelaceReqId && lastLovelaceReqId > 0) {
        if (!success) {
            importErrorMessage = "Import fehlgeschlagen!\nHome Assistant hat die Anfrage verweigert.";
            pendingImportError = true; 
            return;
        }
        if (requestingViewsOnly) HaLovelaceParser::parseViewsList(payload);
        else {
            isImporting = true;
            HaLovelaceParser::parseCards(payload, targetViewIndex, currentImportTab);
        }
        return;
    }

    // Dashboard Liste laden
    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) HaLovelaceParser::parseDashboardList(payload);
        return;
    }

    // Authentifizierung
    if (type == "auth_required") {
        JsonDocument authDoc;
        authDoc["type"] = "auth"; 
        authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; 
        serializeJson(authDoc, authPayload); 
        HaWebsocketLogic_SendPayload(authPayload);
    }
    else if (type == "auth_ok") {
        isHaAuthenticated = true;
        prof_auth = millis();
        Serial.printf("[PROFILING] 2. Auth_OK. Dauer seit Connect: %d ms\n", prof_auth - prof_connected);
        
        JsonDocument subDoc;
        subDoc["id"] = messageIdCounter++; 
        subDoc["type"] = "subscribe_events"; 
        subDoc["event_type"] = "state_changed";
        String subPayload; 
        serializeJson(subDoc, subPayload); 
        HaWebsocketLogic_SendPayload(subPayload);
        
        HaEntityCache::triggerRestStateFetch = true; 
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) isHaConnected = true; 
    else if (event == WebsocketsEvent::ConnectionClosed) { 
        isHaConnected = false; 
        isHaAuthenticated = false; 
    }
}

void haWsTask(void *pvParameters) {
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            
            if (HaEntityCache::triggerRestStateFetch && isHaAuthenticated) {
                HaEntityCache::triggerRestStateFetch = false;
                
                getStatesReqId = messageIdCounter++;
                JsonDocument reqDoc;
                reqDoc["id"] = getStatesReqId;
                reqDoc["type"] = "get_states";
                String reqPayload; 
                serializeJson(reqDoc, reqPayload);
                
                prof_req_sent = millis();
                Serial.printf("[PROFILING] 3. Sende 'get_states' an HA. Zeit seit Auth: %d ms\n", prof_req_sent - prof_auth);
                
                HaWebsocketLogic_SendPayload(reqPayload);
            }

            if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (!haClient.available()) {
                    isHaConnected = false; 
                    isHaAuthenticated = false;
                    
                    prof_start_connect = millis();
                    Serial.println("\n===============================================");
                    Serial.println("[PROFILING] 1. Starte TCP/SSL Connect...");
                    
                    if (!haClient.connect("ws://" + String(SECRET_HA_IP) + ":" + String(SECRET_HA_PORT) + "/api/websocket")) {
                        xSemaphoreGiveRecursive(haClientMutex);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        continue;
                    }
                    prof_connected = millis();
                    Serial.printf("[PROFILING] -> Connect erfolgreich! Dauer: %d ms\n", prof_connected - prof_start_connect);
                    
                } else {
                    haClient.poll();
                    
                    uint32_t now = millis();
                    if (now - lastPingTime > 30000) {
                        if (isHaAuthenticated) {
                            JsonDocument pingDoc;
                            pingDoc["id"] = messageIdCounter++; 
                            pingDoc["type"] = "ping";
                            String pingStr; 
                            serializeJson(pingDoc, pingStr); 
                            haClient.send(pingStr); 
                        }
                        lastPingTime = now;
                    }
                }
                xSemaphoreGiveRecursive(haClientMutex);
            }
            vTaskDelay(pdMS_TO_TICKS(10)); 
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, portMAX_DELAY) == pdTRUE) { 
        haClient.close(); 
        xSemaphoreGiveRecursive(haClientMutex); 
    }
    
    isHaConnected = false; 
    isHaAuthenticated = false; 
    haTaskHandle = NULL; 
    vTaskDelete(NULL);
}

void HaWebsocketLogic_Start() {
    if (haShouldRun && haTaskHandle != NULL) return; 
    
    while (haTaskHandle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    haShouldRun = true; 
    HaEntityCache::Init();
    
    if (haClientMutex == NULL) haClientMutex = xSemaphoreCreateRecursiveMutex();
    
    haClient.onMessage(onMessageCallback); 
    haClient.onEvent(onEventsCallback);
    
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 5, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    HaEntityCache::ClearAll();
}