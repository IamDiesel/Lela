#include "ViewBabySettings.h"
#include "SharedData.h"
#include "BabyCamApi.h"
#include <Preferences.h>
#include <math.h>

static lv_obj_t * settings_overlay = nullptr;
static lv_obj_t * settings_modal = nullptr;
static lv_obj_t * confirm_overlay = nullptr;

static lv_obj_t * sw_flash = nullptr;
static lv_obj_t * sw_scrflash = nullptr;
static lv_obj_t * sw_camscreen = nullptr;
static lv_obj_t * sw_camstream = nullptr;
static lv_obj_t * slider_zoom = nullptr;
static lv_obj_t * slider_qual = nullptr;
static lv_obj_t * slider_speed = nullptr;
static lv_obj_t * dd_res = nullptr;

extern void playToneI2S(uint16_t freq, uint32_t duration_ms, bool isUiSound);

// ==============================================================
// CALLBACKS
// ==============================================================
static void slider_step5_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    val = round(val / 5.0) * 5;
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        Preferences prefs; prefs.begin("catmat", false);
        if (slider == slider_zoom) { BabyCamApi_SetZoom(val); camZoom = val; prefs.putInt("camZoom", val); }
        else if (slider == slider_qual) { BabyCamApi_SetQuality(val); camQuality = val; prefs.putInt("camQuality", val); }
        else if (slider == slider_speed) { BabyCamApi_SetSpeed(val); camSpeed = val; prefs.putInt("camSpeed", val); }
        prefs.end();
    }
}

static void settings_action_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if (obj == sw_flash) BabyCamApi_ToggleFlash();
    else if (obj == sw_scrflash) BabyCamApi_ToggleScrFlash();
    else if (obj == sw_camscreen) BabyCamApi_ToggleScreen();
    else if (obj == sw_camstream) BabyCamApi_ToggleCamera();
}

static void dd_res_cb(lv_event_t * e) {
    char buf[32];
    lv_dropdown_get_selected_str(dd_res, buf, sizeof(buf));
    String res = String(buf);
    currentCamRes = res; 
    
    Preferences prefs;
    prefs.begin("catmat", false);
    prefs.putString("camRes", currentCamRes);
    prefs.end();

    int xIdx = res.indexOf('x');
    if(xIdx != -1) {
        int w = res.substring(0, xIdx).toInt();
        int h = res.substring(xIdx+1).toInt();
        BabyCamApi_SetResolution(w, h);
    }
}

// ==============================================================
// LAYOUT HELPER
// ==============================================================
static lv_obj_t* create_transparent_row(lv_obj_t* parent, int width, int height) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, width, height);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t* create_setting_slider(lv_obj_t* parent, const char* name, lv_obj_t** slider_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 400, 80); // Mehr Platz, damit der Knopf nicht abgeschnitten wird
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    
    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 0);
    
    *slider_ptr = lv_slider_create(cont);
    lv_obj_set_size(*slider_ptr, 360, 20); // Etwas schmaler, gibt Rändern mehr Raum
    lv_obj_align(*slider_ptr, LV_ALIGN_BOTTOM_MID, 0, -10); // Sauberer Abstand nach unten
    lv_obj_clear_flag(*slider_ptr, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_add_event_cb(*slider_ptr, slider_step5_cb, LV_EVENT_ALL, NULL);
    return cont;
}

