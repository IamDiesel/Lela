#include "HaConfigLogic.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm> 
#include "lvgl.h" 

std::vector<HADashboardDef> HaConfigLogic::dashboards;

void HaConfigLogic::Init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }
    Load(); 
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

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf.get());
    if (error) return;

    JsonArray tabs = doc["tabs"];
    for (JsonObject tObj : tabs) {
        HADashboardDef tDef;
        tDef.name = tObj["name"] | "Tab";
        
        JsonArray widgets = tObj["widgets"];
        for (JsonObject wObj : widgets) {
            HAWidgetDef wDef;
            
            // FIX: V7 kompatible Abfragen
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
            
            int i_align = LV_ALIGN_TOP_MID, t_align = LV_ALIGN_BOTTOM_MID, s_align = LV_ALIGN_CENTER;
            int i_margin = 5, t_margin = 5, s_margin = 0;
            
            wDef.icon_align = wObj["icon_align"] | i_align;
            wDef.text_align = wObj["text_align"] | t_align;
            wDef.state_align = wObj["state_align"] | s_align;
            
            wDef.icon_margin = !wObj["icon_margin"].isNull() ? wObj["icon_margin"].as<int>() : i_margin;
            wDef.text_margin = !wObj["text_margin"].isNull() ? wObj["text_margin"].as<int>() : t_margin;
            wDef.state_margin = wObj["state_margin"] | s_margin;
            
            wDef.snap_to_grid = !wObj["snap_to_grid"].isNull() ? wObj["snap_to_grid"].as<bool>() : true;
            
            // Diagramm Settings
            wDef.show_chart = wObj["show_chart"] | false;
            wDef.chart_w_pct = wObj["chart_w_pct"] | 95;
            wDef.chart_h_pct = wObj["chart_h_pct"] | 50;
            wDef.chart_x_ofs = wObj["chart_x_ofs"] | 0;
            wDef.chart_y_ofs = wObj["chart_y_ofs"] | -15;
            wDef.chart_min = wObj["chart_min"] | "";
            wDef.chart_max = wObj["chart_max"] | "";
            
            // Media Player Settings
            wDef.media_content_type = wObj["media_content_type"] | "";
            wDef.media_content_id = wObj["media_content_id"] | "";
            
            tDef.widgets.push_back(wDef);
        }
        dashboards.push_back(tDef);
    }
}

void HaConfigLogic::Save() {
    JsonDocument doc;
    JsonArray tabs = doc["tabs"].to<JsonArray>();
    
    for (const auto& t : dashboards) {
        JsonObject tObj = tabs.add<JsonObject>();
        tObj["name"] = t.name;
        JsonArray widgets = tObj["widgets"].to<JsonArray>();
        
        for (const auto& w : t.widgets) {
            JsonObject wObj = widgets.add<JsonObject>();
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
        }
    }

    File file = LittleFS.open("/ha_config.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
}

void HaConfigLogic::AddTab(String name) {
    HADashboardDef d; d.name = name; dashboards.push_back(d);
}

void HaConfigLogic::DeleteTab(int index) {
    if (index >= 0 && index < dashboards.size()) {
        dashboards.erase(dashboards.begin() + index);
        if (dashboards.size() == 0) AddTab("Wohnen");
    }
}

void HaConfigLogic::RenameTab(int index, String newName) {
    if (index >= 0 && index < dashboards.size()) dashboards[index].name = newName;
}

void HaConfigLogic::MoveTabLeft(int index) {
    if (index > 0 && index < dashboards.size()) std::swap(dashboards[index], dashboards[index - 1]);
}

void HaConfigLogic::MoveTabRight(int index) {
    if (index >= 0 && index < dashboards.size() - 1) std::swap(dashboards[index], dashboards[index + 1]);
}