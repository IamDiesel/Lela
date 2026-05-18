#include "HaEntityCache.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"

volatile bool HaEntityCache::triggerRestStateFetch = false;
volatile uint32_t HaEntityCache::cacheVersion = 0; 

SemaphoreHandle_t HaEntityCache::mutex = nullptr;
std::vector<String> HaEntityCache::trackedEntities;
std::map<String, String> HaEntityCache::globalEntityMap;

std::map<String, std::vector<String>> HaEntityCache::globalOptionsMap;
std::map<String, float> HaEntityCache::globalMinMap;
std::map<String, float> HaEntityCache::globalMaxMap;
std::map<String, float> HaEntityCache::globalStepMap;

std::map<String, String> HaEntityCache::states;
std::map<String, String> HaEntityCache::icons;
std::map<String, String> HaEntityCache::names;
std::map<String, int> HaEntityCache::brightness;
std::map<String, uint32_t> HaEntityCache::rgb;
std::map<String, int> HaEntityCache::white;
std::map<String, bool> HaEntityCache::isRGBW;
std::map<String, String> HaEntityCache::units;
std::map<String, String> HaEntityCache::mediaTitle;
std::map<String, String> HaEntityCache::mediaArtist;
std::map<String, float> HaEntityCache::mediaVolume;
std::map<String, std::vector<String>> HaEntityCache::sourceList;
std::map<String, String> HaEntityCache::source;
std::map<String, int> HaEntityCache::battery;
std::map<String, String> HaEntityCache::fanSpeed;
std::map<String, int> HaEntityCache::positionMap;

std::map<String, float> HaEntityCache::currentTemperatureMap; 
std::map<String, float> HaEntityCache::targetTemperatureMap;  

std::map<String, bool> HaEntityCache::supportsBrightness;
std::map<String, bool> HaEntityCache::supportsColor;
std::map<String, bool> HaEntityCache::supportsTemp;
std::map<String, int> HaEntityCache::colorTemp;
std::map<String, std::vector<String>> HaEntityCache::optionsMap;
std::map<String, float> HaEntityCache::minMap;
std::map<String, float> HaEntityCache::maxMap;
std::map<String, float> HaEntityCache::stepMap;

void HaEntityCache::Init() {
    mutex = xSemaphoreCreateMutex();
}

void HaEntityCache::ClearAll() {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        globalEntityMap.clear();
        globalOptionsMap.clear();
        globalMinMap.clear();
        globalMaxMap.clear();
        globalStepMap.clear();
        
        states.clear();
        icons.clear();
        names.clear();
        brightness.clear();
        rgb.clear();
        white.clear();
        isRGBW.clear();
        units.clear();
        mediaTitle.clear();
        mediaArtist.clear();
        mediaVolume.clear();
        sourceList.clear();
        source.clear();
        battery.clear();
        fanSpeed.clear();
        positionMap.clear();
        
        currentTemperatureMap.clear(); 
        targetTemperatureMap.clear();  
        
        supportsBrightness.clear();
        supportsColor.clear();
        supportsTemp.clear();
        colorTemp.clear();
        optionsMap.clear();
        minMap.clear();
        maxMap.clear();
        stepMap.clear();
        
        xSemaphoreGive(mutex);
    }
}

// --- NEU: Der rekursive Helfer fuer die VIP-Liste ---
static void recursivelyAddTrackedEntities(const HAWidgetDef& w, std::vector<String>& tracked) {
    // 1. Die Haupt-Entitaet des Widgets
    if (w.entity_id.length() > 0 && std::find(tracked.begin(), tracked.end(), w.entity_id) == tracked.end()) {
        tracked.push_back(w.entity_id);
    }
    // 2. Alle Entitaeten, die in den Bedingungen (Conditions) genutzt werden!
    for (const auto& c : w.conditions) {
        if (c.entity.length() > 0 && std::find(tracked.begin(), tracked.end(), c.entity) == tracked.end()) {
            tracked.push_back(c.entity);
        }
    }
    // 3. Wenn es ein Ordner ist: Rekursiv in die Kinder abtauchen!
    for (const auto& child : w.children) {
        recursivelyAddTrackedEntities(child, tracked);
    }
}

void HaEntityCache::UpdateTrackedEntities() {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        trackedEntities.clear();
        
        // Geht ueber alle Dashboards und nutzt unseren neuen, rekursiven Helfer
        for (const auto& tab : HaConfigLogic::dashboards) {
            for (const auto& w : tab.widgets) {
                recursivelyAddTrackedEntities(w, trackedEntities);
            }
        }
        xSemaphoreGive(mutex);
    }
    triggerRestStateFetch = true;
}
// -----------------------------------------------------

