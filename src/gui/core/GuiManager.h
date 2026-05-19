#pragma once
#include <lvgl.h>
#include <Preferences.h>

enum ScreenID {
    SCREEN_DASHBOARD = 0,
    SCREEN_CATMAT,
    SCREEN_BABY,
    SCREEN_HA,
    SCREEN_SETTINGS
};

class GuiManager {
public:
    void init();
    void switchScreen(ScreenID newScreen, lv_scr_load_anim_t anim_type);
    ScreenID getCurrentScreen() const;
    
    // public, damit Ansichten Globale Gesten abfangen koennen
    static void gestureEventWrapper(lv_event_t * e);

    Preferences preferences;
private:
    ScreenID currentScreen;

    void handleGesture(lv_event_t * e);
};

extern GuiManager gui;