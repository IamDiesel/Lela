#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"
#include "HAWidgets.h"
#include "IEditTab.h"
#include <vector>

class HaDialogEdit {
private:
    static lv_obj_t* overlay;
    static lv_obj_t* edit_panel;
    static HAWidget* current_widget;
    static lv_obj_t* kb;
    
    static std::vector<IEditTab*> active_tabs;

    static int orig_w, orig_h, orig_i_align, orig_t_align, orig_s_align;
    static int orig_i_margin, orig_t_margin, orig_s_margin;
    static int orig_c_w, orig_c_h, orig_c_x, orig_c_y;

    static void btn_save_event_cb(lv_event_t * e);
    static void btn_cancel_event_cb(lv_event_t * e);

public:
    static void showWidgetEditDialog(HAWidget* widget);
    static void resetState();
    static void handleWidgetDeleteDrop(HAWidget* widget);
    static void bindKeyboard(lv_obj_t* ta, int mode = LV_KEYBOARD_MODE_TEXT_LOWER);
};