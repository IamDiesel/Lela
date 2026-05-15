#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"
#include "HAWidgets.h"

class HaDialogEdit {
private:
    static lv_obj_t* overlay;
    static lv_obj_t* panel;
    static HAWidget* current_widget;

    static lv_obj_t* ta_name;
    static lv_obj_t* ta_entity;
    static lv_obj_t* ta_icon_search; 
    static lv_obj_t* dd_icon_cat;
    static lv_obj_t* dd_icon;
    static lv_obj_t* dd_type;
    static lv_obj_t* cb_snap; 
    
    // --- DIAGRAMM UI ---
    static lv_obj_t* cb_chart; 
    static lv_obj_t* slider_chart_w;
    static lv_obj_t* slider_chart_h;
    static lv_obj_t* slider_chart_x; 
    static lv_obj_t* slider_chart_y; 
    static lv_obj_t* lbl_c_w_val;
    static lv_obj_t* lbl_c_h_val;
    static lv_obj_t* lbl_c_x_val;    
    static lv_obj_t* lbl_c_y_val;    
    static lv_obj_t* ta_chart_min;
    static lv_obj_t* ta_chart_max;
    
    // --- VACUUM UI (NEU) ---
    static lv_obj_t* slider_vac_w;
    static lv_obj_t* slider_vac_h;
    static lv_obj_t* slider_vac_gap;
    static lv_obj_t* slider_vac_y;
    static lv_obj_t* lbl_v_w_val;
    static lv_obj_t* lbl_v_h_val;
    static lv_obj_t* lbl_v_gap_val;
    static lv_obj_t* lbl_v_y_val;

    static lv_obj_t* slider_icon_margin;
    static lv_obj_t* slider_text_margin;
    static lv_obj_t* lbl_i_m_val;
    static lv_obj_t* lbl_t_m_val;
    
    static lv_obj_t* dd_state_pos;
    static lv_obj_t* slider_state_margin;
    static lv_obj_t* lbl_s_m_val;
    
    static void icon_search_event_cb(lv_event_t * e);
    
    static lv_obj_t* btn_color_on;
    static lv_obj_t* btn_color_off;
    static lv_obj_t* color_picker_overlay;
    static bool picking_color_on;
    static String selected_color_on;
    static String selected_color_off;

    static lv_obj_t* kb;

    static void btn_close_event_cb(lv_event_t * e);
    static void btn_save_event_cb(lv_event_t * e);
    static void ta_event_cb(lv_event_t * e);
    static void dd_icon_cat_event_cb(lv_event_t * e);
    
    static void btn_color_on_event_cb(lv_event_t * e);
    static void btn_color_off_event_cb(lv_event_t * e);
    static void color_tile_event_cb(lv_event_t * e);
    static void close_color_picker_cb(lv_event_t * e);
    static void openColorPicker();
    static void updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix);

public:
    static void showWidgetEditDialog(HAWidget* widget);
    static void resetState();
    static void handleWidgetDeleteDrop(HAWidget* widget);
    static void showLightControlDialog(HAWidget* widget);
};