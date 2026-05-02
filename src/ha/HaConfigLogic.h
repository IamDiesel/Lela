#pragma once
#include <Arduino.h>
#include <vector>

struct HAWidgetDef {
    String entity_id;
    String type;
    int x = 20;
    int y = 20;
    int w = 160;
    int h = 105;
    String name;
    String mdi_icon;
    String color_on;
    String color_off;
    
    int icon_align = 0;  
    int text_align = 2;  
    int state_align = 1; // NEU: Ausrichtung fuer Sensorwerte
    
    int icon_margin = 5; 
    int text_margin = 5; 
    int state_margin = 0; // NEU: Abstand fuer Sensorwerte
    
    bool snap_to_grid = true; 
    
    String media_content_type;
    String media_content_id;
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
    static void RenameTab(int index, String new_name);
    
    static void MoveTabLeft(int index);
    static void MoveTabRight(int index);
};