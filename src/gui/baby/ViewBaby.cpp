#include "ViewBaby.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "SystemLogic.h" 
#include "AudioStreamLogic.h"
#include "ViewTopbar.h"
#include <WiFi.h>

static lv_obj_t * cam_image_obj = nullptr; 
static lv_obj_t * cam_touch_overlay = nullptr; 
static lv_obj_t * lbl_cam_status = nullptr;
static lv_obj_t * lbl_play_icon = nullptr; 

static lv_obj_t * btn_audio = nullptr;
static lv_obj_t * lbl_audio = nullptr;

static lv_obj_t * btn_fs = nullptr;
static lv_obj_t * lbl_fs = nullptr;

static lv_obj_t * lbl_fps = nullptr; 
static lv_obj_t * debug_panel = nullptr; 
static lv_obj_t * debug_label = nullptr;
static lv_obj_t * fs_black_overlay = nullptr; 
static lv_obj_t * btn_ptt = nullptr;
static lv_obj_t * btn_mute = nullptr;
static lv_obj_t * lbl_mute = nullptr;

static uint32_t s_lastBtnColor = 0; 
static int s_lastBtnStateForText = -1;

static void view_baby_del_cb(lv_event_t * e) {
    cam_image_obj = nullptr;
    cam_touch_overlay = nullptr;
    lbl_cam_status = nullptr;
    lbl_play_icon = nullptr;
    btn_audio = nullptr;
    lbl_audio = nullptr;
    btn_fs = nullptr;
    lbl_fs = nullptr;
    lbl_fps = nullptr;
    debug_panel = nullptr;
    debug_label = nullptr;
    fs_black_overlay = nullptr;
    btn_ptt = nullptr;
    btn_mute = nullptr;
    lbl_mute = nullptr;
}

static void btn_audio_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    
    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
        lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
        if (!isAudioStreaming) AudioStreamLogic_Start();
    } else {
        lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
        if (isAudioStreaming) AudioStreamLogic_Stop();
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
    if (fs_black_overlay) {
        lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (cam_image_obj) {
        const lv_img_dsc_t * dsc = (const lv_img_dsc_t *)lv_img_get_src(cam_image_obj);
        if (dsc != NULL) {
            lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
        }
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
            
            if (btn_audio != nullptr) lv_obj_add_state(btn_audio, LV_STATE_CHECKED);
            if (lbl_audio != nullptr) lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
            
        } else {
            lv_obj_clear_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
            lv_label_set_text(lbl_cam_status, LV_SYMBOL_PAUSE " Pausiert");
            
            if (btn_audio != nullptr) lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
            if (lbl_audio != nullptr) lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
        }
    } 
    else if (code == LV_EVENT_LONG_PRESSED && audioDebugEnabled) {
        playToneI2S(1000, 100, true); 
        if (debug_panel != nullptr) lv_obj_clear_flag(debug_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

// FIX: PTT Button und Mute Button Callbacks wiederhergestellt
static void btn_ptt_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_PRESSED) {
        playToneI2S(800, 100, true); 
        lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x00FF00), 0);
        lv_label_set_text(lv_obj_get_child(btn_ptt, 0), LV_SYMBOL_AUDIO " TALK");
    }
    else if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x4FA5D6), 0);
        lv_label_set_text(lv_obj_get_child(btn_ptt, 0), LV_SYMBOL_MUTE " PTT");
    }
}

static void btn_mute_event_cb(lv_event_t * e) { 
    playToneI2S(800, 100, true); 
    if (babyAlarmActive) {
        if (!babyMuted) babyMuted = true; 
        else { babyAlarmActive = false; babyMuted = false; }
    } else {
        isBabyArmed = !isBabyArmed;
    }
}

void ViewBaby_ClearImage() {
    if (cam_image_obj != nullptr) {
        lv_img_set_src(cam_image_obj, NULL);
    }
}

