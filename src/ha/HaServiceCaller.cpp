#include "HaServiceCaller.h"
#include "HaWebsocketLogic.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

class CallerRamAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    void deallocate(void* pointer) override { heap_caps_free(pointer); }
    void* reallocate(void* pointer, size_t new_size) override { return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM); }
};
static CallerRamAllocator callerAlloc;

void HaServiceCaller::CallService(String domain, String service, String entity_id) {
    CallServiceWithData(domain, service, entity_id, "");
}

void HaServiceCaller::CallServiceWithData(String domain, String service, String entity_id, String json_data) {
    if (!HaWebsocketLogic_IsConnected()) return;
    
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = domain;
    doc["service"] = service;
    
    if (entity_id.length() > 0) {
        doc["target"]["entity_id"] = entity_id;
    }

    if (json_data.length() > 0) {
        JsonDocument dataDoc;
        DeserializationError err = deserializeJson(dataDoc, json_data);
        if (!err) {
            doc["service_data"] = dataDoc.as<JsonObject>();
        }
    }

    String payload; 
    serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

// --- NEU: Senden von einfachen Cover-Befehlen (open, close, stop) ---
void HaServiceCaller::CallCoverService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "cover";
    doc["service"] = service; // "open_cover", "close_cover" oder "stop_cover"
    doc["target"]["entity_id"] = entity_id;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

// --- NEU: Senden der exakten Rollladen-Position in % ---
void HaServiceCaller::CallCoverPosition(String entity_id, int position) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "cover";
    doc["service"] = "set_cover_position";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["position"] = position;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallLightService(String entity_id, int brightness, int r, int g, int b, int w) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "light";
    doc["service"] = "turn_on";
    doc["target"]["entity_id"] = entity_id;

    JsonObject service_data = doc["service_data"].to<JsonObject>();
    if (brightness >= 0) service_data["brightness"] = brightness;
    if (r >= 0 && g >= 0 && b >= 0) {
        if (w >= 0) {
            JsonArray rgbw = service_data["rgbw_color"].to<JsonArray>();
            rgbw.add(r); rgbw.add(g); rgbw.add(b); rgbw.add(w);
        } else {
            JsonArray rgb = service_data["rgb_color"].to<JsonArray>();
            rgb.add(r); rgb.add(g); rgb.add(b);
        }
    }
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallLightServiceTemp(String entity_id, int brightness, int color_temp) {
    if (!HaWebsocketLogic_IsConnected()) {
        return;
    }
    
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "light";
    doc["service"] = "turn_on";
    doc["target"]["entity_id"] = entity_id;

    JsonObject service_data = doc["service_data"].to<JsonObject>();
    if (brightness >= 0) {
        service_data["brightness"] = brightness;
    }
    if (color_temp >= 0) {
        service_data["color_temp"] = color_temp;
    }

    String payload; 
    serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallMediaService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = service;
    doc["target"]["entity_id"] = entity_id;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallMediaVolumeService(String entity_id, float volume) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "volume_set";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["volume_level"] = volume;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallMediaSelectSource(String entity_id, String source) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "select_source";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["source"] = source;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::RequestBrowseMedia(String entity_id, String media_content_type, String media_content_id) {
    if (!HaWebsocketLogic_IsConnected()) return;
    lastMediaBrowseReqId = HaWebsocketLogic_GetNextMessageId();
    JsonDocument doc(&callerAlloc);
    doc["id"] = lastMediaBrowseReqId;
    doc["type"] = "media_player/browse_media";
    doc["entity_id"] = entity_id;
    if (media_content_type.length() > 0) doc["media_content_type"] = media_content_type;
    if (media_content_id.length() > 0) doc["media_content_id"] = media_content_id;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallPlayMedia(String entity_id, String media_content_type, String media_content_id) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "play_media";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["media_content_type"] = media_content_type;
    doc["service_data"]["media_content_id"] = media_content_id;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::RequestDashboardList(int target_tab_index) {
    if (!HaWebsocketLogic_IsConnected()) {
        importErrorMessage = "Keine aktive Verbindung zu Home Assistant!\nBitte warte einen Moment.";
        pendingImportError = true;
        return;
    }
    currentImportTab = target_tab_index;
    lastLovelaceDashboardsReqId = HaWebsocketLogic_GetNextMessageId();
    JsonDocument doc(&callerAlloc);
    doc["id"] = lastLovelaceDashboardsReqId;
    doc["type"] = "get_panels";
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::RequestDashboardViews(String url_path) {
    if (!HaWebsocketLogic_IsConnected()) return;
    requestingViewsOnly = true;
    currentImportUrl = url_path;
    lastLovelaceReqId = HaWebsocketLogic_GetNextMessageId();
    JsonDocument doc(&callerAlloc);
    doc["id"] = lastLovelaceReqId;
    doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::RequestDashboardCards(String url_path, int view_index) {
    if (!HaWebsocketLogic_IsConnected()) return;
    requestingViewsOnly = false;
    targetViewIndex = view_index;
    lastLovelaceReqId = HaWebsocketLogic_GetNextMessageId();
    JsonDocument doc(&callerAlloc);
    doc["id"] = lastLovelaceReqId;
    doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallVacuumService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "vacuum";
    doc["service"] = service;
    doc["target"]["entity_id"] = entity_id;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}

void HaServiceCaller::CallVacuumSetFanSpeed(String entity_id, String speed) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(&callerAlloc);
    doc["id"] = HaWebsocketLogic_GetNextMessageId();
    doc["type"] = "call_service";
    doc["domain"] = "vacuum";
    doc["service"] = "set_fan_speed";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["fan_speed"] = speed;
    String payload; serializeJson(doc, payload);
    HaWebsocketLogic_SendPayload(payload);
}