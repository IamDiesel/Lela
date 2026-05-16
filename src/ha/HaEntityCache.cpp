#include "HaEntityCache.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"

volatile bool HaEntityCache::triggerRestStateFetch = false;
volatile uint32_t HaEntityCache::cacheVersion = 0; // NEU: Initialisierung

SemaphoreHandle_t HaEntityCache::mutex = nullptr;
std::vector<String> HaEntityCache::trackedEntities;

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

std::map<String, bool> HaEntityCache::supportsBrightness;
std::map<String, bool> HaEntityCache::supportsColor;
std::map<String, bool> HaEntityCache::supportsTemp;
std::map<String, int> HaEntityCache::colorTemp;

void HaEntityCache::Init() {
    mutex = xSemaphoreCreateMutex();
}

void HaEntityCache::ClearAll() {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
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
        
        supportsBrightness.clear();
        supportsColor.clear();
        supportsTemp.clear();
        colorTemp.clear();
        
        xSemaphoreGive(mutex);
    }
}

void HaEntityCache::UpdateTrackedEntities() {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        trackedEntities.clear();
        for (const auto& tab : HaConfigLogic::dashboards) {
            for (const auto& w : tab.widgets) {
                if (std::find(trackedEntities.begin(), trackedEntities.end(), w.entity_id) == trackedEntities.end()) {
                    trackedEntities.push_back(w.entity_id);
                }
            }
        }
        xSemaphoreGive(mutex);
    }
    triggerRestStateFetch = true;
}

void HaEntityCache::ProcessParsedEntity(JsonObject doc) {
    String entity_id = doc["entity_id"].as<String>();

    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        if (std::find(trackedEntities.begin(), trackedEntities.end(), entity_id) != trackedEntities.end()) {
            states[entity_id] = doc["state"].as<String>();
            JsonObject attr = doc["attributes"];
            
            if (!attr.isNull()) {
                icons[entity_id] = attr["icon"] | "";
                names[entity_id] = attr["friendly_name"] | "";
                units[entity_id] = attr["unit_of_measurement"] | "";
                battery[entity_id] = attr["battery_level"] | -1;
                fanSpeed[entity_id] = attr["fan_speed"] | "";
                
                if (entity_id.startsWith("light.")) {
                    brightness[entity_id] = attr["brightness"] | -1;
                    colorTemp[entity_id] = attr["color_temp"] | -1;

                    bool s_bri = false;
                    bool s_col = false;
                    bool s_tmp = false;
                    isRGBW[entity_id] = false;

                    JsonArray c_m_s = attr["supported_color_modes"];
                    if (!c_m_s.isNull()) {
                        for (JsonVariant v : c_m_s) {
                            String mode = v.as<String>();
                            if (mode == "brightness") {
                                s_bri = true;
                            } else if (mode == "color_temp") {
                                s_bri = true;
                                s_tmp = true;
                            } else if (mode == "hs" || mode == "rgb" || mode == "xy") {
                                s_bri = true;
                                s_col = true;
                            } else if (mode == "rgbw" || mode == "rgbww") {
                                s_bri = true;
                                s_col = true;
                                isRGBW[entity_id] = true;
                            }
                        }
                    }

                    supportsBrightness[entity_id] = s_bri;
                    supportsColor[entity_id] = s_col;
                    supportsTemp[entity_id] = s_tmp;
                    
                    JsonArray rgb_color = attr["rgb_color"];
                    if (!rgb_color.isNull() && rgb_color.size() >= 3) {
                        uint8_t r = rgb_color[0];
                        uint8_t g = rgb_color[1];
                        uint8_t b = rgb_color[2];
                        rgb[entity_id] = (uint32_t)(r << 16) | (g << 8) | b;
                    } else {
                        rgb[entity_id] = 0xFFFFFFFF;
                    }
                    
                    JsonArray rgbw_color = attr["rgbw_color"];
                    if (!rgbw_color.isNull() && rgbw_color.size() >= 4) {
                        white[entity_id] = rgbw_color[3];
                    } else {
                        white[entity_id] = -1;
                    }
                } else if (entity_id.startsWith("media_player.")) {
                    mediaTitle[entity_id] = attr["media_title"] | "";
                    mediaArtist[entity_id] = attr["media_artist"] | "";
                    mediaVolume[entity_id] = attr["volume_level"] | -1.0f;
                    source[entity_id] = attr["source"] | "";
                    
                    JsonArray s_list = attr["source_list"];
                    sourceList[entity_id].clear();
                    
                    if (!s_list.isNull()) {
                        for (JsonVariant v : s_list) {
                            sourceList[entity_id].push_back(v.as<String>());
                        }
                    }
                }
            }
            // NEU: Signalisiere der GUI, dass frische Daten da sind!
            cacheVersion++;
        }
        xSemaphoreGive(mutex);
    }
}

uint32_t HaEntityCache::GetCacheVersion() {
    return cacheVersion;
}

String HaEntityCache::GetState(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = states[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

String HaEntityCache::GetCachedIcon(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = icons[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

int HaEntityCache::GetBrightness(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        int res = -1;
        if (brightness.count(entity_id)) {
            res = brightness[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return -1;
}

uint32_t HaEntityCache::GetRGB(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        uint32_t res = 0xFFFFFFFF;
        if (rgb.count(entity_id)) {
            res = rgb[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return 0xFFFFFFFF;
}

int HaEntityCache::GetWhite(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        int res = -1;
        if (white.count(entity_id)) {
            res = white[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return -1;
}

bool HaEntityCache::IsRGBW(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        bool res = false;
        if (isRGBW.count(entity_id)) {
            res = isRGBW[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return false;
}

String HaEntityCache::GetUnit(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = units[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

String HaEntityCache::GetMediaTitle(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = mediaTitle[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

String HaEntityCache::GetMediaArtist(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = mediaArtist[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

float HaEntityCache::GetMediaVolume(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        float res = -1.0f;
        if (mediaVolume.count(entity_id)) {
            res = mediaVolume[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return -1.0f;
}

std::vector<String> HaEntityCache::GetMediaSourceList(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        std::vector<String> res = sourceList[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return std::vector<String>();
}

String HaEntityCache::GetMediaSource(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = source[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

bool HaEntityCache::EntityExists(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        bool res = states.find(entity_id) != states.end();
        xSemaphoreGive(mutex);
        return res;
    }
    return false;
}

String HaEntityCache::GetEntityName(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = names[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

int HaEntityCache::GetBattery(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        int res = -1;
        if (battery.count(entity_id)) {
            res = battery[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return -1;
}

String HaEntityCache::GetFanSpeed(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        String res = fanSpeed[entity_id];
        xSemaphoreGive(mutex);
        return res;
    }
    return "";
}

bool HaEntityCache::SupportsBrightness(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        bool res = false;
        if (supportsBrightness.count(entity_id)) {
            res = supportsBrightness[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return false;
}

bool HaEntityCache::SupportsColor(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        bool res = false;
        if (supportsColor.count(entity_id)) {
            res = supportsColor[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return false;
}

bool HaEntityCache::SupportsColorTemp(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        bool res = false;
        if (supportsTemp.count(entity_id)) {
            res = supportsTemp[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return false;
}

int HaEntityCache::GetColorTemp(String entity_id) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        int res = -1;
        if (colorTemp.count(entity_id)) {
            res = colorTemp[entity_id];
        }
        xSemaphoreGive(mutex);
        return res;
    }
    return -1;
}