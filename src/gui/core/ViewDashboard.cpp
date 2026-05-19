#include "ViewDashboard.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "ViewTopbar.h"
#include <WiFi.h>

static lv_obj_t * lbl_blocker;
static lv_obj_t * btnBaby;
static lv_obj_t * lblBaby;
static lv_obj_t * btnCat;
static lv_obj_t * lblCat;

static lv_obj_t * btnHA;
static lv_obj_t * lblHA;

static bool s_lastBabyAlarm = false;
static bool s_lastBabyMuted = false;
static bool s_lastBlinkBaby = false;
static bool s_lastCatAlarm = false;
static bool s_lastCatMuted = false;
static bool s_lastBlinkCat = false;

static void set_scr_opaque_black(lv_obj_t * scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

// FIX: "ViewDashboard::" hinzugefuegt, damit der Linker die Funktion findet!
void ViewDashboard::btn_baby_event_cb(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
    
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) {
        return;
    }
    
    playToneI2S(800, 100, true);
    
    if (babyAlarmActive && !muteBaby) {
        muteBaby = true;
    } else {
        gui.switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE); 
    }
}

// FIX: "ViewDashboard::" hinzugefuegt!
void ViewDashboard::btn_cat_event_cb(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
    
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) {
        return;
    }
    
    playToneI2S(800, 100, true);
    
    if (alarmActive && !muteAlarm) {
        muteAlarm = true;
    } else {
        gui.switchScreen(SCREEN_CATMAT, LV_SCR_LOAD_ANIM_NONE); 
    }
}

// FIX: "ViewDashboard::" hinzugefuegt und Lade-Animation (Spinner) integriert!
void ViewDashboard::btn_ha_event_cb(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
    
    if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) {
        return;
    }
    
    playToneI2S(800, 100, true);
    
    lv_obj_t* btn = lv_event_get_target(e);

    // Ladekreis sofort auf dem Button erzeugen
    lv_obj_t* spinner = lv_spinner_create(btn, 1000, 60);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_center(spinner);

    // LVGL zwingen, den Ladekreis sofort zu zeichnen
    lv_timer_handler();

    // 50ms warten, damit die Animation aufpoppt, dann erst das HA System laden
    lv_timer_t* t = lv_timer_create([](lv_timer_t* timer) {
        gui.switchScreen(SCREEN_HA, LV_SCR_LOAD_ANIM_FADE_ON);
        lv_timer_del(timer); 
    }, 50, NULL);
}

