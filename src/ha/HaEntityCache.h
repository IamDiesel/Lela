#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ==============================================================================
// HaEntityCache - Live-Zustaende der platzierten (getrackten) Entitaeten
// ==============================================================================
// Geschrieben vom HA_WS_Task (ueber HaEntitySync), gelesen von der GUI. Alle
// Zugriffe laufen unter einem Mutex; Getter liefern Kopien.
// Die GUI erkennt Aenderungen an cacheVersion.
//
// Speicher: je Entitaet EIN JsonDocument im PSRAM mit dem Abbild
//     {"s": Zustand, "a": {gefilterte Attribute}}
// Alle Getter werten dieses Abbild beim Aufruf aus.
//
// Frueher lagen die Werte in rund 30 std::map<String, ...> - jede mit eigener
// Kopie der Entity-ID als Schluessel, alles im knappen internen RAM. Seit die
// Attribute wieder ankommen (subscribe_entities), waren das bei 100
// Entitaeten rund 100 KB internes RAM. Das fehlte dem WLAN-Treiber
// (ESP-Hosted braucht fuer jeden Empfangsblock 4,6 KB zusammenhaengendes
// DMA-RAM) - bei groesseren Antworten (Medien-Browser) Absturz in
// sdio_rx_get_buffer.
//
// Nicht platzierte Entitaeten (Suche, Hinzufuegen) liegen im HaEntityCatalog.
// ==============================================================================
class HaEntityCache {
public:
    static volatile uint32_t cacheVersion;

    static void Init();
    static void ClearAll();

    // Baut die Liste der zu abonnierenden Entitaeten aus HaConfigLogic::dashboards
    // neu auf (Widgets, Ordnerkinder, Bedingungs-Entitaeten).
    static void UpdateTrackedEntities();

    // Kopie der getrackten Entitaeten (threadsicher).
    static std::vector<String> GetTrackedEntities();

    // --- Schreiben: nur HaEntitySync (HA_WS_Task) ---
    // Formate wie bei HA subscribe_entities (kompakte Zustaende):
    //   ApplyFull:  {"s": Zustand, "a": {Attribute}}          ersetzt das Abbild
    //   ApplyDiff:  {"+": {"s":..,"a":{..}}, "-": {"a":[..]}}  arbeitet Aenderungen ein
    //   ApplyRemoved: Entitaet existiert in HA nicht mehr
    //   Prune: Abbilder nicht mehr abonnierter Entitaeten verwerfen
    static void ApplyFull(const char* entity_id, JsonObjectConst entity);
    static void ApplyDiff(const char* entity_id, JsonObjectConst change);
    static void ApplyRemoved(const char* entity_id);
    static void Prune(const std::vector<String>& keep);

    // --- Lesen (GUI) ---
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
    static std::vector<String> GetFanSpeedList(String entity_id);   // Sauger: waehlbare Saugstufen
    static int GetPosition(String entity_id);

    static float GetCurrentTemperature(String entity_id);
    static float GetTargetTemperature(String entity_id);

    static bool SupportsBrightness(String entity_id);
    static bool SupportsColor(String entity_id);
    static bool SupportsColorTemp(String entity_id);
    static int GetColorTemp(String entity_id);   // Mired

    // Optionen (select) bzw. HVAC-Modi (climate); Grenzen und Schrittweite
    // (number/text bzw. min_temp/max_temp/target_temp_step bei climate).
    static std::vector<String> GetOptions(String entity_id);
    static float GetMin(String entity_id);
    static float GetMax(String entity_id);
    static float GetStep(String entity_id);

    static uint32_t GetCacheVersion();

private:
    static SemaphoreHandle_t mutex;
    static std::vector<String> trackedEntities;
    static std::map<String, JsonDocument> entities;

    // Abbild einer Entitaet; Null-Objekt, wenn unbekannt. Nur unter mutex.
    static JsonObjectConst findLocked(const String& entity_id);
    static JsonDocument& snapshotLocked(const char* entity_id);
    static void touch();

    // Liest unter dem Mutex aus dem Abbild: fn(Zustandsobjekt, Attribute).
    // Unbekannte Entitaet oder kein Mutex -> fallback.
    template <typename T, typename F>
    static T read(const String& entity_id, T fallback, F fn);
};
