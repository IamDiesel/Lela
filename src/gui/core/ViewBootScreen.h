#pragma once
#include <lvgl.h>

// Die Zustaende unserer State-Machine.
// Der Start wartet nur noch auf das WLAN: Home Assistant verbindet sich erst
// beim Oeffnen der HA-App (GuiManager::switchScreen) - ein frueherer Zustand
// "warte auf HA" lief deshalb immer in seine 2,5-s-Wartezeit.
enum BootState {
    BOOT_STATE_INIT = 0,
    BOOT_STATE_WIFI,
    BOOT_STATE_DONE,
    BOOT_STATE_TIMEOUT
};

class ViewBootScreen {
public:
    static void show();
    
    // Wird von der main.cpp aufgerufen, um die Animation voranzutreiben
    static void setBootState(BootState state, const char* extraInfo = "");
};

// Wird weiterhin exportiert (wird intern aber nur noch gelesen)
extern lv_obj_t * splash_status_label;