void ViewBaby_SetImage(const void* src) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    if (cam_image_obj != nullptr) {
        lv_img_dsc_t* dsc = (lv_img_dsc_t*)src; 
        if (dsc->header.w > 0 && dsc->header.h > 0) {
            lv_img_set_src(cam_image_obj, src);
            
            if (vidFSMode) {
                float zoom_x = 1280.0f / dsc->header.w;
                float zoom_y = 720.0f / dsc->header.h;
                float min_zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y; 
                lv_img_set_zoom(cam_image_obj, (uint16_t)(min_zoom * 256.0f));
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(cam_image_obj); 
            } else {
                float zoom_x = 1024.0f / dsc->header.w;
                float zoom_y = 576.0f / dsc->header.h;
                float min_zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y;
                if(min_zoom > 1.0f) min_zoom = 1.0f; 
                lv_img_set_zoom(cam_image_obj, (uint16_t)(min_zoom * 256.0f)); 
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20); 
            }
            lv_obj_invalidate(cam_image_obj); 
            if (lbl_cam_status != nullptr) lv_obj_add_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
        } else {
            if (lbl_cam_status != nullptr) { lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(lbl_cam_status, "Fehler: Bildmasse 0x0"); }
            ViewBaby_StopStreamOnError();
        }
    }
}

void ViewBaby_SetStatus(const char* text) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    if (lbl_cam_status != nullptr) { lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(lbl_cam_status, text); }
}

void ViewBaby_StopStreamOnError() {
    requestBabyStream = false; isStreamActive = false; 
    if (gui.getCurrentScreen() != SCREEN_BABY) return;
    
    if (lbl_play_icon != nullptr) lv_obj_clear_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN);
    
    if (btn_audio != nullptr) lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
    if (lbl_audio != nullptr) lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
}