void HaEntityCache::ProcessParsedEntity(JsonObject doc) {
    String entity_id = doc["entity_id"].as<String>();

    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String f_name = "";
        JsonObject attr = doc["attributes"];
        if (!attr.isNull() && !attr["friendly_name"].isNull()) {
            f_name = attr["friendly_name"].as<String>();
        }
        
        if (entity_id.startsWith("light.") || entity_id.startsWith("switch.") || 
            entity_id.startsWith("select.") || entity_id.startsWith("input_select.") || 
            entity_id.startsWith("number.") || entity_id.startsWith("input_number.") ||
            entity_id.startsWith("text.") || entity_id.startsWith("input_text.") || 
            entity_id.startsWith("cover.") || entity_id.startsWith("climate.") || 
            entity_id.startsWith("vacuum.") || entity_id.startsWith("media_player.") ||
            entity_id.startsWith("button.") || entity_id.startsWith("input_button.")) {
            
            if (globalEntityMap.size() < 300) {
                globalEntityMap[entity_id] = f_name;
            }
        }

        if (!attr.isNull()) {
            if (entity_id.startsWith("select.") || entity_id.startsWith("input_select.")) {
                if (attr["options"].is<JsonArray>()) {
                    globalOptionsMap[entity_id].clear();
                    for (JsonVariant v : attr["options"].as<JsonArray>()) {
                        globalOptionsMap[entity_id].push_back(v.as<String>());
                    }
                }
            }
            else if (entity_id.startsWith("number.") || entity_id.startsWith("input_number.") || 
                     entity_id.startsWith("text.") || entity_id.startsWith("input_text.")) {
                if (!attr["min"].isNull()) globalMinMap[entity_id] = attr["min"].as<float>();
                if (!attr["max"].isNull()) globalMaxMap[entity_id] = attr["max"].as<float>();
                if (!attr["step"].isNull()) globalStepMap[entity_id] = attr["step"].as<float>();
            }
            else if (entity_id.startsWith("climate.")) {
                if (attr["hvac_modes"].is<JsonArray>()) {
                    globalOptionsMap[entity_id].clear();
                    for (JsonVariant v : attr["hvac_modes"].as<JsonArray>()) {
                        globalOptionsMap[entity_id].push_back(v.as<String>());
                    }
                }
                if (!attr["min_temp"].isNull()) globalMinMap[entity_id] = attr["min_temp"].as<float>();
                if (!attr["max_temp"].isNull()) globalMaxMap[entity_id] = attr["max_temp"].as<float>();
                if (!attr["target_temp_step"].isNull()) globalStepMap[entity_id] = attr["target_temp_step"].as<float>();
            }
        }

        if (std::find(trackedEntities.begin(), trackedEntities.end(), entity_id) != trackedEntities.end()) {
            states[entity_id] = doc["state"].as<String>();
            
            if (!attr.isNull()) {
                icons[entity_id] = attr["icon"] | "";
                names[entity_id] = f_name;
                units[entity_id] = attr["unit_of_measurement"] | "";
                battery[entity_id] = attr["battery_level"] | -1;
                fanSpeed[entity_id] = attr["fan_speed"] | "";
                
                if (attr["options"].is<JsonArray>()) {
                    optionsMap[entity_id].clear();
                    for (JsonVariant v : attr["options"].as<JsonArray>()) {
                        optionsMap[entity_id].push_back(v.as<String>());
                    }
                }
                if (!attr["min"].isNull()) minMap[entity_id] = attr["min"].as<float>();
                if (!attr["max"].isNull()) maxMap[entity_id] = attr["max"].as<float>();
                if (!attr["step"].isNull()) stepMap[entity_id] = attr["step"].as<float>();
                
                if (entity_id.startsWith("light.")) {
                    brightness[entity_id] = attr["brightness"] | -1;
                    colorTemp[entity_id] = attr["color_temp"] | -1;
                    bool s_bri = false; bool s_col = false; bool s_tmp = false; isRGBW[entity_id] = false;

                    JsonArray c_m_s = attr["supported_color_modes"];
                    if (!c_m_s.isNull()) {
                        for (JsonVariant v : c_m_s) {
                            String mode = v.as<String>();
                            if (mode == "brightness") s_bri = true;
                            else if (mode == "color_temp") { s_bri = true; s_tmp = true; }
                            else if (mode == "hs" || mode == "rgb" || mode == "xy") { s_bri = true; s_col = true; }
                            else if (mode == "rgbw" || mode == "rgbww") { s_bri = true; s_col = true; isRGBW[entity_id] = true; }
                        }
                    }
                    supportsBrightness[entity_id] = s_bri; supportsColor[entity_id] = s_col; supportsTemp[entity_id] = s_tmp;
                    
                    JsonArray rgb_color = attr["rgb_color"];
                    if (!rgb_color.isNull() && rgb_color.size() >= 3) {
                        uint8_t r = rgb_color[0]; uint8_t g = rgb_color[1]; uint8_t b = rgb_color[2];
                        rgb[entity_id] = (uint32_t)(r << 16) | (g << 8) | b;
                    } else {
                        rgb[entity_id] = 0xFFFFFFFF;
                    }
                    JsonArray rgbw_color = attr["rgbw_color"];
                    if (!rgbw_color.isNull() && rgbw_color.size() >= 4) white[entity_id] = rgbw_color[3];
                    else white[entity_id] = -1;
                } 
                else if (entity_id.startsWith("media_player.")) {
                    mediaTitle[entity_id] = attr["media_title"] | "";
                    mediaArtist[entity_id] = attr["media_artist"] | "";
                    mediaVolume[entity_id] = attr["volume_level"] | -1.0f;
                    source[entity_id] = attr["source"] | "";
                    JsonArray s_list = attr["source_list"]; sourceList[entity_id].clear();
                    if (!s_list.isNull()) {
                        for (JsonVariant v : s_list) sourceList[entity_id].push_back(v.as<String>());
                    }
                }
                else if (entity_id.startsWith("cover.")) {
                    if (!attr["current_position"].isNull()) positionMap[entity_id] = attr["current_position"].as<int>();
                    else positionMap[entity_id] = -1;
                }
                else if (entity_id.startsWith("climate.")) {
                    if (!attr["current_temperature"].isNull()) currentTemperatureMap[entity_id] = attr["current_temperature"].as<float>();
                    else currentTemperatureMap[entity_id] = -99.0f; 

                    if (!attr["temperature"].isNull()) targetTemperatureMap[entity_id] = attr["temperature"].as<float>();
                    else targetTemperatureMap[entity_id] = -99.0f;

                    if (attr["hvac_modes"].is<JsonArray>()) {
                        optionsMap[entity_id].clear();
                        for (JsonVariant v : attr["hvac_modes"].as<JsonArray>()) {
                            optionsMap[entity_id].push_back(v.as<String>());
                        }
                    }
                    if (!attr["min_temp"].isNull()) minMap[entity_id] = attr["min_temp"].as<float>();
                    if (!attr["max_temp"].isNull()) maxMap[entity_id] = attr["max_temp"].as<float>();
                    if (!attr["target_temp_step"].isNull()) stepMap[entity_id] = attr["target_temp_step"].as<float>();
                }
            }
            cacheVersion++;
        }
        xSemaphoreGive(mutex);
    }
}

