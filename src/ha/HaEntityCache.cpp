// ==============================================================================
// HaEntityCache - Implementierung (siehe Header)
// ==============================================================================

#include "HaEntityCache.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "HaPsramAllocator.h"
#include <algorithm>

volatile uint32_t HaEntityCache::cacheVersion = 0;

SemaphoreHandle_t HaEntityCache::mutex = nullptr;
std::vector<String> HaEntityCache::trackedEntities;
std::map<String, JsonDocument> HaEntityCache::entities;

// ------------------------------------------------------------------------------
// Verwaltung
// ------------------------------------------------------------------------------
void HaEntityCache::Init()
{
    // Nur einmal anlegen: Init() laeuft bei jedem Betreten des HA-Screens.
    if (mutex == nullptr)
        mutex = xSemaphoreCreateMutex();
}

void HaEntityCache::ClearAll()
{
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY))
    {
        entities.clear();
        xSemaphoreGive(mutex);
    }
}

// Sammelt alle Entitaeten eines Widgets: eigene, Bedingungen, Ordnerkinder.
static void recursivelyAddTrackedEntities(const HAWidgetDef &w, std::vector<String> &tracked)
{
    if (w.entity_id.length() > 0 && std::find(tracked.begin(), tracked.end(), w.entity_id) == tracked.end())
    {
        tracked.push_back(w.entity_id);
    }
    for (const auto &c : w.conditions)
    {
        if (c.entity.length() > 0 && std::find(tracked.begin(), tracked.end(), c.entity) == tracked.end())
        {
            tracked.push_back(c.entity);
        }
    }
    for (const auto &child : w.children)
    {
        recursivelyAddTrackedEntities(child, tracked);
    }
}

void HaEntityCache::UpdateTrackedEntities()
{
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY))
    {
        trackedEntities.clear();
        for (const auto &tab : HaConfigLogic::dashboards)
        {
            for (const auto &w : tab.widgets)
            {
                recursivelyAddTrackedEntities(w, trackedEntities);
            }
        }
        xSemaphoreGive(mutex);
    }
    // Das Neu-Abonnieren stoesst der Aufrufer an (HaWebsocketLogic_UpdateTrackedEntities
    // -> HaEntitySync::RequestResync). Der Cache selbst kennt die Verbindung nicht.
}

std::vector<String> HaEntityCache::GetTrackedEntities()
{
    // Kopie unter dem Mutex: der loopTask baut die Liste beim Speichern neu auf,
    // waehrend der HA_WS_Task sie liest.
    std::vector<String> copy;
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY))
    {
        copy = trackedEntities;
        xSemaphoreGive(mutex);
    }
    return copy;
}

uint32_t HaEntityCache::GetCacheVersion() { return cacheVersion; }

void HaEntityCache::touch()
{
    cacheVersion = cacheVersion + 1;   // volatile: kein ++ (C++20-Warnung)
}

// ------------------------------------------------------------------------------
// Schreiben (HA_WS_Task)
// ------------------------------------------------------------------------------
JsonDocument &HaEntityCache::snapshotLocked(const char *entity_id)
{
    auto it = entities.find(entity_id);
    if (it == entities.end())
    {
        it = entities.emplace(String(entity_id), JsonDocument(HaPsramAlloc())).first;
    }
    return it->second;
}

void HaEntityCache::ApplyFull(const char *entity_id, JsonObjectConst entity)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    snap.clear();
    snap["s"] = entity["s"];
    if (entity["a"].is<JsonObjectConst>()) snap["a"] = entity["a"];
    else                                   snap["a"].to<JsonObject>();
    touch();

    xSemaphoreGive(mutex);
}

