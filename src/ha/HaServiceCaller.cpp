#include "HaServiceCaller.h"
#include "HaWebsocketLogic.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include "HaPsramAllocator.h"


void HaServiceCaller::CallService(String domain, String service, String entity_id) {
    CallServiceWithData(domain, service, entity_id, "");
}

void HaServiceCaller::CallServiceWithData(String domain, String service, String entity_id, String json_data) {
    if (!HaWebsocketLogic_IsConnected()) return;
    
    JsonDocument doc(HaPsramAlloc());
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

    HaWebsocketLogic_SendRequest(doc);
}

// Rollladen: open_cover, close_cover, stop_cover
void HaServiceCaller::CallCoverService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "cover";
    doc["service"] = service; // "open_cover", "close_cover" oder "stop_cover"
    doc["target"]["entity_id"] = entity_id;
    HaWebsocketLogic_SendRequest(doc);
}

// Rollladen auf Position (0-100 %) fahren
void HaServiceCaller::CallCoverPosition(String entity_id, int position) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "cover";
    doc["service"] = "set_cover_position";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["position"] = position;
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallLightService(String entity_id, int brightness, int r, int g, int b, int w) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
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
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallLightServiceTemp(String entity_id, int brightness, int color_temp) {
    if (!HaWebsocketLogic_IsConnected()) {
        return;
    }
    
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "light";
    doc["service"] = "turn_on";
    doc["target"]["entity_id"] = entity_id;

    JsonObject service_data = doc["service_data"].to<JsonObject>();
    if (brightness >= 0) {
        service_data["brightness"] = brightness;
    }
    if (color_temp > 0) {
        // HA hat den Parameter color_temp (Mired) 2026 entfernt; ein Aufruf
        // damit schlaegt fehl. Die Oberflaeche rechnet intern weiter in Mired,
        // gesendet wird Kelvin (K = 1e6 / Mired).
        service_data["color_temp_kelvin"] = 1000000 / color_temp;
    }

    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallMediaService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = service;
    doc["target"]["entity_id"] = entity_id;
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallMediaVolumeService(String entity_id, float volume) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "volume_set";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["volume_level"] = volume;
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallMediaSelectSource(String entity_id, String source) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "select_source";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["source"] = source;
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::RequestBrowseMedia(String entity_id, String media_content_type, String media_content_id) {
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "media_player/browse_media";
    doc["entity_id"] = entity_id;
    if (media_content_type.length() > 0) doc["media_content_type"] = media_content_type;
    if (media_content_id.length() > 0) doc["media_content_id"] = media_content_id;

    // Nicht gesendet (keine Verbindung): Fehler melden, sonst dreht der
    // Lade-Spinner im Browser endlos.
    if (!HaWebsocketLogic_IsConnected() || HaWebsocketLogic_SendRequest(doc, &lastMediaBrowseReqId) == 0) {
        pendingMediaBrowserError = true;
    }
}

void HaServiceCaller::CallPlayMedia(String entity_id, String media_content_type, String media_content_id) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "media_player";
    doc["service"] = "play_media";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["media_content_type"] = media_content_type;
    doc["service_data"]["media_content_id"] = media_content_id;
    HaWebsocketLogic_SendRequest(doc);
}

// Gemeinsame Fehlermeldung, wenn eine Import-Anfrage nicht rausgeht. Ohne sie
// blieb das Ladefenster stehen (frueher stille Returns).
static void reportImportNotSent() {
    importErrorMessage = "Keine aktive Verbindung zu Home Assistant!\nBitte warte einen Moment.";
    pendingImportError = true;
}

void HaServiceCaller::RequestDashboardList(int target_tab_index) {
    currentImportTab = target_tab_index;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "get_panels";
    if (!HaWebsocketLogic_IsConnected() || HaWebsocketLogic_SendRequest(doc, &lastLovelaceDashboardsReqId) == 0) {
        reportImportNotSent();
    }
}

void HaServiceCaller::RequestDashboardViews(String url_path) {
    requestingViewsOnly = true;
    currentImportUrl = url_path;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    if (!HaWebsocketLogic_IsConnected() || HaWebsocketLogic_SendRequest(doc, &lastLovelaceReqId) == 0) {
        reportImportNotSent();
    }
}

void HaServiceCaller::RequestDashboardCards(String url_path, int view_index) {
    requestingViewsOnly = false;
    targetViewIndex = view_index;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "lovelace/config";
    if (url_path.length() > 0 && url_path != "lovelace") doc["url_path"] = url_path;
    if (!HaWebsocketLogic_IsConnected() || HaWebsocketLogic_SendRequest(doc, &lastLovelaceReqId) == 0) {
        reportImportNotSent();
    }
}

void HaServiceCaller::CallVacuumService(String entity_id, String service) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "vacuum";
    doc["service"] = service;
    doc["target"]["entity_id"] = entity_id;
    HaWebsocketLogic_SendRequest(doc);
}

void HaServiceCaller::CallVacuumSetFanSpeed(String entity_id, String speed) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "vacuum";
    doc["service"] = "set_fan_speed";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["fan_speed"] = speed;
    HaWebsocketLogic_SendRequest(doc);
}
// [...] (Bestehende Methoden bleiben unveraendert)

// Klima: Zieltemperatur setzen
void HaServiceCaller::CallClimateSetTemperature(String entity_id, float temperature) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "climate";
    doc["service"] = "set_temperature";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["temperature"] = temperature;
    HaWebsocketLogic_SendRequest(doc);
}

// Klima: Modus (heat/cool/auto/off ...) setzen
void HaServiceCaller::CallClimateSetHvacMode(String entity_id, String hvac_mode) {
    if (!HaWebsocketLogic_IsConnected()) return;
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    doc["domain"] = "climate";
    doc["service"] = "set_hvac_mode";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["hvac_mode"] = hvac_mode;
    HaWebsocketLogic_SendRequest(doc);
}

// Text setzen (text.* und input_text.*)
void HaServiceCaller::CallTextSetValue(String entity_id, String value) {
    if (!HaWebsocketLogic_IsConnected()) return;
    
    JsonDocument doc(HaPsramAlloc());
    doc["type"] = "call_service";
    
    // Die Domaene entscheidet sich nach dem Praefix
    doc["domain"] = entity_id.startsWith("text.") ? "text" : "input_text";
    doc["service"] = "set_value";
    doc["target"]["entity_id"] = entity_id;
    doc["service_data"]["value"] = value;
    
    HaWebsocketLogic_SendRequest(doc);
}