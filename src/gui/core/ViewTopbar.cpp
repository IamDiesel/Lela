#include "ViewTopbar.h"
#include "SharedData.h"
#include <WiFi.h>
#include <M5Unified.h>

static lv_obj_t * top_bar_obj = nullptr;
static lv_obj_t * top_label_clock = nullptr;
static lv_obj_t * top_icon_ble = nullptr;
static lv_obj_t * top_icon_ha = nullptr;

static lv_obj_t * top_icon_bat = nullptr;
static lv_obj_t * top_icon_charge = nullptr;
static lv_obj_t * top_label_bat_pct = nullptr;

// NEU: UI Element und Speicher fuer Fehlermeldungen links
static lv_obj_t * top_label_status = nullptr;
static String s_topbarStatusMsg = "";

static int s_top_clock_min = -1;
static int s_top_iconBleState = -1;
static int s_top_wifiStatus = -1;
static int s_top_batteryPercent = -1;
static bool s_top_lastCharging = false; 

static uint32_t s_last_slow_update = 0;
static int s_cachedWifiStatus = 0;

void ViewTopbar_Create(lv_obj_t* parent) {
    s_top_clock_min = -1; 
    s_top_iconBleState = -1; 
    s_top_wifiStatus = -1; 
    s_top_batteryPercent = -1;
    s_top_lastCharging = false;
    
    s_last_slow_update = 0; 
    s_topbarStatusMsg = "";

    top_bar_obj = lv_obj_create(parent);
    lv_obj_set_size(top_bar_obj, 1280, 80); 
    lv_obj_align(top_bar_obj, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar_obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(top_bar_obj, 150, 0); 
    lv_obj_set_style_border_width(top_bar_obj, 0, 0);
    lv_obj_clear_flag(top_bar_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(top_bar_obj, LV_OBJ_FLAG_CLICKABLE);

    // NEU: Das Label für Fehlermeldungen (links)
    top_label_status = lv_label_create(top_bar_obj);
    lv_obj_set_width(top_label_status, 380); // Genug Platz links
    lv_label_set_long_mode(top_label_status, LV_LABEL_LONG_SCROLL_CIRCULAR); // Scrollen wenn zu lang
    lv_obj_set_style_text_font(top_label_status, &lv_font_montserrat_20, 0); 
    lv_obj_set_style_text_color(top_label_status, lv_color_hex(0xFF5555), 0); // Rot zur Warnung
    lv_label_set_text(top_label_status, ""); 
    lv_obj_align(top_label_status, LV_ALIGN_TOP_LEFT, 20, 25); 

    top_label_clock = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_label_clock, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(top_label_clock, lv_color_hex(0xAAAAAA), 0); 
    lv_label_set_text(top_label_clock, "--:--"); 
    lv_obj_align(top_label_clock, LV_ALIGN_TOP_MID, 0, 20); 
    
    top_icon_ble = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_icon_ble, &lv_font_montserrat_24, 0); 
    lv_label_set_text(top_icon_ble, LV_SYMBOL_BLUETOOTH); 
    lv_obj_align(top_icon_ble, LV_ALIGN_TOP_MID, -200, 20); 
    
    top_icon_ha = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_icon_ha, &lv_font_montserrat_24, 0); 
    lv_label_set_text(top_icon_ha, LV_SYMBOL_HOME); 
    lv_obj_align(top_icon_ha, LV_ALIGN_TOP_MID, -100, 20); 
    
    top_icon_bat = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_icon_bat, &lv_font_montserrat_24, 0); 
    lv_label_set_text(top_icon_bat, LV_SYMBOL_BATTERY_FULL); 
    lv_obj_set_style_text_color(top_icon_bat, lv_color_white(), 0); 
    lv_obj_align(top_icon_bat, LV_ALIGN_TOP_MID, 130, 20); 

    top_icon_charge = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_icon_charge, &lv_font_montserrat_16, 0); 
    lv_label_set_text(top_icon_charge, LV_SYMBOL_CHARGE); 
    lv_obj_set_style_text_color(top_icon_charge, lv_color_white(), 0); 
    lv_obj_align_to(top_icon_charge, top_icon_bat, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(top_icon_charge, LV_OBJ_FLAG_HIDDEN);

    top_label_bat_pct = lv_label_create(top_bar_obj); 
    lv_obj_set_style_text_font(top_label_bat_pct, &lv_font_montserrat_24, 0); 
    lv_label_set_text(top_label_bat_pct, "100%"); 
    lv_obj_set_style_text_color(top_label_bat_pct, lv_color_white(), 0); 
    lv_obj_align_to(top_label_bat_pct, top_icon_bat, LV_ALIGN_OUT_RIGHT_MID, 8, 0); 
}

