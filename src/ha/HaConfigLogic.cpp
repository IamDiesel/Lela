#include "HaConfigLogic.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm> 
#include "lvgl.h" 
#include <esp_heap_caps.h>

class ConfigRamAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    void deallocate(void* pointer) override { heap_caps_free(pointer); }
    void* reallocate(void* pointer, size_t new_size) override { return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM); }
};
static ConfigRamAllocator configAlloc;

std::vector<HADashboardDef> HaConfigLogic::dashboards;

void HaConfigLogic::Init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }
    Load(); 
}

// --- NEU: Rekursiver Helfer zum Deserialisieren (Laden) von Widgets und deren Kindern ---
static void deserializeWidget(JsonObject wObj, HAWidgetDef& wDef) {
    if (!wObj["entity"].isNull()) wDef.entity_id = wObj["entity"].as<String>();
    if (!wObj["type"].isNull()) wDef.type = wObj["type"].as<String>();
    
    wDef.x = wObj["x"] | 20;
    wDef.y = wObj["y"] | 20;
    wDef.w = wObj["w"] | 160;
    wDef.h = wObj["h"] | 105;
    
    wDef.name = wObj["name"] | "";
    wDef.mdi_icon = wObj["mdi_icon"] | "";
    wDef.color_on = wObj["color_on"] | "";
    wDef.color_off = wObj["color_off"] | "";
    
    wDef.icon_align = wObj["icon_align"] | LV_ALIGN_TOP_MID;
    wDef.text_align = wObj["text_align"] | LV_ALIGN_BOTTOM_MID;
    wDef.state_align = wObj["state_align"] | LV_ALIGN_CENTER;
    
    wDef.icon_margin = !wObj["icon_margin"].isNull() ? wObj["icon_margin"].as<int>() : 5;
    wDef.text_margin = !wObj["text_margin"].isNull() ? wObj["text_margin"].as<int>() : 5;
    wDef.state_margin = wObj["state_margin"] | 0;
    
    wDef.snap_to_grid = !wObj["snap_to_grid"].isNull() ? wObj["snap_to_grid"].as<bool>() : true;
    
    wDef.show_chart = wObj["show_chart"] | false;
    wDef.chart_w_pct = wObj["chart_w_pct"] | 95;
    wDef.chart_h_pct = wObj["chart_h_pct"] | 50;
    wDef.chart_x_ofs = wObj["chart_x_ofs"] | 0;
    wDef.chart_y_ofs = wObj["chart_y_ofs"] | -15;
    wDef.chart_min = wObj["chart_min"] | "";
    wDef.chart_max = wObj["chart_max"] | "";
    
    wDef.media_content_type = wObj["media_content_type"] | "";
    wDef.media_content_id = wObj["media_content_id"] | "";

    wDef.tap_action_domain = wObj["ta_dom"] | "";
    wDef.tap_action_service = wObj["ta_srv"] | "";
    wDef.tap_action_target = wObj["ta_tgt"] | "";

    wDef.select_options = wObj["select_options"] | "";
    wDef.slider_min = wObj["slider_min"] | 0.0f;
    wDef.slider_max = wObj["slider_max"] | 100.0f;
    wDef.slider_step = wObj["slider_step"] | 1.0f;

    // --- NEU: Laden der Bedingungen ---
    wDef.conditions_type = wObj["conditions_type"] | "AND";
    if (wObj["conditions"].is<JsonArray>()) {
        for (JsonObject j_c : wObj["conditions"].as<JsonArray>()) {
            HACondition c;
            c.entity = j_c["entity"] | "";
            c.op = j_c["op"] | "==";
            c.value = j_c["value"] | "";
            wDef.conditions.push_back(c);
        }
    }

    // --- NEU: Laden der Kinder-Widgets ---
    if (wObj["children"].is<JsonArray>()) {
        for (JsonObject j_child : wObj["children"].as<JsonArray>()) {
            HAWidgetDef childDef;
            deserializeWidget(j_child, childDef);
            wDef.children.push_back(childDef);
        }
    }
}

