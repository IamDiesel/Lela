#pragma once
#include <Arduino.h>
#include <vector>

struct HAWidgetDef {
    String entity_id = "";
    String type = "light";
    int x = 20;
    int y = 20;
    int w = 160;
    int h = 105;
    String name = "";
    String mdi_icon = "";
    String color_on = "";
    String color_off = "";
    
    int icon_align = 0;  // 0 = LV_ALIGN_TOP_MID
    int text_align = 2;  // 2 = LV_ALIGN_BOTTOM_MID
    int state_align = 1; // 1 = LV_ALIGN_CENTER
    
    int icon_margin = 5;
    int text_margin = 5;
    int state_margin = 0;
    
    bool snap_to_grid = true;
    
    // --- DIAGRAMM SETTINGS ---
    bool show_chart = false; 
    int chart_w_pct = 95;
    int chart_h_pct = 50;
    int chart_x_ofs = 0;     // X-Verschiebung
    int chart_y_ofs = -15;   // Y-Verschiebung
    String chart_min = "";
    String chart_max = "";
    
    // --- MEDIA SETTINGS ---
    String media_content_type = "";
    String media_content_id = "";

    // --- NEU: Entkoppelte Klick-Aktion ---
    String tap_action_domain = "";
    String tap_action_service = "";
    String tap_action_target = "";
};

struct HADashboardDef {
    String name;
    std::vector<HAWidgetDef> widgets;
};

class HaConfigLogic {
public:
    static std::vector<HADashboardDef> dashboards;
    static void Init();
    static void Load();
    static void Save();
    static void AddTab(String name);
    static void DeleteTab(int index);
    static void RenameTab(int index, String newName);
    static void MoveTabLeft(int index);
    static void MoveTabRight(int index);
};