void ViewTopbar_Update() {
    if (top_bar_obj == nullptr) return;

    // NEU: Live Update des Fehlertextes
    if (s_topbarStatusMsg != topbarStatusMsg) {
        s_topbarStatusMsg = topbarStatusMsg;
        lv_label_set_text(top_label_status, s_topbarStatusMsg.c_str());
    }

    if (s_last_slow_update == 0 || millis() - s_last_slow_update > 1000) {
        s_cachedWifiStatus = !wifiEnabled ? 0 : (WiFi.status() == WL_CONNECTED ? 1 : 2);
        s_last_slow_update = millis();
        if (s_last_slow_update == 0) s_last_slow_update = 1; 
    }

    if (timeSynced) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10)) {
            if (s_top_clock_min != timeinfo.tm_min) {
                lv_label_set_text_fmt(top_label_clock, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                s_top_clock_min = timeinfo.tm_min;
            }
        }
    }

    bool fastBlink = (millis() % 600 < 300);
    int curIconBleState = (matEnabled || kippyEnabled) ? (matEnabled && !connected ? (fastBlink ? 1 : 2) : 3) : 0;
    if (s_top_iconBleState != curIconBleState) {
        lv_color_t ble_col = (curIconBleState == 1) ? lv_color_hex(0xFF8800) : ((curIconBleState == 3) ? lv_color_hex(0x00FF00) : lv_color_hex(0x555555));
        lv_obj_set_style_text_color(top_icon_ble, ble_col, 0); s_top_iconBleState = curIconBleState;
    }
            
    if (s_top_wifiStatus != s_cachedWifiStatus) {
        lv_color_t wifi_col = (s_cachedWifiStatus == 0) ? lv_color_hex(0x555555) : (s_cachedWifiStatus == 1 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000));
        lv_obj_set_style_text_color(top_icon_ha, wifi_col, 0); s_top_wifiStatus = s_cachedWifiStatus;
    }
            
    if (s_top_batteryPercent != batteryPercent || s_top_lastCharging != isBatteryCharging) {
        const char * bat_icon = LV_SYMBOL_BATTERY_EMPTY;
        if (batteryPercent >= 80) bat_icon = LV_SYMBOL_BATTERY_FULL; 
        else if (batteryPercent >= 60) bat_icon = LV_SYMBOL_BATTERY_3;
        else if (batteryPercent >= 40) bat_icon = LV_SYMBOL_BATTERY_2; 
        else if (batteryPercent >= 15) bat_icon = LV_SYMBOL_BATTERY_1;
        
        lv_label_set_text(top_icon_bat, bat_icon);
        lv_label_set_text_fmt(top_label_bat_pct, "%d%%", batteryPercent);

        if (isBatteryCharging) {
            lv_obj_set_style_text_color(top_icon_bat, lv_color_hex(0x00FF00), 0);
            lv_obj_set_style_text_color(top_label_bat_pct, lv_color_hex(0x00FF00), 0);
            lv_obj_clear_flag(top_icon_charge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_color_t color = batteryPercent < 15 ? lv_color_hex(0xFF0000) : lv_color_white();
            lv_obj_set_style_text_color(top_icon_bat, color, 0);
            lv_obj_set_style_text_color(top_label_bat_pct, color, 0);
            lv_obj_add_flag(top_icon_charge, LV_OBJ_FLAG_HIDDEN);
        }
        s_top_batteryPercent = batteryPercent;
        s_top_lastCharging = isBatteryCharging;
    }
}

void ViewTopbar_SetHidden(bool hidden) {
    if (top_bar_obj) {
        if (hidden) lv_obj_add_flag(top_bar_obj, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(top_bar_obj, LV_OBJ_FLAG_HIDDEN);
    }
}