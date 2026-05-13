#include "ViewSettings.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "SystemLogic.h"
#include "BleLogic.h"
#include "LVGL_Driver.h"
#include <WiFi.h>

#define TEXT_COLOR (isDarkMode ? lv_color_white() : lv_color_black())

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

// ==============================================================
// GLOABLE UI OBJEKTE
// ==============================================================
static lv_obj_t * label_thr_val, * label_time_val, * label_mjpeg_drop_val, * label_cam_ref_val; 
static lv_obj_t * sl_master, * sl_slave, * lbl_master, * lbl_slave;
static lv_obj_t * lbl_sw_wifi, * lbl_sw_mat, * lbl_sw_kippy;
static lv_obj_t * lbl_setup_mat, * lbl_setup_kip;
static lv_obj_t * text_ble_info, * text_sys_info;

static lv_obj_t * qr_overlay, * qr_screenshot, * btn_stop_screenshot, * lbl_qr_ip; 
static lv_obj_t * setup_overlay, * setup_qr, * btn_cancel_setup, * lbl_setup_qr; 

static lv_obj_t * scan_overlay, * scan_spinner, * lbl_scan_info;
static lv_obj_t * btn_save_mac, * btn_cancel_mac, * btn_rescan_mac, * btn_continue_mac; 

// Listen-System Variablen
static lv_obj_t * scan_list = nullptr;
static lv_obj_t * scan_list_btns[MAX_SCAN_DEVICES];
static lv_obj_t * scan_list_labels[MAX_SCAN_DEVICES];
static int selected_scan_index = -1;

static lv_obj_t * stream_overlay = nullptr, * sw_expert = nullptr;
static lv_obj_t * ta_ip, * ta_vid, * ta_aud, * lbl_ip, * lbl_vid, * lbl_aud;

static const int cam_intervals[] = {10, 20, 30, 50, 75, 100, 200, 300, 500, 750, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};

// ==============================================================
// HELPER FUNKTIONEN
// ==============================================================
static void update_sliders_ui() {
    if (!matEnabled || (!kippyEnabled && !wifiEnabled)) { lv_obj_add_state(sl_master, LV_STATE_DISABLED); lv_obj_set_style_text_color(lbl_master, lv_color_hex(0x555555), 0); } 
    else { lv_obj_clear_state(sl_master, LV_STATE_DISABLED); lv_obj_set_style_text_color(lbl_master, TEXT_COLOR, 0); }
    if (!kippyEnabled || !wifiEnabled) { lv_obj_add_state(sl_slave, LV_STATE_DISABLED); lv_obj_set_style_text_color(lbl_slave, lv_color_hex(0x555555), 0); } 
    else { lv_obj_clear_state(sl_slave, LV_STATE_DISABLED); lv_obj_set_style_text_color(lbl_slave, TEXT_COLOR, 0); }
}

static lv_obj_t* create_text_label(lv_obj_t* parent, const char* text) { lv_obj_t * lbl = lv_label_create(parent); lv_label_set_text(lbl, text); lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0); lv_obj_set_style_text_color(lbl, TEXT_COLOR, 0); return lbl; }
static lv_obj_t* create_white_label(lv_obj_t* parent, const char* text) { lv_obj_t * lbl = lv_label_create(parent); lv_label_set_text(lbl, text); lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0); lv_obj_set_style_text_color(lbl, lv_color_white(), 0); return lbl; }
static lv_obj_t* create_header(lv_obj_t* parent, const char* text) { lv_obj_t * header = lv_label_create(parent); lv_label_set_text(header, text); lv_obj_set_style_text_font(header, &lv_font_montserrat_28, 0); lv_obj_set_style_text_color(header, lv_color_hex(0x00A0FF), 0); lv_obj_set_style_pad_top(header, 30, 0); return header; }
static lv_obj_t* create_helper_cont(lv_obj_t* parent, int height) { lv_obj_t * cont = lv_obj_create(parent); lv_obj_set_size(cont, 800, height); lv_obj_set_style_bg_opa(cont, 0, 0); lv_obj_set_style_border_width(cont, 0, 0); lv_obj_set_style_pad_all(cont, 0, 0); lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE); return cont; }

