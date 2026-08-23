#include "ViewBaby.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "SystemLogic.h" 
#include "AudioStreamLogic.h"
#include "BabyCamApi.h"
#include "ViewTopbar.h"
#include <WiFi.h>
#include <math.h>

static lv_obj_t * cam_image_obj = nullptr; 
static lv_obj_t * cam_touch_overlay = nullptr; 
static lv_obj_t * lbl_cam_status = nullptr;
static lv_obj_t * lbl_play_icon = nullptr; 

static lv_obj_t * btn_audio = nullptr;
static lv_obj_t * lbl_audio = nullptr;
static lv_obj_t * btn_fs = nullptr;
static lv_obj_t * lbl_fs = nullptr;
static lv_obj_t * lbl_fps = nullptr; 
static lv_obj_t * fs_black_overlay = nullptr; 
static lv_obj_t * btn_ptt = nullptr;
static lv_obj_t * btn_mute = nullptr;
static lv_obj_t * lbl_mute = nullptr;
static lv_obj_t * btn_settings = nullptr;
static lv_obj_t * lbl_cam_bat_main = nullptr;
static lv_obj_t * lbl_cam_bat_fs = nullptr;

// Settings Modal
static lv_obj_t * settings_modal = nullptr;
static lv_obj_t * sw_flash = nullptr;
static lv_obj_t * sw_scrflash = nullptr;
static lv_obj_t * sw_camscreen = nullptr;
static lv_obj_t * sw_camstream = nullptr;
static lv_obj_t * slider_zoom = nullptr;
static lv_obj_t * slider_qual = nullptr;
static lv_obj_t * slider_speed = nullptr;
static lv_obj_t * dd_res = nullptr;

static uint32_t s_lastBtnColor = 0; 
static int s_lastBtnStateForText = -1;

static void view_baby_del_cb(lv_event_t * e) {
    cam_image_obj = nullptr; cam_touch_overlay = nullptr; lbl_cam_status = nullptr;
    lbl_play_icon = nullptr; btn_audio = nullptr; lbl_audio = nullptr; btn_fs = nullptr;
    lbl_fs = nullptr; lbl_fps = nullptr; fs_black_overlay = nullptr; btn_ptt = nullptr;
    btn_mute = nullptr; lbl_mute = nullptr; btn_settings = nullptr; settings_modal = nullptr;
    lbl_cam_bat_main = nullptr; lbl_cam_bat_fs = nullptr;
}

static void btn_audio_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
        lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
        if (!isAudioStreaming) AudioStreamLogic_StartBaby();
    } else {
        lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
        if (isAudioStreaming) AudioStreamLogic_StopBaby();
    }
}

static void btn_fs_event_cb(lv_event_t * e) {
    if (!isStreamActive) return; 
    playToneI2S(800, 100, true); 
    lv_obj_clear_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL); 
    vidFSMode = true;  
}

void ViewBaby_ExitFS() {
    if (fs_black_overlay) lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
    if (cam_image_obj) {
        const lv_img_dsc_t * dsc = (const lv_img_dsc_t *)lv_img_get_src(cam_image_obj);
        if (dsc != NULL) lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
        lv_img_set_zoom(cam_image_obj, 256); 
        lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20);
    }
    lv_obj_invalidate(lv_scr_act());
}

static void cam_image_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        playToneI2S(800, 100, true); 
        isStreamActive = !isStreamActive; 
        requestBabyStream = isStreamActive; 
        
        if (isStreamActive) {
            lv_obj_add_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
            lv_label_set_text(lbl_cam_status, LV_SYMBOL_WIFI " Verbinde...");
            if (btn_audio) lv_obj_add_state(btn_audio, LV_STATE_CHECKED);
            if (lbl_audio) lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
            AudioStreamLogic_StartBaby();
        } else {
            lv_obj_clear_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
            lv_label_set_text(lbl_cam_status, LV_SYMBOL_PAUSE " Pausiert");
            if (btn_audio) lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
            if (lbl_audio) lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
            AudioStreamLogic_StopBaby();
        }
    } 
}

