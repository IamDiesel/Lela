#include "ViewCatMat.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "ViewTopbar.h"
#include <WiFi.h>
#include <cmath>
#include <ctime>
#include <cstdio>

#define TEXT_COLOR (isDarkMode ? lv_color_white() : lv_color_black())

#define ICON_RADAR_X -135
#define ICON_RADAR_Y 500
#define ICON_HA_X 135
#define ICON_HA_Y 500

static int s_haTrackerState = -1;
static int s_catStateUi = -1; 
static int s_catRssi = -999;
static int32_t s_currentPressure = -99999;
static int32_t s_rawPressure = -99999;

static uint32_t s_lastBtnColor = 0; 
static int s_lastBtnStateForText = -1;
static int s_lastGraphMode = -1; 

static lv_obj_t * label_status; 
static lv_obj_t * label_pressure; 
static lv_obj_t * label_avg; 
static lv_obj_t * label_raw; 
static lv_obj_t * label_limit; 
static lv_obj_t * chart; 
static lv_chart_series_t * ser_avg; 
static lv_obj_t * label_legend; 
static lv_obj_t * label_latest_val;
static lv_obj_t * btn_auto_fit; 
static lv_obj_t * tick_y[7]; 
static lv_obj_t * tick_x[7]; 
static lv_obj_t * cat_cont; 
static lv_obj_t * cat_line_head; 
static lv_obj_t * cat_line_eye_l; 
static lv_obj_t * cat_line_eye_r; 
static lv_obj_t * cat_line_mouth;
static lv_obj_t * label_cat;
static lv_obj_t * home_cont; 
static lv_obj_t * away_cont; 
static lv_obj_t * label_ha_tracker;
static lv_obj_t * btn_mute; 

static bool chart_expanded = false;
static bool chartNeedsUpdate = false; 

static void set_cat_color(lv_color_t color) {
    lv_obj_set_style_line_color(cat_line_head, color, 0); 
    lv_obj_set_style_line_color(cat_line_eye_l, color, 0);
    lv_obj_set_style_line_color(cat_line_eye_r, color, 0); 
    lv_obj_set_style_line_color(cat_line_mouth, color, 0);
}

static void updateLegendText() {
    const char* expandedText = "";
    const char* collapsedText = "";

    switch (currentGraphMode) {
        case 0: expandedText = "DRUCK SENSOR"; collapsedText = "D\nR\nU\nC\nK"; break;
        case 1: expandedText = "MATTE RSSI"; collapsedText = "B\nL\nE"; break;
        case 2: expandedText = "WLAN RSSI"; collapsedText = "W\nL\nA\nN"; break;
        case 3: expandedText = "BLE INTERVALL"; collapsedText = "I\nN\nT"; break;
        case 4: expandedText = "KIPPY RSSI"; collapsedText = "K\nI\nP"; break;
        default: expandedText = "SENSOR"; collapsedText = "D\nA\nT\nA"; break;
    }
    lv_label_set_text(label_legend, chart_expanded ? expandedText : collapsedText);
}