// ==============================================================
// CALLBACKS (Aktionen)
// ==============================================================
static void stream_expert_sw_cb(lv_event_t* e) {
    bool expert = lv_obj_has_state(sw_expert, LV_STATE_CHECKED);
    if (expert) { lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ta_ip, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lbl_vid, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(ta_vid, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(lbl_aud, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(ta_aud, LV_OBJ_FLAG_HIDDEN); } 
    else { lv_obj_clear_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(ta_ip, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lbl_vid, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ta_vid, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lbl_aud, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ta_aud, LV_OBJ_FLAG_HIDDEN); }
}

static void showStreamSettingsPopup() {
    if (stream_overlay != nullptr) return;
    playToneI2S(800, 100, true);

    stream_overlay = lv_obj_create(lv_disp_get_layer_sys(NULL));
    lv_obj_set_size(stream_overlay, 1280, 720);
    lv_obj_set_style_bg_color(stream_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(stream_overlay, 200, 0);
    lv_obj_add_flag(stream_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* panel = lv_obj_create(stream_overlay);
    lv_obj_set_size(panel, 800, 400);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);

    lv_obj_t* title = create_white_label(panel, "Stream Verbindung"); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t* lbl_exp = create_white_label(panel, "Expertenmodus (Ganze URL bearbeiten)"); lv_obj_align(lbl_exp, LV_ALIGN_TOP_LEFT, 20, 70);

    sw_expert = lv_switch_create(panel); lv_obj_align(sw_expert, LV_ALIGN_TOP_LEFT, 430, 65);
    if (useCustomUrls) lv_obj_add_state(sw_expert, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_expert, stream_expert_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_ip = create_white_label(panel, "Kamera IP-Adresse (z.B. 192.168.2.86):"); lv_obj_align(lbl_ip, LV_ALIGN_TOP_LEFT, 20, 130);
    ta_ip = lv_textarea_create(panel); lv_obj_set_size(ta_ip, 700, 50); lv_obj_align(ta_ip, LV_ALIGN_TOP_LEFT, 20, 160); lv_textarea_set_one_line(ta_ip, true); lv_textarea_set_text(ta_ip, streamIp.c_str());

    lbl_vid = create_white_label(panel, "Video Stream URL:"); lv_obj_align(lbl_vid, LV_ALIGN_TOP_LEFT, 20, 120);
    ta_vid = lv_textarea_create(panel); lv_obj_set_size(ta_vid, 700, 50); lv_obj_align(ta_vid, LV_ALIGN_TOP_LEFT, 20, 150); lv_textarea_set_one_line(ta_vid, true); lv_textarea_set_text(ta_vid, camEntity.c_str());

    lbl_aud = create_white_label(panel, "Audio Stream URL:"); lv_obj_align(lbl_aud, LV_ALIGN_TOP_LEFT, 20, 220);
    ta_aud = lv_textarea_create(panel); lv_obj_set_size(ta_aud, 700, 50); lv_obj_align(ta_aud, LV_ALIGN_TOP_LEFT, 20, 250); lv_textarea_set_one_line(ta_aud, true); lv_textarea_set_text(ta_aud, babyStreamUrl.c_str());

    lv_obj_t* kb = lv_keyboard_create(stream_overlay); lv_obj_set_size(kb, 1280, 300); lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0); lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    auto ta_event_cb = [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_FOCUSED) { lv_keyboard_set_textarea((lv_obj_t*)lv_event_get_user_data(e), lv_event_get_target(e)); lv_obj_clear_flag((lv_obj_t*)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN); } 
        else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) { lv_obj_add_flag((lv_obj_t*)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN); }
    };
    lv_obj_add_event_cb(ta_ip, ta_event_cb, LV_EVENT_ALL, kb); lv_obj_add_event_cb(ta_vid, ta_event_cb, LV_EVENT_ALL, kb); lv_obj_add_event_cb(ta_aud, ta_event_cb, LV_EVENT_ALL, kb);
    lv_obj_add_event_cb(kb, [](lv_event_t* e){ if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL) { if (lv_keyboard_get_textarea(lv_event_get_target(e))) lv_obj_clear_state(lv_keyboard_get_textarea(lv_event_get_target(e)), LV_STATE_FOCUSED); lv_obj_add_flag(lv_event_get_target(e), LV_OBJ_FLAG_HIDDEN); } }, LV_EVENT_ALL, NULL);
    stream_expert_sw_cb(NULL);

    lv_obj_t* btn_save = lv_btn_create(panel); lv_obj_set_size(btn_save, 200, 60); lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -20); lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x27AE60), 0);
    lv_obj_t* l_s = create_white_label(btn_save, "Speichern"); lv_obj_center(l_s);
    lv_obj_add_event_cb(btn_save, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        useCustomUrls = lv_obj_has_state(sw_expert, LV_STATE_CHECKED);
        if (!useCustomUrls) { streamIp = lv_textarea_get_text(ta_ip); streamIp.trim(); camEntity = "http://" + streamIp + ":8080/api-stream/v1/video?random=0"; babyStreamUrl = "http://" + streamIp + ":8080/api-stream/v1/audio"; } 
        else { camEntity = lv_textarea_get_text(ta_vid); camEntity.trim(); babyStreamUrl = lv_textarea_get_text(ta_aud); babyStreamUrl.trim(); }
        preferences.begin("catmat", false); preferences.putBool("useCstUrl", useCustomUrls); preferences.putString("strIp", streamIp); preferences.putString("camEntity", camEntity); preferences.putString("babyUrl", babyStreamUrl); preferences.end();
        lv_obj_del_async(stream_overlay); stream_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_cancel = lv_btn_create(panel); lv_obj_set_size(btn_cancel, 200, 60); lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -20); lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xAA0000), 0);
    lv_obj_t* l_c = create_white_label(btn_cancel, "Abbrechen"); lv_obj_center(l_c);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) { playToneI2S(600, 100, true); lv_obj_del_async(stream_overlay); stream_overlay = nullptr; }, LV_EVENT_CLICKED, NULL);
}

// Handler fuer die interaktiven Zeilen in der Bluetooth-Liste
static void scan_list_btn_cb(lv_event_t * e) {
    playToneI2S(800, 50, true);
    lv_obj_t * clicked_btn = (lv_obj_t*)lv_event_get_target(e);
    selected_scan_index = (int)(intptr_t)lv_event_get_user_data(e);

    // Alle Buttons zuruecksetzen
    for(int i = 0; i < MAX_SCAN_DEVICES; i++) {
        if(scan_list_btns[i]) {
            lv_obj_set_style_bg_color(scan_list_btns[i], lv_color_hex(0x222222), 0);
        }
    }
    // Geklickten Button blau hervorheben
    lv_obj_set_style_bg_color(clicked_btn, lv_color_hex(0x00A0FF), 0);
}

static void btn_scan_mat_cb(lv_event_t * e) { playToneI2S(800, 100, true); selected_scan_index = -1; BleLogic_StartSetupScan(1, true); lv_label_set_text(lbl_scan_info, "Suche Matte..."); lv_obj_clear_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(scan_overlay); }
static void btn_scan_kip_cb(lv_event_t * e) { playToneI2S(800, 100, true); selected_scan_index = -1; BleLogic_StartSetupScan(2, true); lv_label_set_text(lbl_scan_info, "Suche Tracker..."); lv_obj_clear_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(scan_overlay); }
static void btn_rescan_mac_cb(lv_event_t * e) { playToneI2S(800, 100, true); selected_scan_index = -1; BleLogic_StartSetupScan(setupScanMode, true); }
static void btn_continue_mac_cb(lv_event_t * e) { playToneI2S(800, 100, true); if (!isSetupScanning) BleLogic_StartSetupScan(setupScanMode, false); }
static void btn_cancel_mac_cb(lv_event_t * e) { playToneI2S(600, 100, true); BleLogic_CloseSetupWindow(); lv_obj_add_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN); }

static void btn_save_mac_cb(lv_event_t * e) { 
    if (selected_scan_index >= 0) {
        playToneI2S(800, 100, true); 
        BleLogic_SaveDevice(selected_scan_index); 
        lv_obj_add_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN); 
        lv_label_set_text_fmt(lbl_setup_mat, "Matte:\n%s", savedMatMac.c_str());
        lv_label_set_text_fmt(lbl_setup_kip, "Kippy:\n%s", savedKippyMac.c_str());
    } else {
        playToneI2S(400, 150, true); // Fehler-Ton, wenn nichts gewaehlt wurde
    }
}

