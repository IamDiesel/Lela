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
        Serial.print("\n[WS OUT] ");
        unsigned int printLen = (payload.length() > 300) ? 300 : payload.length();
        Serial.write(payload.c_str(), printLen);
        if (payload.length() > 300) Serial.print(" ... (gekuerzt)");
        Serial.println();
        
        haClient.send(payload);
        xSemaphoreGiveRecursive(haClientMutex);
    }
}

void onMessageCallback(WebsocketsMessage message) {
    if (!message.isText()) return;
    String payload = message.data();

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

    // ==============================================================
    // CHUNK EVENT (Gilt fuer Initial-Load UND Live-Updates!)
    // ==============================================================
    if (type == "event") {
        JsonDocument eventFilter;
        eventFilter["event"]["result"] = true;
        JsonDocument eventDoc;
        
        if (!deserializeJson(eventDoc, payload, DeserializationOption::Filter(eventFilter))) {
            String jsonResultString = eventDoc["event"]["result"].as<String>();
            
            if (jsonResultString != "null" && jsonResultString.length() > 0) {
                JsonDocument dataDoc;
                DeserializationError err = deserializeJson(dataDoc, jsonResultString);
                
                if (!err && dataDoc.is<JsonArray>()) {
                    JsonArray arr = dataDoc.as<JsonArray>();
                    for (JsonObject entity : arr) {
                        String eid = entity["entity_id"] | entity["id"] | "";
                        if (eid.length() > 0) {
                            entity["entity_id"] = eid; 
                            HaEntityCache::ProcessParsedEntity(entity);
                        }
                    }
                    Serial.printf("[DEBUG-EVENT] Chunk-Werte (ID: %d) erfolgreich in Cache uebertragen.\n", msgId);
                }
            }
        }
        
        // Wenn wir in Phase 1 (Start) sind, schalten wir den naechsten Chunk frei.
        // WICHTIG: Wir senden KEIN unsubscribe! Der Chunk bleibt als Live-Listener offen!
        if (currentSyncState == SYNC_FETCHING_CHUNKS && msgId == templateSubId) {
            currentVipIndex += currentChunkSize;
            waitingForChunk = false; 
            Serial.printf("[DEBUG-BOOT] Chunk %d abgeschlossen. Gehe zu Index: %d\n", msgId, currentVipIndex);
        }
        return;
    }

    if (type == "result" && msgId == templateSubId) {
        if (!success) {
            Serial.println("[DEBUG-BOOT] HA hat Chunk abgelehnt! Ueberspringe...");
            currentVipIndex += currentChunkSize;
            waitingForChunk = false;
        } else {
            Serial.println("[DEBUG-BOOT] Chunk Request vom Server akzeptiert.");
        }
        return;
    }

    // ==============================================================
    // RESULT-VERARBEITUNG (Media & Import)
    // ==============================================================
    if (msgId == lastMediaBrowseReqId && lastMediaBrowseReqId > 0) {
        if (!success) { pendingMediaBrowserError = true; return; }
        JsonDocument browseFilter; browseFilter["result"]["children"] = true; 
        JsonDocument doc; deserializeJson(doc, payload, DeserializationOption::Filter(browseFilter));
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
        if (requestingViewsOnly) HaLovelaceParser::parseViewsList(payload);
        else { isImporting = true; HaLovelaceParser::parseCards(payload, targetViewIndex, currentImportTab); }
        return;
    }

    if (msgId == lastLovelaceDashboardsReqId && lastLovelaceDashboardsReqId > 0) {
        if (success) HaLovelaceParser::parseDashboardList(payload);
        return;
    }

    // ==============================================================
    // AUTHENTIFIZIERUNG & ROUTING
    // ==============================================================
    if (type == "auth_required") {
        JsonDocument authDoc;
        authDoc["type"] = "auth"; 
        authDoc["access_token"] = SECRET_HA_TOKEN;
        String authPayload; serializeJson(authDoc, authPayload); 
        HaWebsocketLogic_SendPayload(authPayload);
    }
    else if (type == "auth_ok") {
        Serial.println("[DEBUG-AUTH] Authentifizierung erfolgreich!");
        isHaAuthenticated = true;
        
        // Sowohl beim Start als auch beim Reconnect feuern wir die Chunks neu ab.
        // Das stellt sicher, dass wir auf dem aktuellsten Stand sind und die Abo-Listener stehen!
        Serial.println("[DEBUG-AUTH] Starte VIP-Batching (Gleichzeitig unser Live-Abo!)...");
        HaEntityCache::triggerRestStateFetch = true; 
    }
}
void haWsTask(void *pvParameters) {
    static uint32_t chunkRequestTime = 0;
    
    while (haShouldRun) {
        if (WiFi.status() == WL_CONNECTED) {
            
            if (HaEntityCache::triggerRestStateFetch && isHaAuthenticated) {
                HaEntityCache::triggerRestStateFetch = false;
                currentSyncState = SYNC_FETCHING_CHUNKS;
                currentVipIndex = 0;
                waitingForChunk = false;
                Serial.println("[DEBUG-TASK] Initialisiere Chunk- & Abo-Sequenz.");
            }

            if (currentSyncState == SYNC_FETCHING_CHUNKS) {
                
                if (waitingForChunk && millis() - chunkRequestTime > 3000) {
                    Serial.printf("[DEBUG-TASK] TIMEOUT! Chunk %d lieferte keine Antwort. Ueberspringe...\n", currentVipIndex);
                    currentVipIndex += currentChunkSize;
                    waitingForChunk = false;
                }

                if (!waitingForChunk) {
                    std::vector<String> vips = HaEntityCache::GetTrackedEntities();

                    if (currentVipIndex >= vips.size()) {
                        Serial.println("[DEBUG-TASK] Alle VIPs geladen. System laeuft ab jetzt im lautlosen Sniper-Modus!");
                        currentSyncState = SYNC_SUBSCRIBED;
                        
                        // ABSOLUTER GAMECHANGER: 
                        // Wir senden hier ABSICHTLICH KEIN subscribe_events! 
                        // Die offenen Templates übernehmen ab sofort die Live-Updates. 
                        continue; 
                    }

                    templateSubId = messageIdCounter++;
                    currentChunkSize = 0;
                    
                    String tmpl;
                    tmpl.reserve(2048); 
                    tmpl = "[";
                    
                    for (size_t i = currentVipIndex; i < vips.size() && currentChunkSize < 8; i++, currentChunkSize++) {
                        tmpl += "{\"entity_id\":\"";
                        tmpl += vips[i];
                        tmpl += "\",\"state\":{{ states('";
                        tmpl += vips[i];
                        tmpl += "') | default('null') | to_json }} }";
                        
                        if (currentChunkSize < 7 && i < vips.size() - 1) tmpl += ",";
                    }
                    tmpl += "]";
                    
                    JsonDocument reqDoc;
                    reqDoc["id"] = templateSubId; 
                    reqDoc["type"] = "render_template"; 
                    reqDoc["template"] = tmpl;
                    
                    String reqPayload; serializeJson(reqDoc, reqPayload); 
                    
                    Serial.print("\n[DEBUG-TASK] Sende Chunk Abo: ");
                    unsigned int pLen = (reqPayload.length() > 120) ? 120 : reqPayload.length();
                    Serial.write(reqPayload.c_str(), pLen);
                    Serial.println();
                    
                    HaWebsocketLogic_SendPayload(reqPayload); 
                    
                    waitingForChunk = true; 
                    chunkRequestTime = millis();
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
                            
                            String pingStr; serializeJson(pingDoc, pingStr); 
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
        Serial.println("[DEBUG] Websocket Verbunden.");
    } else if (event == WebsocketsEvent::ConnectionClosed) { 
        isHaConnected = false; 
        isHaAuthenticated = false; 
        Serial.println("[DEBUG] Websocket Getrennt.");
    }
}



void HaWebsocketLogic_Start() {
    if (haShouldRun && haTaskHandle != NULL) {
        return; 
    }
    
    while (haTaskHandle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    haShouldRun = true; 
    currentSyncState = SYNC_INIT; 
    currentVipIndex = 0;
    waitingForChunk = false;
    HaEntityCache::Init();
    
    if (haClientMutex == NULL) {
        haClientMutex = xSemaphoreCreateRecursiveMutex();
    }
    
    haClient.onMessage(onMessageCallback); 
    haClient.onEvent(onEventsCallback);
    
    xTaskCreatePinnedToCore(haWsTask, "HA_WS_Task", 16384, NULL, 5, (TaskHandle_t*)&haTaskHandle, 1); 
}

void HaWebsocketLogic_Stop() {
    haShouldRun = false; 
    HaEntityCache::ClearAll();
}