static void updateChartData() {
    if (!chart || !ser_avg) return;
    int points = graphTimeSeconds;
    if (points > HISTORY_SIZE) points = HISTORY_SIZE;
    if (points < 10) points = 10;
    if (lv_chart_get_point_count(chart) != points) lv_chart_set_point_count(chart, points);

    int32_t val_min = 999999, val_max = -999999;
    int32_t latest_valid_val = -32000;
    bool has_data = false;
    
    for (int i = 0; i < points; i++) {
        int s = points - 1 - i; 
        int idx = historyIdx - 1 - s; 
        if (idx < 0) idx += HISTORY_SIZE; 
        int32_t val = pressureHistory[idx]; 
        if (s >= historyCount) val = -32000; 

        if (val != -32000) {
            lv_chart_set_value_by_id(chart, ser_avg, i, val);
            if (val < val_min) val_min = val; 
            if (val > val_max) val_max = val;
            latest_valid_val = val; 
            has_data = true;
        } else {
            lv_chart_set_value_by_id(chart, ser_avg, i, LV_CHART_POINT_NONE);
        }
    }
    
    static float sticky_min = 0.0, sticky_max = 4500.0;
    static uint32_t shrink_timer = 0; 
    
    if (has_data) {
        float data_range = val_max - val_min; 
        if (data_range < 5.0) data_range = 5.0;
        float padding = data_range * 0.05; 
        float target_max = val_max + padding;
        float target_min = val_min - padding;

        if (force_auto_fit) {
            sticky_max = target_max; sticky_min = target_min; force_auto_fit = false; shrink_timer = millis();
        } else {
            bool expanded = false;
            if (val_max > sticky_max - (padding * 0.5)) { sticky_max = target_max; expanded = true; }
            if (val_min < sticky_min + (padding * 0.5)) { sticky_min = target_min; expanded = true; }
            if (expanded) shrink_timer = millis();
            else {
                if (target_max < sticky_max - (data_range * 0.3) && target_min > sticky_min + (data_range * 0.3)) {
                    if (millis() - shrink_timer > 3000) { sticky_max -= (sticky_max - target_max) * 0.1; sticky_min += (target_min - sticky_min) * 0.1; }
                } else shrink_timer = millis(); 
            }
        }
    }
    if(sticky_max <= sticky_min) sticky_max = sticky_min + 10.0;

    int y_sections = 4; 
    int32_t step = ceil((sticky_max - sticky_min) / y_sections);
    if(step <= 1) step = 1; else if(step <= 5) step = 5; else if(step <= 10) step = 10; else if(step <= 20) step = 20; else if(step <= 50) step = 50; else step = ceil(step / 100.0) * 100;

    int32_t l_min = floor(sticky_min / step) * step; 
    int32_t l_max = l_min + (y_sections * step);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, l_min, l_max);

    if (has_data) {
        int y_pos = (1.0 - ((float)(latest_valid_val - l_min) / (l_max - l_min))) * lv_obj_get_height(chart) - 25; 
        if (y_pos < 5) y_pos = 15; if (y_pos > lv_obj_get_height(chart) - 20) y_pos = lv_obj_get_height(chart) - 20; 
        lv_obj_align_to(label_latest_val, chart, LV_ALIGN_TOP_LEFT, lv_obj_get_width(chart) - 45, y_pos);
        lv_label_set_text_fmt(label_latest_val, "%d", latest_valid_val); 
        lv_obj_clear_flag(label_latest_val, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(label_latest_val, LV_OBJ_FLAG_HIDDEN);

    for(int i=0; i<=6; i++) { lv_obj_add_flag(tick_y[i], LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(tick_x[i], LV_OBJ_FLAG_HIDDEN); }
    
    if (chart_expanded) {
        for (int i = 0; i <= y_sections; i++) {
            lv_label_set_text_fmt(tick_y[i], "%d", l_min + (i * step));
            lv_obj_align_to(tick_y[i], chart, LV_ALIGN_OUT_LEFT_BOTTOM, -10, - (i * lv_obj_get_height(chart) / y_sections) + 12); 
            lv_obj_clear_flag(tick_y[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_label_set_text_fmt(tick_y[0], "%d", l_min); lv_obj_align_to(tick_y[0], chart, LV_ALIGN_OUT_LEFT_BOTTOM, -10, 12); lv_obj_clear_flag(tick_y[0], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(tick_y[y_sections], "%d", l_max); lv_obj_align_to(tick_y[y_sections], chart, LV_ALIGN_OUT_LEFT_TOP, -10, -12); lv_obj_clear_flag(tick_y[y_sections], LV_OBJ_FLAG_HIDDEN);
    }
    
    int num_x_labels = chart_expanded ? 4 : 2; 
    for (int i = 0; i <= num_x_labels; i++) {
        int seconds_ago = points - 1 - (i * (points - 1) / num_x_labels);
        if (seconds_ago < 0) seconds_ago = 0;
        
        char buf[32];
        if (showTimeOnX && timeSynced) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 10)) {
                time_t now = mktime(&timeinfo);
                now -= seconds_ago;
                struct tm * past = localtime(&now);
                snprintf(buf, sizeof(buf), "%02d:%02d", past->tm_hour, past->tm_min);
            } else snprintf(buf, sizeof(buf), "--:--");
        } else {
            if (seconds_ago == 0) snprintf(buf, sizeof(buf), "Jetzt");
            else if (seconds_ago >= 60) snprintf(buf, sizeof(buf), "-%dm", seconds_ago / 60); 
            else snprintf(buf, sizeof(buf), "-%ds", seconds_ago);
        }
        
        lv_label_set_text(tick_x[i], buf);
        int x_offset = (i * lv_obj_get_width(chart)) / num_x_labels;
        
        if (i == 0) lv_obj_align_to(tick_x[i], chart, LV_ALIGN_OUT_BOTTOM_LEFT, -20, 10);
        else if (i == num_x_labels) lv_obj_align_to(tick_x[i], chart, LV_ALIGN_OUT_BOTTOM_RIGHT, 20, 10);
        else lv_obj_align_to(tick_x[i], chart, LV_ALIGN_OUT_BOTTOM_LEFT, x_offset - 40, 10);
        
        lv_obj_clear_flag(tick_x[i], LV_OBJ_FLAG_HIDDEN);
        if (chart_expanded) lv_obj_move_foreground(tick_x[i]);
    }

    lv_chart_refresh(chart);
}

static void btn_auto_fit_event_cb(lv_event_t * e) { playToneI2S(800, 100, true); force_auto_fit = true; chartNeedsUpdate = true; }
static void btn_mute_event_cb(lv_event_t * e) { 
    playToneI2S(800, 100, true);
    if (alarmActive || disconnectAlarmActive) {
        if (!muted) muted = true; else { alarmActive = false; disconnectAlarmActive = false; muted = false; isArmed = false; }
    } else {
        isArmed = !isArmed;
    }
}

static void chart_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    chart_expanded = !chart_expanded; updateLegendText(); 
    if (chart_expanded) {
        lv_obj_set_size(chart, 1000, 500); 
        lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0); 
        lv_obj_set_style_bg_color(chart, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0); 
        lv_obj_set_style_border_width(chart, 2, LV_PART_MAIN); 
        lv_obj_set_style_line_color(chart, lv_color_hex(0x444444), LV_PART_MAIN); 
        lv_chart_set_div_line_count(chart, 4, 4); 

        lv_obj_align_to(label_legend, chart, LV_ALIGN_OUT_TOP_MID, 0, -20); 
        
        ViewTopbar_SetHidden(true); 
        lv_obj_add_flag(label_pressure, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(label_avg, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(label_raw, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(label_limit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN); 
        
        lv_obj_move_foreground(chart); 
        lv_obj_move_foreground(label_legend);
        lv_obj_move_foreground(label_latest_val);
        
        lv_obj_add_flag(btn_mute, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(cat_cont, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(label_cat, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(away_cont, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(label_ha_tracker, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_clear_flag(btn_auto_fit, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_align_to(btn_auto_fit, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 60); 
        lv_obj_move_foreground(btn_auto_fit);
    } else {
        lv_obj_set_size(chart, 600, 180); 
        lv_obj_align(chart, LV_ALIGN_TOP_MID, -20, 280); 
        lv_obj_set_style_bg_opa(chart, 0, 0); 
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN); 
        lv_obj_set_style_line_color(chart, lv_color_hex(0x333333), LV_PART_MAIN); 
        lv_chart_set_div_line_count(chart, 3, 5); 

        lv_obj_align_to(label_legend, chart, LV_ALIGN_OUT_RIGHT_MID, 15, 0); 
        lv_obj_add_flag(btn_auto_fit, LV_OBJ_FLAG_HIDDEN);
        
        ViewTopbar_SetHidden(false); 
        lv_obj_clear_flag(label_pressure, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        
        if (!isTrackerMode) {
            lv_obj_clear_flag(label_avg, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(label_raw, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(label_limit, LV_OBJ_FLAG_HIDDEN); 
        }
        
        lv_obj_clear_flag(btn_mute, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_clear_flag(cat_cont, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_clear_flag(label_cat, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_clear_flag(label_ha_tracker, LV_OBJ_FLAG_HIDDEN); 
        
        s_catStateUi = -1;
        s_haTrackerState = -1;
        s_currentPressure = -99999;
    }
    chartNeedsUpdate = true;
}

lv_obj_t* ViewCatMat::build() {
    s_haTrackerState = -1; s_catStateUi = -1; s_catRssi = -999;
    s_currentPressure = -99999; s_rawPressure = -99999;
    s_lastBtnColor = 0; s_lastBtnStateForText = -1; s_lastGraphMode = -1; 

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) return nullptr; 

    lv_obj_set_style_bg_color(scr, isDarkMode ? lv_color_hex(0x111111) : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(scr);

    label_status = lv_label_create(scr); 
    lv_label_set_text(label_status, "Suche..."); 
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_status, TEXT_COLOR, 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 100); 
    
    label_pressure = lv_label_create(scr); 
    lv_obj_set_style_text_font(label_pressure, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_text_color(label_pressure, TEXT_COLOR, 0);
    lv_label_set_text(label_pressure, "0"); 
    lv_obj_align(label_pressure, LV_ALIGN_TOP_MID, 0, 140); 
    
    label_avg = lv_label_create(scr); 
    lv_label_set_text(label_avg, "Schnitt: 0"); 
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_avg, TEXT_COLOR, 0);
    lv_obj_align(label_avg, LV_ALIGN_TOP_MID, 0, 220); 
    
    label_raw = lv_label_create(scr); 
    lv_obj_set_style_text_color(label_raw, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_font(label_raw, &lv_font_montserrat_16, 0); 
    lv_label_set_text(label_raw, "RAW: 0"); 
    lv_obj_align_to(label_raw, label_avg, LV_ALIGN_OUT_LEFT_MID, -40, 0); 
    
    label_limit = lv_label_create(scr); 
    lv_obj_set_style_text_color(label_limit, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_font(label_limit, &lv_font_montserrat_16, 0); 
    lv_label_set_text_fmt(label_limit, "Limit: %d", thresholdVal); 
    lv_obj_align_to(label_limit, label_avg, LV_ALIGN_OUT_RIGHT_MID, 40, 0); 

    chart = lv_chart_create(scr); 
    lv_obj_set_size(chart, 600, 180); 
    lv_obj_align(chart, LV_ALIGN_TOP_MID, -20, 280); 
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); 
    lv_chart_set_point_count(chart, 100); 
    
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR); 
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR); 
    
    lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN); 
    lv_obj_set_style_bg_opa(chart, 0, 0); 
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN); 
    lv_obj_set_style_line_color(chart, lv_color_hex(0x333333), LV_PART_MAIN); 
    lv_chart_set_div_line_count(chart, 3, 5); 
    
    ser_avg = lv_chart_add_series(chart, lv_color_hex(0x00A0FF), LV_CHART_AXIS_PRIMARY_Y); 
    lv_chart_set_all_value(chart, ser_avg, LV_CHART_POINT_NONE); 
    lv_obj_add_event_cb(chart, chart_event_cb, LV_EVENT_CLICKED, NULL);
    
    label_legend = lv_label_create(scr); updateLegendText(); 
    lv_obj_set_style_text_font(label_legend, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_color(label_legend, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_align(label_legend, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_align_to(label_legend, chart, LV_ALIGN_OUT_RIGHT_MID, 15, 0); 
    
    label_latest_val = lv_label_create(scr); 
    lv_label_set_text(label_latest_val, "0"); 
    lv_obj_set_style_text_font(label_latest_val, &lv_font_montserrat_20, 0); 
    lv_obj_set_style_text_color(label_latest_val, lv_color_hex(0x00A0FF), 0); 
    lv_obj_set_style_bg_color(label_latest_val, lv_color_hex(0x333333), 0); 
    lv_obj_set_style_bg_opa(label_latest_val, 220, 0); 
    lv_obj_set_style_pad_all(label_latest_val, 8, 0); 
    lv_obj_set_style_radius(label_latest_val, 10, 0); 
    lv_obj_add_flag(label_latest_val, LV_OBJ_FLAG_HIDDEN);
    
    btn_auto_fit = lv_btn_create(scr); 
    lv_obj_set_size(btn_auto_fit, 200, 50); 
    lv_obj_add_event_cb(btn_auto_fit, btn_auto_fit_event_cb, LV_EVENT_CLICKED, NULL); 
    lv_obj_t * label_auto_fit = lv_label_create(btn_auto_fit); 
    lv_label_set_text(label_auto_fit, "AUTO FIT"); 
    lv_obj_set_style_text_font(label_auto_fit, &lv_font_montserrat_20, 0); 
    lv_obj_center(label_auto_fit); 
    lv_obj_add_flag(btn_auto_fit, LV_OBJ_FLAG_HIDDEN); 
    
    for (int i = 0; i <= 6; i++) { 
        tick_y[i] = lv_label_create(scr); 
        lv_obj_set_style_text_font(tick_y[i], &lv_font_montserrat_16, 0); 
        lv_obj_set_style_text_color(tick_y[i], lv_color_hex(0x888888), 0); 
        lv_obj_add_flag(tick_y[i], LV_OBJ_FLAG_HIDDEN); 
        
        tick_x[i] = lv_label_create(scr); 
        lv_obj_set_style_text_font(tick_x[i], &lv_font_montserrat_16, 0); 
        lv_obj_set_width(tick_x[i], 80); 
        lv_obj_set_style_text_align(tick_x[i], LV_TEXT_ALIGN_CENTER, 0); 
        lv_obj_set_style_text_color(tick_x[i], lv_color_hex(0x888888), 0); 
        lv_obj_add_flag(tick_x[i], LV_OBJ_FLAG_HIDDEN); 
    }

    cat_cont = lv_obj_create(scr); 
    lv_obj_set_size(cat_cont, 108, 96); 
    lv_obj_set_style_bg_opa(cat_cont, 0, 0); 
    lv_obj_set_style_border_width(cat_cont, 0, 0); 
    lv_obj_add_flag(cat_cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 
    lv_obj_set_style_pad_all(cat_cont, 0, 0); 
    lv_obj_align(cat_cont, LV_ALIGN_TOP_MID, ICON_RADAR_X, ICON_RADAR_Y); 
    
    static const lv_point_t cat_head[] = {{18,24}, {30,42}, {72,42}, {84,24}, {90,54}, {72,78}, {51,84}, {30,78}, {12,54}, {18,24}};
    static const lv_point_t eye_l[] = {{30,54}, {39,48}, {48,54}}; 
    static const lv_point_t eye_r[] = {{54,54}, {63,48}, {72,54}}; 
    static const lv_point_t mouth[] = {{57,66}, {51,72}, {45,66}};
    
    cat_line_head = lv_line_create(cat_cont); lv_line_set_points(cat_line_head, cat_head, 10); lv_obj_set_style_line_width(cat_line_head, 6, 0); lv_obj_set_style_line_rounded(cat_line_head, true, 0);
    cat_line_eye_l = lv_line_create(cat_cont); lv_line_set_points(cat_line_eye_l, eye_l, 3); lv_obj_set_style_line_width(cat_line_eye_l, 6, 0); 
    cat_line_eye_r = lv_line_create(cat_cont); lv_line_set_points(cat_line_eye_r, eye_r, 3); lv_obj_set_style_line_width(cat_line_eye_r, 6, 0); 
    cat_line_mouth = lv_line_create(cat_cont); lv_line_set_points(cat_line_mouth, mouth, 3); lv_obj_set_style_line_width(cat_line_mouth, 6, 0); 
    
    label_cat = lv_label_create(scr); 
    lv_label_set_text(label_cat, "Suche..."); 
    lv_obj_set_style_text_font(label_cat, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(label_cat, TEXT_COLOR, 0);
    lv_obj_align(label_cat, LV_ALIGN_TOP_MID, ICON_RADAR_X, ICON_RADAR_Y + 100); 

    home_cont = lv_obj_create(scr); 
    lv_obj_set_size(home_cont, 108, 96); 
    lv_obj_set_style_bg_opa(home_cont, 0, 0); 
    lv_obj_set_style_border_width(home_cont, 0, 0); 
    lv_obj_add_flag(home_cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 
    lv_obj_set_style_pad_all(home_cont, 0, 0); 
    lv_obj_align(home_cont, LV_ALIGN_TOP_MID, ICON_HA_X, ICON_HA_Y);
    lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN); 
    
    static const lv_point_t h_house[] = {{0, 42}, {54, 0}, {108, 42}, {108, 96}, {0, 96}, {0, 42}};
    lv_obj_t * l_house = lv_line_create(home_cont); lv_line_set_points(l_house, h_house, 6); lv_obj_set_style_line_width(l_house, 6, 0); lv_obj_set_style_line_color(l_house, lv_color_hex(0x00FF00), 0);
    static const lv_point_t h_cat_sit[] = {{72,96}, {36,96}, {36,72}, {42,48}, {36,30}, {45,39}, {63,39}, {72,30}, {66,48}, {72,72}, {72,96}};
    lv_obj_t * h_cat = lv_line_create(home_cont); lv_line_set_points(h_cat, h_cat_sit, 11); lv_obj_set_style_line_width(h_cat, 6, 0); lv_obj_set_style_line_color(h_cat, lv_color_hex(0x00FF00), 0); lv_obj_set_style_line_rounded(h_cat, true, 0);

    away_cont = lv_obj_create(scr); 
    lv_obj_set_size(away_cont, 108, 96); 
    lv_obj_set_style_bg_opa(away_cont, 0, 0); 
    lv_obj_set_style_border_width(away_cont, 0, 0); 
    lv_obj_add_flag(away_cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 
    lv_obj_set_style_pad_all(away_cont, 0, 0); 
    lv_obj_align(away_cont, LV_ALIGN_TOP_MID, ICON_HA_X, ICON_HA_Y); 
    lv_obj_add_flag(away_cont, LV_OBJ_FLAG_HIDDEN); 
    
    static const lv_point_t a_tree1[] = {{9,84}, {9,42}, {0,42}, {9,12}, {18,42}, {9,42}}; 
    static const lv_point_t a_tree2[] = {{99,84}, {99,42}, {90,42}, {99,12}, {108,42}, {99,42}};
    static const lv_point_t a_cat_walk[] = {{30,48}, {36,66}, {51,66}, {63,60}, {66,51}, {72,51}, {78,60}, {72,69}, {72,84}, {66,84}, {66,75}, {51,75}, {51,84}, {45,84}, {45,75}, {36,66}};
    
    lv_obj_t * l_t1 = lv_line_create(away_cont); lv_line_set_points(l_t1, a_tree1, 6); lv_obj_set_style_line_width(l_t1, 6, 0); lv_obj_set_style_line_color(l_t1, lv_color_hex(0xFF8800), 0);
    lv_obj_t * l_t2 = lv_line_create(away_cont); lv_line_set_points(l_t2, a_tree2, 6); lv_obj_set_style_line_width(l_t2, 6, 0); lv_obj_set_style_line_color(l_t2, lv_color_hex(0xFF8800), 0);
    lv_obj_t * l_acat = lv_line_create(away_cont); lv_line_set_points(l_acat, a_cat_walk, 16); lv_obj_set_style_line_width(l_acat, 6, 0); lv_obj_set_style_line_color(l_acat, lv_color_hex(0xFF8800), 0); lv_obj_set_style_line_rounded(l_acat, true, 0);
    
    label_ha_tracker = lv_label_create(scr); 
    lv_label_set_text(label_ha_tracker, "Lade HA..."); 
    lv_obj_set_style_text_font(label_ha_tracker, &lv_font_montserrat_24, 0); 
    lv_obj_align(label_ha_tracker, LV_ALIGN_TOP_MID, ICON_HA_X, ICON_HA_Y + 100); 
    lv_obj_set_style_text_color(label_ha_tracker, lv_color_hex(0x555555), 0);

    btn_mute = lv_btn_create(scr); 
    lv_obj_set_size(btn_mute, 600, 80); 
    lv_obj_align(btn_mute, LV_ALIGN_BOTTOM_MID, 0, -40); 
    lv_obj_add_event_cb(btn_mute, btn_mute_event_cb, LV_EVENT_CLICKED, NULL); 
    
    lv_obj_t * label_mute = lv_label_create(btn_mute); 
    lv_label_set_text(label_mute, "LADE..."); 
    lv_obj_set_style_text_font(label_mute, &lv_font_montserrat_24, 0); 
    lv_obj_center(label_mute);

    chart_expanded = false;
    return scr;
}

void ViewCatMat::update() {
    if (gui.getCurrentScreen() != SCREEN_CATMAT) return;

    if (s_lastGraphMode != currentGraphMode) { updateLegendText(); s_lastGraphMode = currentGraphMode; }

    ViewTopbar_Update();

    int curBtnState = alarmActive ? (muted ? 4 : 3) : (disconnectAlarmActive ? (muted ? 6 : 5) : (isArmed ? 2 : 1));
    uint32_t targetBtnColor = 0x555555; const char* targetBtnText = "";

    bool fastBlink = (millis() % 600 < 300);

    switch(curBtnState) {
        case 1: targetBtnText = "ALARM AKTIVIEREN"; targetBtnColor = 0x555555; break;
        case 2: targetBtnText = "ALARM DEAKTIVIEREN"; targetBtnColor = 0x00A0FF; break;
        case 3: targetBtnText = "CATMAT ALARM STUMM"; targetBtnColor = fastBlink ? 0xFF0000 : 0x660000; break;
        case 4: targetBtnText = "CATMAT ALARM RESET"; targetBtnColor = 0xFF8800; break;
        case 5: targetBtnText = "OFFLINE-WARNUNG STUMM"; targetBtnColor = fastBlink ? 0x0000FF : 0x000055; break;
        case 6: targetBtnText = "OFFLINE-WARNUNG IGNORIEREN"; targetBtnColor = 0x0088CC; break;
    }

    if (s_lastBtnStateForText != curBtnState) {
        lv_label_set_text(lv_obj_get_child(btn_mute, 0), targetBtnText);
        s_lastBtnStateForText = curBtnState;
    }
    if (s_lastBtnColor != targetBtnColor) {
        lv_obj_set_style_bg_color(btn_mute, lv_color_hex(targetBtnColor), 0);
        s_lastBtnColor = targetBtnColor;
    }

    static uint32_t lastChartUpdate = 0;
    if (millis() - lastChartUpdate > 1000) { lastChartUpdate = millis(); chartNeedsUpdate = true; }
    if (chartNeedsUpdate) { updateChartData(); chartNeedsUpdate = false; }
    lv_label_set_text_fmt(label_limit, "Limit: %d", thresholdVal);

    if (isTrackerMode) {
        if (millis() - lastCatSeenTime > 5000) lv_label_set_text(label_pressure, "Suche..."); else lv_label_set_text_fmt(label_pressure, "%d dBm", catRssi);
        lv_label_set_text(label_status, "RADAR AKTIV");
        lv_obj_add_flag(label_avg, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(label_raw, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(label_limit, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(chart, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(label_legend, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (!chart_expanded) { lv_obj_clear_flag(label_avg, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(label_raw, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(label_limit, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(chart, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(label_legend, LV_OBJ_FLAG_HIDDEN); }

        if (s_currentPressure != currentPressure || s_rawPressure != rawPressure) {
            if (!matEnabled) { lv_label_set_text(label_pressure, "OFF"); lv_label_set_text(label_avg, "DEAKTIVIERT"); lv_label_set_text(label_raw, "RAW: --"); } 
            else if (connected) { lv_label_set_text_fmt(label_pressure, "%d", currentPressure); lv_label_set_text_fmt(label_avg, muted ? "STUMM" : "Schnitt: %d", currentAvg); lv_label_set_text_fmt(label_raw, "RAW: %d", rawPressure); } 
            else { lv_label_set_text(label_pressure, "- - -"); lv_label_set_text(label_avg, fastBlink ? "Verbinde..." : ""); lv_label_set_text(label_raw, "RAW: --"); }
            lv_obj_set_style_text_color(label_pressure, TEXT_COLOR, 0); lv_obj_set_style_text_color(label_avg, TEXT_COLOR, 0);
            s_currentPressure = currentPressure; s_rawPressure = rawPressure;
        }
        
        // --- NEU: Status Anzeige basierend auf Datenquelle ---
        char statusStr[64] = "";
        if (useMqttForMat) {
            if (millis() - proxyLastUpdate > 15000) {
                snprintf(statusStr, sizeof(statusStr), "Proxy: Offline");
            } else {
                snprintf(statusStr, sizeof(statusStr), "Proxy: %s", proxyStatusMsg.c_str());
            }
        } else {
            if (matEnabled && kippyEnabled) { snprintf(statusStr, sizeof(statusStr), "M: %s | K: %s", connected ? "OK" : "Sucht", (lastCatSeenTime == 0 || millis() - lastCatSeenTime > 20000) ? "Abwesend" : "Aktiv"); } 
            else if (matEnabled) { snprintf(statusStr, sizeof(statusStr), "Matte: %s", connected ? "Verbunden" : "Suche..."); } 
            else if (kippyEnabled) { snprintf(statusStr, sizeof(statusStr), "Kippy: %s", (lastCatSeenTime == 0 || millis() - lastCatSeenTime > 20000) ? "Abwesend" : "Aktiv"); } 
            else { snprintf(statusStr, sizeof(statusStr), "BLE Deaktiviert"); }
        }
        lv_label_set_text(label_status, statusStr); 
        lv_obj_set_style_text_color(label_status, TEXT_COLOR, 0); 
    }
    
    int curCatState = 0;
    if (!kippyEnabled) curCatState = 0; 
    else if (lastCatSeenTime == 0 || millis() - lastCatSeenTime > 20000) curCatState = 1; 
    else if (catRssi > -75) curCatState = 2; 
    else if (catRssi > -85) curCatState = 3; 
    else curCatState = 4;

    if (s_catStateUi != curCatState || (curCatState > 1 && s_catRssi != catRssi)) {
        lv_color_t tc = lv_color_hex(0x555555);
        if (curCatState == 0) lv_label_set_text(label_cat, "OFF");
        else if (curCatState == 1) lv_label_set_text(label_cat, "Abwesend");
        else if (curCatState == 2) { tc = lv_color_hex(0x00FF00); lv_label_set_text_fmt(label_cat, "Nah (%d)", catRssi); }
        else if (curCatState == 3) { tc = lv_color_hex(0xFFFF00); lv_label_set_text_fmt(label_cat, "Mittel (%d)", catRssi); }
        else if (curCatState == 4) { tc = lv_color_hex(0xFF8800); lv_label_set_text_fmt(label_cat, "Fern (%d)", catRssi); }
        
        set_cat_color(tc); lv_obj_set_style_text_color(label_cat, tc, 0);
        if (!chart_expanded) { lv_obj_clear_flag(cat_cont, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(label_cat, LV_OBJ_FLAG_HIDDEN); }
        s_catStateUi = curCatState; s_catRssi = catRssi;
    }

    int curHaTrackerState = (!wifiEnabled) ? 1 : ((!isTrackerDataValid || WiFi.status() != WL_CONNECTED) ? 2 : (isCatAtHome ? 3 : 4));
    if (s_haTrackerState != curHaTrackerState) {
        if (!chart_expanded) {
            if (curHaTrackerState == 1) { lv_label_set_text(label_ha_tracker, "WLAN Aus"); lv_obj_set_style_text_color(label_ha_tracker, lv_color_hex(0x555555), 0); lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(away_cont, LV_OBJ_FLAG_HIDDEN); }
            else if (curHaTrackerState == 2) { lv_label_set_text(label_ha_tracker, "Lade HA..."); lv_obj_set_style_text_color(label_ha_tracker, lv_color_hex(0x555555), 0); lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(away_cont, LV_OBJ_FLAG_HIDDEN); }
            else if (curHaTrackerState == 3) { lv_label_set_text(label_ha_tracker, "Zuhause"); lv_obj_set_style_text_color(label_ha_tracker, lv_color_hex(0x00FF00), 0); lv_obj_clear_flag(home_cont, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(away_cont, LV_OBJ_FLAG_HIDDEN); }
            else { lv_label_set_text(label_ha_tracker, "Unterwegs"); lv_obj_set_style_text_color(label_ha_tracker, lv_color_hex(0xFF8800), 0); lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(away_cont, LV_OBJ_FLAG_HIDDEN); }
        }
        s_haTrackerState = curHaTrackerState;
    }
}