#include "ViewBabySettings.h"
#include "SharedData.h"
#include "BabyCamApi.h"
#include "BabyService.h"
#include "ViewVideoPip.h"
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
static lv_obj_t * slider_expo = nullptr;
static lv_obj_t * sw_lowlat = nullptr;

// Video-Overlay (Bild im Bild auf den anderen Bildschirmen). Lage und Groesse
// werden direkt am Overlay per Ziehen eingestellt (siehe ViewVideoPip).
static lv_obj_t * dd_pip_fps    = nullptr;
static lv_obj_t * dd_pip_res    = nullptr;

// Auswahl der Overlay-Bildrate. Reihenfolge = Reihenfolge der Dropdown-Texte.
static const int  PIP_FPS_VALUES[] = { 1, 2, 3, 5, 8, 10, 15 };
static const int  PIP_FPS_COUNT    = sizeof(PIP_FPS_VALUES) / sizeof(PIP_FPS_VALUES[0]);
static const char PIP_FPS_OPTIONS[] = "1 fps\n2 fps\n3 fps\n5 fps\n8 fps\n10 fps\n15 fps";

// Erster Eintrag der Overlay-Aufloesung: kleinste, die die Kamera anbietet.
static const char PIP_RES_AUTO[] = "Automatisch";

// Belichtung hat bisher keinen Platz in SharedData - sie wird nur hier
// gebraucht und ueberlebt den Neustart in den Preferences.
static int camExposure = 50;

extern void playToneI2S(uint16_t freq, uint32_t duration_ms, bool isUiSound);

// ==============================================================
// CALLBACKS
// ==============================================================
static void slider_sync_value(lv_obj_t* slider);   // weiter unten definiert

static void slider_step5_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    val = round(val / 5.0) * 5;
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    slider_sync_value(slider);   // waehrend des Schiebens, nicht erst danach

    
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        Preferences prefs; prefs.begin("catmat", false);
        if (slider == slider_zoom) { BabyCamApi_SetZoom(val); camZoom = val; prefs.putInt("camZoom", val); }
        else if (slider == slider_qual) { BabyCamApi_SetQuality(val); camQuality = val; prefs.putInt("camQuality", val); }
        // Der Regler zeigt BILDRATE, die Kamera erwartet ECO-STUFE. api_RE.md,
        // Abschnitt 5: /api/v1/camera/speed nimmt 0 = maximale fps und
        // 100 = maximaler Eco-Modus. Die alte Beschriftung "Speed (0-100%)"
        // las sich genau andersherum - wer den Regler nach rechts zog, hat die
        // Kamera in den Sparmodus geschickt und bekam 1 fps. Der Wert wird
        // deshalb hier gedreht; gespeichert bleibt die API-Stufe.
        else if (slider == slider_speed) {
            const int eco = 100 - val;
            BabyCamApi_SetSpeed(eco); camSpeed = eco; prefs.putInt("camSpeed", eco);
        }
        else if (slider == slider_expo)  { BabyCamApi_SetExposure(val); camExposure = val; prefs.putInt("camExpo", val); }
        prefs.end();
    }
}

static void settings_action_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if (obj == sw_flash) BabyCamApi_ToggleFlash();
    else if (obj == sw_scrflash) BabyCamApi_ToggleScrFlash();
    else if (obj == sw_camscreen) BabyCamApi_ToggleScreen();
    else if (obj == sw_camstream) BabyCamApi_ToggleCamera();
    else if (obj == sw_lowlat) {
        videoLowLatency = lv_obj_has_state(sw_lowlat, LV_STATE_CHECKED);
        Preferences prefs; prefs.begin("catmat", false);
        prefs.putBool("vidLowLat", videoLowLatency);
        prefs.end();
    }
}

// Aufloesung der Baby-Ansicht. Gesetzt wird sie vom BabyService, sobald das
// Video laeuft und die Baby-Ansicht der Abnehmer ist (das Overlay hat eine
// eigene Aufloesung).
static void dd_res_cb(lv_event_t * e) {
    char buf[32];
    lv_dropdown_get_selected_str(dd_res, buf, sizeof(buf));
    if (String(buf).indexOf('x') <= 0) return;   // Platzhalter "(warte auf Kamera)"
    BabyService_SetBabyResolution(String(buf));
}

