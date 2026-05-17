#pragma once
#include <Arduino.h>
#include "lvgl.h"

class HaEditToolbar {
public:
    static void show();
    static void hide();
    static void resetState();

private:
    static lv_obj_t* btn_import;
    static lv_obj_t* btn_delete_tab; 
    static lv_obj_t* btn_rename_tab; 
    static lv_obj_t* btn_add_widget; 
    static lv_obj_t* btn_tab_left;
    static lv_obj_t* btn_tab_right;
};