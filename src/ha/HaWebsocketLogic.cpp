#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h> 

using namespace websockets;

class SpiRamAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    void deallocate(void* pointer) override { heap_caps_free(pointer); }
    void* reallocate(void* pointer, size_t new_size) override { return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM); }
};
static SpiRamAllocator psramAlloc;

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

bool HaWebsocketLogic_IsConnected() { return isHaConnected && isHaAuthenticated; }
uint32_t HaWebsocketLogic_GetNextMessageId() { return messageIdCounter++; }

void HaWebsocketLogic_SendPayload(const String& payload) {
    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    String payload = message.data();

    if (payload.indexOf("\"type\":\"event\"") != -1 || payload.indexOf("\"type\": \"event\"") != -1) {
        if (payload.indexOf("\"state_changed\"") != -1) {
            JsonDocument filter(&psramAlloc);
            filter["event"]["data"]["entity_id"] = true; filter["event"]["data"]["new_state"] = true;
            JsonDocument doc(&psramAlloc);
            if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
                HaEntityCache::ProcessParsedEntity(doc["event"]["data"]["new_state"]);
            }
        }
        return; 
    }

    JsonDocument headerFilter(&psramAlloc);
    headerFilter["type"] = true; headerFilter["id"] = true; headerFilter["success"] = true;
    JsonDocument headerDoc(&psramAlloc);
    if (deserializeJson(headerDoc, payload, DeserializationOption::Filter(headerFilter), DeserializationOption::NestingLimit(200))) return;

    String type = headerDoc["type"] | "";
    uint32_t msgId = headerDoc["id"] | 0;
    bool success = headerDoc["success"] | false;

    if (msgId == getStatesReqId && getStatesReqId > 0 && success) {
        JsonDocument statesFilter(&psramAlloc);
        statesFilter["result"][0]["entity_id"] = true; statesFilter["result"][0]["state"] = true; statesFilter["result"][0]["attributes"] = true; 
        JsonDocument doc(&psramAlloc);
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(statesFilter), DeserializationOption::NestingLimit(250))) {
            for (JsonObject entity : doc["result"].as<JsonArray>()) HaEntityCache::ProcessParsedEntity(entity);
        }
        return;
    }

    if (msgId == lastMediaBrowseReqId && lastMediaBrowseReqId > 0) {
        if (!success) { pendingMediaBrowserError = true; return; }
        JsonDocument browseFilter(&psramAlloc);
        browseFilter["result"]["children"] = true; 
        JsonDocument doc(&psramAlloc);
        deserializeJson(doc, payload, DeserializationOption::Filter(browseFilter));
        currentMediaFolder.clear();
        JsonArray children = doc["result"]["children"];
        if (!children.isNull()) {
            for (JsonObject child : children) {
                MediaBrowserItem item;
                item.title = child["title"] | "Unbekannt"; item.media_content_type = child["media_content_type"] | "";
                item.media_content_id = child["media_content_id"] | ""; item.can_expand = child["can_expand"] | false;
                item.can_play = child["can_play"] | false;
                currentMediaFolder.push_back(item);
                if (currentMediaFolder.size() > 100) break; 
            }
        }
        pendingMediaBrowserUpdate = true;
        return;
    }

    if (msgId == lastLovelaceReqId && lastLovelaceReqId > 0) {
        if (!success) {
            importErrorMessage = "Import fehlgeschlagen!\nHome Assistant hat die Anfrage verweigert.";
            pendingImportError = true; return;
        }
        if (requestingViewsOnly) HaLovelaceParser::parseViewsList(payload);
        else {
            isImporting = true;
            HaLovelaceParser::parseCards(payload, targetViewIndex, currentImportTab);
        }
        return;
    }

    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) HaLovelaceParser::parseDashboardList(payload);
        return;
    }

    if (type == "auth_required") {
        JsonDocument authDoc(&psramAlloc);
        authDoc["type"] = "auth"; authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; serializeJson(authDoc, authPayload); 
        HaWebsocketLogic_SendPayload(authPayload);
    }
    else if (type == "auth_ok") {
        isHaAuthenticated = true;
        JsonDocument subDoc(&psramAlloc);
        subDoc["id"] = messageIdCounter++; subDoc["type"] = "subscribe_events"; subDoc["event_type"] = "state_changed";
        String subPayload; serializeJson(subDoc, subPayload); 
        HaWebsocketLogic_SendPayload(subPayload);
        HaEntityCache::triggerRestStateFetch = true; 
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) isHaConnected = true; 
    else if (event == WebsocketsEvent::ConnectionClosed) { isHaConnected = false; isHaAuthenticated = false; }
}

void haWsTask(void *pvParameters) {
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            if (HaEntityCache::triggerRestStateFetch && isHaAuthenticated) {
                HaEntityCache::triggerRestStateFetch = false;
                getStatesReqId = messageIdCounter++;
                JsonDocument reqDoc(&psramAlloc);
                reqDoc["id"] = getStatesReqId; reqDoc["type"] = "get_states"; 
                String reqPayload; serializeJson(reqDoc, reqPayload); 
                HaWebsocketLogic_SendPayload(reqPayload); 
            }

            if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (!haClient.available()) {
                    isHaConnected = false; isHaAuthenticated = false;
                    if (!haClient.connect("ws://" + String(SECRET_HA_IP) + ":" + String(SECRET_HA_PORT) + "/api/websocket")) {
                        xSemaphoreGiveRecursive(haClientMutex);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        continue;
                    }
                } else {
                    haClient.poll();
                    uint32_t now = millis();
                    if (now - lastPingTime > 30000) {
                        if (isHaAuthenticated) {
                            JsonDocument pingDoc(&psramAlloc);
                            pingDoc["id"] = messageIdCounter++; pingDoc["type"] = "ping";
                            String pingStr; serializeJson(pingDoc, pingStr); 
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

    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, portMAX_DELAY) == pdTRUE) { haClient.close(); xSemaphoreGiveRecursive(haClientMutex); }
    isHaConnected = false; isHaAuthenticated = false; haTaskHandle = NULL; 
    vTaskDelete(NULL);
}

void HaWebsocketLogic_Start() {
    if (haShouldRun && haTaskHandle != NULL) return; 
    while (haTaskHandle != NULL) vTaskDelay(pdMS_TO_TICKS(10));
    
    haShouldRun = true; 
    HaEntityCache::Init();
    if (haClientMutex == NULL) haClientMutex = xSemaphoreCreateRecursiveMutex();
    
    haClient.onMessage(onMessageCallback); haClient.onEvent(onEventsCallback);
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 1, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    HaEntityCache::ClearAll();
}