std::vector<String> HaEntityCache::SearchEntities(String query, size_t max_results) {
    std::vector<String> results;
    if (query.length() < 2) return results;
    query.toLowerCase();
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        for (auto const& pair : globalEntityMap) {
            String lowerId = pair.first; lowerId.toLowerCase();
            String lowerName = pair.second; lowerName.toLowerCase();
            if (lowerId.indexOf(query) != -1 || lowerName.indexOf(query) != -1) {
                results.push_back(pair.first);
                if (results.size() >= max_results) break;
            }
        }
        xSemaphoreGive(mutex);
    }
    return results;
}

uint32_t HaEntityCache::GetCacheVersion() { return cacheVersion; }
String HaEntityCache::GetState(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = states[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
String HaEntityCache::GetCachedIcon(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = icons[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
int HaEntityCache::GetBrightness(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { int res = -1; if (brightness.count(entity_id)) res = brightness[entity_id]; xSemaphoreGive(mutex); return res; } return -1; }
uint32_t HaEntityCache::GetRGB(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { uint32_t res = 0xFFFFFFFF; if (rgb.count(entity_id)) res = rgb[entity_id]; xSemaphoreGive(mutex); return res; } return 0xFFFFFFFF; }
int HaEntityCache::GetWhite(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { int res = -1; if (white.count(entity_id)) res = white[entity_id]; xSemaphoreGive(mutex); return res; } return -1; }
bool HaEntityCache::IsRGBW(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { bool res = false; if (isRGBW.count(entity_id)) res = isRGBW[entity_id]; xSemaphoreGive(mutex); return res; } return false; }
String HaEntityCache::GetUnit(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = units[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
String HaEntityCache::GetMediaTitle(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = mediaTitle[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
String HaEntityCache::GetMediaArtist(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = mediaArtist[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
float HaEntityCache::GetMediaVolume(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = -1.0f; if (mediaVolume.count(entity_id)) res = mediaVolume[entity_id]; xSemaphoreGive(mutex); return res; } return -1.0f; }
std::vector<String> HaEntityCache::GetMediaSourceList(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { std::vector<String> res = sourceList[entity_id]; xSemaphoreGive(mutex); return res; } return std::vector<String>(); }
String HaEntityCache::GetMediaSource(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = source[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
bool HaEntityCache::EntityExists(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { bool res = globalEntityMap.find(entity_id) != globalEntityMap.end(); xSemaphoreGive(mutex); return res; } return false; }
String HaEntityCache::GetEntityName(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = names[entity_id]; if (res == "" && globalEntityMap.count(entity_id)) res = globalEntityMap[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
int HaEntityCache::GetBattery(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { int res = -1; if (battery.count(entity_id)) res = battery[entity_id]; xSemaphoreGive(mutex); return res; } return -1; }
String HaEntityCache::GetFanSpeed(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { String res = fanSpeed[entity_id]; xSemaphoreGive(mutex); return res; } return ""; }
int HaEntityCache::GetPosition(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { int res = -1; if (positionMap.count(entity_id)) res = positionMap[entity_id]; xSemaphoreGive(mutex); return res; } return -1; }
bool HaEntityCache::SupportsBrightness(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { bool res = false; if (supportsBrightness.count(entity_id)) res = supportsBrightness[entity_id]; xSemaphoreGive(mutex); return res; } return false; }
bool HaEntityCache::SupportsColor(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { bool res = false; if (supportsColor.count(entity_id)) res = supportsColor[entity_id]; xSemaphoreGive(mutex); return res; } return false; }
bool HaEntityCache::SupportsColorTemp(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { bool res = false; if (supportsTemp.count(entity_id)) res = supportsTemp[entity_id]; xSemaphoreGive(mutex); return res; } return false; }
int HaEntityCache::GetColorTemp(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { int res = -1; if (colorTemp.count(entity_id)) res = colorTemp[entity_id]; xSemaphoreGive(mutex); return res; } return -1; }
std::vector<String> HaEntityCache::GetOptions(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { std::vector<String> res = optionsMap[entity_id]; xSemaphoreGive(mutex); return res; } return std::vector<String>(); }
float HaEntityCache::GetMin(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 0.0f; if (minMap.count(entity_id)) res = minMap[entity_id]; xSemaphoreGive(mutex); return res; } return 0.0f; }
float HaEntityCache::GetMax(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 100.0f; if (maxMap.count(entity_id)) res = maxMap[entity_id]; xSemaphoreGive(mutex); return res; } return 100.0f; }
float HaEntityCache::GetStep(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 1.0f; if (stepMap.count(entity_id)) res = stepMap[entity_id]; xSemaphoreGive(mutex); return res; } return 1.0f; }
float HaEntityCache::GetCurrentTemperature(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = -99.0f; if (currentTemperatureMap.count(entity_id)) res = currentTemperatureMap[entity_id]; xSemaphoreGive(mutex); return res; } return -99.0f; }
float HaEntityCache::GetTargetTemperature(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = -99.0f; if (targetTemperatureMap.count(entity_id)) res = targetTemperatureMap[entity_id]; xSemaphoreGive(mutex); return res; } return -99.0f; }

std::vector<String> HaEntityCache::GetGlobalOptions(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { std::vector<String> res = globalOptionsMap[entity_id]; xSemaphoreGive(mutex); return res; } return std::vector<String>(); }
float HaEntityCache::GetGlobalMin(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 0.0f; if (globalMinMap.count(entity_id)) res = globalMinMap[entity_id]; xSemaphoreGive(mutex); return res; } return 0.0f; }
float HaEntityCache::GetGlobalMax(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 100.0f; if (globalMaxMap.count(entity_id)) res = globalMaxMap[entity_id]; xSemaphoreGive(mutex); return res; } return 100.0f; }
float HaEntityCache::GetGlobalStep(String entity_id) { if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) { float res = 1.0f; if (globalStepMap.count(entity_id)) res = globalStepMap[entity_id]; xSemaphoreGive(mutex); return res; } return 1.0f; }