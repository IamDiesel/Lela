#include "HaWebsocketLogic.h"
#include "HaLovelaceParser.h"
#include "secrets.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h> 

using namespace websockets;

static WebsocketsClient haClient;
static bool isHaConnected = false;
static bool isHaAuthenticated = false;
static SemaphoreHandle_t haClientMutex = NULL;

static uint32_t lastPingTime = 0;
static uint32_t messageIdCounter = 1;

// --- Die neue Batching State Machine ---
enum SyncState {
    SYNC_INIT = 0,
    SYNC_FETCHING_CHUNKS = 1,
    SYNC_SUBSCRIBED = 2
};
static SyncState currentSyncState = SYNC_INIT;
static uint32_t templateSubId = 0; 

static int currentVipIndex = 0;
static int currentChunkSize = 0;
static bool waitingForChunk = false;

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
        
        // ANTI-CRASH FIX: Keine String-Addition (+ Operator), um RAM zu schonen!
        //Serial.print("\n[WS OUT] ");
        unsigned int printLen = (payload.length() > 300) ? 300 : payload.length();
        //Serial.write(payload.c_str(), printLen);
        if (payload.length() > 300) Serial.print(" ... (gekuerzt)");
        //Serial.println();
        
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    
    String payload = message.data();

    // 1. Live State Updates und unser Template-Event
    if (payload.indexOf("\"type\":\"event\"") != -1 || payload.indexOf("\"type\": \"event\"") != -1) {
        
        // A) Normales State Changed Event
        if (payload.indexOf("\"state_changed\"") != -1) {
            JsonDocument filter; 
            filter["event"]["data"]["entity_id"] = true; 
            filter["event"]["data"]["new_state"] = true;
            
            JsonDocument doc; 
            if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
                HaEntityCache::ProcessParsedEntity(doc["event"]["data"]["new_state"]);
            }
        }
        
        // B) --- NEU: Das asynchrone Event für unser Template abfangen ---
        if (payload.indexOf("\"result\":") != -1 && payload.indexOf("\"id\":" + String(getStatesReqId)) != -1) {
             prof_req_recv = millis();
             Serial.printf("[PROFILING] 4. 'Template' Antwort da! Latenz: %d ms, Groesse: %d Bytes\n", prof_req_recv - prof_req_sent, payload.length());

             uint32_t parse_start = millis();

             // KEIN PSRAM MALLOC MEHR! Wir parsen direkt aus dem RAM.
             JsonDocument doc;
             DeserializationError err = deserializeJson(doc, payload);

             uint32_t parse_end = millis();
             Serial.printf("[PROFILING] 5. JSON Parse dauerte: %d ms\n", parse_end - parse_start);

if (!err) {
                 uint32_t process_start = millis();
                 
                 JsonArray arr = doc["event"]["result"].as<JsonArray>();
                 
                 // --- DIE RETTUNG FÜR DEN COPROZESSOR ---
                 // Anstatt 99x ProcessParsedEntity aufzurufen und den Mutex zu quälen,
                 // schieben wir das ganze Array in einem Rutsch in den Cache!
                 HaEntityCache::ProcessBulkStates(arr);
                 
                 uint32_t process_end = millis();
                 Serial.printf("[PROFILING] 6. Bulk-Update dauerte: %d ms fuer %d Entities\n", process_end - process_start, arr.size());
             } else {
                 Serial.printf("[PROFILING] JSON ERROR: %s\n", err.c_str());
             }
        }
        
        // WICHTIG: Dieses return beendet den Callback für alle "event" Nachrichten,
        // damit sie nicht in den Header-Check unten laufen.
        return; 
    }

    // 2. Standard Header Check für alle anderen Nachrichten
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

    // 3. ACK für das Template ignorieren
    if (msgId == getStatesReqId && type == "result") {
       if(!success) {
          Serial.println("[PROFILING] ERROR: HA hat das Template abgelehnt!");
       }
       return; 
    }

    // 4. Media Browser Antwort
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

    // 5. Lovelace Import
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
        if (requestingViewsOnly) HaLovelaceParser::parseViewsList(payload);
        else { isImporting = true; HaLovelaceParser::parseCards(payload, targetViewIndex, currentImportTab); }
        return;
    }

    // 6. Dashboard Liste laden
    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) HaLovelaceParser::parseDashboardList(payload);
        return;
    }

    // 7. Authentifizierung
    if (type == "auth_required") {
        JsonDocument authDoc;
        authDoc["type"] = "auth"; 
        authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; 
        serializeJson(authDoc, authPayload); 
        HaWebsocketLogic_SendPayload(authPayload);
    }
    else if (type == "auth_ok") {
        //Serial.println("[DEBUG-AUTH] Authentifizierung erfolgreich!");
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
    static uint32_t chunkRequestTime = 0;
    
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            
            if (HaEntityCache::triggerRestStateFetch && isHaAuthenticated) {
                HaEntityCache::triggerRestStateFetch = false;
                
                getStatesReqId = messageIdCounter++;
                
                // --- NEU: Wir senden das Template, statt 'get_states' ---
                // Füge hier die JSON Liste deiner 99 Entitäten ein (kopiere sie aus dem Python Script)
                String entityListJson = "[\"sensor.powerstream_4932_inverter_output_watts\", \"input_boolean.dobby_seg_kueche\", \"input_button.samsung_tv_info\", \"input_boolean.dobby_seg_schlafzimmer\", \"sensor.pc_daniel_power\", \"light.shelly_snowboard_rgb\", \"sensor.pixel_9_pro_next_alarm\", \"light.schlafzimmerlicht\", \"button.philips_2200_series_power_on_no_clean\", \"input_boolean.philips_alarm_daniel\", \"input_button.tv_ok\", \"switch.shelly_schlaf_arbeitszimmer\", \"input_button.samsung_tv_down\", \"sensor.bett_anne_power\", \"sensor.bett_daniel_power\", \"switch.kuche_kaffee_on\", \"switch.verdampfer_on\", \"input_button.bose_audio_voldown\", \"sensor.smart_plugs_leistungsaufnahme\", \"input_button.bose_audio_poweron\", \"button.philips_2200_series_power_off\", \"switch.pc_daniel_on\", \"light.switchbot_rgbicww_strip_light\", \"input_button.sony_audio_mute\", \"input_button.samsung_tv_left\", \"media_player.55pus655\", \"input_button.tv_up\", \"input_button.samsung_tv_voldown\", \"input_button.samsung_tv_exit\", \"sensor.router_power\", \"input_button.sony_audio_input\", \"input_button.samsung_tv_ok\", \"input_button.tv_back\", \"input_button.tv_home\", \"input_button.bose_audio_bt\", \"switch.tv_schlafzimmer_on\", \"light.sb_lampe_wohnzimmerlampe_rgb\", \"input_button.tv_on_off\", \"input_button.bose_audio_pc\", \"input_button.tv_down\", \"input_button.tv_source\", \"switch.plug_mini_eu_2\", \"light.55pus655_ambilight\", \"sensor.energie_einspeisung\", \"input_boolean.dobby_seg_flur\", \"input_button.tv_left\", \"sensor.temperatur\", \"input_boolean.dobby_seg_kinderzimmer\", \"binary_sensor.philips_2200_series_espresso_led\", \"input_boolean.dobby_seg_wohnzimmer\", \"input_button.amp_source_last\", \"vacuum.valetudo_acclaimedfancyviper\", \"sensor.kuche_kaffee_power\", \"binary_sensor.philips_2200_series_coffee_led\", \"script.dobby_segment_reinigung_starten\", \"sensor.wohnzimmer_lampe_power\", \"binary_sensor.philips_2200_series_play_pause_led\", \"binary_sensor.philips_2200_series_steam_led\", \"input_button.samsung_tv_menue\", \"input_button.samsung_tv_tools\", \"input_button.samsung_tv_power\", \"input_button.sony_audio_power\", \"sensor.feuchtigkeit_2\", \"input_button.samsung_tv_return\", \"switch.bad_on\", \"sensor.temperatur_2\", \"sensor.bad_power\", \"binary_sensor.philips_2200_series_hot_water_led\", \"sensor.ecoflow_energie_smartplugs\", \"sensor.powerstream_4932_battery_input_watts\", \"input_button.samsung_tv_smarthub\", \"input_button.samsung_tv_mute\", \"sensor.powerstream_4932_solar_1_watts\", \"input_button.bose_audio_volup\", \"light.shelly_snowboard_led_dimmer\", \"button.philips_2200_series_power_on\", \"switch.wohnzimmer_lampe_on\", \"input_button.amp_vol_down\", \"input_button.sony_audio_voldown\", \"input_button.amp_on_off\", \"switch.router_on\", \"input_button.tv_right\", \"input_button.samsung_tv_up\", \"light.sb_lampe_dimmerized_light\", \"input_button.sony_audio_volup\", \"sensor.feuchtigkeit\", \"sensor.powerstream_4932_solar_2_watts\", \"sensor.tv_schlafzimmer_power\", \"input_button.samsung_tv_volup\", \"switch.wohnzimmer_on\", \"sensor.wohnzimmer_power\", \"sensor.shelly2pmg3_8cbfea979848_switch_0_power\", \"light.stehlampe_gross_wohnzimmer_on\", \"input_button.amp_source_next\", \"input_button.amp_vol_up\", \"input_button.samsung_tv_right\", \"input_button.samsung_tv_source\", \"input_boolean.philips_alarm_fox\", \"switch.bett_daniel_on\"]";
                
                String templateStr = "{% set result = namespace(items=[]) %}{% for e in " + entityListJson + " %}{% set st = states(e) %}{% set result.items = result.items + [{'id': e, 'state': st}] %}{% endfor %}{{ result.items | tojson }}";

                JsonDocument reqDoc;
                reqDoc["id"] = getStatesReqId;
                reqDoc["type"] = "render_template";
                reqDoc["template"] = templateStr;
                
                String reqPayload; 
                serializeJson(reqDoc, reqPayload);
                
                prof_req_sent = millis();
                Serial.printf("[PROFILING] 3. Sende Template an HA. Zeit seit Auth: %d ms\n", prof_req_sent - prof_auth);
                
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
            // ANTI-STAU FIX: Reduziert auf 2ms! Der WLAN Chip wird deutlich aggressiver geleert.
            vTaskDelay(pdMS_TO_TICKS(2)); 
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

void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        isHaConnected = true; 
        //Serial.println("[DEBUG] Websocket Verbunden.");
    } else if (event == WebsocketsEvent::ConnectionClosed) { 
        isHaConnected = false; 
        isHaAuthenticated = false; 
        //Serial.println("[DEBUG] Websocket Getrennt.");
    }
}



void HaWebsocketLogic_Start() {
    if (haShouldRun && haTaskHandle != NULL) return; 
    
    while (haTaskHandle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    haShouldRun = true; 
    currentSyncState = SYNC_INIT; 
    currentVipIndex = 0;
    waitingForChunk = false;
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