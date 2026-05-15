#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "secrets.h"
#include "ViewHomeAssistant.h" 
#include "HaConfigLogic.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h> 
#include "lvgl.h" 

using namespace websockets;

// PSRAM Allocator für sicheres Speichermanagement großer JSONs
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

static uint32_t lastPingTime = 0;
static uint32_t messageIdCounter = 1;
static uint32_t getStatesReqId = 0; // Speichert die ID für unsere initiale Zustandsabfrage

static volatile TaskHandle_t haTaskHandle = NULL;
static volatile bool haShouldRun = false;

static std::map<String, String> haEntityStates;
static SemaphoreHandle_t haStateMutex = NULL;
static SemaphoreHandle_t haClientMutex = NULL;

static std::map<String, String> haEntityIcons;
static std::map<String, String> haEntityNames;

static std::map<String, int> haEntityBrightness;
static std::map<String, uint32_t> haEntityRGB;
static std::map<String, int> haEntityWhite;
static std::map<String, bool> haEntityIsRGBW;

static std::map<String, String> haEntityUnits;
static std::map<String, String> haEntityMediaTitle;
static std::map<String, String> haEntityMediaArtist;
static std::map<String, float> haEntityMediaVolume;

static std::map<String, std::vector<String>> haEntitySourceList;
static std::map<String, String> haEntitySource;

std::vector<String> trackedEntities;

std::vector<String> availableDashboardUrls;
std::vector<String> availableDashboardTitles;
volatile bool pendingDashboardList = false;
volatile bool pendingImportError = false;
String importErrorMessage = ""; 

std::vector<String> availableViewTitles;
volatile bool pendingViewList = false;
String currentImportUrl = "";

volatile bool isImporting = false;

static int currentImportTab = -1;
static uint32_t lastLovelaceReqId = 0;
static uint32_t lastLovelaceDashboardsReqId = 0;
static uint32_t lastMediaBrowseReqId = 0; 
static bool requestingViewsOnly = false; 
static int targetViewIndex = 0;

std::vector<MediaBrowserItem> currentMediaFolder;
volatile bool pendingMediaBrowserUpdate = false;
volatile bool pendingMediaBrowserError = false;

volatile bool triggerRestStateFetch = false; 

