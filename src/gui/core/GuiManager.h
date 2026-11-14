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

    // Baut die offene Ansicht neu auf, z.B. nach einem Wechsel des
    // Farbschemas (UiTheme). Waehrend der Umstellung (D1-D4) nur fuer
    // Ansichten, die das Schema schon nutzen; die anderen uebernehmen es beim
    // naechsten Aufbau.
    void reloadCurrentScreen();
    
    // public, damit Ansichten Globale Gesten abfangen koennen
    static void gestureEventWrapper(lv_event_t * e);

    Preferences preferences;
private:
    ScreenID currentScreen;

    void handleGesture(lv_event_t * e);
};

extern GuiManager gui;