// --- NEU: Rekursiver Helfer zum Serialisieren (Speichern) von Widgets und deren Kindern ---
static void serializeWidget(JsonObject wObj, const HAWidgetDef& w) {
    wObj["entity"] = w.entity_id;
    wObj["type"] = w.type;
    wObj["x"] = w.x;
    wObj["y"] = w.y;
    wObj["w"] = w.w;
    wObj["h"] = w.h;
    
    wObj["icon_align"] = w.icon_align;
    wObj["text_align"] = w.text_align;
    wObj["state_align"] = w.state_align;
    wObj["icon_margin"] = w.icon_margin;
    wObj["text_margin"] = w.text_margin;
    wObj["state_margin"] = w.state_margin;
    wObj["snap_to_grid"] = w.snap_to_grid;
    
    wObj["show_chart"] = w.show_chart;
    wObj["chart_w_pct"] = w.chart_w_pct;
    wObj["chart_h_pct"] = w.chart_h_pct;
    wObj["chart_x_ofs"] = w.chart_x_ofs;
    wObj["chart_y_ofs"] = w.chart_y_ofs;
    if (w.chart_min.length() > 0) wObj["chart_min"] = w.chart_min;
    if (w.chart_max.length() > 0) wObj["chart_max"] = w.chart_max;
    
    if (w.name.length() > 0) wObj["name"] = w.name;
    if (w.mdi_icon.length() > 0) wObj["mdi_icon"] = w.mdi_icon;
    if (w.color_on.length() > 0) wObj["color_on"] = w.color_on;
    if (w.color_off.length() > 0) wObj["color_off"] = w.color_off;
    
    if (w.media_content_type.length() > 0) wObj["media_content_type"] = w.media_content_type;
    if (w.media_content_id.length() > 0) wObj["media_content_id"] = w.media_content_id;

    if (w.tap_action_domain.length() > 0) wObj["ta_dom"] = w.tap_action_domain;
    if (w.tap_action_service.length() > 0) wObj["ta_srv"] = w.tap_action_service;
    if (w.tap_action_target.length() > 0) wObj["ta_tgt"] = w.tap_action_target;

    if (w.select_options.length() > 0) wObj["select_options"] = w.select_options;
    wObj["slider_min"] = w.slider_min;
    wObj["slider_max"] = w.slider_max;
    wObj["slider_step"] = w.slider_step;

    // --- NEU: Speichern der Bedingungen ---
    if (w.conditions.size() > 0) {
        wObj["conditions_type"] = w.conditions_type;
        JsonArray j_conds = wObj["conditions"].to<JsonArray>();
        for (const auto& c : w.conditions) {
            JsonObject j_c = j_conds.add<JsonObject>();
            j_c["entity"] = c.entity;
            j_c["op"] = c.op;
            j_c["value"] = c.value;
        }
    }

    // --- NEU: Speichern der Kinder-Widgets ---
    if (w.children.size() > 0) {
        JsonArray j_children = wObj["children"].to<JsonArray>();
        for (const auto& child : w.children) {
            JsonObject j_child = j_children.add<JsonObject>();
            serializeWidget(j_child, child);
        }
    }
}

void HaConfigLogic::Load() {
    dashboards.clear();
    
    if (!LittleFS.exists("/ha_config.json")) {
        HADashboardDef defTab;
        defTab.name = "Wohnen";
        dashboards.push_back(defTab);
        Save();
        return;
    }

    File file = LittleFS.open("/ha_config.json", "r");
    if (!file) return;

    size_t size = file.size();
    std::unique_ptr<char[]> buf(new char[size]);
    file.readBytes(buf.get(), size);
    file.close();

    JsonDocument doc(&configAlloc);
    DeserializationError error = deserializeJson(doc, buf.get());
    if (error) return;

    JsonArray tabs = doc["tabs"];
    for (JsonObject tObj : tabs) {
        HADashboardDef tDef;
        tDef.name = tObj["name"] | "Tab";
        
        JsonArray widgets = tObj["widgets"];
        for (JsonObject wObj : widgets) {
            HAWidgetDef wDef;
            deserializeWidget(wObj, wDef);
            tDef.widgets.push_back(wDef);
        }
        dashboards.push_back(tDef);
    }
}

void HaConfigLogic::Save() {
    JsonDocument doc(&configAlloc);
    JsonArray tabs = doc["tabs"].to<JsonArray>();
    
    for (const auto& t : dashboards) {
        JsonObject tObj = tabs.add<JsonObject>();
        tObj["name"] = t.name;
        JsonArray widgets = tObj["widgets"].to<JsonArray>();
        
        for (const auto& w : t.widgets) {
            JsonObject wObj = widgets.add<JsonObject>();
            serializeWidget(wObj, w);
        }
    }

    File file = LittleFS.open("/ha_config.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
}

void HaConfigLogic::AddTab(String name) { HADashboardDef d; d.name = name; dashboards.push_back(d); }
void HaConfigLogic::DeleteTab(int index) { if (index >= 0 && index < dashboards.size()) { dashboards.erase(dashboards.begin() + index); if (dashboards.size() == 0) AddTab("Wohnen"); } }
void HaConfigLogic::RenameTab(int index, String newName) { if (index >= 0 && index < dashboards.size()) dashboards[index].name = newName; }
void HaConfigLogic::MoveTabLeft(int index) { if (index > 0 && index < dashboards.size()) std::swap(dashboards[index], dashboards[index - 1]); }
void HaConfigLogic::MoveTabRight(int index) { if (index >= 0 && index < dashboards.size() - 1) std::swap(dashboards[index], dashboards[index + 1]); }