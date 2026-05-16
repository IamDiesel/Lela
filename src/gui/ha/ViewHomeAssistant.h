#pragma once
#include "lvgl.h"
#include <vector>
#include "HAWidgets.h"
#include <Arduino.h>

class ViewHomeAssistant {
private:
    static lv_obj_t* label_edit;
    static int editingTabIndex;

    // --- NEU: Helper Funktionen korrekt deklariert ---
    static void helper_loadWidgets();
    static void helper_saveWidgets();
    static void helper_buildEditUI();
    static void helper_destroyEditUI();

    static void btn_back_event_cb(lv_event_t * e);
    static void btn_import_event_cb(lv_event_t * e);
    static void btn_delete_tab_event_cb(lv_event_t * e);
    static void btn_rename_tab_event_cb(lv_event_t * e);
    static void btn_add_widget_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);
    static void tabview_event_cb(lv_event_t * e);
    static void btn_edit_event_cb(lv_event_t * e);

public:
    static lv_obj_t* screen;
    static lv_obj_t* tabview;
    static lv_obj_t* btn_edit;
    static lv_obj_t* trash_btn;
    static std::vector<HAWidget*> widgets;
    static std::vector<lv_obj_t*> tab_pages;
    static volatile bool pendingHaReload;
    static uint16_t currentActiveTab;

    static String generateEntityId(String type, String input);
    static lv_obj_t* build();
    static void update();
    static void clearWidgets();
};