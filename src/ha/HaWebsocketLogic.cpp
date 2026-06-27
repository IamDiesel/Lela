#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

static WebsocketsClient haClient;
static bool isHaConnected = false;
static bool isHaAuthenticated = false;
static SemaphoreHandle_t haClientMutex = NULL;

static uint32_t lastPingTime = 0;
static uint32_t messageIdCounter = 1;

static uint32_t getTrackedStatesReqId = 0; 
static uint32_t getGlobalEntitiesReqId = 0; 
static uint32_t delayedGlobalFetchTime = 0; // NEU: Verzoegerter Fetch-Timer

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

    // Schnelles Vorab-Filtern fuer normale Live-State-Updates
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

    // --- Template 1: Instant-Load fuer die sichtbaren Widgets ---
    if (msgId == getTrackedStatesReqId && getTrackedStatesReqId > 0 && success) {
        JsonDocument resFilter; 
        resFilter["result"] = true; 
        
        JsonDocument resDoc;
        if (!deserializeJson(resDoc, payload, DeserializationOption::Filter(resFilter))) {
            const char* renderedJson = resDoc["result"];
            if (renderedJson) {
                JsonDocument doc;
                if (!deserializeJson(doc, renderedJson)) {
                    for (JsonObject entity : doc.as<JsonArray>()) {
                        if (!entity.isNull()) {
                            HaEntityCache::ProcessParsedEntity(entity);
                        }
                    }
                }
            }
        }
        return;
    }

    // --- Template 2: Sicherer und blockierungsfreier Load fuer das globale Menue ---
    if (getGlobalEntitiesReqId > 0 && msgId >= getGlobalEntitiesReqId && msgId <= getGlobalEntitiesReqId + 2 && success) {
        JsonDocument resFilter; 
        resFilter["result"] = true; 
        
        JsonDocument resDoc;
        if (!deserializeJson(resDoc, payload, DeserializationOption::Filter(resFilter))) {
            const char* renderedJson = resDoc["result"];
            if (renderedJson) {
                JsonDocument doc;
                
                if (!deserializeJson(doc, renderedJson)) {
                    int yieldCounter = 0;
                    JsonArray arr = doc.as<JsonArray>();
                    for (JsonObject obj : arr) {
                        
                        // ANTI-STARVATION FIX: Wir locken den Mutex nur fuer Mikrosekunden pro Element!
                        // Core 1 (UI) kann nun exakt dazwischen funken, um die Widgets abzurufen!
                        if (HaEntityCache::mutex && xSemaphoreTake(HaEntityCache::mutex, portMAX_DELAY)) {
                            String e_id = obj["id"].as<String>();
                            String e_name = obj["name"].as<String>();
                            
                            HaEntityCache::globalEntityMap[e_id] = e_name;
                            
                            if (obj["opt"].is<JsonArray>()) {
                                HaEntityCache::globalOptionsMap[e_id].clear();
                                for (JsonVariant v : obj["opt"].as<JsonArray>()) {
                                    HaEntityCache::globalOptionsMap[e_id].push_back(v.as<String>());
                                }
                            }
                            if (obj["min"].is<float>()) HaEntityCache::globalMinMap[e_id] = obj["min"].as<float>();
                            if (obj["max"].is<float>()) HaEntityCache::globalMaxMap[e_id] = obj["max"].as<float>();
                            if (obj["step"].is<float>()) HaEntityCache::globalStepMap[e_id] = obj["step"].as<float>();
                            
                            xSemaphoreGive(HaEntityCache::mutex);
                        }
                        
                        // ANTI-CRASH FIX: Nach jedem 15. Element MUSS Core 0 atmen! 
                        // So wird der Task-Watchdog rechtzeitig bedient -> 0 Abstuerze.
                        yieldCounter++;
                        if (yieldCounter % 15 == 0) {
                            vTaskDelay(pdMS_TO_TICKS(5));
                        }
                    }
                }
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
                
                // Thread-sichere Kopie der getrackten Entitaeten erstellen
                std::vector<String> localTracked;
                if (HaEntityCache::mutex && xSemaphoreTake(HaEntityCache::mutex, portMAX_DELAY)) {
                    localTracked = HaEntityCache::trackedEntities;
                    xSemaphoreGive(HaEntityCache::mutex);
                }
                
                // Abfrage 1: Nur die States fuer die sichtbaren Widgets laden
                if (localTracked.size() > 0) {
                    getTrackedStatesReqId = messageIdCounter++;
                    
                    // FIXED: We ask for the FULL STATE OBJECT as dictionary but manually strip large fields.
                    // This prevents HA from sending huge base64 'entity_picture' strings which crash the ESP32 RAM!
                    String templTracked = "[";
                    for(size_t i=0; i<localTracked.size(); i++) {
                        templTracked += "{% set s = states['" + localTracked[i] + "'] %}{% if s %}{\"entity_id\":\"{{s.entity_id}}\",\"state\":{{s.state|tojson}},\"attributes\":{\"icon\":{{s.attributes.get('icon')|tojson}},\"friendly_name\":{{s.attributes.get('friendly_name')|tojson}},\"unit_of_measurement\":{{s.attributes.get('unit_of_measurement')|tojson}},\"battery_level\":{{s.attributes.get('battery_level')|tojson}},\"fan_speed\":{{s.attributes.get('fan_speed')|tojson}},\"brightness\":{{s.attributes.get('brightness')|tojson}},\"color_temp\":{{s.attributes.get('color_temp')|tojson}},\"rgb_color\":{{s.attributes.get('rgb_color')|tojson}},\"rgbw_color\":{{s.attributes.get('rgbw_color')|tojson}},\"supported_color_modes\":{{s.attributes.get('supported_color_modes')|tojson}},\"media_title\":{{s.attributes.get('media_title')|tojson}},\"media_artist\":{{s.attributes.get('media_artist')|tojson}},\"volume_level\":{{s.attributes.get('volume_level')|tojson}},\"source\":{{s.attributes.get('source')|tojson}},\"source_list\":{{s.attributes.get('source_list')|tojson}},\"current_position\":{{s.attributes.get('current_position')|tojson}},\"current_temperature\":{{s.attributes.get('current_temperature')|tojson}},\"temperature\":{{s.attributes.get('temperature')|tojson}},\"options\":{{s.attributes.get('options')|tojson}},\"hvac_modes\":{{s.attributes.get('hvac_modes')|tojson}},\"min\":{{s.attributes.get('min')|tojson}},\"max\":{{s.attributes.get('max')|tojson}},\"step\":{{s.attributes.get('step')|tojson}},\"min_temp\":{{s.attributes.get('min_temp')|tojson}},\"max_temp\":{{s.attributes.get('max_temp')|tojson}},\"target_temp_step\":{{s.attributes.get('target_temp_step')|tojson}}}}{% else %}null{% endif %}";
                        if(i < localTracked.size() - 1) templTracked += ",";
                    }
                    templTracked += "]";

                    JsonDocument reqTracked;
                    reqTracked["id"] = getTrackedStatesReqId;
                    reqTracked["type"] = "render_template";
                    reqTracked["template"] = templTracked;
                    String req1; serializeJson(reqTracked, req1);
                    HaWebsocketLogic_SendPayload(req1);
                }

                // STAGGER-FIX: Die schwere Abfrage 2 fuer das Globale-Menue wird um 
                // 1,5 Sekunden verzögert, damit die UI zu 100% flüssig hochfährt!
                delayedGlobalFetchTime = millis() + 1500;
            }

            // Wenn die 1,5 Sekunden um sind und die Widgets erfolgreich gezeichnet wurden:
            if (delayedGlobalFetchTime > 0 && millis() > delayedGlobalFetchTime) {
                delayedGlobalFetchTime = 0;
                if (isHaAuthenticated) {
                    
                    // LIMIT-FIX & SPLIT-FIX: Wir teilen die riesige 60KB+ Abfrage in 3 kleine Happen auf.
                    // So verhindern wir, dass der TCP-Socket und der JSON-Parser den ESP32 blockieren
                    // und den Watchdog ausloesen.
                    String templ1 = "{% set ns = namespace(first=true, count=0) %}["
                                    "{% for s in states %}{% if s.domain in ['light','switch','cover','vacuum'] %}{% if ns.count < 150 %}"
                                    "{% if not ns.first %},{% endif %}{\"id\":\"{{s.entity_id}}\",\"name\":{{ s.attributes.friendly_name | default('') | tojson }}}"
                                    "{% set ns.first = false %}{% set ns.count = ns.count + 1 %}"
                                    "{% endif %}{% endif %}{% endfor %}]";
                                    
                    String templ2 = "{% set ns = namespace(first=true, count=0) %}["
                                    "{% for s in states %}{% if s.domain in ['select','input_select','climate','media_player'] %}{% if ns.count < 150 %}"
                                    "{% if not ns.first %},{% endif %}{\"id\":\"{{s.entity_id}}\",\"name\":{{ s.attributes.friendly_name | default('') | tojson }}"
                                    "{% if s.domain in ['select','input_select','climate'] %},\"opt\":{{ s.attributes.options | default(s.attributes.hvac_modes | default([])) | tojson }}{% endif %}"
                                    "{% if s.domain in ['climate'] %},\"min\":{{ s.attributes.min_temp | default('null') }},\"max\":{{ s.attributes.max_temp | default('null') }},\"step\":{{ s.attributes.target_temp_step | default('null') }}{% endif %}"
                                    "}{% set ns.first = false %}{% set ns.count = ns.count + 1 %}"
                                    "{% endif %}{% endif %}{% endfor %}]";

                    String templ3 = "{% set ns = namespace(first=true, count=0) %}["
                                    "{% for s in states %}{% if s.domain in ['number','input_number','text','input_text','button','input_button'] %}{% if ns.count < 150 %}"
                                    "{% if not ns.first %},{% endif %}{\"id\":\"{{s.entity_id}}\",\"name\":{{ s.attributes.friendly_name | default('') | tojson }}"
                                    "{% if s.domain in ['number','input_number','text','input_text'] %},\"min\":{{ s.attributes.min | default('null') }},\"max\":{{ s.attributes.max | default('null') }},\"step\":{{ s.attributes.step | default('null') }}{% endif %}"
                                    "}{% set ns.first = false %}{% set ns.count = ns.count + 1 %}"
                                    "{% endif %}{% endif %}{% endfor %}]";

                    getGlobalEntitiesReqId = messageIdCounter;
                    messageIdCounter += 3;

                    JsonDocument req1; req1["id"] = getGlobalEntitiesReqId; req1["type"] = "render_template"; req1["template"] = templ1;
                    String r1; serializeJson(req1, r1); HaWebsocketLogic_SendPayload(r1);

                    JsonDocument req2; req2["id"] = getGlobalEntitiesReqId + 1; req2["type"] = "render_template"; req2["template"] = templ2;
                    String r2; serializeJson(req2, r2); HaWebsocketLogic_SendPayload(r2);

                    JsonDocument req3; req3["id"] = getGlobalEntitiesReqId + 2; req3["type"] = "render_template"; req3["template"] = templ3;
                    String r3; serializeJson(req3, r3); HaWebsocketLogic_SendPayload(r3);
                }
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
    
    // WATCHDOG-FIX: Prio auf 0 setzen, damit der Task beim Lesen großer WebSocket-Nachrichten
    // seine CPU-Zeit ordentlich mit dem IDLE0 Task (der den Watchdog fuettert) teilt!
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 5, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    HaEntityCache::ClearAll();
}