// ==============================================================
// MODULARE UI BUILDER FUNKTIONEN
// ==============================================================
static void buildSensorSection(lv_obj_t* parent) {
    create_header(parent, LV_SYMBOL_SETTINGS " SENSOR & GRAPH");
    
    label_thr_val = create_text_label(parent, ""); lv_label_set_text_fmt(label_thr_val, "Limit: %d Digits", thresholdVal);
    lv_obj_t * slider_thr = lv_slider_create(parent); lv_obj_set_size(slider_thr, 600, 20); lv_slider_set_range(slider_thr, 50, 600); lv_slider_set_value(slider_thr, thresholdVal, LV_ANIM_OFF); 
    lv_obj_add_event_cb(slider_thr, [](lv_event_t* e){ playToneI2S(1000, 50, true); int val = (lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)) / 10) * 10; lv_slider_set_value((lv_obj_t*)lv_event_get_target(e), val, LV_ANIM_OFF); thresholdVal = val; lv_label_set_text_fmt(label_thr_val, "Limit: %d Digits", thresholdVal); preferences.begin("catmat", false); preferences.putInt("thr", thresholdVal); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);

    label_time_val = create_text_label(parent, ""); lv_label_set_text_fmt(label_time_val, "Graph Dauer: %d s", graphTimeSeconds);
    lv_obj_t * slider_time = lv_slider_create(parent); lv_obj_set_size(slider_time, 600, 20); lv_slider_set_range(slider_time, 10, 900); lv_slider_set_value(slider_time, graphTimeSeconds, LV_ANIM_OFF); 
    lv_obj_add_event_cb(slider_time, [](lv_event_t* e){ playToneI2S(1000, 50, true); int val = (lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)) / 5) * 5; lv_slider_set_value((lv_obj_t*)lv_event_get_target(e), val, LV_ANIM_OFF); graphTimeSeconds = val; lv_label_set_text_fmt(label_time_val, "Graph Dauer: %d s", graphTimeSeconds); requestChartUpdate = true; preferences.begin("catmat", false); preferences.putInt("gtime", graphTimeSeconds); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_tx = create_helper_cont(parent, 60); lv_obj_t * label_time_x = create_text_label(cont_tx, "X-Achse: Uhrzeit"); lv_obj_align(label_time_x, LV_ALIGN_LEFT_MID, 20, 0); 
    lv_obj_t * switch_time_x = lv_switch_create(cont_tx); lv_obj_align(switch_time_x, LV_ALIGN_RIGHT_MID, -20, 0); if(showTimeOnX) lv_obj_add_state(switch_time_x, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(switch_time_x, [](lv_event_t* e){ playToneI2S(800, 100, true); showTimeOnX = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("timeX", showTimeOnX); preferences.end(); requestChartUpdate = true; }, LV_EVENT_VALUE_CHANGED, NULL);
    
    create_text_label(parent, "Graph Datenquelle:"); 
    lv_obj_t * dd_graph_mode = lv_dropdown_create(parent); lv_obj_set_width(dd_graph_mode, 600); lv_obj_set_style_text_font(dd_graph_mode, &lv_font_montserrat_24, 0); lv_dropdown_set_options(dd_graph_mode, "Druckwert\nBLE RSSI\nWLAN RSSI\nBLE Intervall\nKippy RSSI"); lv_dropdown_set_selected(dd_graph_mode, currentGraphMode); 
    lv_obj_add_event_cb(dd_graph_mode, [](lv_event_t* e){ playToneI2S(800, 100, true); currentGraphMode = lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e)); historyIdx = 0; historyCount = 0; for(int i=0; i<HISTORY_SIZE; i++) pressureHistory[i] = -32000; requestChartUpdate = true; preferences.begin("catmat", false); preferences.putInt("gMode", currentGraphMode); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * btn_tara = lv_btn_create(parent); lv_obj_set_size(btn_tara, 400, 60); lv_obj_set_style_bg_color(btn_tara, lv_color_hex(0x333333), 0); 
    lv_obj_add_event_cb(btn_tara, [](lv_event_t* e){ playToneI2S(800, 100, true); taraOffset = rawPressure; preferences.begin("catmat", false); preferences.putUInt("off", taraOffset); preferences.end(); }, LV_EVENT_CLICKED, NULL); 
    lv_obj_t * label_tara = create_white_label(btn_tara, "TARA KALIBRIEREN"); lv_obj_center(label_tara);
}

