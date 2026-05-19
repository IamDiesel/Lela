#include "ViewPower.h"
#include "ViewQuickSettings.h"
#include "SharedData.h"
#include "LVGL_Driver.h"
#include <WiFi.h>
#include <M5Unified.h>

static lv_obj_t * screen_off_overlay = nullptr;

static void screen_off_click_cb(lv_event_t * e) {
    // Weckt das Display wieder auf
    M5.Display.setBrightness((brightnessPercent * 255) / 100);
    
    if (screen_off_overlay != nullptr) {
        lv_obj_del_async(screen_off_overlay); 
        screen_off_overlay = nullptr;
    }
}

static void alarm_wake_timer_cb(lv_timer_t * timer) {
    if (screen_off_overlay != nullptr) {
        // Wenn ein Alarm reinkommt, waehrend das Display aus ist -> Aufwecken
        if (alarmActive || babyAlarmActive || disconnectAlarmActive) {
            M5.Display.setBrightness((brightnessPercent * 255) / 100);
            lv_obj_del_async(screen_off_overlay);
            screen_off_overlay = nullptr;
        }
    }
}

void ViewPower::init() {
    lv_timer_create(alarm_wake_timer_cb, 500, NULL);
}

void ViewPower::btn_screen_off_event_cb(lv_event_t * e) {
    if (screen_off_overlay != nullptr) {
        return;
    }
    
    screen_off_overlay = lv_obj_create(lv_disp_get_layer_sys(NULL));
    lv_obj_set_size(screen_off_overlay, 1280, 720);
    lv_obj_set_style_bg_color(screen_off_overlay, lv_color_black(), 0); 
    lv_obj_set_style_bg_opa(screen_off_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen_off_overlay, 0, 0);
    
    lv_obj_add_flag(screen_off_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_off_overlay, screen_off_click_cb, LV_EVENT_PRESSED, NULL);

    M5.Display.setBrightness(0);
    ViewQuickSettings::hide(); // Schliesst das Menue im Hintergrund
}

void ViewPower::btn_shutdown_event_cb(lv_event_t * e) { 
    playToneI2S(600, 100, true);
    intentionalDisconnect = true; 
    
    WiFi.disconnect(true, true); 
    WiFi.mode(WIFI_OFF); 
    
    lv_obj_t * sd_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sd_scr, lv_color_hex(0x000000), 0);
    
    lv_obj_t * lbl = lv_label_create(sd_scr); 
    lv_label_set_text(lbl, "Geraet wird heruntergefahren...\n\n(Power-Taste lange drücken \nzum Aufwecken)"); 
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl); 
    
    lv_scr_load_anim(sd_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    lv_timer_handler(); 
    delay(1500); 
    
    M5.Power.powerOff(); 
}