lv_obj_t* ViewBaby::build() {
    s_lastBtnColor = 0; s_lastBtnStateForText = -1;
    requestBabyStream = false; isStreamActive = false; 

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) return nullptr;

    lv_obj_add_event_cb(scr, view_baby_del_cb, LV_EVENT_DELETE, NULL);

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(scr);

    cam_image_obj = lv_img_create(scr); 
    lv_obj_set_size(cam_image_obj, 1024, 576); 
    lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20); 

    lbl_fps = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_fps, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_fps, lv_color_hex(0x00FF00), 0);
    lv_obj_align_to(lbl_fps, cam_image_obj, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_obj_add_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN); 

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
    lv_label_set_long_mode(lbl_cam_status, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_cam_status, 600); 
    lv_obj_set_style_text_align(lbl_cam_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_cam_status, LV_SYMBOL_IMAGE " Klick fuer Stream");
    lv_obj_set_style_text_font(lbl_cam_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cam_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align_to(lbl_cam_status, cam_image_obj, LV_ALIGN_OUT_TOP_MID, 0, -10); 

    cam_touch_overlay = lv_obj_create(scr);
    lv_obj_set_size(cam_touch_overlay, 1024, 576);
    lv_obj_align(cam_touch_overlay, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(cam_touch_overlay, 0, 0); 
    lv_obj_set_style_border_width(cam_touch_overlay, 0, 0);
    lv_obj_clear_flag(cam_touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cam_touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(cam_touch_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_add_event_cb(cam_touch_overlay, cam_image_event_cb, LV_EVENT_ALL, NULL);

    btn_audio = lv_btn_create(scr);
    lv_obj_set_size(btn_audio, 100, 100); 
    lv_obj_align_to(btn_audio, cam_touch_overlay, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_add_flag(btn_audio, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(btn_audio, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_style_radius(btn_audio, 50, 0); 
    lv_obj_set_style_border_width(btn_audio, 0, 0);
    lv_obj_set_style_bg_color(btn_audio, lv_color_hex(0x555555), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_audio, 180, LV_STATE_DEFAULT); 
    lv_obj_set_style_bg_color(btn_audio, lv_color_hex(0x00FF00), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(btn_audio, LV_OPA_COVER, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(btn_audio, btn_audio_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_audio = lv_label_create(btn_audio);
    lv_obj_set_style_text_font(lbl_audio, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_audio, lv_color_white(), 0);
    lv_obj_center(lbl_audio);

    if (isAudioStreaming) {
        lv_obj_add_state(btn_audio, LV_STATE_CHECKED);
        lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
    } else {
        lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
        lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
    }

    btn_fs = lv_btn_create(scr);
    lv_obj_set_size(btn_fs, 100, 100); 
    lv_obj_align_to(btn_fs, cam_touch_overlay, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
    lv_obj_set_style_bg_color(btn_fs, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(btn_fs, 180, 0); 
    lv_obj_set_style_radius(btn_fs, 50, 0); 
    lv_obj_set_style_border_width(btn_fs, 0, 0);
    lv_obj_clear_flag(btn_fs, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(btn_fs, btn_fs_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_fs = lv_label_create(btn_fs);
    lv_label_set_text(lbl_fs, "[ ]"); 
    lv_obj_set_style_text_font(lbl_fs, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_fs, lv_color_white(), 0);
    lv_obj_center(lbl_fs);

    debug_panel = lv_obj_create(scr);
    lv_obj_set_size(debug_panel, 600, 400);
    lv_obj_align(debug_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(debug_panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(debug_panel, 220, 0); 
    lv_obj_add_flag(debug_panel, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_add_flag(debug_panel, LV_OBJ_FLAG_CLICKABLE); 
    
    lv_obj_add_event_cb(debug_panel, [](lv_event_t * e) {
        lv_obj_t * panel = (lv_obj_t *)lv_event_get_target(e);
        playToneI2S(600, 100, true); lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, NULL);
    
    debug_label = lv_label_create(debug_panel);
    lv_obj_set_width(debug_label, 560);
    lv_label_set_long_mode(debug_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(debug_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(debug_label, lv_color_hex(0x00FF00), 0);
    lv_label_set_text(debug_label, "Warte auf Logs...\n(Tippen zum Schliessen)");

    btn_ptt = lv_btn_create(scr);
    lv_obj_set_size(btn_ptt, 100, 40);
    lv_obj_align(btn_ptt, LV_ALIGN_BOTTOM_MID, -55, -35); 
    lv_obj_set_style_radius(btn_ptt, 20, 0); 
    lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x4FA5D6), 0);
    lv_obj_add_event_cb(btn_ptt, btn_ptt_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_t * lbl_ptt = lv_label_create(btn_ptt);
    lv_label_set_text(lbl_ptt, LV_SYMBOL_MUTE " PTT");
    lv_obj_center(lbl_ptt);

    btn_mute = lv_btn_create(scr);
    lv_obj_set_size(btn_mute, 100, 40);
    lv_obj_align(btn_mute, LV_ALIGN_BOTTOM_MID, 55, -35); 
    lv_obj_set_style_radius(btn_mute, 20, 0); 
    lv_obj_add_event_cb(btn_mute, btn_mute_event_cb, LV_EVENT_CLICKED, NULL);
    
    lbl_mute = lv_label_create(btn_mute);
    lv_label_set_text(lbl_mute, "LADE...");
    lv_obj_center(lbl_mute);

    fs_black_overlay = lv_obj_create(scr);
    lv_obj_set_size(fs_black_overlay, 1280, 720);
    lv_obj_center(fs_black_overlay);
    lv_obj_set_style_bg_color(fs_black_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(fs_black_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fs_black_overlay, 0, 0);
    lv_obj_clear_flag(fs_black_overlay, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN); 

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

    if (debug_panel != nullptr && !lv_obj_has_flag(debug_panel, LV_OBJ_FLAG_HIDDEN)) {
        String fullLog = "";
        for(int i=0; i<10; i++) {
            int idx = (audioLogIdx + i) % 10;
            if(audioLogs[idx].length() > 0) fullLog += audioLogs[idx] + "\n";
        }
        if (debug_label != nullptr) lv_label_set_text(debug_label, fullLog.c_str());
    }
    
    bool fastBlink = (millis() % 600 < 300);

    int curBtnState = babyAlarmActive ? (babyMuted ? 4 : 3) : (isBabyArmed ? 2 : 1);
    uint32_t targetBtnColor = 0x555555; const char* targetBtnText = "";

    switch(curBtnState) {
        case 1: targetBtnText = LV_SYMBOL_BELL " OFF"; targetBtnColor = 0x555555; break;
        case 2: targetBtnText = LV_SYMBOL_BELL " ON";  targetBtnColor = 0x00A0FF; break;
        case 3: targetBtnText = LV_SYMBOL_MUTE " MUTE";     targetBtnColor = fastBlink ? 0xFF0000 : 0x660000; break;
        case 4: targetBtnText = LV_SYMBOL_REFRESH " RESET";  targetBtnColor = 0xFF8800; break;
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