void HaEntityCache::ApplyDiff(const char *entity_id, JsonObjectConst change)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    JsonObject attrs = snap["a"].is<JsonObject>() ? snap["a"].as<JsonObject>()
                                                  : snap["a"].to<JsonObject>();

    // "+": geaenderte Werte (nur die geaenderten Attribute!)
    JsonObjectConst plus = change["+"];
    if (!plus.isNull())
    {
        if (!plus["s"].isNull()) snap["s"] = plus["s"];
        for (JsonPairConst kv : plus["a"].as<JsonObjectConst>())
        {
            attrs[kv.key()] = kv.value();
        }
    }

    // "-": entfallene Attribute (z. B. brightness, wenn ein Licht ausgeht)
    for (JsonVariantConst key : change["-"]["a"].as<JsonArrayConst>())
    {
        attrs.remove(key.as<const char *>());
    }
    touch();

    xSemaphoreGive(mutex);
}

void HaEntityCache::ApplyRemoved(const char *entity_id)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    JsonDocument &snap = snapshotLocked(entity_id);
    snap.clear();
    snap["s"] = "unavailable";
    snap["a"].to<JsonObject>();
    touch();

    xSemaphoreGive(mutex);
}

void HaEntityCache::Prune(const std::vector<String> &keep)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;

    for (auto it = entities.begin(); it != entities.end();)
    {
        if (std::find(keep.begin(), keep.end(), it->first) == keep.end()) it = entities.erase(it);
        else ++it;
    }

    xSemaphoreGive(mutex);
}

// ------------------------------------------------------------------------------
// Lesen (GUI)
// ------------------------------------------------------------------------------
JsonObjectConst HaEntityCache::findLocked(const String &entity_id)
{
    auto it = entities.find(entity_id);
    return (it == entities.end()) ? JsonObjectConst() : it->second.as<JsonObjectConst>();
}

template <typename T, typename F>
T HaEntityCache::read(const String &entity_id, T fallback, F fn)
{
    if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return fallback;
    JsonObjectConst e = findLocked(entity_id);
    T res = e.isNull() ? fallback : fn(e, e["a"].as<JsonObjectConst>());
    xSemaphoreGive(mutex);
    return res;
}

// Liest ein JSON-Array von Zeichenketten.
static std::vector<String> toStringList(JsonVariantConst v)
{
    std::vector<String> out;
    for (JsonVariantConst item : v.as<JsonArrayConst>()) out.push_back(item.as<String>());
    return out;
}

// Prueft, ob supported_color_modes einen der Modi enthaelt.
static bool hasColorMode(JsonObjectConst a, std::initializer_list<const char *> modes)
{
    for (JsonVariantConst v : a["supported_color_modes"].as<JsonArrayConst>())
    {
        const char *m = v.as<const char *>();
        if (!m) continue;
        for (const char *wanted : modes)
        {
            if (strcmp(m, wanted) == 0) return true;
        }
    }
    return false;
}

static bool isClimate(const String &id) { return id.startsWith("climate."); }

// Ganzzahl aus einem Zahlwert, egal ob HA ihn als 80 oder 80.0 liefert.
static int intOr(JsonVariantConst v, int fallback)
{
    return v.is<float>() ? (int)lroundf(v.as<float>()) : fallback;
}

String HaEntityCache::GetState(String id)
{
    return read<String>(id, "", [](JsonObjectConst e, JsonObjectConst) { return String(e["s"] | ""); });
}

String HaEntityCache::GetCachedIcon(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["icon"] | ""); });
}

String HaEntityCache::GetEntityName(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["friendly_name"] | ""); });
}

String HaEntityCache::GetUnit(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["unit_of_measurement"] | ""); });
}

bool HaEntityCache::EntityExists(String id)
{
    // true, sobald HA fuer diese (abonnierte) Entitaet einen Zustand geliefert hat.
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst) { return true; });
}

// --- Licht ---
int HaEntityCache::GetBrightness(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["brightness"], -1); });
}

uint32_t HaEntityCache::GetRGB(String id)
{
    return read<uint32_t>(id, 0xFFFFFFFF, [](JsonObjectConst, JsonObjectConst a) -> uint32_t {
        JsonArrayConst c = a["rgb_color"];
        if (c.isNull() || c.size() < 3) return 0xFFFFFFFF;
        return ((uint32_t)(uint8_t)(c[0] | 0) << 16) | ((uint32_t)(uint8_t)(c[1] | 0) << 8) | (uint8_t)(c[2] | 0);
    });
}

