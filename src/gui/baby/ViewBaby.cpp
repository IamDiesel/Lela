#include "ViewBaby.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "SystemLogic.h" 
#include "AudioStreamLogic.h"
#include "BabyCamApi.h"
#include "BabyService.h"
#include "ViewTopbar.h"
#include "ViewBabySettings.h"
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
static lv_obj_t * fs_black_overlay = nullptr; 
static lv_obj_t * btn_ptt = nullptr;
static lv_obj_t * btn_mute = nullptr;
static lv_obj_t * lbl_mute = nullptr;
static lv_obj_t * btn_settings = nullptr;
static lv_obj_t * lbl_cam_bat_main = nullptr;
static lv_obj_t * lbl_cam_bat_fs = nullptr;

static uint32_t s_lastBtnColor = 0; 
static int s_lastBtnStateForText = -1;

static void view_baby_del_cb(lv_event_t * e) {
    // Die Ansicht ist weg - Video und Audio laufen nach dem gewaehlten Modus
    // weiter (BabyService). Nur Gegensprechen gehoert ausschliesslich hierher:
    // der PTT-Knopf existiert nicht mehr und kann kein RELEASED mehr liefern.
    BabyService_SetBabyViewVisible(false);
    AudioStreamLogic_StopPTT();
    pttSwipeLock = false;

    cam_image_obj = nullptr; cam_touch_overlay = nullptr; lbl_cam_status = nullptr;
    lbl_play_icon = nullptr; btn_audio = nullptr; lbl_audio = nullptr; btn_fs = nullptr;
    lbl_fs = nullptr; lbl_fps = nullptr; fs_black_overlay = nullptr; btn_ptt = nullptr;
    btn_mute = nullptr; lbl_mute = nullptr; btn_settings = nullptr;
    lbl_cam_bat_main = nullptr; lbl_cam_bat_fs = nullptr;
}

// Knopf und Symbol folgen dem Modus des BabyService, siehe syncModeControls().
static void btn_audio_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    BabyService_SetAudio(lv_obj_has_state(btn, LV_STATE_CHECKED));
}

static void btn_fs_event_cb(lv_event_t * e) {
    if (!isStreamActive) return; 
    playToneI2S(800, 100, true); 
    lv_obj_clear_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL); 
    vidFSMode = true;  
}

// Verlaesst den Vollbildmodus vollstaendig und ist mehrfach aufrufbar.
//
// Der Zustandswechsel gehoert HIERHER und nicht zum Aufrufer: frueher musste
// jeder Aufrufer `vidFSMode` selbst zuruecksetzen. Wer das vergass, liess die
// Oberflaeche in einem Zwischenzustand zurueck - schwarzes Vollbild ohne
// Bedienelemente und ohne Weg heraus.
void ViewBaby_ExitFS() {
    vidFSMode = false;

    if (fs_black_overlay) lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);

    if (cam_image_obj) {
        // Die Bildquelle bewusst loeschen statt ihre alten Masse zu uebernehmen.
        // Im Vollbild wird der Deskriptor nicht gepflegt (dort geht das Bild
        // direkt ueber den PPA), nach einem Aufloesungswechsel stuenden darin
        // also die Masse der VORIGEN Aufloesung. Das naechste Bild im
        // Fenstermodus setzt ihn ohnehin neu.
        lv_img_set_src(cam_image_obj, NULL);
        lv_img_set_zoom(cam_image_obj, 256); 
        lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20);
    }

    // Die Kopfleiste wird beim Betreten des Vollbilds ausgeblendet und muss
    // hier wieder erscheinen - sonst ist der Bildschirm bedienbar, sieht aber
    // weiter nach Vollbild aus.
    ViewTopbar_SetHidden(false);

    lv_obj_invalidate(lv_scr_act());
}

