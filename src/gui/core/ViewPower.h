#pragma once
#include <lvgl.h>

class ViewPower {
public:
    // Startet den Hintergrund-Timer, der das Display bei Alarmen aufweckt
    static void init();
    
    // Callbacks für die Buttons in den QuickSettings
    static void btn_shutdown_event_cb(lv_event_t * e);
    static void btn_screen_off_event_cb(lv_event_t * e);
};