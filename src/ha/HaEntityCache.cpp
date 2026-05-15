#include "HaEntityCache.h"
#include "HaConfigLogic.h"
#include <algorithm>

volatile bool HaEntityCache::triggerRestStateFetch = false;
SemaphoreHandle_t HaEntityCache::mutex = NULL;

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

void HaEntityCache::Init() {
    if (mutex == NULL) mutex = xSemaphoreCreateMutex();
}

void HaEntityCache::ClearAll() {
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        states.clear(); icons.clear(); names.clear(); brightness.clear();
        rgb.clear(); white.clear(); isRGBW.clear(); units.clear();
        mediaTitle.clear(); mediaArtist.clear(); mediaVolume.clear();
        sourceList.clear(); source.clear(); trackedEntities.clear();
        xSemaphoreGive(mutex);
    }
}

void HaEntityCache::UpdateTrackedEntities() {
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        trackedEntities.clear();
        for (const auto& tab : HaConfigLogic::dashboards) {
            for (const auto& w : tab.widgets) {
                if (std::find(trackedEntities.begin(), trackedEntities.end(), w.entity_id) == trackedEntities.end()) {
                    trackedEntities.push_back(w.entity_id);
                }
            }
        }
        triggerRestStateFetch = true; 
        xSemaphoreGive(mutex);
    }
}

void HaEntityCache::ProcessParsedEntity(JsonObject doc) {
    String entityId = doc["entity_id"] | "";
    String newState = doc["state"] | "";
    
    bool isRelevant = false;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (std::find(trackedEntities.begin(), trackedEntities.end(), entityId) != trackedEntities.end()) {
            isRelevant = true;
        }
        xSemaphoreGive(mutex);
    }
    
    if (!isRelevant || entityId.length() == 0) return;

    String unit = doc["attributes"]["unit_of_measurement"] | "";
    String name = doc["attributes"]["friendly_name"] | "";
    String icon = doc["attributes"]["icon"] | "";
    
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        states[entityId] = newState;
        if (name.length() > 0) names[entityId] = name;
        if (icon.length() > 0) icons[entityId] = icon;
        
        if (unit.length() > 0 && newState != "unavailable" && newState != "unknown") units[entityId] = unit;
        else units[entityId] = "";
        
        JsonObject attrs = doc["attributes"];
        int bri = attrs["brightness"] | -1;
        if (bri >= 0) brightness[entityId] = bri;

        JsonArray rgb_arr = attrs["rgb_color"];
        JsonArray rgbw_arr = attrs["rgbw_color"];

        if (!rgbw_arr.isNull() && rgbw_arr.size() == 4) {
            isRGBW[entityId] = true;
            rgb[entityId] = (rgbw_arr[0].as<uint32_t>() << 16) | (rgbw_arr[1].as<uint32_t>() << 8) | rgbw_arr[2].as<uint32_t>();
            white[entityId] = rgbw_arr[3].as<int>();
        } else if (!rgb_arr.isNull() && rgb_arr.size() == 3) {
            isRGBW[entityId] = false;
            rgb[entityId] = (rgb_arr[0].as<uint32_t>() << 16) | (rgb_arr[1].as<uint32_t>() << 8) | rgb_arr[2].as<uint32_t>();
        }

        String mTitle = attrs["media_title"] | "";
        String mArtist = attrs["media_artist"] | "";
        float mVol = attrs["volume_level"] | -1.0f;
        String mSource = attrs["source"] | "";
        JsonArray sList = attrs["source_list"];

        if (mTitle.length() > 0) mediaTitle[entityId] = mTitle;
        if (mArtist.length() > 0) mediaArtist[entityId] = mArtist;
        if (mVol >= 0) mediaVolume[entityId] = mVol;
        if (mSource.length() > 0) source[entityId] = mSource;
        if (!sList.isNull()) {
            std::vector<String> sources;
            for (JsonVariant v : sList) sources.push_back(v.as<String>());
            sourceList[entityId] = sources;
        }
        xSemaphoreGive(mutex);
    }
}

String HaEntityCache::GetState(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (states.find(entity_id) != states.end()) result = states[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetCachedIcon(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (icons.find(entity_id) != icons.end()) result = icons[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

int HaEntityCache::GetBrightness(String entity_id) {
    int result = -1;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (brightness.find(entity_id) != brightness.end()) result = brightness[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

uint32_t HaEntityCache::GetRGB(String entity_id) {
    uint32_t result = 0xFFFFFFFF;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (rgb.find(entity_id) != rgb.end()) result = rgb[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

int HaEntityCache::GetWhite(String entity_id) {
    int result = -1;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (white.find(entity_id) != white.end()) result = white[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

bool HaEntityCache::IsRGBW(String entity_id) {
    bool result = false;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (isRGBW.find(entity_id) != isRGBW.end()) result = isRGBW[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetUnit(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (units.find(entity_id) != units.end()) result = units[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetMediaTitle(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mediaTitle.find(entity_id) != mediaTitle.end()) result = mediaTitle[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetMediaArtist(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mediaArtist.find(entity_id) != mediaArtist.end()) result = mediaArtist[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

float HaEntityCache::GetMediaVolume(String entity_id) {
    float result = -1.0f;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mediaVolume.find(entity_id) != mediaVolume.end()) result = mediaVolume[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

std::vector<String> HaEntityCache::GetMediaSourceList(String entity_id) {
    std::vector<String> result;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (sourceList.find(entity_id) != sourceList.end()) result = sourceList[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetMediaSource(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (source.find(entity_id) != source.end()) result = source[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}

bool HaEntityCache::EntityExists(String entity_id) {
    bool result = false;
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        result = (states.find(entity_id) != states.end() || names.find(entity_id) != names.end() || icons.find(entity_id) != icons.end());
        xSemaphoreGive(mutex);
    }
    return result;
}

String HaEntityCache::GetEntityName(String entity_id) {
    String result = "";
    if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (names.find(entity_id) != names.end()) result = names[entity_id];
        xSemaphoreGive(mutex);
    }
    return result;
}