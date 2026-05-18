#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"
#include "HAWidgets.h"

class HaDialogEdit {
private:
    static lv_obj_t* overlay;
    static lv_obj_t* edit_panel;
    static HAWidget* current_widget;
    static lv_obj_t* kb;

    // Statische Felder (werden beim Speichern ausgelesen)
    static lv_obj_t* ta_name;
    static lv_obj_t* ta_icon_search; 
    static lv_obj_t* dd_icon_cat;
    static lv_obj_t* dd_icon;
    static lv_obj_t* cb_snap; 
    
    static lv_obj_t* cb_chart; 
    static lv_obj_t* slider_chart_w;
    static lv_obj_t* slider_chart_h;
    static lv_obj_t* slider_chart_x; 
    static lv_obj_t* slider_chart_y; 
    static lv_obj_t* ta_chart_min;
    static lv_obj_t* ta_chart_max;
    
    static lv_obj_t* slider_vac_w;
    static lv_obj_t* slider_vac_h;
    static lv_obj_t* slider_vac_gap;
    static lv_obj_t* slider_vac_y;

    static lv_obj_t* dd_icon_pos;
    static lv_obj_t* dd_text_pos;
    static lv_obj_t* dd_state_pos;
    static lv_obj_t* slider_icon_margin;
    static lv_obj_t* slider_text_margin;
    static lv_obj_t* slider_state_margin;

    static lv_obj_t* lbl_i_m_val;
    static lv_obj_t* lbl_t_m_val;
    static lv_obj_t* lbl_s_m_val;
    static lv_obj_t* lbl_c_w_val;
    static lv_obj_t* lbl_c_h_val;
    static lv_obj_t* lbl_c_x_val;
    static lv_obj_t* lbl_c_y_val;
    static lv_obj_t* lbl_v_w_val;
    static lv_obj_t* lbl_v_h_val;
    static lv_obj_t* lbl_v_gap_val;
    static lv_obj_t* lbl_v_y_val;

    static lv_obj_t* btn_color_on;
    static lv_obj_t* btn_color_off;
    
    static bool picking_color_on;
    static String selected_color_on;
    static String selected_color_off;
    static String last_search_term;

    // --- NEU: UI-Elemente fuer den Bedingungs-Tab ---
    static lv_obj_t* dd_cond_type;
    static lv_obj_t* ta_cond1_entity;
    static lv_obj_t* dd_cond1_op;
    static lv_obj_t* ta_cond1_val;
    static lv_obj_t* ta_cond2_entity;
    static lv_obj_t* dd_cond2_op;
    static lv_obj_t* ta_cond2_val;

    // Backups für Abbruch
    static int orig_w, orig_h, orig_i_align, orig_t_align, orig_s_align;
    static int orig_i_margin, orig_t_margin, orig_s_margin;
    static int orig_c_w, orig_c_h, orig_c_x, orig_c_y;

    // Hilfsfunktionen für UI Aufbau
    static void buildSizeTab(lv_obj_t* parent, HAWidget* w);
    static void buildDisplayTab(lv_obj_t* parent, HAWidget* w);
    static void buildLayoutTab(lv_obj_t* parent, HAWidget* w);
    static void buildChartTab(lv_obj_t* parent, HAWidget* w);
    static void buildVacuumTab(lv_obj_t* parent, HAWidget* w);
    static void buildConditionTab(lv_obj_t* parent, HAWidget* w); // NEU

    static void updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix);
    static void bindKeyboardToTextarea(lv_obj_t* ta);

    // Callbacks
    static void icon_search_event_cb(lv_event_t * e);
    static void dd_icon_cat_event_cb(lv_event_t * e);
    static void btn_save_event_cb(lv_event_t * e);
    static void btn_cancel_event_cb(lv_event_t * e);
    static void layout_change_cb(lv_event_t* e);

public:
    static void showWidgetEditDialog(HAWidget* widget);
    static void resetState();
    static void handleWidgetDeleteDrop(HAWidget* widget);
};