static lv_obj_t* create_setting_switch(lv_obj_t* parent, const char* name, lv_obj_t** sw_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 200, 80); // Mehr Platz für Schalter und Text
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    
    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);
    
    *sw_ptr = lv_switch_create(cont);
    lv_obj_align(*sw_ptr, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(*sw_ptr, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_add_event_cb(*sw_ptr, settings_action_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return cont;
}

// ==============================================================
// PUBLIC FUNCTIONS
// ==============================================================
void ViewBabySettings_Create(lv_obj_t* parent) {
    // --- 1. FULLSCREEN OVERLAY (Verdunkelung & Klick-Schutz) ---
    settings_overlay = lv_obj_create(parent);
    lv_obj_set_size(settings_overlay, 1280, 720);
    lv_obj_center(settings_overlay);
    lv_obj_set_style_bg_color(settings_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(settings_overlay, 160, 0); // Leicht abgedunkelter Hintergrund
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);

    // Klick neben das Modal schließt es
    lv_obj_add_event_cb(settings_overlay, [](lv_event_t* e){
        if (lv_event_get_target(e) == settings_overlay) { // Nur auslösen, wenn der Background geklickt wird
            playToneI2S(600, 100, true);
            lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_CLICKED, NULL);


    // --- 2. CONFIRMATION OVERLAY (Bist du sicher?) ---
    confirm_overlay = lv_obj_create(parent);
    lv_obj_set_size(confirm_overlay, 1280, 720);
    lv_obj_center(confirm_overlay);
    lv_obj_set_style_bg_color(confirm_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(confirm_overlay, 200, 0);
    lv_obj_add_flag(confirm_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(confirm_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(confirm_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* confirm_panel = lv_obj_create(confirm_overlay);
    lv_obj_set_size(confirm_panel, 500, 300);
    lv_obj_center(confirm_panel);
    lv_obj_set_style_bg_color(confirm_panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(confirm_panel, lv_color_hex(0xAA0000), 0);
    lv_obj_set_style_border_width(confirm_panel, 3, 0);

    lv_obj_t* lbl_warn = lv_label_create(confirm_panel);
    lv_label_set_text(lbl_warn, LV_SYMBOL_WARNING " Bist du sicher?");
    lv_obj_set_style_text_font(lbl_warn, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_warn, lv_color_white(), 0);
    lv_obj_align(lbl_warn, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* btn_yes = lv_btn_create(confirm_panel);
    lv_obj_set_size(btn_yes, 180, 80);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 20, -30);
    lv_obj_set_style_bg_color(btn_yes, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_yes = lv_label_create(btn_yes); lv_label_set_text(lbl_yes, "JA"); lv_obj_center(lbl_yes);
    lv_obj_add_event_cb(btn_yes, [](lv_event_t* e){ playToneI2S(800, 100, true); BabyCamApi_PowerOff(); lv_obj_add_flag(confirm_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_no = lv_btn_create(confirm_panel);
    lv_obj_set_size(btn_no, 180, 80);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -20, -30);
    lv_obj_set_style_bg_color(btn_no, lv_color_hex(0x555555), 0);
    lv_obj_t* lbl_no = lv_label_create(btn_no); lv_label_set_text(lbl_no, "NEIN"); lv_obj_center(lbl_no);
    lv_obj_add_event_cb(btn_no, [](lv_event_t* e){ playToneI2S(600, 100, true); lv_obj_add_flag(confirm_overlay, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, NULL);


    // --- 3. MAIN SETTINGS MODAL ---
    settings_modal = lv_obj_create(settings_overlay);
    lv_obj_set_size(settings_modal, 900, 600);
    lv_obj_center(settings_modal);
    lv_obj_set_style_bg_color(settings_modal, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_color(settings_modal, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(settings_modal, 2, 0);
    lv_obj_set_scrollbar_mode(settings_modal, LV_SCROLLBAR_MODE_OFF); 
    lv_obj_add_flag(settings_modal, LV_OBJ_FLAG_CLICKABLE); // Absorbiert Klicks, damit der Hintergrund sie nicht fängt
    lv_obj_clear_flag(settings_modal, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    // Perfekte vertikale Verteilung der Zeilen
    lv_obj_set_flex_flow(settings_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_modal, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(settings_modal, 15, 0);

    // ZEILE 1: Switches
    lv_obj_t* row1 = create_transparent_row(settings_modal, 840, 80);
    create_setting_switch(row1, "Blitz", &sw_flash);
    create_setting_switch(row1, "SCR-Blitz", &sw_scrflash);
    create_setting_switch(row1, "Screen On/Off", &sw_camscreen);
    create_setting_switch(row1, "Cam On/Off", &sw_camstream);

    // ZEILE 2: Sliders
    lv_obj_t* row2 = create_transparent_row(settings_modal, 840, 80);
    create_setting_slider(row2, "Zoom (0-100%)", &slider_zoom);
    create_setting_slider(row2, "Quality (0-100%)", &slider_qual);

    // ZEILE 3: Speed & Dropdown
    lv_obj_t* row3 = create_transparent_row(settings_modal, 840, 80);
    create_setting_slider(row3, "Speed (0-100%)", &slider_speed);

    lv_obj_t* cont_dd = lv_obj_create(row3);
    lv_obj_set_size(cont_dd, 400, 80);
    lv_obj_clear_flag(cont_dd, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont_dd, 0, 0);
    lv_obj_set_style_border_width(cont_dd, 0, 0);
    lv_obj_set_style_pad_all(cont_dd, 0, 0);

    lv_obj_t* lbl_dd = lv_label_create(cont_dd);
    lv_label_set_text(lbl_dd, "Kamera Aufloesung:");
    lv_obj_set_style_text_color(lbl_dd, lv_color_white(), 0);
    lv_obj_align(lbl_dd, LV_ALIGN_TOP_LEFT, 10, 0);

    dd_res = lv_dropdown_create(cont_dd);
    lv_obj_set_size(dd_res, 360, 40);
    lv_obj_align(dd_res, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(dd_res, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(dd_res, dd_res_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ZEILE 4: Action Buttons (Front/Main & Beenden)
    lv_obj_t* row4 = create_transparent_row(settings_modal, 840, 70);

    lv_obj_t* btn_lense = lv_btn_create(row4);
    lv_obj_set_size(btn_lense, 400, 70);
    lv_obj_t* lbl_lense = lv_label_create(btn_lense); 
    lv_label_set_text(lbl_lense, "Cam Front/Main"); 
    lv_obj_center(lbl_lense);
    lv_obj_add_event_cb(btn_lense, [](lv_event_t* e){ playToneI2S(800, 100, true); BabyCamApi_ToggleLense(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_pwr = lv_btn_create(row4);
    lv_obj_set_size(btn_pwr, 400, 70);
    lv_obj_set_style_bg_color(btn_pwr, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_pwr = lv_label_create(btn_pwr); 
    lv_label_set_text(lbl_pwr, "App Beenden"); 
    lv_obj_center(lbl_pwr);
    lv_obj_add_event_cb(btn_pwr, [](lv_event_t* e){ playToneI2S(800, 100, true); lv_obj_clear_flag(confirm_overlay, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(confirm_overlay); }, LV_EVENT_CLICKED, NULL);

    // ZEILE 5: Schließen Button (ganz unten, gleiche Hoehe wie Action Buttons)
    lv_obj_t* btn_close = lv_btn_create(settings_modal);
    lv_obj_set_size(btn_close, 840, 70);
    lv_obj_t* lbl_close = lv_label_create(btn_close); 
    lv_label_set_text(lbl_close, "Schliessen"); 
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e){ playToneI2S(600, 100, true); lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN); }, LV_EVENT_CLICKED, NULL);
}

void ViewBabySettings_Show() {
    if (!settings_overlay) return;
    
    // Aktuelle Werte einspielen
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
    
    // Komplettes Overlay einblenden und sicherstellen, dass es ganz oben liegt
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_overlay);
}

void ViewBabySettings_Update() {
    // LEER! Die UI-Updates wurden aus der Loop verbannt.
    // Das verhinderte das Dropdown daran, offen zu bleiben, 
    // weil das Parent ständig über die Dropdown-Liste gezeichnet wurde.
}