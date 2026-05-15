#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class HaEntityCache {
public:
    static volatile bool triggerRestStateFetch;

    static void Init();
    static void ClearAll();
    static void UpdateTrackedEntities();
    static void ProcessParsedEntity(JsonObject doc);
    
    static String GetState(String entity_id);
    static String GetCachedIcon(String entity_id);
    static int GetBrightness(String entity_id);
    static uint32_t GetRGB(String entity_id);
    static int GetWhite(String entity_id);
    static bool IsRGBW(String entity_id);
    static String GetUnit(String entity_id);
    static String GetMediaTitle(String entity_id);
    static String GetMediaArtist(String entity_id);
    static float GetMediaVolume(String entity_id);
    static std::vector<String> GetMediaSourceList(String entity_id);
    static String GetMediaSource(String entity_id);
    static bool EntityExists(String entity_id);
    static String GetEntityName(String entity_id);
    static int GetBattery(String entity_id);
    static String GetFanSpeed(String entity_id);

    // --- NEU: Licht Fähigkeiten und Farbtemperatur ---
    static bool SupportsBrightness(String entity_id);
    static bool SupportsColor(String entity_id);
    static bool SupportsColorTemp(String entity_id);
    static int GetColorTemp(String entity_id);

private:
    static SemaphoreHandle_t mutex;
    static std::vector<String> trackedEntities;

    static std::map<String, String> states;
    static std::map<String, String> icons;
    static std::map<String, String> names;
    static std::map<String, int> brightness;
    static std::map<String, uint32_t> rgb;
    static std::map<String, int> white;
    static std::map<String, bool> isRGBW;
    static std::map<String, String> units;
    static std::map<String, String> mediaTitle;
    static std::map<String, String> mediaArtist;
    static std::map<String, float> mediaVolume;
    static std::map<String, std::vector<String>> sourceList;
    static std::map<String, String> source;
    static std::map<String, int> battery;
    static std::map<String, String> fanSpeed;

    // --- NEU: Caching der Fähigkeiten ---
    static std::map<String, bool> supportsBrightness;
    static std::map<String, bool> supportsColor;
    static std::map<String, bool> supportsTemp;
    static std::map<String, int> colorTemp;
};