#pragma once
#include <Arduino.h>
#include <functional>
#include "LVGL_Driver.h"

class HaColorPicker {
private:
    static lv_obj_t* overlay;
    static std::function<void(String)> onColorSelected;

    static void tile_event_cb(lv_event_t * e);
    static void close_event_cb(lv_event_t * e);

public:
    static void show(String currentColor, std::function<void(String)> callback);
    static void hide();
    static bool isActive();
};