static void buildVideoSection(lv_obj_t* parent) {
    create_header(parent, LV_SYMBOL_VIDEO " BABY-MONITOR & AUDIO");
    
    lv_obj_t * btn_stream_set = lv_btn_create(parent); lv_obj_set_size(btn_stream_set, 400, 60); lv_obj_set_style_bg_color(btn_stream_set, lv_color_hex(0x8E44AD), 0); 
    lv_obj_add_event_cb(btn_stream_set, [](lv_event_t* e){ showStreamSettingsPopup(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_stream_set = create_white_label(btn_stream_set, LV_SYMBOL_EDIT " STREAM VERBINDUNG"); lv_obj_center(l_stream_set);

    label_cam_ref_val = create_text_label(parent, ""); if (cameraRefreshMs < 1000) lv_label_set_text_fmt(label_cam_ref_val, "Update Intervall: %d ms", cameraRefreshMs); else lv_label_set_text_fmt(label_cam_ref_val, "Update Intervall: %d Sek", cameraRefreshMs / 1000);
    int start_idx = 10; for (int i=0; i<20; i++) { if (cameraRefreshMs <= cam_intervals[i]) { start_idx = i; break; } }
    lv_obj_t * slider_cam_ref = lv_slider_create(parent); lv_obj_set_size(slider_cam_ref, 600, 20); lv_slider_set_range(slider_cam_ref, 0, 19); lv_slider_set_value(slider_cam_ref, start_idx, LV_ANIM_OFF); 
    lv_obj_add_event_cb(slider_cam_ref, [](lv_event_t* e){ playToneI2S(1000, 50, true); int idx = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); cameraRefreshMs = cam_intervals[idx]; if (cameraRefreshMs < 1000) lv_label_set_text_fmt(label_cam_ref_val, "Update Intervall: %d ms", cameraRefreshMs); else lv_label_set_text_fmt(label_cam_ref_val, "Update Intervall: %d Sek", cameraRefreshMs / 1000); }, LV_EVENT_VALUE_CHANGED, NULL); 
    lv_obj_add_event_cb(slider_cam_ref, [](lv_event_t* e){ preferences.begin("catmat", false); preferences.putInt("camRef", cameraRefreshMs); preferences.end(); }, LV_EVENT_RELEASED, NULL);

    label_mjpeg_drop_val = create_text_label(parent, ""); if (mjpegDropThreshold == 0) lv_label_set_text(label_mjpeg_drop_val, "Latenz-Drop: Aggressiv (0 KB)"); else lv_label_set_text_fmt(label_mjpeg_drop_val, "Latenz-Drop: %d KB", mjpegDropThreshold / 1024);
    lv_obj_t * slider_mjpeg_drop = lv_slider_create(parent); lv_obj_set_size(slider_mjpeg_drop, 600, 20); lv_slider_set_range(slider_mjpeg_drop, 0, 16); lv_slider_set_value(slider_mjpeg_drop, mjpegDropThreshold / 1024, LV_ANIM_OFF); 
    lv_obj_add_event_cb(slider_mjpeg_drop, [](lv_event_t* e){ playToneI2S(1000, 50, true); int val = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); mjpegDropThreshold = val * 1024; if (val == 0) lv_label_set_text(label_mjpeg_drop_val, "Latenz-Drop: Aggressiv (0 KB)"); else lv_label_set_text_fmt(label_mjpeg_drop_val, "Latenz-Drop: %d KB", val); }, LV_EVENT_VALUE_CHANGED, NULL); 
    lv_obj_add_event_cb(slider_mjpeg_drop, [](lv_event_t* e){ preferences.begin("catmat", false); preferences.putInt("mjDrop", mjpegDropThreshold); preferences.end(); }, LV_EVENT_RELEASED, NULL);

    create_text_label(parent, "Audio-Format (Qualitaet):"); 
    lv_obj_t * dd_audio_fmt = lv_dropdown_create(parent); lv_obj_set_width(dd_audio_fmt, 600); lv_obj_set_style_text_font(dd_audio_fmt, &lv_font_montserrat_24, 0); lv_dropdown_set_options(dd_audio_fmt, "0: Unkomprimiert (PCM/WAV)\n1: Komprimiert (AAC)"); lv_dropdown_set_selected(dd_audio_fmt, audioFormat); 
    lv_obj_add_event_cb(dd_audio_fmt, [](lv_event_t* e){ playToneI2S(800, 100, true); audioFormat = lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e)); Preferences prefs; prefs.begin("catmat", false); prefs.putInt("audioFmt", audioFormat); prefs.end(); }, LV_EVENT_VALUE_CHANGED, NULL);

    create_text_label(parent, "Kamera Aufloesung (Hack):"); 
    lv_obj_t * dd_cam_hack = lv_dropdown_create(parent); lv_obj_set_width(dd_cam_hack, 600); lv_obj_set_style_text_font(dd_cam_hack, &lv_font_montserrat_24, 0); lv_dropdown_set_options(dd_cam_hack, "0: Deaktiviert\n1: 320x240\n2: 480x360 (16:9)\n3: 640x360\n4: 640x480\n5: 800x450\n6: 800x600\n7: 1024x768\n8: 1280x720 (HD)\n9: 1280x960"); lv_dropdown_set_selected(dd_cam_hack, camHackMode); 
    lv_obj_add_event_cb(dd_cam_hack, [](lv_event_t* e){ playToneI2S(800, 100, true); camHackMode = lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e)); Preferences prefs; prefs.begin("catmat", false); prefs.putInt("camHackM", camHackMode); prefs.end(); }, LV_EVENT_VALUE_CHANGED, NULL);
}

