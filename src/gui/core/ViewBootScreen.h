#pragma once
#include <lvgl.h>

// Die Zustaende unserer State-Machine
enum BootState {
    BOOT_STATE_INIT = 0,
    BOOT_STATE_WIFI,
    BOOT_STATE_HA,
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