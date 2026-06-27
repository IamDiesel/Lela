#pragma once
#include <Arduino.h>
#include "lvgl.h"
#include <vector>
#include "HAWidgets.h"

class ViewHomeAssistant {
public:
    static lv_obj_t* build();
    static void update();
    
    static lv_obj_t* screen;
    static lv_obj_t* tabview;
    static lv_obj_t* btn_edit;
    static lv_obj_t* label_edit;
    static lv_obj_t* trash_btn; 

    static volatile bool pendingHaReload;
    static uint16_t currentActiveTab;

    static void clearWidgets();
    static String generateEntityId(String type, String input);
    
    static void helper_saveWidgets(); 

private:
    static std::vector<HAWidget*> widgets;
    static std::vector<lv_obj_t*> tab_pages;

    static void helper_loadWidgets();

    static void btn_back_event_cb(lv_event_t * e);
    static void btn_edit_event_cb(lv_event_t * e);
    static void tabview_event_cb(lv_event_t * e);
};