static void cam_image_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        playToneI2S(800, 100, true); 

        // Im Vollbild ist der Tipper die Geste zum Verlassen - nicht zum
        // Pausieren. Sonst gibt es im Vollbild keinen Weg zurueck, ohne
        // vorher den Stream abzuwuergen. Pausieren geht danach im
        // Normalmodus ueber denselben Tipper.
        if (vidFSMode) {
            ViewBaby_ExitFS();
            return;
        }

        // Wie bisher: Abspielen startet Bild UND Ton, Pausieren beendet
        // beides. Den Ton allein schaltet der Audio-Knopf.
        if (!BabyMode_HasVideo(BabyService_GetMode())) {
            BabyService_SetMode(BabyMode::VideoAudio);
            ViewBaby_SetStatus("Verbinde...");
        } else {
            BabyService_SetMode(BabyMode::Off);
            ViewBaby_SetStatus("Pausiert");
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

// Erster Druck quittiert den Alarm (Ton und Popup aus), der zweite setzt ihn
// zurueck. Die Mixer-Lautstaerke des Babyphones bleibt dabei unberuehrt.
static void btn_mute_event_cb(lv_event_t * e) { 
    playToneI2S(800, 100, true); 
    if (babyAlarmActive) {
        if (!babyAlarmAcked) babyAlarmAcked = true; 
        else { babyAlarmActive = false; babyAlarmAcked = false; }
    } else {
        isBabyArmed = !isBabyArmed;
    }
}

void ViewBaby_SetImage(const void* src) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    
    if (cam_image_obj) {
        lv_img_dsc_t* dsc = (lv_img_dsc_t*)src; 
        
        if (dsc->header.w > 0 && dsc->header.h > 0) {
            // Die Zoom-Skalierung (lv_img_set_zoom) wurde bereits performant 
            // im VideoLogic-Task angewendet! Wir setzen hier nur noch die Quelle.
            lv_img_set_src(cam_image_obj, src);
            
            if (vidFSMode) {
                // Im Fullscreen-Modus (falls LVGL überhaupt noch zeichnet)
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(cam_image_obj); 
            } else {
                // Im normalen UI-Modus
                lv_obj_set_size(cam_image_obj, dsc->header.w, dsc->header.h);
                lv_obj_align(cam_image_obj, LV_ALIGN_CENTER, 0, -20); 
            }
            
            lv_obj_invalidate(cam_image_obj); 
            
            if (lbl_cam_status) {
                lv_obj_add_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
            }
        } else {
            // Der Decoder laesst solche Bilder gar nicht durch (Probe vor dem
            // Dekodieren). Bleibt als Anzeige, falls sich das je aendert; der
            // Stream laeuft weiter - ein einzelnes kaputtes Bild ist kein
            // Grund, den Modus zu wechseln.
            ViewBaby_SetStatus("Fehler: Bildmasse 0x0");
        }
    }
}



static void setStatusLabel(const char* text) {
    if (lbl_cam_status) { 
        lv_obj_clear_flag(lbl_cam_status, LV_OBJ_FLAG_HIDDEN); 
        String padded = String("                                                                                ") + LV_SYMBOL_IMAGE + " " + text;
        lv_label_set_text(lbl_cam_status, padded.c_str()); 
    }
}

// Wird auch aus dem Video-Netz-Task gerufen (unter der LVGL-Sperre). Der
// Stream laeuft inzwischen auch, wenn die Baby-Ansicht nicht sichtbar ist -
// dann gibt es kein Ziel fuer die Meldung.
void ViewBaby_SetStatus(const char* text) {
    if (gui.getCurrentScreen() != SCREEN_BABY) return; 
    setStatusLabel(text);
}

