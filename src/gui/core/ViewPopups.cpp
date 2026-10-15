#include "ViewPopups.h"
#include "SharedData.h"
#include "GuiManager.h"

static lv_obj_t * dual_alarm_container = nullptr;
static lv_obj_t * pnl_baby = nullptr;
static lv_obj_t * lbl_baby_alarm = nullptr;
static lv_obj_t * pnl_cat = nullptr;
static lv_obj_t * lbl_cat_alarm = nullptr;

void ViewPopups::init() {
    if (dual_alarm_container != nullptr) return; 

    lv_obj_t * sys_layer = lv_layer_top();

    // --- Dual Alarm Container skaliert auf 1280x720 ---
    dual_alarm_container = lv_obj_create(sys_layer);
    lv_obj_set_size(dual_alarm_container, 1280, 720);
    lv_obj_set_style_bg_color(dual_alarm_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(dual_alarm_container, 220, 0); 
    lv_obj_set_style_border_width(dual_alarm_container, 0, 0);
    // Ohne Innenabstand und Rundung: die Panels sind genau bildschirmgross
    // und wuerden sonst um den Theme-Abstand verschoben.
    lv_obj_set_style_pad_all(dual_alarm_container, 0, 0);
    lv_obj_set_style_radius(dual_alarm_container, 0, 0);
    lv_obj_clear_flag(dual_alarm_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dual_alarm_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(dual_alarm_container); 

    // Baby Alarm Panel
    pnl_baby = lv_btn_create(dual_alarm_container);
    lv_obj_set_style_bg_color(pnl_baby, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(pnl_baby, 0, 0); // Eckige Kanten für echten Vollbild-Look
    lv_obj_set_style_border_width(pnl_baby, 0, 0);
    // Tippen quittiert (Ton und Popup aus). Zuruecksetzen geht wie bisher in
    // der Baby-Ansicht. Die Mixer-Lautstaerke bleibt unberuehrt.
    lv_obj_add_event_cb(pnl_baby, [](lv_event_t* e){ 
        playToneI2S(800, 100, true); 
        babyAlarmAcked = true; 
    }, LV_EVENT_CLICKED, NULL);
    
    lbl_baby_alarm = lv_label_create(pnl_baby);
    lv_obj_set_style_text_font(lbl_baby_alarm, &lv_font_montserrat_48, 0); // Riesige Schrift
    lv_label_set_text(lbl_baby_alarm, LV_SYMBOL_BELL " BABY ALARM!\n(Tippen zum Stummschalten)");
    lv_obj_set_style_text_align(lbl_baby_alarm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_baby_alarm);

    // Katzen Alarm Panel
    pnl_cat = lv_btn_create(dual_alarm_container);
    lv_obj_set_style_bg_color(pnl_cat, lv_color_hex(0xFF4400), 0);
    lv_obj_set_style_radius(pnl_cat, 0, 0); // Eckige Kanten für echten Vollbild-Look
    lv_obj_set_style_border_width(pnl_cat, 0, 0);
    lv_obj_add_event_cb(pnl_cat, [](lv_event_t* e){ 
        playToneI2S(800, 100, true); 
        catAlarmAcked = true; 
    }, LV_EVENT_CLICKED, NULL);
    
    lbl_cat_alarm = lv_label_create(pnl_cat);
    lv_obj_set_style_text_font(lbl_cat_alarm, &lv_font_montserrat_48, 0); // Riesige Schrift
    lv_label_set_text(lbl_cat_alarm, LV_SYMBOL_BELL " KATZEN ALARM!\n(Tippen zum Stummschalten)");
    lv_obj_set_style_text_align(lbl_cat_alarm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_cat_alarm);
}

// Wird in jedem Durchlauf der Hauptschleife gerufen. LVGL-Aufrufe deshalb nur
// bei einer Aenderung: jedes Setzen von Groesse oder Farbe erklaert das Objekt
// fuer ungueltig, und ein bildschirmfuellendes Popup wuerde sonst alle paar
// Millisekunden komplett neu gezeichnet.
void ViewPopups::update() {
    if (dual_alarm_container == nullptr) return;   // init() noch nicht gelaufen

    // Zuletzt angewendeter Zustand. -1 = noch nichts angewendet.
    static int s_lastLayout = -1;   // 0 = aus, 1 = nur Baby, 2 = nur Katze, 3 = beide
    static int s_lastBlink  = -1;

    // --- Alarm Cross-Logic ---
    const bool catNeedsPopup  = (alarmActive && !catAlarmAcked);
    const bool babyNeedsPopup = (babyAlarmActive && !babyAlarmAcked);
    
    const ScreenID scr = gui.getCurrentScreen();

    bool showCat = false;
    bool showBaby = false;

    // Zeige das Popup fuer das Ereignis, das gerade NICHT auf dem Schirm ist.
    // Das Dashboard hat eigene Alarmknoepfe; Einstellungen und Home Assistant
    // haben keine eigene Alarmanzeige.
    if (scr == SCREEN_SETTINGS || scr == SCREEN_HA) {
        showCat = catNeedsPopup;
        showBaby = babyNeedsPopup;
    } else if (scr == SCREEN_BABY) {
        showCat = catNeedsPopup; 
    } else if (scr == SCREEN_CATMAT) {
        showBaby = babyNeedsPopup; 
    }

    const int layout = (showBaby ? 1 : 0) | (showCat ? 2 : 0);

    if (layout != s_lastLayout) {
        s_lastLayout = layout;
        s_lastBlink  = -1;   // Farbe nach einem Layoutwechsel neu setzen

        if (layout == 0) {
            lv_obj_add_flag(dual_alarm_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(dual_alarm_container, LV_OBJ_FLAG_HIDDEN);
            // Ueber allem anderen auf der obersten Ebene (z.B. einem
            // Video-Overlay) - ein Alarm darf nie verdeckt sein.
            lv_obj_move_foreground(dual_alarm_container);

            if (showBaby) lv_obj_clear_flag(pnl_baby, LV_OBJ_FLAG_HIDDEN);
            else          lv_obj_add_flag(pnl_baby, LV_OBJ_FLAG_HIDDEN);
            if (showCat)  lv_obj_clear_flag(pnl_cat, LV_OBJ_FLAG_HIDDEN);
            else          lv_obj_add_flag(pnl_cat, LV_OBJ_FLAG_HIDDEN);

            if (showCat && showBaby) {
                // Geteilter Bildschirm, je 1280x360
                lv_obj_set_size(pnl_baby, 1280, 360);
                lv_obj_align(pnl_baby, LV_ALIGN_TOP_MID, 0, 0);
                lv_obj_set_size(pnl_cat, 1280, 360);
                lv_obj_align(pnl_cat, LV_ALIGN_BOTTOM_MID, 0, 0);
            } else if (showBaby) {
                lv_obj_set_size(pnl_baby, 1280, 720);
                lv_obj_align(pnl_baby, LV_ALIGN_CENTER, 0, 0);
            } else {
                lv_obj_set_size(pnl_cat, 1280, 720);
                lv_obj_align(pnl_cat, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }

    if (layout == 0) return;

    // Farben blinken lassen - nur beim Phasenwechsel neu setzen.
    const int blink = (millis() % 600 < 300) ? 1 : 0;
    if (blink == s_lastBlink) return;
    s_lastBlink = blink;

    if (showBaby) lv_obj_set_style_bg_color(pnl_baby, blink ? lv_color_hex(0xFF0000) : lv_color_hex(0x660000), 0);
    if (showCat)  lv_obj_set_style_bg_color(pnl_cat,  blink ? lv_color_hex(0xFF4400) : lv_color_hex(0x882200), 0);
}