int HaEntityCache::GetWhite(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) -> int {
        JsonArrayConst c = a["rgbw_color"];
        return (c.isNull() || c.size() < 4) ? -1 : (int)(c[3] | -1);
    });
}

bool HaEntityCache::IsRGBW(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) { return hasColorMode(a, {"rgbw", "rgbww"}); });
}

bool HaEntityCache::SupportsBrightness(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) {
        return hasColorMode(a, {"brightness", "color_temp", "hs", "rgb", "xy", "rgbw", "rgbww"});
    });
}

bool HaEntityCache::SupportsColor(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) {
        return hasColorMode(a, {"hs", "rgb", "xy", "rgbw", "rgbww"});
    });
}

bool HaEntityCache::SupportsColorTemp(String id)
{
    return read<bool>(id, false, [](JsonObjectConst, JsonObjectConst a) { return hasColorMode(a, {"color_temp"}); });
}

int HaEntityCache::GetColorTemp(String id)
{
    // Farbtemperatur in Mired. Neuere HA-Versionen liefern nur noch
    // color_temp_kelvin - dann wird umgerechnet (Mired = 1e6 / K).
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) -> int {
        const int mired = a["color_temp"] | -1;
        if (mired > 0) return mired;
        const int kelvin = a["color_temp_kelvin"] | 0;
        return (kelvin > 0) ? (1000000 / kelvin) : -1;
    });
}

// --- Medien ---
String HaEntityCache::GetMediaTitle(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["media_title"] | ""); });
}

String HaEntityCache::GetMediaArtist(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["media_artist"] | ""); });
}

float HaEntityCache::GetMediaVolume(String id)
{
    return read<float>(id, -1.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["volume_level"] | -1.0f); });
}

std::vector<String> HaEntityCache::GetMediaSourceList(String id)
{
    return read<std::vector<String>>(id, {}, [](JsonObjectConst, JsonObjectConst a) { return toStringList(a["source_list"]); });
}

String HaEntityCache::GetMediaSource(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["source"] | ""); });
}

// --- Sauger / Rollladen ---
int HaEntityCache::GetBattery(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["battery_level"], -1); });
}

String HaEntityCache::GetFanSpeed(String id)
{
    return read<String>(id, "", [](JsonObjectConst, JsonObjectConst a) { return String(a["fan_speed"] | ""); });
}

std::vector<String> HaEntityCache::GetFanSpeedList(String id)
{
    return read<std::vector<String>>(id, {}, [](JsonObjectConst, JsonObjectConst a) { return toStringList(a["fan_speed_list"]); });
}

int HaEntityCache::GetPosition(String id)
{
    return read<int>(id, -1, [](JsonObjectConst, JsonObjectConst a) { return intOr(a["current_position"], -1); });
}

// --- Klima ---
float HaEntityCache::GetCurrentTemperature(String id)
{
    return read<float>(id, -99.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["current_temperature"] | -99.0f); });
}

float HaEntityCache::GetTargetTemperature(String id)
{
    return read<float>(id, -99.0f, [](JsonObjectConst, JsonObjectConst a) { return (float)(a["temperature"] | -99.0f); });
}

// --- Optionen und Grenzen ---
std::vector<String> HaEntityCache::GetOptions(String id)
{
    const char *key = isClimate(id) ? "hvac_modes" : "options";
    return read<std::vector<String>>(id, {}, [key](JsonObjectConst, JsonObjectConst a) { return toStringList(a[key]); });
}

float HaEntityCache::GetMin(String id)
{
    const char *key = isClimate(id) ? "min_temp" : "min";
    return read<float>(id, 0.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 0.0f); });
}

float HaEntityCache::GetMax(String id)
{
    const char *key = isClimate(id) ? "max_temp" : "max";
    return read<float>(id, 100.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 100.0f); });
}

float HaEntityCache::GetStep(String id)
{
    const char *key = isClimate(id) ? "target_temp_step" : "step";
    return read<float>(id, 1.0f, [key](JsonObjectConst, JsonObjectConst a) { return (float)(a[key] | 1.0f); });
}