lv_obj_t* ViewDashboard::build() {
    s_lastBabyAlarm = false; 
    s_lastBabyMuted = false; 
    s_lastBlinkBaby = false;
    
    s_lastCatAlarm = false; 
    s_lastCatMuted = false; 
    s_lastBlinkCat = false;

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) {
        return nullptr;
    }

    set_scr_opaque_black(scr);
    
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    btnBaby = lv_btn_create(scr);
    lv_obj_set_size(btnBaby, lv_pct(50), lv_pct(100));
    lv_obj_align(btnBaby, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btnBaby, lv_color_hex(0x4FA5D6), 0); 
    lv_obj_set_style_radius(btnBaby, 0, 0);
    lv_obj_set_style_border_width(btnBaby, 0, 0);
    lv_obj_add_event_cb(btnBaby, btn_baby_event_cb, LV_EVENT_CLICKED, NULL);

    lblBaby = lv_label_create(btnBaby);
    lv_label_set_text(lblBaby, "Baby\nMonitor");
    lv_obj_center(lblBaby);
    lv_obj_set_style_text_color(lblBaby, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblBaby, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_align(lblBaby, LV_TEXT_ALIGN_CENTER, 0);

    btnCat = lv_btn_create(scr);
    lv_obj_set_size(btnCat, lv_pct(50), lv_pct(100));
    lv_obj_align(btnCat, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btnCat, lv_color_hex(0xE67E22), 0); 
    lv_obj_set_style_radius(btnCat, 0, 0);
    lv_obj_set_style_border_width(btnCat, 0, 0);
    lv_obj_add_event_cb(btnCat, btn_cat_event_cb, LV_EVENT_CLICKED, NULL);

    lblCat = lv_label_create(btnCat);
    lv_label_set_text(lblCat, "CatMat");
    lv_obj_center(lblCat);
    lv_obj_set_style_text_color(lblCat, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblCat, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_align(lblCat, LV_TEXT_ALIGN_CENTER, 0);

    btnHA = lv_btn_create(scr);
    lv_obj_set_size(btnHA, 280, 280); 
    lv_obj_align(btnHA, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_style_bg_color(btnHA, lv_color_hex(0x222222), 0); 
    lv_obj_set_style_bg_opa(btnHA, 160, 0); 
    lv_obj_set_style_radius(btnHA, LV_RADIUS_CIRCLE, 0); 
    
    lv_obj_set_style_border_width(btnHA, 4, 0);
    lv_obj_set_style_border_color(btnHA, lv_color_hex(0x555555), 0);
    lv_obj_set_style_shadow_width(btnHA, 0, 0); 
    
    lv_obj_add_event_cb(btnHA, btn_ha_event_cb, LV_EVENT_CLICKED, NULL);

    lblHA = lv_label_create(btnHA);
    lv_label_set_text(lblHA, LV_SYMBOL_HOME "\nHA");
    lv_obj_center(lblHA);
    lv_obj_set_style_text_color(lblHA, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblHA, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_align(lblHA, LV_TEXT_ALIGN_CENTER, 0);

    ViewTopbar_Create(scr);

    lv_obj_t* swipe_blocker = lv_obj_create(scr);
    lv_obj_set_size(swipe_blocker, 1280, 100);
    lv_obj_align(swipe_blocker, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(swipe_blocker, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(swipe_blocker, 150, 0);
    lv_obj_set_style_border_width(swipe_blocker, 0, 0);
    lv_obj_add_flag(swipe_blocker, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(swipe_blocker, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(swipe_blocker, LV_OBJ_FLAG_SCROLLABLE);

    lbl_blocker = lv_label_create(swipe_blocker);
    lv_label_set_text(lbl_blocker, LV_SYMBOL_SETTINGS "   " LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl_blocker, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_blocker, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_blocker, LV_ALIGN_TOP_MID, 0, 30);

    return scr;
}

void ViewDashboard::update() {
    if (gui.getCurrentScreen() != SCREEN_DASHBOARD) {
        return;
    }

    ViewTopbar_Update();

    bool fastBlink = (millis() % 1000 < 500);

    if (babyAlarmActive && !muteBaby) {
        if (fastBlink != s_lastBlinkBaby || !s_lastBabyAlarm || s_lastBabyMuted) {
            lv_obj_set_style_bg_color(btnBaby, fastBlink ? lv_color_hex(0xFF0000) : lv_color_hex(0xAA0000), 0);
            lv_label_set_text(lblBaby, LV_SYMBOL_BELL "\nBABY\nALARM!\n(Klick = Mute)");
            
            s_lastBabyAlarm = true; 
            s_lastBabyMuted = false; 
            s_lastBlinkBaby = fastBlink;
        }
    } else {
        if (s_lastBabyAlarm || s_lastBabyMuted != muteBaby) {
            lv_obj_set_style_bg_color(btnBaby, lv_color_hex(0x4FA5D6), 0);
            lv_label_set_text(lblBaby, "Baby\nMonitor");
            
            s_lastBabyAlarm = false; 
            s_lastBabyMuted = muteBaby;
        }
    }

    if (alarmActive && !muteAlarm) {
        if (fastBlink != s_lastBlinkCat || !s_lastCatAlarm || s_lastCatMuted) {
            lv_obj_set_style_bg_color(btnCat, fastBlink ? lv_color_hex(0xFF0000) : lv_color_hex(0xAA0000), 0);
            lv_label_set_text(lblCat, LV_SYMBOL_BELL "\nCATMAT\nALARM!\n(Klick = Mute)");
            
            s_lastCatAlarm = true; 
            s_lastCatMuted = false; 
            s_lastBlinkCat = fastBlink;
        }
    } else {
        if (s_lastCatAlarm || s_lastCatMuted != muteAlarm) {
            lv_obj_set_style_bg_color(btnCat, lv_color_hex(0xE67E22), 0);
            lv_label_set_text(lblCat, "CatMat");
            
            s_lastCatAlarm = false; 
            s_lastCatMuted = muteAlarm;
        }
    }
}