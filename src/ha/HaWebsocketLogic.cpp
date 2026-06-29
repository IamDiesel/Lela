#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// WICHTIG: Der SpiRamAllocator wurde hier komplett entfernt!
// Wir nutzen fuer die Live-Updates wieder den ultraschnellen internen RAM.

static WebsocketsClient haClient;
static bool isHaConnected = false;
static bool isHaAuthenticated = false;
static SemaphoreHandle_t haClientMutex = NULL;

static uint32_t lastPingTime = 0;
static uint32_t messageIdCounter = 1;
static uint32_t getStatesReqId = 0; 
static volatile TaskHandle_t haTaskHandle = NULL;
static volatile bool haShouldRun = false;

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
    if (!message.isText()) {
        return;
    }
    
    String payload = message.data();

    // Schnelles Vorab-Filtern fuer normale State-Updates
    if (payload.indexOf("\"type\":\"event\"") != -1 || payload.indexOf("\"type\": \"event\"") != -1) {
        if (payload.indexOf("\"state_changed\"") != -1) {
            JsonDocument filter; // Nutzt internen RAM!
            filter["event"]["data"]["entity_id"] = true; 
            filter["event"]["data"]["new_state"] = true;
            
            JsonDocument doc; // Nutzt internen RAM!
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

    // Vollstaendiger State-Sync beim Start
    if (msgId == getStatesReqId && getStatesReqId > 0 && success) {
        JsonDocument statesFilter;
        statesFilter["result"][0]["entity_id"] = true; 
        statesFilter["result"][0]["state"] = true; 
        statesFilter["result"][0]["attributes"] = true; 
        
        JsonDocument doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(statesFilter), DeserializationOption::NestingLimit(250))) {
            for (JsonObject entity : doc["result"].as<JsonArray>()) {
                HaEntityCache::ProcessParsedEntity(entity);
            }
        }
        return;
    }

    // Media Browser Antwort
    if (msgId == lastMediaBrowseReqId && lastMediaBrowseReqId > 0) {
        if (!success) { 
            pendingMediaBrowserError = true; 
            return; 
        }
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
                
                if (currentMediaFolder.size() > 100) {
                    break; 
                }
            }
        }
        pendingMediaBrowserUpdate = true;
        return;
    }

    // Lovelace Import (Views oder Cards)
    if (msgId == lastLovelaceReqId && lastLovelaceReqId > 0) {
        if (!success) {
            importErrorMessage = "Import fehlgeschlagen!\nHome Assistant hat die Anfrage verweigert.";
            pendingImportError = true; 
            return;
        }
        if (requestingViewsOnly) {
            HaLovelaceParser::parseViewsList(payload);
        } else {
            isImporting = true;
            HaLovelaceParser::parseCards(payload, targetViewIndex, currentImportTab);
        }
        return;
    }

    // Dashboard Liste laden
    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) {
            HaLovelaceParser::parseDashboardList(payload);
        }
        return;
    }

    // Authentifizierungsprozess
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
    if (event == WebsocketsEvent::ConnectionOpened) {
        isHaConnected = true; 
    } else if (event == WebsocketsEvent::ConnectionClosed) { 
        isHaConnected = false; 
        isHaAuthenticated = false; 
    }
}

void haWsTask(void *pvParameters) {
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            
            // Wenn initial (nach Login) alle States angefragt werden sollen
            if (HaEntityCache::triggerRestStateFetch && isHaAuthenticated) {
                HaEntityCache::triggerRestStateFetch = false;
                getStatesReqId = messageIdCounter++;
                
                JsonDocument reqDoc;
                reqDoc["id"] = getStatesReqId; 
                reqDoc["type"] = "get_states"; 
                
                String reqPayload; 
                serializeJson(reqDoc, reqPayload); 
                HaWebsocketLogic_SendPayload(reqPayload); 
            }

            if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (!haClient.available()) {
                    isHaConnected = false; 
                    isHaAuthenticated = false;
                    
                    if (!haClient.connect("ws://" + String(SECRET_HA_IP) + ":" + String(SECRET_HA_PORT) + "/api/websocket")) {
                        xSemaphoreGiveRecursive(haClientMutex);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        continue;
                    }
                } else {
                    haClient.poll();
                    
                    // Ping senden, um Websocket am Leben zu halten
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
    if (haShouldRun && haTaskHandle != NULL) {
        return; 
    }
    
    while (haTaskHandle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    haShouldRun = true; 
    HaEntityCache::Init();
    
    if (haClientMutex == NULL) {
        haClientMutex = xSemaphoreCreateRecursiveMutex();
    }
    
    haClient.onMessage(onMessageCallback); 
    haClient.onEvent(onEventsCallback);
    
    // WICHTIG: Prioritaet 5! So räumt der Task die Netzwerk-Pakete
    // aus dem Puffer, selbst wenn LVGL gerade CPU verbraucht.
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 5, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    HaEntityCache::ClearAll();
}