#pragma once
#include <Arduino.h>

class HaServiceCaller {
public:
    static void CallService(String domain, String service, String entity_id);
    static void CallLightService(String entity_id, int brightness, int r, int g, int b, int w);
    
    static void CallMediaService(String entity_id, String service);
    static void CallMediaVolumeService(String entity_id, float volume);
    static void CallMediaSelectSource(String entity_id, String source);
    
    static void RequestBrowseMedia(String entity_id, String media_content_type = "", String media_content_id = "");
    static void CallPlayMedia(String entity_id, String media_content_type, String media_content_id);
    
    static void RequestDashboardList(int target_tab_index);
    static void RequestDashboardViews(String url_path);
    static void RequestDashboardCards(String url_path, int view_index);

    // NEU: Vacuum Befehle
    static void CallVacuumService(String entity_id, String service);
    static void CallVacuumSetFanSpeed(String entity_id, String speed);
};