void safeSend(const String& payload) {
    if (haClientMutex != NULL && xSemaphoreTakeRecursive(haClientMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

int HaWebsocketLogic_GetBrightness(String entity_id) {
    int res = -1;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityBrightness.find(entity_id) != haEntityBrightness.end()) res = haEntityBrightness[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

uint32_t HaWebsocketLogic_GetRGB(String entity_id) {
    uint32_t res = 0xFFFFFFFF; 
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityRGB.find(entity_id) != haEntityRGB.end()) res = haEntityRGB[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

int HaWebsocketLogic_GetWhite(String entity_id) {
    int res = -1;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityWhite.find(entity_id) != haEntityWhite.end()) res = haEntityWhite[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

bool HaWebsocketLogic_IsRGBW(String entity_id) {
    bool res = false;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityIsRGBW.find(entity_id) != haEntityIsRGBW.end()) res = haEntityIsRGBW[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

String HaWebsocketLogic_GetUnit(String entity_id) {
    String res = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityUnits.find(entity_id) != haEntityUnits.end()) res = haEntityUnits[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

String HaWebsocketLogic_GetMediaTitle(String entity_id) {
    String res = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityMediaTitle.find(entity_id) != haEntityMediaTitle.end()) res = haEntityMediaTitle[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

String HaWebsocketLogic_GetMediaArtist(String entity_id) {
    String res = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityMediaArtist.find(entity_id) != haEntityMediaArtist.end()) res = haEntityMediaArtist[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

float HaWebsocketLogic_GetMediaVolume(String entity_id) {
    float res = -1.0f;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityMediaVolume.find(entity_id) != haEntityMediaVolume.end()) res = haEntityMediaVolume[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

std::vector<String> HaWebsocketLogic_GetMediaSourceList(String entity_id) {
    std::vector<String> res;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntitySourceList.find(entity_id) != haEntitySourceList.end()) res = haEntitySourceList[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

String HaWebsocketLogic_GetMediaSource(String entity_id) {
    String res = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntitySource.find(entity_id) != haEntitySource.end()) res = haEntitySource[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return res;
}

bool HaWebsocketLogic_EntityExists(String entity_id) {
    bool exists = false;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        exists = (haEntityStates.find(entity_id) != haEntityStates.end() || 
                  haEntityNames.find(entity_id) != haEntityNames.end() ||
                  haEntityIcons.find(entity_id) != haEntityIcons.end());
        xSemaphoreGive(haStateMutex);
    }
    return exists;
}

String HaWebsocketLogic_GetEntityName(String entity_id) {
    String name = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityNames.find(entity_id) != haEntityNames.end()) name = haEntityNames[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return name;
}

String HaWebsocketLogic_GetCachedIcon(String entity_id) {
    String icon = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (haEntityIcons.find(entity_id) != haEntityIcons.end()) icon = haEntityIcons[entity_id];
        xSemaphoreGive(haStateMutex);
    }
    return icon;
}

void HaWebsocketLogic_UpdateTrackedEntities() {
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        trackedEntities.clear();
        for (const auto& tab : HaConfigLogic::dashboards) {
            for (const auto& w : tab.widgets) {
                if (std::find(trackedEntities.begin(), trackedEntities.end(), w.entity_id) == trackedEntities.end()) {
                    trackedEntities.push_back(w.entity_id);
                }
            }
        }
        
        triggerRestStateFetch = true; // Jetzt der Trigger für den WS get_states call
        xSemaphoreGive(haStateMutex);
    }
}

void updateEntityState(const String& entity_id, const String& state) {
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (std::find(trackedEntities.begin(), trackedEntities.end(), entity_id) != trackedEntities.end()) {
            haEntityStates[entity_id] = state;
        }
        xSemaphoreGive(haStateMutex);
    }
}

String HaWebsocketLogic_GetState(String entity_id) {
    String result = "";
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        auto it = haEntityStates.find(entity_id);
        if (it != haEntityStates.end()) result = it->second;
        xSemaphoreGive(haStateMutex);
    }
    return result;
}

bool HaWebsocketLogic_IsConnected() { return isHaConnected && isHaAuthenticated; }

// Hilfsfunktion, um die geparsten Daten eines Objekts auszuwerten
void processParsedEntity(JsonObject doc) {
    String entityId = doc["entity_id"] | "";
    String newState = doc["state"] | "";
    
    // Wir ignorieren Entitäten, die nicht auf unserem Dashboard sind (spart massiv RAM)
    bool isRelevant = false;
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (std::find(trackedEntities.begin(), trackedEntities.end(), entityId) != trackedEntities.end()) {
            isRelevant = true;
        }
        xSemaphoreGive(haStateMutex);
    }
    if (!isRelevant || entityId.length() == 0) return;

    String unit = doc["attributes"]["unit_of_measurement"] | "";
    String name = doc["attributes"]["friendly_name"] | "";
    String icon = doc["attributes"]["icon"] | "";
    
    updateEntityState(entityId, newState);
    
    JsonObject attrs = doc["attributes"];
    int bri = attrs["brightness"] | -1;
    JsonArray rgb = attrs["rgb_color"];
    JsonArray rgbw = attrs["rgbw_color"];
    String mTitle = attrs["media_title"] | "";
    String mArtist = attrs["media_artist"] | "";
    float mVol = attrs["volume_level"] | -1.0f;
    String mSource = attrs["source"] | "";
    JsonArray sList = attrs["source_list"];

    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        
        if (name.length() > 0) haEntityNames[entityId] = name;
        if (icon.length() > 0) haEntityIcons[entityId] = icon;

        if (unit.length() > 0 && newState != "unavailable" && newState != "unknown") {
            haEntityUnits[entityId] = unit;
        } else {
            haEntityUnits[entityId] = "";
        }
        
        if (bri >= 0) haEntityBrightness[entityId] = bri;

        if (!rgbw.isNull() && rgbw.size() == 4) {
            haEntityIsRGBW[entityId] = true;
            haEntityRGB[entityId] = (rgbw[0].as<uint32_t>() << 16) | (rgbw[1].as<uint32_t>() << 8) | rgbw[2].as<uint32_t>();
            haEntityWhite[entityId] = rgbw[3].as<int>();
        } else if (!rgb.isNull() && rgb.size() == 3) {
            haEntityIsRGBW[entityId] = false;
            haEntityRGB[entityId] = (rgb[0].as<uint32_t>() << 16) | (rgb[1].as<uint32_t>() << 8) | rgb[2].as<uint32_t>();
        }

        if (mTitle.length() > 0) haEntityMediaTitle[entityId] = mTitle;
        if (mArtist.length() > 0) haEntityMediaArtist[entityId] = mArtist;
        if (mVol >= 0) haEntityMediaVolume[entityId] = mVol;

        if (mSource.length() > 0) haEntitySource[entityId] = mSource;
        if (!sList.isNull()) {
            std::vector<String> sources;
            for (JsonVariant v : sList) {
                sources.push_back(v.as<String>());
            }
            haEntitySourceList[entityId] = sources;
        }
        xSemaphoreGive(haStateMutex);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    String payload = message.data();

    // 1. Verarbeitung von Echtzeit-Events ("state_changed")
    if (payload.indexOf("\"type\":\"event\"") != -1 || payload.indexOf("\"type\": \"event\"") != -1) {
        if (payload.indexOf("\"state_changed\"") != -1) {
            JsonDocument filter(&psramAlloc);
            filter["event"]["data"]["entity_id"] = true;
            filter["event"]["data"]["new_state"] = true;
            
            JsonDocument doc(&psramAlloc);
            DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
            if (!err) {
                // Wir mappen die Event-Struktur auf unsere gemeinsame Funktion
                JsonObject entityData = doc["event"]["data"]["new_state"];
                processParsedEntity(entityData);
            }
        }
        return; 
    }

    // 2. Schnelle Header-Analyse für System-Responses
    JsonDocument headerFilter(&psramAlloc);
    headerFilter["type"] = true;
    headerFilter["id"] = true;
    headerFilter["success"] = true;

    JsonDocument headerDoc(&psramAlloc);
    DeserializationError err = deserializeJson(headerDoc, payload, DeserializationOption::Filter(headerFilter), DeserializationOption::NestingLimit(200));
    if (err) return;

    String type = headerDoc["type"] | "";
    uint32_t msgId = headerDoc["id"] | 0;
    bool success = headerDoc["success"] | false;

    // 3. NEU: Verarbeitung der initialen "get_states" Antwort
    if (msgId == getStatesReqId && getStatesReqId > 0 && success) {
        JsonDocument statesFilter(&psramAlloc);
        // Wir iterieren durch das große result Array und filtern uns nur das Wichtige heraus
        statesFilter["result"][0]["entity_id"] = true;
        statesFilter["result"][0]["state"] = true;
        statesFilter["result"][0]["attributes"] = true; 

        JsonDocument doc(&psramAlloc);
        DeserializationError stateErr = deserializeJson(doc, payload, DeserializationOption::Filter(statesFilter), DeserializationOption::NestingLimit(250));
        
        if (!stateErr) {
            JsonArray results = doc["result"];
            for (JsonObject entity : results) {
                processParsedEntity(entity);
            }
        }
        return;
    }

    // --- Der Rest bleibt identisch (MediaBrowser, Lovelace Import, Auth) ---
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

    if (msgId == lastLovelaceReqId && lastLovelaceReqId > 0) {
        if (!success) {
            importErrorMessage = "Import fehlgeschlagen!\nHome Assistant hat die Anfrage verweigert.";
            pendingImportError = true; return;
        }

        if (requestingViewsOnly) {
            if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                availableViewTitles.clear();
                JsonDocument viewFilter(&psramAlloc);
                viewFilter["result"]["views"][0]["title"] = true; viewFilter["result"]["views"][0]["path"] = true; viewFilter["result"]["views"][0]["icon"] = true;
                viewFilter["result"]["result"]["views"][0]["title"] = true; viewFilter["result"]["result"]["views"][0]["path"] = true; viewFilter["result"]["result"]["views"][0]["icon"] = true;
                
                JsonDocument doc(&psramAlloc);
                DeserializationError viewErr = deserializeJson(doc, payload, DeserializationOption::Filter(viewFilter), DeserializationOption::NestingLimit(200));
                if (!viewErr) {
                    JsonArray views = doc["result"]["views"];
                    if (views.isNull()) views = doc["result"]["result"]["views"];
                    if (!views.isNull() && views.size() > 0) {
                        int idx = 0;
                        for (JsonObject v : views) {
                            String title = v["title"].as<String>();
                            if (title.length() == 0) title = v["path"].as<String>();
                            if (title.length() == 0) title = v["icon"].as<String>();
                            if (title.length() == 0) title = "Tab " + String(idx + 1);
                            availableViewTitles.push_back(title); idx++;
                        }
                    }
                }
                if (availableViewTitles.empty()) availableViewTitles.push_back("Standard-Ansicht");
                xSemaphoreGive(haStateMutex);
            }
            pendingViewList = true;
            return;
        } 
        else {
            isImporting = true;
            String targetPayload = payload;
            
            int viewsIdx = payload.indexOf("\"views\":");
            if (viewsIdx != -1) {
                int arrayStart = payload.indexOf('[', viewsIdx);
                if (arrayStart != -1) {
                    int depth = 0, currentView = 0, startPos = -1, endPos = -1;
                    bool inQuotes = false, escape = false;
                    for (int i = arrayStart; i < (int)payload.length(); i++) {
                        char c = payload[i];
                        if (escape) { escape = false; continue; }
                        if (c == '\\') { escape = true; continue; }
                        if (c == '"') { inQuotes = !inQuotes; continue; }
                        if (!inQuotes) {
                            if (c == '[' || c == '{') {
                                if (depth == 1 && c == '{') if (currentView == targetViewIndex) startPos = i;
                                depth++;
                            } else if (c == ']' || c == '}') {
                                depth--;
                                if (depth == 1 && c == '}') { if (currentView == targetViewIndex) { endPos = i + 1; break; } currentView++; }
                                if (depth == 0) break; 
                            }
                        }
                    }
                    if (startPos != -1 && endPos != -1) targetPayload = payload.substring(startPos, endPos);
                }
            }

            int pos = 0, margin = 20, cols = 6, cardW = 160, cardH = 160, i = 0, loopCount = 0;
            std::vector<String> foundEntities; 
            
            while ((pos = targetPayload.indexOf("\"entity\":\"", pos)) != -1) {
                if (++loopCount % 20 == 0) vTaskDelay(pdMS_TO_TICKS(5)); 
                
                pos += 10; 
                int endPos = targetPayload.indexOf("\"", pos);
                if (endPos != -1) {
                    String entity = targetPayload.substring(pos, endPos);
                    if (entity.indexOf('.') != -1 && std::find(foundEntities.begin(), foundEntities.end(), entity) == foundEntities.end()) {
                        foundEntities.push_back(entity);
                        
                        String widgetType = "light";
                        if (entity.startsWith("sensor.") || entity.startsWith("binary_sensor.") || entity.startsWith("weather.") || entity.startsWith("sun.")) widgetType = "sensor";
                        else if (entity.startsWith("button.") || entity.startsWith("input_button.") || entity.startsWith("script.") || entity.startsWith("automation.") || entity.startsWith("scene.")) widgetType = "action";
                        else if (entity.startsWith("media_player.")) widgetType = "media_player";

                        String customIcon = "", customName = "";
                        int objStart = targetPayload.lastIndexOf('{', pos);
                        int objEnd = targetPayload.indexOf('}', pos);
                        if (objStart != -1 && objEnd != -1 && objStart < pos && objEnd > pos) {
                            String objStr = targetPayload.substring(objStart, objEnd);
                            int iconPos = objStr.indexOf("\"icon\":\"");
                            if (iconPos != -1) { int iconEnd = objStr.indexOf("\"", iconPos + 8); if (iconEnd != -1) customIcon = objStr.substring(iconPos + 8, iconEnd); }
                            int namePos = objStr.indexOf("\"name\":\"");
                            if (namePos != -1) { int nameEnd = objStr.indexOf("\"", namePos + 8); if (nameEnd != -1) customName = objStr.substring(namePos + 8, nameEnd); }
                        }

                        if (customIcon.length() == 0) customIcon = HaWebsocketLogic_GetCachedIcon(entity);
                        if (customName.length() == 0) customName = HaWebsocketLogic_GetEntityName(entity);
                        
                        HAWidgetDef wDef;
                        wDef.entity_id = entity; wDef.type = widgetType;
                        wDef.x = margin + (i % cols) * (cardW + margin); wDef.y = margin + (i / cols) * (cardH + margin);
                        wDef.name = customName; wDef.mdi_icon = customIcon; wDef.w = cardW; wDef.h = cardH;
                        
                        wDef.icon_align = LV_ALIGN_TOP_MID;
                        wDef.text_align = LV_ALIGN_BOTTOM_MID;
                        
                        HaConfigLogic::dashboards[currentImportTab].widgets.push_back(wDef);
                        i++;
                    }
                    pos = endPos;
                } else {
                    break; 
                }
            }
            
            if (i == 0) pendingImportError = true;
            else { 
                HaConfigLogic::Save(); 
                HaWebsocketLogic_UpdateTrackedEntities(); 
                ViewHomeAssistant::pendingHaReload = true; 
            }
            
            isImporting = false;
            return;
        }
    }

    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) {
            if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                JsonDocument filter(&psramAlloc);
                filter["result"] = true;
                JsonDocument doc(&psramAlloc);
                deserializeJson(doc, payload, DeserializationOption::Filter(filter));

                availableDashboardUrls.clear(); availableDashboardTitles.clear();
                availableDashboardTitles.push_back("Standard (Overview)"); availableDashboardUrls.push_back("");

                JsonObject panels = doc["result"];
                for (JsonPair kv : panels) {
                    JsonObject panel = kv.value();
                    if (String(panel["component_name"] | "") == "lovelace") {
                        String url = panel["url_path"].as<String>();
                        String title = panel["title"].as<String>();
                        if (title.length() == 0) title = url;
                        if (url != "lovelace" && url != "") { availableDashboardUrls.push_back(url); availableDashboardTitles.push_back(title); }
                    }
                }
                xSemaphoreGive(haStateMutex);
            }
            pendingDashboardList = true; 
        }
        return;
    }

    if (type == "auth_required") {
        JsonDocument authDoc(&psramAlloc);
        authDoc["type"] = "auth"; authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; serializeJson(authDoc, authPayload); 
        safeSend(authPayload);
    }
    else if (type == "auth_ok") {
        isHaAuthenticated = true;
        
        // 1. Wir abonnieren Live-Events
        JsonDocument subDoc(&psramAlloc);
        subDoc["id"] = messageIdCounter++; 
        subDoc["type"] = "subscribe_events"; 
        subDoc["event_type"] = "state_changed";
        String subPayload; serializeJson(subDoc, subPayload); 
        safeSend(subPayload);

        // 2. NEU: Wir lösen den initialen Fetch ALLER Stati über den Websocket aus
        triggerRestStateFetch = true; 
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        isHaConnected = true; 
    }
    else if (event == WebsocketsEvent::ConnectionClosed) { 
        isHaConnected = false; 
        isHaAuthenticated = false; 
    }
}


void haWsTask(void *pvParameters) {
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            
            // Haben wir den Befehl erhalten, die aktuellen Stati zu fetchen?
            if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (triggerRestStateFetch && isHaAuthenticated) {
                    triggerRestStateFetch = false;
                    
                    getStatesReqId = messageIdCounter++;
                    JsonDocument reqDoc(&psramAlloc);
                    reqDoc["id"] = getStatesReqId; 
                    reqDoc["type"] = "get_states"; 
                    
                    String reqPayload; 
                    serializeJson(reqDoc, reqPayload); 
                    safeSend(reqPayload); 
                }
                xSemaphoreGive(haStateMutex);
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
    if (haStateMutex == NULL) haStateMutex = xSemaphoreCreateMutex();
    if (haClientMutex == NULL) haClientMutex = xSemaphoreCreateRecursiveMutex();
    
    haClient.onMessage(onMessageCallback);
    haClient.onEvent(onEventsCallback);
    
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 1, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    
    if (haStateMutex != NULL && xSemaphoreTake(haStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        std::map<String, String>().swap(haEntityStates);
        std::map<String, String>().swap(haEntityIcons);
        std::map<String, String>().swap(haEntityNames);
        
        std::map<String, int>().swap(haEntityBrightness);
        std::map<String, uint32_t>().swap(haEntityRGB);
        std::map<String, int>().swap(haEntityWhite);
        std::map<String, bool>().swap(haEntityIsRGBW);
        
        std::map<String, String>().swap(haEntityUnits);
        std::map<String, String>().swap(haEntityMediaTitle);
        std::map<String, String>().swap(haEntityMediaArtist);
        std::map<String, float>().swap(haEntityMediaVolume);
        
        std::map<String, std::vector<String>>().swap(haEntitySourceList);
        std::map<String, String>().swap(haEntitySource);
        
        std::vector<String>().swap(trackedEntities);
        std::vector<String>().swap(availableDashboardUrls);
        std::vector<String>().swap(availableDashboardTitles);
        std::vector<String>().swap(availableViewTitles);
        
        xSemaphoreGive(haStateMutex);
    }
}

void HaWebsocketLogic_CallService(String domain, String service, String entity_id) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; doc["type"] = "call_service"; doc["domain"] = domain; doc["service"] = service;
    JsonObject target = doc["target"].to<JsonObject>(); target["entity_id"] = entity_id;
    String payload; serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_CallLightService(String entity_id, int brightness, int r, int g, int b, int w) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; 
    doc["type"] = "call_service"; 
    doc["domain"] = "light"; 
    doc["service"] = "turn_on";
    
    JsonObject target = doc["target"].to<JsonObject>(); 
    target["entity_id"] = entity_id;
    
    JsonObject service_data = doc["service_data"].to<JsonObject>();
    if (brightness >= 0) {
        service_data["brightness"] = brightness;
    }
    
    if (r >= 0 && g >= 0 && b >= 0) {
        if (w >= 0) {
            JsonArray rgbw = service_data["rgbw_color"].to<JsonArray>();
            rgbw.add(r); rgbw.add(g); rgbw.add(b); rgbw.add(w);
        } else {
            JsonArray rgb = service_data["rgb_color"].to<JsonArray>();
            rgb.add(r); rgb.add(g); rgb.add(b);
        }
    }
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_CallMediaService(String entity_id, String service) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; 
    doc["type"] = "call_service"; 
    doc["domain"] = "media_player"; 
    doc["service"] = service;
    
    JsonObject target = doc["target"].to<JsonObject>(); 
    target["entity_id"] = entity_id;
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_CallMediaVolumeService(String entity_id, float volume) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; 
    doc["type"] = "call_service"; 
    doc["domain"] = "media_player"; 
    doc["service"] = "volume_set";
    
    JsonObject target = doc["target"].to<JsonObject>(); 
    target["entity_id"] = entity_id;
    
    JsonObject service_data = doc["service_data"].to<JsonObject>();
    service_data["volume_level"] = volume;
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_CallMediaSelectSource(String entity_id, String source) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; 
    doc["type"] = "call_service"; 
    doc["domain"] = "media_player"; 
    doc["service"] = "select_source";
    
    JsonObject target = doc["target"].to<JsonObject>(); 
    target["entity_id"] = entity_id;
    
    JsonObject service_data = doc["service_data"].to<JsonObject>();
    service_data["source"] = source;
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_RequestBrowseMedia(String entity_id, String media_content_type, String media_content_id) {
    if (!isHaAuthenticated) return;
    lastMediaBrowseReqId = messageIdCounter++; 
    
    JsonDocument doc(&psramAlloc);
    doc["id"] = lastMediaBrowseReqId; 
    doc["type"] = "media_player/browse_media"; 
    doc["entity_id"] = entity_id;
    
    if (media_content_type.length() > 0) doc["media_content_type"] = media_content_type;
    if (media_content_id.length() > 0) doc["media_content_id"] = media_content_id;
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_CallPlayMedia(String entity_id, String media_content_type, String media_content_id) {
    if (!isHaAuthenticated) return;
    JsonDocument doc(&psramAlloc);
    doc["id"] = messageIdCounter++; 
    doc["type"] = "call_service"; 
    doc["domain"] = "media_player"; 
    doc["service"] = "play_media";
    
    JsonObject target = doc["target"].to<JsonObject>(); 
    target["entity_id"] = entity_id;
    
    JsonObject service_data = doc["service_data"].to<JsonObject>();
    service_data["media_content_type"] = media_content_type;
    service_data["media_content_id"] = media_content_id;
    
    String payload; 
    serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_RequestDashboardList(int target_tab_index) {
    if (!isHaAuthenticated) {
        importErrorMessage = "Keine aktive Verbindung zu Home Assistant!\nBitte warte einen Moment.";
        pendingImportError = true; return;
    }
    currentImportTab = target_tab_index;
    lastLovelaceDashboardsReqId = messageIdCounter++; 
    JsonDocument doc(&psramAlloc);
    doc["id"] = lastLovelaceDashboardsReqId; doc["type"] = "get_panels"; 
    String payload; serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_RequestDashboardViews(String url_path) {
    if (!isHaAuthenticated) return;
    requestingViewsOnly = true; currentImportUrl = url_path;
    lastLovelaceReqId = messageIdCounter++; 
    JsonDocument doc(&psramAlloc);
    doc["id"] = lastLovelaceReqId; doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    String payload; serializeJson(doc, payload); 
    safeSend(payload);
}

void HaWebsocketLogic_RequestDashboardCards(String url_path, int view_index) {
    if (!isHaAuthenticated) return;
    requestingViewsOnly = false; targetViewIndex = view_index;
    lastLovelaceReqId = messageIdCounter++; 
    JsonDocument doc(&psramAlloc);
    doc["id"] = lastLovelaceReqId; doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    String payload; serializeJson(doc, payload); 
    safeSend(payload);
}