static void buildSystemSection(lv_obj_t* parent) {
    create_header(parent, LV_SYMBOL_LIST " SYSTEM & OPTIK");
    
    lv_obj_t * cont_ad = create_helper_cont(parent, 60); lv_obj_t * label_ad = create_text_label(cont_ad, "Audio Debug Overlay"); lv_obj_align(label_ad, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * switch_ad = lv_switch_create(cont_ad); lv_obj_align(switch_ad, LV_ALIGN_RIGHT_MID, -20, 0); if(audioDebugEnabled) lv_obj_add_state(switch_ad, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(switch_ad, [](lv_event_t* e){ playToneI2S(800, 100, true); audioDebugEnabled = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("audDbg", audioDebugEnabled); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_fps = create_helper_cont(parent, 60); lv_obj_t * lbl_fps_title = create_text_label(cont_fps, "Video FPS Anzeige"); lv_obj_align(lbl_fps_title, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_fps = lv_switch_create(cont_fps); lv_obj_align(sw_fps, LV_ALIGN_RIGHT_MID, -20, 0); if(showFps) lv_obj_add_state(sw_fps, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_fps, [](lv_event_t* e) { showFps = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); Preferences prefs; prefs.begin("catmat", false); prefs.putBool("showFps", showFps); prefs.end(); playToneI2S(800, 100, true); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_dm = create_helper_cont(parent, 60); lv_obj_t * label_dark = create_text_label(cont_dm, "Dark Mode"); lv_obj_align(label_dark, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * switch_dark = lv_switch_create(cont_dm); lv_obj_align(switch_dark, LV_ALIGN_RIGHT_MID, -20, 0); if(isDarkMode) lv_obj_add_state(switch_dark, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(switch_dark, [](lv_event_t* e){ playToneI2S(800, 100, true); isDarkMode = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("dark", isDarkMode); preferences.end(); gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_MOVE_BOTTOM); }, LV_EVENT_VALUE_CHANGED, NULL);
}

static void buildNetworkSection(lv_obj_t* parent) {
    create_header(parent, LV_SYMBOL_WIFI " FUNKVERBINDUNGEN");
    
    lv_obj_t * cont_wlan = create_helper_cont(parent, 60); lbl_sw_wifi = create_text_label(cont_wlan, LV_SYMBOL_WIFI " WLAN"); lv_obj_align(lbl_sw_wifi, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_wifi = lv_switch_create(cont_wlan); lv_obj_align(sw_wifi, LV_ALIGN_RIGHT_MID, -20, 0); if(wifiEnabled) lv_obj_add_state(sw_wifi, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_wifi, [](lv_event_t* e){ playToneI2S(800, 100, true); wifiEnabled = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("wifiEn", wifiEnabled); preferences.end(); calcMultiplex(); update_sliders_ui(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_mqtt = create_helper_cont(parent, 60); lv_obj_t * lbl_sw_mqtt = create_text_label(cont_mqtt, LV_SYMBOL_LIST " MQTT"); lv_obj_align(lbl_sw_mqtt, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_mqtt = lv_switch_create(cont_mqtt); lv_obj_align(sw_mqtt, LV_ALIGN_RIGHT_MID, -20, 0); if(mqttEnabled) lv_obj_add_state(sw_mqtt, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_mqtt, [](lv_event_t* e){ playToneI2S(800, 100, true); mqttEnabled = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("mqttEn", mqttEnabled); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_mat = create_helper_cont(parent, 60); lbl_sw_mat = create_text_label(cont_mat, LV_SYMBOL_BLUETOOTH " BT Matte"); lv_obj_align(lbl_sw_mat, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_mat = lv_switch_create(cont_mat); lv_obj_align(sw_mat, LV_ALIGN_RIGHT_MID, -20, 0); if(matEnabled) lv_obj_add_state(sw_mat, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_mat, [](lv_event_t* e){ playToneI2S(800, 100, true); matEnabled = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("matEn", matEnabled); preferences.end(); calcMultiplex(); update_sliders_ui(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_kip = create_helper_cont(parent, 60); lbl_sw_kippy = create_text_label(cont_kip, LV_SYMBOL_BLUETOOTH " BT Kippy"); lv_obj_align(lbl_sw_kippy, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_kippy = lv_switch_create(cont_kip); lv_obj_align(sw_kippy, LV_ALIGN_RIGHT_MID, -20, 0); if(kippyEnabled) lv_obj_add_state(sw_kippy, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_kippy, [](lv_event_t* e){ playToneI2S(800, 100, true); kippyEnabled = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("kipEn", kippyEnabled); preferences.end(); calcMultiplex(); update_sliders_ui(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * cont_mqtt_mat = create_helper_cont(parent, 60); lv_obj_t * lbl_sw_mqtt_mat = create_text_label(cont_mqtt_mat, LV_SYMBOL_BLUETOOTH " Daten via Proxy"); lv_obj_align(lbl_sw_mqtt_mat, LV_ALIGN_LEFT_MID, 20, 0); lv_obj_t * sw_mqtt_mat = lv_switch_create(cont_mqtt_mat); lv_obj_align(sw_mqtt_mat, LV_ALIGN_RIGHT_MID, -20, 0); if(useMqttForMat) lv_obj_add_state(sw_mqtt_mat, LV_STATE_CHECKED); 
    lv_obj_add_event_cb(sw_mqtt_mat, [](lv_event_t* e){ playToneI2S(800, 100, true); useMqttForMat = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED); preferences.begin("catmat", false); preferences.putBool("useMqtt", useMqttForMat); preferences.end(); }, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_master = create_text_label(parent, ""); lv_label_set_text_fmt(lbl_master, "Prio Matte: %d%%", prioMaster); sl_master = lv_slider_create(parent); lv_obj_set_size(sl_master, 600, 20); lv_slider_set_range(sl_master, 0, 100); lv_slider_set_value(sl_master, prioMaster, LV_ANIM_OFF); 
    lv_obj_add_event_cb(sl_master, [](lv_event_t* e){ playToneI2S(1000, 50, true); prioMaster = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); lv_label_set_text_fmt(lbl_master, "Prio Matte: %d%%", prioMaster); preferences.begin("catmat", false); preferences.putInt("prioM", prioMaster); preferences.end(); calcMultiplex(); }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lbl_slave = create_text_label(parent, ""); lv_label_set_text_fmt(lbl_slave, "Split Kippy/WLAN: %d%%", prioSlave); sl_slave = lv_slider_create(parent); lv_obj_set_size(sl_slave, 600, 20); lv_slider_set_range(sl_slave, 0, 100); lv_slider_set_value(sl_slave, prioSlave, LV_ANIM_OFF); 
    lv_obj_add_event_cb(sl_slave, [](lv_event_t* e){ playToneI2S(1000, 50, true); prioSlave = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)); lv_label_set_text_fmt(lbl_slave, "Split Kippy/WLAN: %d%%", prioSlave); preferences.begin("catmat", false); preferences.putInt("prioS", prioSlave); preferences.end(); calcMultiplex(); }, LV_EVENT_VALUE_CHANGED, NULL); 
    
    update_sliders_ui();

    lv_obj_t * btn_ap_setup = lv_btn_create(parent); lv_obj_set_size(btn_ap_setup, 400, 60); lv_obj_set_style_bg_color(btn_ap_setup, lv_color_hex(0x00A0FF), 0); lv_obj_t * l_ap = create_white_label(btn_ap_setup, "Setup (WLAN/Cam)"); lv_obj_center(l_ap);
    lv_obj_add_event_cb(btn_ap_setup, [](lv_event_t* e) { playToneI2S(800, 100, true); if(isSetupScanning) return; uint32_t randNum = esp_random() % 90000000 + 10000000; apPassword = String(randNum); lv_label_set_text_fmt(lbl_setup_qr, "Lela Setup-WLAN\nSSID: Lela-Setup\nPW: %s\nIP: 192.168.4.1", apPassword.c_str()); String qrData = "WIFI:S:Lela-Setup;T:WPA;P:" + apPassword + ";;"; lv_qrcode_update(setup_qr, qrData.c_str(), qrData.length()); lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(setup_overlay); pendingWebSetupMode = 1; }, LV_EVENT_CLICKED, NULL); 

    lv_obj_t * btn_sta_setup = lv_btn_create(parent); lv_obj_set_size(btn_sta_setup, 400, 60); lv_obj_set_style_bg_color(btn_sta_setup, lv_color_hex(0x00A0FF), 0); lv_obj_t * l_sta = create_white_label(btn_sta_setup, "Setup (MQTT/Sys)"); lv_obj_center(l_sta);
    lv_obj_add_event_cb(btn_sta_setup, [](lv_event_t* e) { playToneI2S(800, 100, true); if(isSetupScanning) return; String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Kein_WLAN"; lv_label_set_text_fmt(lbl_setup_qr, "SYSTEM SETUP\nLink: http://%s", ipStr.c_str()); String qrData = "http://" + ipStr; lv_qrcode_update(setup_qr, qrData.c_str(), qrData.length()); lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(setup_overlay); pendingWebSetupMode = 2; }, LV_EVENT_CLICKED, NULL);
}

static void buildSetupSection(lv_obj_t* parent) {
    create_header(parent, LV_SYMBOL_SETTINGS " GERAETE SETUP");
    
    lv_obj_t * cont_mac1 = create_helper_cont(parent, 80); lbl_setup_mat = create_text_label(cont_mac1, ""); lv_obj_set_width(lbl_setup_mat, 400); lv_label_set_text_fmt(lbl_setup_mat, "Matte:\n%s", savedMatMac.c_str()); lv_obj_align(lbl_setup_mat, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_t * btn_scan_mat = lv_btn_create(cont_mac1); lv_obj_set_size(btn_scan_mat, 160, 50); lv_obj_set_style_bg_color(btn_scan_mat, lv_color_hex(0x333333), 0); lv_obj_align(btn_scan_mat, LV_ALIGN_RIGHT_MID, -20, 0); lv_obj_add_event_cb(btn_scan_mat, btn_scan_mat_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_scan_mat = create_white_label(btn_scan_mat, LV_SYMBOL_REFRESH " Suche"); lv_obj_center(l_scan_mat);
    
    lv_obj_t * cont_mac2 = create_helper_cont(parent, 80); lbl_setup_kip = create_text_label(cont_mac2, ""); lv_obj_set_width(lbl_setup_kip, 400); lv_label_set_text_fmt(lbl_setup_kip, "Kippy:\n%s", savedKippyMac.c_str()); lv_obj_align(lbl_setup_kip, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_t * btn_scan_kip = lv_btn_create(cont_mac2); lv_obj_set_size(btn_scan_kip, 160, 50); lv_obj_set_style_bg_color(btn_scan_kip, lv_color_hex(0x333333), 0); lv_obj_align(btn_scan_kip, LV_ALIGN_RIGHT_MID, -20, 0); lv_obj_add_event_cb(btn_scan_kip, btn_scan_kip_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_scan_kip = create_white_label(btn_scan_kip, LV_SYMBOL_REFRESH " Suche"); lv_obj_center(l_scan_kip);
    
    lv_obj_t * btn_info = lv_btn_create(parent); lv_obj_set_size(btn_info, 400, 60); lv_obj_set_style_bg_color(btn_info, lv_color_hex(0x222222), 0); 
    lv_obj_t * lbl_info = lv_label_create(btn_info); lv_label_set_text(lbl_info, LV_SYMBOL_LIST " INFORMATIONEN"); lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_24, 0); lv_obj_set_style_text_color(lbl_info, lv_color_hex(0x00A0FF), 0); lv_obj_center(lbl_info);
    lv_obj_add_event_cb(btn_info, [](lv_event_t * e) {
        playToneI2S(800, 100, true);
        static uint32_t last_click = 0; static int click_count = 0; uint32_t now = millis();
        if (now - last_click > 1500) click_count = 0; 
        click_count++; last_click = now;
        lv_obj_t * lbl = lv_obj_get_child((lv_obj_t*)lv_event_get_target(e), 0);
        if (click_count >= 5) {
            click_count = 0; if (lbl) lv_label_set_text(lbl, LV_SYMBOL_LIST " INFORMATIONEN"); 
            String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Kein_WLAN"; 
            String qrData = "http://" + ipStr + "/screenshot"; lv_qrcode_update(qr_screenshot, qrData.c_str(), qrData.length()); 
            if (lbl_qr_ip) { lv_label_set_text_fmt(lbl_qr_ip, "Link: http://%s/screenshot", ipStr.c_str()); lv_obj_align_to(lbl_qr_ip, qr_screenshot, LV_ALIGN_OUT_BOTTOM_MID, 0, 20); }
            lv_obj_clear_flag(qr_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(qr_overlay); pendingScreenshotMode = 1; 
        } else { if (lbl) lv_label_set_text_fmt(lbl, LV_SYMBOL_LIST " INFORMATIONEN (%d/5)", click_count); }
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_restart = lv_btn_create(parent); lv_obj_set_size(btn_restart, 400, 60); lv_obj_set_style_bg_color(btn_restart, lv_color_hex(0xAA0000), 0); 
    lv_obj_add_event_cb(btn_restart, [](lv_event_t* e){ playToneI2S(800, 100, true); ESP.restart(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_restart = create_white_label(btn_restart, LV_SYMBOL_POWER " NEUSTART"); lv_obj_center(lbl_restart);

    text_ble_info = create_text_label(parent, "Lade BLE Daten..."); lv_label_set_long_mode(text_ble_info, LV_LABEL_LONG_WRAP); lv_obj_set_width(text_ble_info, 800); lv_label_set_recolor(text_ble_info, true); 
    text_sys_info = create_text_label(parent, "Lade System Daten..."); lv_label_set_long_mode(text_sys_info, LV_LABEL_LONG_WRAP); lv_obj_set_width(text_sys_info, 800);
    lv_obj_t * spacer = lv_obj_create(parent); lv_obj_set_size(spacer, 10, 80); lv_obj_set_style_bg_opa(spacer, 0, 0); lv_obj_set_style_border_width(spacer, 0, 0);
}

static void buildOverlays(lv_obj_t* scr) {
    qr_overlay = lv_obj_create(scr); lv_obj_set_size(qr_overlay, 600, 600); lv_obj_center(qr_overlay); lv_obj_set_style_bg_color(qr_overlay, lv_color_hex(0x222222), 0); lv_obj_set_style_border_color(qr_overlay, lv_color_hex(0x00A0FF), 0); lv_obj_set_style_border_width(qr_overlay, 4, 0); lv_obj_add_flag(qr_overlay, LV_OBJ_FLAG_HIDDEN); 
    qr_screenshot = lv_qrcode_create(qr_overlay, 300, lv_color_hex(0x00A0FF), lv_color_hex(0x222222)); lv_obj_set_style_border_width(qr_screenshot, 0, 0); lv_obj_set_style_pad_all(qr_screenshot, 0, 0); lv_obj_align(qr_screenshot, LV_ALIGN_TOP_MID, 0, 30); 
    lbl_qr_ip = lv_label_create(qr_overlay); lv_obj_set_width(lbl_qr_ip, 500); lv_obj_set_style_text_align(lbl_qr_ip, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align_to(lbl_qr_ip, qr_screenshot, LV_ALIGN_OUT_BOTTOM_MID, 0, 30); lv_obj_set_style_text_font(lbl_qr_ip, &lv_font_montserrat_24, 0); lv_obj_set_style_text_color(lbl_qr_ip, lv_color_white(), 0); lv_label_set_text(lbl_qr_ip, "Link: ---");
    btn_stop_screenshot = lv_btn_create(qr_overlay); lv_obj_set_size(btn_stop_screenshot, 300, 60); lv_obj_align(btn_stop_screenshot, LV_ALIGN_BOTTOM_MID, 0, -30); lv_obj_set_style_bg_color(btn_stop_screenshot, lv_color_hex(0xAA0000), 0); 
    lv_obj_add_event_cb(btn_stop_screenshot, [](lv_event_t* e){ playToneI2S(600, 100, true); lv_obj_add_flag(qr_overlay, LV_OBJ_FLAG_HIDDEN); pendingScreenshotMode = 2; }, LV_EVENT_CLICKED, NULL); 
    lv_obj_t * l_stop = create_white_label(btn_stop_screenshot, LV_SYMBOL_STOP " Beenden"); lv_obj_center(l_stop);

    setup_overlay = lv_obj_create(scr); lv_obj_set_size(setup_overlay, 600, 600); lv_obj_center(setup_overlay); lv_obj_set_style_bg_color(setup_overlay, lv_color_hex(0x222222), 0); lv_obj_set_style_border_color(setup_overlay, lv_color_hex(0xFF8800), 0); lv_obj_set_style_border_width(setup_overlay, 4, 0); lv_obj_add_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN); 
    setup_qr = lv_qrcode_create(setup_overlay, 300, lv_color_hex(0xFF8800), lv_color_hex(0x222222)); lv_obj_set_style_border_width(setup_qr, 0, 0); lv_obj_set_style_pad_all(setup_qr, 0, 0); lv_obj_align(setup_qr, LV_ALIGN_TOP_MID, 0, 30); 
    lbl_setup_qr = lv_label_create(setup_overlay); lv_obj_set_width(lbl_setup_qr, 500); lv_obj_set_style_text_align(lbl_setup_qr, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align_to(lbl_setup_qr, setup_qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 30); lv_obj_set_style_text_font(lbl_setup_qr, &lv_font_montserrat_24, 0); lv_obj_set_style_text_color(lbl_setup_qr, lv_color_white(), 0); lv_label_set_text(lbl_setup_qr, "---");
    btn_cancel_setup = lv_btn_create(setup_overlay); lv_obj_set_size(btn_cancel_setup, 300, 60); lv_obj_align(btn_cancel_setup, LV_ALIGN_BOTTOM_MID, 0, -30); lv_obj_set_style_bg_color(btn_cancel_setup, lv_color_hex(0xAA0000), 0); lv_obj_add_event_cb(btn_cancel_setup, [](lv_event_t* e){ playToneI2S(600, 100, true); ESP.restart(); }, LV_EVENT_CLICKED, NULL); 
    lv_obj_t * l_cancel_set = create_white_label(btn_cancel_setup, LV_SYMBOL_STOP " Abbrechen"); lv_obj_center(l_cancel_set);

    scan_overlay = lv_obj_create(scr); lv_obj_set_size(scan_overlay, lv_pct(100), lv_pct(100)); lv_obj_set_style_bg_color(scan_overlay, lv_color_hex(0x111111), 0); lv_obj_set_style_bg_opa(scan_overlay, 255, 0); lv_obj_set_style_border_width(scan_overlay, 0, 0); lv_obj_add_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN);
    scan_spinner = lv_spinner_create(scan_overlay, 1000, 60); lv_obj_set_size(scan_spinner, 64, 64); lv_obj_align(scan_spinner, LV_ALIGN_TOP_MID, -200, 50);
    lbl_scan_info = lv_label_create(scan_overlay); lv_label_set_text(lbl_scan_info, "Suche..."); lv_obj_set_style_text_font(lbl_scan_info, &lv_font_montserrat_28, 0); lv_obj_set_style_text_color(lbl_scan_info, lv_color_white(), 0); lv_obj_align(lbl_scan_info, LV_ALIGN_TOP_MID, 50, 60);
    
    // DIE NEUE LISTEN-ARCHITEKTUR
    scan_list = lv_obj_create(scan_overlay);
    lv_obj_set_size(scan_list, 600, 260);
    lv_obj_align(scan_list, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_bg_color(scan_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(scan_list, 2, 0);
    lv_obj_set_style_border_color(scan_list, lv_color_hex(0x333333), 0);
    lv_obj_set_flex_flow(scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scan_list, 5, 0);
    lv_obj_set_style_pad_row(scan_list, 5, 0);

    btn_rescan_mac = lv_btn_create(scan_overlay); lv_obj_set_size(btn_rescan_mac, 200, 60); lv_obj_align(btn_rescan_mac, LV_ALIGN_BOTTOM_MID, -120, -140); lv_obj_set_style_bg_color(btn_rescan_mac, lv_color_hex(0x555555), 0); lv_obj_add_event_cb(btn_rescan_mac, btn_rescan_mac_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_rescan = create_white_label(btn_rescan_mac, "Neu"); lv_obj_center(l_rescan); 
    btn_continue_mac = lv_btn_create(scan_overlay); lv_obj_set_size(btn_continue_mac, 200, 60); lv_obj_align(btn_continue_mac, LV_ALIGN_BOTTOM_MID, 120, -140); lv_obj_set_style_bg_color(btn_continue_mac, lv_color_hex(0xFF8800), 0); lv_obj_add_event_cb(btn_continue_mac, btn_continue_mac_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_continue = create_white_label(btn_continue_mac, "Weiter"); lv_obj_center(l_continue); 
    btn_cancel_mac = lv_btn_create(scan_overlay); lv_obj_set_size(btn_cancel_mac, 200, 60); lv_obj_align(btn_cancel_mac, LV_ALIGN_BOTTOM_MID, -120, -50); lv_obj_set_style_bg_color(btn_cancel_mac, lv_color_hex(0xAA0000), 0); lv_obj_add_event_cb(btn_cancel_mac, btn_cancel_mac_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_cancel = create_white_label(btn_cancel_mac, "Abbruch"); lv_obj_center(l_cancel); 
    btn_save_mac = lv_btn_create(scan_overlay); lv_obj_set_size(btn_save_mac, 200, 60); lv_obj_align(btn_save_mac, LV_ALIGN_BOTTOM_MID, 120, -50); lv_obj_set_style_bg_color(btn_save_mac, lv_color_hex(0x00A0FF), 0); lv_obj_add_event_cb(btn_save_mac, btn_save_mac_cb, LV_EVENT_CLICKED, NULL); lv_obj_t * l_save = create_white_label(btn_save_mac, "Sichern"); lv_obj_center(l_save);
}

// ==============================================================
// KLASSEN METHODEN
// ==============================================================
lv_obj_t* ViewSettings::build() {
    memset(scan_list_btns, 0, sizeof(scan_list_btns));
    memset(scan_list_labels, 0, sizeof(scan_list_labels));

    lv_obj_t* scr = lv_obj_create(NULL);
    if(!scr) return nullptr;
    lv_obj_set_style_bg_color(scr, isDarkMode ? lv_color_hex(0x111111) : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    lv_obj_t * top_bar = lv_obj_create(scr); lv_obj_set_size(top_bar, 1280, 80); lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0); lv_obj_set_style_bg_opa(top_bar, 0, 0); lv_obj_set_style_border_width(top_bar, 0, 0); lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t * btn_back = lv_btn_create(top_bar); lv_obj_set_size(btn_back, 200, 60); lv_obj_set_style_radius(btn_back, 30, 0); lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0); lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 5); lv_obj_add_event_cb(btn_back, [](lv_event_t * e) { playToneI2S(800, 100, true); gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_MOVE_BOTTOM); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = create_white_label(btn_back, LV_SYMBOL_LEFT " ZURUECK"); lv_obj_center(lbl_back);

    lv_obj_t * scroll_cont = lv_obj_create(scr); lv_obj_set_size(scroll_cont, 1280, 640); lv_obj_align(scroll_cont, LV_ALIGN_BOTTOM_MID, 0, 0); lv_obj_set_style_bg_opa(scroll_cont, 0, 0); lv_obj_set_style_border_width(scroll_cont, 0, 0); lv_obj_set_style_pad_all(scroll_cont, 20, 0); lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(scroll_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); lv_obj_set_style_pad_row(scroll_cont, 25, 0); 
    lv_obj_add_event_cb(scroll_cont, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    lv_obj_t * title = create_text_label(scroll_cont, "Einstellungen"); lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0); lv_obj_set_style_pad_bottom(title, 20, 0);

    buildSensorSection(scroll_cont);
    buildVideoSection(scroll_cont);
    buildSystemSection(scroll_cont);
    buildNetworkSection(scroll_cont);
    buildSetupSection(scroll_cont);
    buildOverlays(scr);

    return scr;
}

void ViewSettings::update() {
    if (gui.getCurrentScreen() != SCREEN_SETTINGS) return;

    if (pendingScreenshotMode == 2) {
        pendingScreenshotMode = 0; 
        if (qr_overlay && !lv_obj_has_flag(qr_overlay, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(qr_overlay, LV_OBJ_FLAG_HIDDEN); 
    }

    if (webSetupMode > 0 || pendingWebSetupMode > 0) return;

    if (!lv_obj_has_flag(scan_overlay, LV_OBJ_FLAG_HIDDEN)) {
        bool isScanning, showSaveBtn;
        char infoBuf[128];
        BleLogic_GetScanStatus(infoBuf, sizeof(infoBuf), isScanning, showSaveBtn);

        if (isScanning) lv_obj_clear_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(lbl_scan_info, infoBuf);
        
        if (showSaveBtn && selected_scan_index >= 0) lv_obj_clear_flag(btn_save_mac, LV_OBJ_FLAG_HIDDEN); 
        else lv_obj_add_flag(btn_save_mac, LV_OBJ_FLAG_HIDDEN);

        if (bleUpdateQueue != NULL && uxQueueMessagesWaiting(bleUpdateQueue) > 0) {
            if (lvgl_port_lock(50)) {
                BleUpdateEvent ev;
                bool list_changed = false;
                
                while (xQueueReceive(bleUpdateQueue, &ev, 0) == pdTRUE) {
                    list_changed = true;

                    if (ev.isClear) {
                        lv_obj_clean(scan_list); 
                        memset(scan_list_btns, 0, sizeof(scan_list_btns));
                        memset(scan_list_labels, 0, sizeof(scan_list_labels));
                        selected_scan_index = -1;
                    } 
                    else if (ev.isNew) {
                        if (ev.index < MAX_SCAN_DEVICES) {
                            lv_obj_t* btn = lv_btn_create(scan_list);
                            lv_obj_set_width(btn, lv_pct(100));
                            lv_obj_set_height(btn, 50);
                            lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
                            lv_obj_add_event_cb(btn, scan_list_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)ev.index);
                            
                            lv_obj_t* lbl = lv_label_create(btn);
                            lv_label_set_text_fmt(lbl, "%s [%s]  %d dBm", ev.device.name, ev.device.mac, ev.device.rssi);
                            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
                            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
                            lv_obj_center(lbl);

                            scan_list_btns[ev.index] = btn;
                            scan_list_labels[ev.index] = lbl;
                        }
                    } 
                    else {
                        if (ev.index < MAX_SCAN_DEVICES && scan_list_labels[ev.index]) {
                            lv_label_set_text_fmt(scan_list_labels[ev.index], "%s [%s]  %d dBm", ev.device.name, ev.device.mac, ev.device.rssi);
                        }
                    }
                }

                if (list_changed) {
                    lv_obj_invalidate(scan_list); 
                }
                
                lvgl_port_unlock();
            }
        }
    }

    static uint32_t lastInfoUpdate = 0;
    if (millis() - lastInfoUpdate > 1000) { 
        lastInfoUpdate = millis();

        String bleInfo = "Matte: ";
        bleInfo += matEnabled ? (connected ? "Verbunden" : "Getrennt") : "Deaktiviert";
        bleInfo += " ("; bleInfo += connected ? -60 : 0; bleInfo += " dBm)\nIntervall: "; bleInfo += (int)avgInterval; bleInfo += " ms\n\nKippy: ";
        bleInfo += kippyEnabled ? ((lastCatSeenTime == 0 || millis() - lastCatSeenTime > 30000) ? "Abwesend" : "Aktiv") : "Deaktiviert";
        bleInfo += "\nSignal: "; bleInfo += (kippyEnabled && lastCatSeenTime != 0 && (millis() - lastCatSeenTime <= 30000)) ? catRssi : 0; bleInfo += " dBm\n\nWLAN HA: "; bleInfo += wifiEnabled ? "Aktiv" : "Deaktiviert";
        lv_label_set_text(text_ble_info, bleInfo.c_str());

        uint32_t sec = (millis() - startTime) / 1000;
        String sysInfo = "WLAN: ";
        sysInfo += (WiFi.status() == WL_CONNECTED ? "Verbunden" : "Getrennt");
        sysInfo += " ("; sysInfo += WiFi.RSSI(); sysInfo += " dBm)\nMQTT: "; sysInfo += mqttClient.connected() ? "Verbunden" : "Getrennt";
        sysInfo += "\nProxy WLAN: "; sysInfo += proxyWifiRssi; sysInfo += " dBm\nProxy Status: "; sysInfo += (millis() - proxyLastUpdate < 15000) ? proxyStatusMsg : "Offline";
        sysInfo += "\nIP: "; sysInfo += WiFi.localIP().toString(); sysInfo += "\nHA Alarm: "; sysInfo += alarmActive ? "Aktiv" : "Standby";
        sysInfo += "\n\nESP Akku: "; sysInfo += batteryPercent; sysInfo += " %\nSpeicher: "; sysInfo += (ESP.getFreeHeap() / 1024); 
        sysInfo += " KB\nUptime: "; sysInfo += (sec / 3600); sysInfo += "h "; sysInfo += ((sec % 3600) / 60); sysInfo += "m "; sysInfo += (sec % 60); sysInfo += "s";
        lv_label_set_text(text_sys_info, sysInfo.c_str());
    }
}