static void btn_ptt_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_PRESSED) {
        playToneI2S(800, 100, true); 
        lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x00FF00), 0);
        lv_label_set_text(lv_obj_get_child(btn_ptt, 0), LV_SYMBOL_AUDIO " TALK");
        pttSwipeLock = true; 
        AudioStreamLogic_StartPTT();
    }
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x4FA5D6), 0);
        lv_label_set_text(lv_obj_get_child(btn_ptt, 0), LV_SYMBOL_MUTE " PTT");
        AudioStreamLogic_StopPTT();
        pttSwipeLock = false; 
    }
}

static void btn_mute_event_cb(lv_event_t * e) { 
    playToneI2S(800, 100, true); 
    if (babyAlarmActive) {
        if (!muteBaby) muteBaby = true; 
        else { babyAlarmActive = false; muteBaby = false; }
    } else {
        isBabyArmed = !isBabyArmed;
    }
}

// --- SETTINGS MODAL LOGIK ---
static void slider_step5_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    val = round(val / 5.0) * 5;
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        if (slider == slider_zoom) BabyCamApi_SetZoom(val);
        else if (slider == slider_qual) BabyCamApi_SetQuality(val);
        else if (slider == slider_speed) BabyCamApi_SetSpeed(val);
    }
}

static void settings_action_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if (obj == sw_flash) BabyCamApi_ToggleFlash();
    else if (obj == sw_scrflash) BabyCamApi_ToggleScrFlash();
    else if (obj == sw_camscreen) BabyCamApi_ToggleScreen();
    else if (obj == sw_camstream) BabyCamApi_ToggleCamera();
}

static void btn_lense_cb(lv_event_t * e) { BabyCamApi_ToggleLense(); }
static void btn_power_cb(lv_event_t * e) { BabyCamApi_PowerOff(); }
static void dd_res_cb(lv_event_t * e) {
    char buf[32];
    lv_dropdown_get_selected_str(dd_res, buf, sizeof(buf));
    String res = String(buf);
    int xIdx = res.indexOf('x');
    if(xIdx != -1) {
        int w = res.substring(0, xIdx).toInt();
        int h = res.substring(xIdx+1).toInt();
        BabyCamApi_SetResolution(w, h);
    }
}

static lv_obj_t* create_setting_slider(lv_obj_t* parent, const char* name, lv_obj_t** slider_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 380, 70);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    *slider_ptr = lv_slider_create(cont);
    lv_obj_set_size(*slider_ptr, 340, 20);
    lv_slider_set_range(*slider_ptr, 0, 100);
    lv_obj_align(*slider_ptr, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(*slider_ptr, slider_step5_cb, LV_EVENT_ALL, NULL);
    return cont;
}

static lv_obj_t* create_setting_switch(lv_obj_t* parent, const char* name, lv_obj_t** sw_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 180, 70);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);
    *sw_ptr = lv_switch_create(cont);
    lv_obj_align(*sw_ptr, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(*sw_ptr, settings_action_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return cont;
}

static void btn_open_settings_cb(lv_event_t * e) {
    lv_slider_set_value(slider_zoom, camZoom, LV_ANIM_OFF);
    lv_slider_set_value(slider_qual, camQuality, LV_ANIM_OFF);
    lv_slider_set_value(slider_speed, camSpeed, LV_ANIM_OFF);
    if(camFlash) lv_obj_add_state(sw_flash, LV_STATE_CHECKED); else lv_obj_clear_state(sw_flash, LV_STATE_CHECKED);
    if(camScrFlash) lv_obj_add_state(sw_scrflash, LV_STATE_CHECKED); else lv_obj_clear_state(sw_scrflash, LV_STATE_CHECKED);
    if(camScreenOn) lv_obj_add_state(sw_camscreen, LV_STATE_CHECKED); else lv_obj_clear_state(sw_camscreen, LV_STATE_CHECKED);
    if(camStatus == "PLAY") lv_obj_add_state(sw_camstream, LV_STATE_CHECKED); else lv_obj_clear_state(sw_camstream, LV_STATE_CHECKED);
    
    String opts = "";
    int selIdx = 0;
    for(int i=0; i<camResCount; i++) {
        opts += camResolutions[i];
        if(i < camResCount-1) opts += "\n";
        if(camResolutions[i] == currentCamRes) selIdx = i;
    }
    lv_dropdown_set_options(dd_res, opts.c_str());
    lv_dropdown_set_selected(dd_res, selIdx);
    
    lv_obj_clear_flag(settings_modal, LV_OBJ_FLAG_HIDDEN);
}

void ViewBaby_ClearImage() { if (cam_image_obj) lv_img_set_src(cam_image_obj, NULL); }

void ViewBaby_SetImage(const void* src) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    if (cam_image_obj) {
        lv_img_dsc_t* dsc = (lv_img_dsc_t*)src; 
        if (dsc->header.w > 0 && dsc->header.h > 0) {
            lv_img_set_src(cam_image_obj, src);
            if (vidFSMode) {
                float min_zoom = min(1280.0f / dsc->header.w, 720.0f / dsc->header.h); 
                lv_img_set_zoom(cam_image_obj, (uint16_t)(min_zoom * 256.0f));
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(cam_image_obj); 
            } else {
                float min_zoom = min(1.0f, min(1024.0f / dsc->header.w, 576.0f / dsc->header.h));
                lv_img_set_zoom(cam_image_obj, (uint16_t)(min_zoom * 256.0f)); 
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20); 
            }
            lv_obj_invalidate(cam_image_obj); 
            if (lbl_cam_status) lv_obj_add_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
        } else {
            if (lbl_cam_status) { lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(lbl_cam_status, "Fehler: Bildmasse 0x0"); }
            ViewBaby_StopStreamOnError();
        }
    }
}

