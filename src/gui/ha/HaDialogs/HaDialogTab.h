#pragma once
#include <Arduino.h>
#include "lvgl.h"

class HaDialogTab {
public:
    static void showAddTabDialog();
    static void showRenameTabDialog();

    // Fragt nach und loescht erst nach Bestaetigung den aktiven Tab.
    static void deleteCurrentTab();
    static void resetState();
    
private:
    static lv_obj_t* kb_overlay;
    static lv_obj_t* ta_new_tab;
    static int editingTabIndex;

    // Rueckfrage "Tab loeschen?"
    static lv_obj_t* confirm_overlay;
    static void closeConfirm();
    static void confirm_delete_cb(lv_event_t* e);
    
    static void createOverlay(const char* placeholder_or_text, bool is_placeholder);
    static void kb_event_cb(lv_event_t * e);
};