// --- Video-Overlay ---------------------------------------------------------
static void dd_pip_cb(lv_event_t * e) {
    lv_obj_t* dd = lv_event_get_target(e);
    const uint16_t sel = lv_dropdown_get_selected(dd);
    playToneI2S(800, 100, true);

    if (dd == dd_pip_fps) {
        if (sel < PIP_FPS_COUNT) BabyService_SetPipFps(PIP_FPS_VALUES[sel]);
    } else if (dd == dd_pip_res) {
        if (sel == 0) {
            BabyService_SetPipResolution("");
        } else {
            char buf[32];
            lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
            BabyService_SetPipResolution(String(buf));
        }
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

// Haelt die Prozentanzeige eines Reglers auf Stand.
//
// Die Anzeige haengt als user_data am Regler selbst. Dadurch braucht weder der
// Ereignis-Callback noch das Einspielen der gespeicherten Werte eine
// Fallunterscheidung ueber alle Regler - wer den Regler hat, hat auch seine
// Beschriftung.
static void slider_sync_value(lv_obj_t* slider) {
    if (!slider) return;
    lv_obj_t* lbl = (lv_obj_t*)lv_obj_get_user_data(slider);
    if (!lbl) return;
    lv_label_set_text_fmt(lbl, "%d%%", (int)lv_slider_get_value(slider));
}

static lv_obj_t* create_setting_slider(lv_obj_t* parent, const char* name, lv_obj_t** slider_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 400, 80); // Mehr Platz, damit der Knopf nicht abgeschnitten wird
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    // Name links, Prozentwert rechts - beide auf derselben Grundlinie, damit
    // die vier Regler untereinander eine saubere Spalte bilden.
    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 0);

    lv_obj_t* lbl_val = lv_label_create(cont);
    lv_label_set_text(lbl_val, "0%");
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(lbl_val, LV_ALIGN_TOP_RIGHT, -10, 0);

    *slider_ptr = lv_slider_create(cont);
    lv_obj_set_size(*slider_ptr, 360, 20); // Etwas schmaler, gibt Rändern mehr Raum
    lv_obj_align(*slider_ptr, LV_ALIGN_BOTTOM_MID, 0, -10); // Sauberer Abstand nach unten
    lv_obj_clear_flag(*slider_ptr, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_user_data(*slider_ptr, lbl_val);
    lv_obj_add_event_cb(*slider_ptr, slider_step5_cb, LV_EVENT_ALL, NULL);
    return cont;
}

// Beschriftung oben, Auswahlliste darunter - in der Breite eines Viertels
// der Zeile, damit vier Einstellungen nebeneinander passen.
static lv_obj_t* create_setting_dropdown(lv_obj_t* parent, const char* name, const char* options, lv_obj_t** dd_ptr) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 205, 80);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, 0);

    *dd_ptr = lv_dropdown_create(cont);
    lv_obj_set_size(*dd_ptr, 195, 40);
    lv_obj_align(*dd_ptr, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_dropdown_set_options(*dd_ptr, options);
    lv_obj_clear_flag(*dd_ptr, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(*dd_ptr, dd_pip_cb, LV_EVENT_VALUE_CHANGED, NULL);
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
    lv_obj_set_size(settings_modal, 900, 690);   // 690: Platz fuer die Overlay-Zeile
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
    create_setting_switch(row1, "Latenz-Modus", &sw_lowlat);

    // ZEILE 2: Sliders
    lv_obj_t* row2 = create_transparent_row(settings_modal, 840, 80);
    create_setting_slider(row2, "Zoom", &slider_zoom);
    create_setting_slider(row2, "Qualitaet", &slider_qual);

    // ZEILE 3: Speed & Dropdown
    lv_obj_t* row3 = create_transparent_row(settings_modal, 840, 80);
    create_setting_slider(row3, "Bildrate (100 = max)", &slider_speed);

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

    // ZEILE 3b: Belichtung und lokale Aufnahme auf der Kamera
    lv_obj_t* row3b = create_transparent_row(settings_modal, 840, 80);
    create_setting_slider(row3b, "Belichtung", &slider_expo);

    lv_obj_t* btn_rec = lv_btn_create(row3b);
    lv_obj_set_size(btn_rec, 400, 60);
    lv_obj_set_style_bg_color(btn_rec, lv_color_hex(0x8B1A1A), 0);
    lv_obj_t* lbl_rec = lv_label_create(btn_rec);
    lv_label_set_text(lbl_rec, LV_SYMBOL_VIDEO "  Aufnahme starten/stoppen");
    lv_obj_center(lbl_rec);
    lv_obj_add_event_cb(btn_rec, [](lv_event_t* e){
        playToneI2S(800, 100, true);
        BabyCamApi_RecordVideo();
    }, LV_EVENT_CLICKED, NULL);

    // ZEILE 3c: Video-Overlay auf den anderen Bildschirmen
    lv_obj_t* row3c = create_transparent_row(settings_modal, 840, 80);
    create_setting_dropdown(row3c, "Overlay-Bildrate",   PIP_FPS_OPTIONS, &dd_pip_fps);
    create_setting_dropdown(row3c, "Overlay-Aufloesung", PIP_RES_AUTO,    &dd_pip_res);

    lv_obj_t* btn_pip_reset = lv_btn_create(row3c);
    lv_obj_set_size(btn_pip_reset, 400, 60);
    lv_obj_t* lbl_pip_reset = lv_label_create(btn_pip_reset);
    lv_label_set_text(lbl_pip_reset, LV_SYMBOL_REFRESH "  Overlay-Lage zuruecksetzen");
    lv_obj_center(lbl_pip_reset);
    lv_obj_add_event_cb(btn_pip_reset, [](lv_event_t* e){
        playToneI2S(800, 100, true);
        ViewVideoPip::resetLayout();
    }, LV_EVENT_CLICKED, NULL);

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
    lv_slider_set_value(slider_speed, 100 - camSpeed, LV_ANIM_OFF);   // Eco-Stufe -> Bildrate

    slider_sync_value(slider_zoom);
    slider_sync_value(slider_qual);
    slider_sync_value(slider_speed);

    // Belichtung beim ersten Oeffnen aus dem Flash nachladen.
    static bool expoLoaded = false;
    if (!expoLoaded) {
        Preferences prefs; prefs.begin("catmat", true);
        camExposure = prefs.getInt("camExpo", 50);
        prefs.end();
        expoLoaded = true;
    }
    lv_slider_set_value(slider_expo, camExposure, LV_ANIM_OFF);
    slider_sync_value(slider_expo);

    if (videoLowLatency) lv_obj_add_state(sw_lowlat, LV_STATE_CHECKED);
    else                 lv_obj_clear_state(sw_lowlat, LV_STATE_CHECKED);
    
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
    // Solange die Kamera noch keine Liste gemeldet hat, wenigstens die
    // aktuelle Aufloesung anbieten - ein leeres Dropdown sieht aus wie ein
    // Fehler und laesst sich nicht bedienen.
    if (opts.length() == 0) opts = (currentCamRes.length() > 0) ? currentCamRes : String("(warte auf Kamera)");

    lv_dropdown_set_options(dd_res, opts.c_str());
    lv_dropdown_set_selected(dd_res, selIdx);

    // --- Video-Overlay ---
    int fpsIdx = 0;
    for (int i = 0; i < PIP_FPS_COUNT; i++) {
        if (PIP_FPS_VALUES[i] <= BabyService_GetPipFps()) fpsIdx = i;
    }
    lv_dropdown_set_selected(dd_pip_fps, fpsIdx);

    // Aufloesungen der Kamera (oder die zuletzt bekannte Liste), davor
    // "Automatisch". Eine gespeicherte, die die Kamera nicht mehr anbietet,
    // faellt auf "Automatisch" zurueck - genau das tut auch der BabyService.
    const String pipList = BabyService_GetResolutionOptions();
    const String pipSet  = BabyService_GetPipResolutionSetting();
    String pipOpts = PIP_RES_AUTO;
    int pipSel = 0, idx = 1, start = 0;
    while (pipList.length() > 0 && start <= (int)pipList.length()) {
        int end = pipList.indexOf('\n', start);
        if (end < 0) end = pipList.length();
        const String entry = pipList.substring(start, end);
        start = end + 1;
        if (entry.length() == 0) continue;
        pipOpts += "\n" + entry;
        if (entry == pipSet) pipSel = idx;
        idx++;
    }
    lv_dropdown_set_options(dd_pip_res, pipOpts.c_str());
    lv_dropdown_set_selected(dd_pip_res, pipSel);
    
    // Komplettes Overlay einblenden und sicherstellen, dass es ganz oben liegt
    lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_overlay);
}

void ViewBabySettings_Update() {
    // LEER! Die UI-Updates wurden aus der Loop verbannt.
    // Das verhinderte das Dropdown daran, offen zu bleiben, 
    // weil das Parent ständig über die Dropdown-Liste gezeichnet wurde.
}