void ViewBaby_SetStatus(const char* text) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    if (lbl_cam_status) { lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(lbl_cam_status, text); }
}

void ViewBaby_StopStreamOnError() {
    requestBabyStream = false; isStreamActive = false; 
    if (gui.getCurrentScreen() != SCREEN_BABY) return;
    if (lbl_play_icon) lv_obj_clear_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN);
    if (btn_audio) lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
    if (lbl_audio) lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
    AudioStreamLogic_StopBaby();
}

lv_obj_t* ViewBaby::build() {
    s_lastBtnColor = 0; s_lastBtnStateForText = -1;
    requestBabyStream = false; isStreamActive = false; 

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) return nullptr;
    lv_obj_add_event_cb(scr, view_baby_del_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x111111), 0);
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(scr);

    cam_image_obj = lv_img_create(scr); 
    lv_obj_set_size(cam_image_obj, 1024, 576); 
    lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20); 

    lbl_fps = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_fps, lv_color_hex(0x00FF00), 0);
    lv_obj_align_to(lbl_fps, cam_image_obj, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_obj_add_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN); 
    
    lbl_cam_bat_main = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_cam_bat_main, lv_color_white(), 0);
    lv_obj_align_to(lbl_cam_bat_main, cam_image_obj, LV_ALIGN_TOP_RIGHT, -15, 15);

    lbl_play_icon = lv_label_create(cam_image_obj);
    lv_label_set_text(lbl_play_icon, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(lbl_play_icon, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_text_color(lbl_play_icon, lv_color_white(), 0);
    lv_obj_set_style_bg_color(lbl_play_icon, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lbl_play_icon, 150, 0); 
    lv_obj_set_style_pad_all(lbl_play_icon, 30, 0);
    lv_obj_set_style_radius(lbl_play_icon, 80, 0); 
    lv_obj_align(lbl_play_icon, LV_ALIGN_CENTER, 0, 0); 

    lbl_cam_status = lv_label_create(scr);
    lv_label_set_text(lbl_cam_status, LV_SYMBOL_IMAGE " Klick fuer Stream");
    lv_obj_set_style_text_font(lbl_cam_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cam_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align_to(lbl_cam_status, cam_image_obj, LV_ALIGN_OUT_TOP_MID, 0, -10); 

    cam_touch_overlay = lv_obj_create(scr);
    lv_obj_set_size(cam_touch_overlay, 1024, 576);
    lv_obj_align(cam_touch_overlay, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(cam_touch_overlay, 0, 0); 
    lv_obj_set_style_border_width(cam_touch_overlay, 0, 0);
    lv_obj_add_flag(cam_touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(cam_touch_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_add_event_cb(cam_touch_overlay, cam_image_event_cb, LV_EVENT_ALL, NULL);

    btn_audio = lv_btn_create(scr);
    lv_obj_set_size(btn_audio, 100, 100); 
    lv_obj_align_to(btn_audio, cam_touch_overlay, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_add_flag(btn_audio, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_style_radius(btn_audio, 50, 0); 
    lv_obj_set_style_bg_color(btn_audio, lv_color_hex(0x555555), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_audio, lv_color_hex(0x00FF00), LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn_audio, btn_audio_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lbl_audio = lv_label_create(btn_audio);
    lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
    lv_obj_center(lbl_audio);

    btn_fs = lv_btn_create(scr);
    lv_obj_set_size(btn_fs, 100, 100); 
    lv_obj_align_to(btn_fs, cam_touch_overlay, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
    lv_obj_set_style_bg_color(btn_fs, lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(btn_fs, 50, 0); 
    lv_obj_add_event_cb(btn_fs, btn_fs_event_cb, LV_EVENT_CLICKED, NULL);
    lbl_fs = lv_label_create(btn_fs);
    lv_label_set_text(lbl_fs, "[ ]"); 
    lv_obj_center(lbl_fs);
    
    btn_settings = lv_btn_create(scr);
    lv_obj_set_size(btn_settings, 80, 80); 
    lv_obj_align(btn_settings, LV_ALIGN_TOP_RIGHT, -20, 90);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(btn_settings, 40, 0); 
    lv_obj_add_event_cb(btn_settings, btn_open_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_set = lv_label_create(btn_settings);
    lv_label_set_text(lbl_set, LV_SYMBOL_SETTINGS); 
    lv_obj_center(lbl_set);

    btn_ptt = lv_btn_create(scr);
    lv_obj_set_size(btn_ptt, 180, 80); // Groesser!
    lv_obj_align(btn_ptt, LV_ALIGN_BOTTOM_MID, -100, -20); 
    lv_obj_set_style_radius(btn_ptt, 20, 0); 
    lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x4FA5D6), 0);
    lv_obj_add_event_cb(btn_ptt, btn_ptt_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t * lbl_ptt = lv_label_create(btn_ptt);
    lv_label_set_text(lbl_ptt, LV_SYMBOL_MUTE " PTT");
    lv_obj_set_style_text_font(lbl_ptt, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_ptt);

    btn_mute = lv_btn_create(scr);
    lv_obj_set_size(btn_mute, 180, 80);
    lv_obj_align(btn_mute, LV_ALIGN_BOTTOM_MID, 100, -20); 
    lv_obj_set_style_radius(btn_mute, 20, 0); 
    lv_obj_add_event_cb(btn_mute, btn_mute_event_cb, LV_EVENT_CLICKED, NULL);
    lbl_mute = lv_label_create(btn_mute);
    lv_label_set_text(lbl_mute, "LADE...");
    lv_obj_set_style_text_font(lbl_mute, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_mute);

    fs_black_overlay = lv_obj_create(scr);
    lv_obj_set_size(fs_black_overlay, 1280, 720);
    lv_obj_center(fs_black_overlay);
    lv_obj_set_style_bg_color(fs_black_overlay, lv_color_black(), 0);
    lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN); 
    
    lbl_cam_bat_fs = lv_label_create(fs_black_overlay);
    lv_obj_set_style_text_color(lbl_cam_bat_fs, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_cam_bat_fs, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_cam_bat_fs, LV_ALIGN_TOP_RIGHT, -20, 20);

    // --- Settings Modal ---
    settings_modal = lv_obj_create(scr);
    lv_obj_set_size(settings_modal, 900, 600);
    lv_obj_center(settings_modal);
    lv_obj_set_flex_flow(settings_modal, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(settings_modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(settings_modal, LV_OBJ_FLAG_HIDDEN);

    create_setting_switch(settings_modal, "Blitz", &sw_flash);
    create_setting_switch(settings_modal, "SCR-Blitz", &sw_scrflash);
    create_setting_switch(settings_modal, "Screen On/Off", &sw_camscreen);
    create_setting_switch(settings_modal, "Cam On/Off", &sw_camstream);
    
    lv_obj_t* btn_lense = lv_btn_create(settings_modal);
    lv_obj_set_size(btn_lense, 180, 70);
    lv_obj_t* lbl_lense = lv_label_create(btn_lense); lv_label_set_text(lbl_lense, "Linse Wechseln"); lv_obj_center(lbl_lense);
    lv_obj_add_event_cb(btn_lense, btn_lense_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_pwr = lv_btn_create(settings_modal);
    lv_obj_set_size(btn_pwr, 180, 70);
    lv_obj_set_style_bg_color(btn_pwr, lv_color_hex(0xFF0000), 0);
    lv_obj_t* lbl_pwr = lv_label_create(btn_pwr); lv_label_set_text(lbl_pwr, "App Beenden"); lv_obj_center(lbl_pwr);
    lv_obj_add_event_cb(btn_pwr, btn_power_cb, LV_EVENT_CLICKED, NULL);

    create_setting_slider(settings_modal, "Zoom (0-100%)", &slider_zoom);
    create_setting_slider(settings_modal, "Quality (0-100%)", &slider_qual);
    create_setting_slider(settings_modal, "Speed (0-100%)", &slider_speed);

    dd_res = lv_dropdown_create(settings_modal);
    lv_obj_set_width(dd_res, 380);
    lv_obj_add_event_cb(dd_res, dd_res_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t* btn_close = lv_btn_create(settings_modal);
    lv_obj_set_size(btn_close, 800, 60);
    lv_obj_t* lbl_close = lv_label_create(btn_close); lv_label_set_text(lbl_close, "Schliessen"); lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e){ lv_obj_add_flag(settings_modal, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, NULL);

    // --- NEU: Batterie-Anzeige für die Kamera ---
    lbl_cam_battery = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_cam_battery, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cam_battery, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(lbl_cam_battery, cam_image_obj, LV_ALIGN_TOP_RIGHT, -15, 15);
    lv_obj_add_flag(lbl_cam_battery, LV_OBJ_FLAG_HIDDEN);

    return scr;
}

void ViewBaby::update() {
    if (gui.getCurrentScreen() != SCREEN_BABY) return;
    ViewTopbar_Update();

    if (!vidFSMode && fs_black_overlay != nullptr && !lv_obj_has_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
        ViewTopbar_SetHidden(false); 
    } else if (vidFSMode) {
        ViewTopbar_SetHidden(true); 
    }
    
    if (showFps && isStreamActive && lbl_fps != nullptr) {
        lv_obj_clear_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(lbl_fps, "FPS: %d", currentFps);
    } else if (lbl_fps != nullptr) {
        lv_obj_add_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN);
    }

    if (lbl_cam_bat_main) lv_label_set_text_fmt(lbl_cam_bat_main, LV_SYMBOL_BATTERY_FULL " %d%%", camBatteryPercent);
    if (lbl_cam_bat_fs) lv_label_set_text_fmt(lbl_cam_bat_fs, LV_SYMBOL_BATTERY_FULL " %d%%", camBatteryPercent);

    if (btn_audio != nullptr && lbl_audio != nullptr) {
        bool ui_is_on = lv_obj_has_state(btn_audio, LV_STATE_CHECKED);
        bool system_is_on = isAudioStreaming;
        if (ui_is_on && !system_is_on) {
            lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
            lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
        } else if (!ui_is_on && system_is_on) {
            lv_obj_add_state(btn_audio, LV_STATE_CHECKED);
            lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
        }
    }

    bool fastBlink = (millis() % 600 < 300);
    int curBtnState = babyAlarmActive ? (muteBaby ? 4 : 3) : (isBabyArmed ? 2 : 1);
    uint32_t targetBtnColor = 0x555555; const char* targetBtnText = "";

    switch(curBtnState) {
        case 1: targetBtnText = LV_SYMBOL_BELL " OFF"; targetBtnColor = 0x555555; break;
        case 2: targetBtnText = LV_SYMBOL_BELL " ON";  targetBtnColor = 0x00A0FF; break;
        case 3: targetBtnText = LV_SYMBOL_MUTE " MUTE"; targetBtnColor = fastBlink ? 0xFF0000 : 0x660000; break;
        case 4: targetBtnText = LV_SYMBOL_REFRESH " RESET"; targetBtnColor = 0xFF8800; break;
    }

    if (s_lastBtnStateForText != curBtnState && lbl_mute != nullptr) {
        lv_label_set_text(lbl_mute, targetBtnText);
        s_lastBtnStateForText = curBtnState;
    }
    if (s_lastBtnColor != targetBtnColor && btn_mute != nullptr) {
        lv_obj_set_style_bg_color(btn_mute, lv_color_hex(targetBtnColor), 0);
        s_lastBtnColor = targetBtnColor;
    }
}