// Gleicht Abspielsymbol und Audio-Knopf mit dem Modus des BabyService ab.
// Der Modus kann sich ausserhalb dieser Ansicht aendern (QuickSettings, MQTT,
// Alarm); deshalb wird er hier gelesen statt im Klick-Callback gesetzt.
static void syncModeControls() {
    const BabyMode m = BabyService_GetMode();

    if (lbl_play_icon) {
        const bool iconHidden = lv_obj_has_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN);
        if (BabyMode_HasVideo(m) != iconHidden) {
            if (BabyMode_HasVideo(m)) lv_obj_add_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN);
            else                      lv_obj_clear_flag(lbl_play_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (btn_audio && lbl_audio) {
        const bool checked = lv_obj_has_state(btn_audio, LV_STATE_CHECKED);
        if (BabyMode_HasAudio(m) != checked) {
            if (BabyMode_HasAudio(m)) {
                lv_obj_add_state(btn_audio, LV_STATE_CHECKED);
                lv_label_set_text(lbl_audio, LV_SYMBOL_VOLUME_MAX);
            } else {
                lv_obj_clear_state(btn_audio, LV_STATE_CHECKED);
                lv_label_set_text(lbl_audio, LV_SYMBOL_MUTE);
            }
        }
    }
}

lv_obj_t* ViewBaby::build() {
    s_lastBtnColor = 0; s_lastBtnStateForText = -1;

    // Der Modus bleibt beim Betreten erhalten - laeuft schon Video oder Ton,
    // geht es nahtlos weiter. Die Ansicht haelt die Kamerasitzung auch im
    // Modus "Aus" offen (Akkustand, Einstellungen).
    BabyService_SetBabyViewVisible(true);

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) return nullptr;
    lv_obj_add_event_cb(scr, view_baby_del_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x111111), 0);
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

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
    lv_obj_set_width(lbl_cam_status, 1280); 
    lv_label_set_long_mode(lbl_cam_status, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(lbl_cam_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cam_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_cam_status, LV_ALIGN_TOP_MID, 0, 85); 
    lv_label_set_text(lbl_cam_status, ""); 

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
    lv_obj_clear_flag(btn_audio, LV_OBJ_FLAG_GESTURE_BUBBLE); 
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
    lv_obj_clear_flag(btn_fs, LV_OBJ_FLAG_GESTURE_BUBBLE);
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
    lv_obj_add_event_cb(btn_settings, [](lv_event_t* e){ playToneI2S(800, 100, true); ViewBabySettings_Show(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_set = lv_label_create(btn_settings);
    lv_label_set_text(lbl_set, LV_SYMBOL_SETTINGS); 
    lv_obj_center(lbl_set);

    btn_ptt = lv_btn_create(scr);
    lv_obj_set_size(btn_ptt, 180, 80); 
    lv_obj_align(btn_ptt, LV_ALIGN_BOTTOM_MID, -100, -20); 
    lv_obj_set_style_radius(btn_ptt, 20, 0); 
    lv_obj_set_style_bg_color(btn_ptt, lv_color_hex(0x4FA5D6), 0);
    lv_obj_clear_flag(btn_ptt, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_add_event_cb(btn_ptt, btn_ptt_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t * lbl_ptt = lv_label_create(btn_ptt);
    lv_label_set_text(lbl_ptt, LV_SYMBOL_MUTE " PTT");
    lv_obj_set_style_text_font(lbl_ptt, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_ptt);

    btn_mute = lv_btn_create(scr);
    lv_obj_set_size(btn_mute, 180, 80);
    lv_obj_align(btn_mute, LV_ALIGN_BOTTOM_MID, 100, -20); 
    lv_obj_set_style_radius(btn_mute, 20, 0); 
    lv_obj_clear_flag(btn_mute, LV_OBJ_FLAG_GESTURE_BUBBLE); 
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
    
    // Der Tipper zum Verlassen darf nicht davon abhaengen, dass er zufaellig
    // im 1024x576 grossen Beruehrungsfeld des Fensterbetriebs landet: die
    // schwarze Flaeche liegt im Vollbild darueber und ist 1280x720 gross.
    // Ohne diesen Ausgang war das Vollbild eine Sackgasse, sobald der Tipper
    // ausserhalb dieses Bereichs kam.
    lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fs_black_overlay, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        if (!vidFSMode) return;
        playToneI2S(800, 100, true);
        ViewBaby_ExitFS();
    }, LV_EVENT_CLICKED, NULL);

    lbl_cam_bat_fs = lv_label_create(fs_black_overlay);
    lv_obj_set_style_text_color(lbl_cam_bat_fs, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_cam_bat_fs, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_cam_bat_fs, LV_ALIGN_TOP_RIGHT, -20, 20);

    ViewBabySettings_Create(scr);

    // Bewusst nicht ViewBaby_SetStatus(): waehrend build() ist die Ansicht
    // noch nicht als aktueller Bildschirm eingetragen.
    syncModeControls();
    if (BabyMode_HasVideo(BabyService_GetMode())) setStatusLabel("Verbinde...");

    return scr;
}


void ViewBaby::update() {
    if (gui.getCurrentScreen() != SCREEN_BABY) return;

    // --- NEU: Brutaler Cut für den Vollbildmodus ---
    // Wenn FS aktiv ist, lassen wir das PPA und VideoOverlay die ganze Arbeit machen.
    // Kein LVGL-Label-Update, keine UI-Invalidation!
    if (vidFSMode) {
        ViewTopbar_SetHidden(true); // <--- Direkter Aufruf (LVGL fängt redundante Aufrufe intern ab)
        return; 
    }

    // --- Ab hier läuft nur noch der Windowed-Mode ---
    if (fs_black_overlay != nullptr && !lv_obj_has_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(fs_black_overlay, LV_OBJ_FLAG_HIDDEN);
        ViewTopbar_SetHidden(false); 
    }
    
    ViewTopbar_Update();

    if (showFps && isStreamActive && lbl_fps != nullptr) {
        lv_obj_clear_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(lbl_fps, "FPS: %d", currentFps);
    } else if (lbl_fps != nullptr) {
        lv_obj_add_flag(lbl_fps, LV_OBJ_FLAG_HIDDEN);
    }

    if (lbl_cam_bat_main) lv_label_set_text_fmt(lbl_cam_bat_main, LV_SYMBOL_BATTERY_FULL " %d%%", camBatteryPercent);
    // lbl_cam_bat_fs updaten wir gar nicht mehr, das macht dein VideoOverlay!

    syncModeControls();

    // Sicherheitsventil: Video wegen knappen Speichers pausiert. Einmal beim
    // Eintreten melden; das Fortsetzen meldet der Video-Task selbst
    // ("Verbinde...").
    static bool s_lastMemPaused = false;
    const bool memPaused = BabyService_IsVideoMemoryPaused();
    if (memPaused && !s_lastMemPaused) setStatusLabel("Video pausiert - Speicher knapp");
    s_lastMemPaused = memPaused;

    bool fastBlink = (millis() % 600 < 300);
    int curBtnState = babyAlarmActive ? (babyAlarmAcked ? 4 : 3) : (isBabyArmed ? 2 : 1);
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
    
    ViewBabySettings_Update();
}
