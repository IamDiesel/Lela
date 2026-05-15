#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"

class UIHelper {
public:
    static lv_obj_t* createLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, uint32_t color = 0xFFFFFFFF);
    static lv_obj_t* createButton(lv_obj_t* parent, int w, int h, int align, int x, int y, uint32_t bg_color, const char* text, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createSlider(lv_obj_t* parent, int w, int h, int align, int x, int y, int min_val, int max_val, int val, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createDropdown(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* options, int selected, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createTextarea(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* text, const char* placeholder = "", bool one_line = true);
    static lv_obj_t* createCheckbox(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, bool checked, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
};