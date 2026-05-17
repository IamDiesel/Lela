#pragma once
#include <Arduino.h>
#include "lvgl.h"

class HaDialogTab {
public:
    static void showAddTabDialog();
    static void showRenameTabDialog();
    static void deleteCurrentTab();
    static void resetState();
    
private:
    static lv_obj_t* kb_overlay;
    static lv_obj_t* ta_new_tab;
    static int editingTabIndex;
    
    static void createOverlay(const char* placeholder_or_text, bool is_placeholder);
    static void kb_event_cb(lv_event_t * e);
};