// ==============================================================================
// SettingsUi - Implementierung (siehe Header)
// ==============================================================================

#include "SettingsUi.h"
#include "UiTheme.h"
#include "UiSound.h"      // Tastentoene
#include <string.h>

namespace SettingsUi {

// ------------------------------------------------------------------------------
// Gemeinsame Stile
// ------------------------------------------------------------------------------
static bool s_stylesInit = false;

static lv_style_t s_page, s_section, s_card, s_row, s_rowPressed, s_textCol;
static lv_style_t s_title, s_sub, s_back, s_secTitle, s_foot, s_label, s_desc;
static lv_style_t s_btn, s_btnPressed, s_btnPrimary, s_btnTonal, s_btnDanger, s_btnDisabled;
static lv_style_t s_swMain, s_swInd, s_swKnob;
static lv_style_t s_dd, s_ddList, s_ddSel;
static lv_style_t s_slMain, s_slInd, s_slKnob, s_pill;
static lv_style_t s_ta, s_taFocus, s_taCursor, s_taPlaceholder;
static lv_style_t s_segTrack, s_segItem, s_segItemOn;
static lv_style_t s_chip, s_dot, s_actions, s_btnRow;
static lv_style_t s_kbMain, s_kbItems, s_kbItemsOn;

static lv_obj_t* s_kb   = nullptr;   // Tastatur (gehoert dem Bildschirm)
static lv_obj_t* s_page_obj = nullptr;   // aktuelle Seite (fuer den Tastatur-Platz)

// Ungespeicherte Aenderungen der aktuellen Seite (siehe Header).
static bool s_guarded = false;   // Seite hat eine Leiste "Uebernehmen"
static bool s_dirty   = false;   // seit dem Aufbau wurde etwas veraendert

// Offene Rueckfrage (liegt auf lv_layer_top, gehoert keinem Bildschirm).
static lv_obj_t* s_confirm = nullptr;
static void    (*s_confirmOk)(void*) = nullptr;
static void*     s_confirmData = nullptr;

// Leert einen Stil und fuellt ihn neu (Farbwechsel ohne neue Allokation je Objekt).
static void fresh(lv_style_t* s) {
    if (s_stylesInit) lv_style_reset(s);
    lv_style_init(s);
}

static void flat(lv_style_t* s) {
    lv_style_set_border_width(s, 0);
    lv_style_set_shadow_width(s, 0);
    lv_style_set_outline_width(s, 0);
}

void begin() {
    // Zeiger des vorherigen Aufbaus sind ungueltig (Bildschirm geloescht).
    s_kb = nullptr;
    s_page_obj = nullptr;
    s_guarded = false;
    s_dirty = false;
    closeConfirm();   // gehoerte zum vorherigen Aufbau
    initStyles();
}

void initStyles() {
    // Nur neu fuellen, wenn sich das Schema seit dem letzten Mal geaendert hat:
    // Andere Ansichten (Schnelleinstellungen) rufen das bei jedem Oeffnen.
    static int8_t s_filledDark = -1;
    if (s_stylesInit && s_filledDark == (int8_t)UiTheme::isDark()) return;
    s_filledDark = (int8_t)UiTheme::isDark();

    const UiColors& c = UiTheme::c();

    fresh(&s_page);
    lv_style_set_bg_opa(&s_page, LV_OPA_TRANSP);
    flat(&s_page);
    lv_style_set_radius(&s_page, 0);
    lv_style_set_pad_top(&s_page, 22);
    lv_style_set_pad_bottom(&s_page, 32);
    lv_style_set_pad_left(&s_page, 36);
    lv_style_set_pad_right(&s_page, 36);
    lv_style_set_pad_row(&s_page, 22);

    fresh(&s_section);
    lv_style_set_bg_opa(&s_section, LV_OPA_TRANSP);
    flat(&s_section);
    lv_style_set_pad_all(&s_section, 0);
    lv_style_set_pad_row(&s_section, 10);

    fresh(&s_card);
    lv_style_set_bg_color(&s_card, c.surface);
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    flat(&s_card);
    lv_style_set_radius(&s_card, 20);
    lv_style_set_clip_corner(&s_card, true);
    lv_style_set_pad_all(&s_card, 0);
    lv_style_set_pad_row(&s_card, 0);

    fresh(&s_row);
    lv_style_set_bg_opa(&s_row, LV_OPA_TRANSP);
    lv_style_set_shadow_width(&s_row, 0);
    lv_style_set_outline_width(&s_row, 0);
    lv_style_set_radius(&s_row, 0);
    lv_style_set_border_side(&s_row, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&s_row, c.divider);
    lv_style_set_border_width(&s_row, 1);
    lv_style_set_pad_left(&s_row, 24);
    lv_style_set_pad_right(&s_row, 24);
    lv_style_set_pad_top(&s_row, 14);
    lv_style_set_pad_bottom(&s_row, 14);
    lv_style_set_pad_column(&s_row, 22);
    lv_style_set_min_height(&s_row, 80);

    fresh(&s_rowPressed);
    lv_style_set_bg_color(&s_rowPressed, c.surface2);
    lv_style_set_bg_opa(&s_rowPressed, LV_OPA_COVER);

    fresh(&s_textCol);
    lv_style_set_bg_opa(&s_textCol, LV_OPA_TRANSP);
    flat(&s_textCol);
    lv_style_set_pad_all(&s_textCol, 0);
    lv_style_set_pad_row(&s_textCol, 5);

    fresh(&s_title);
    lv_style_set_text_font(&s_title, &lv_font_montserrat_36);
    lv_style_set_text_color(&s_title, c.text);

    fresh(&s_sub);
    lv_style_set_text_font(&s_sub, &lv_font_montserrat_16);
    lv_style_set_text_color(&s_sub, c.muted);

    fresh(&s_back);
    lv_style_set_text_font(&s_back, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_back, c.link);

    fresh(&s_secTitle);
    lv_style_set_text_font(&s_secTitle, &lv_font_montserrat_16);
    lv_style_set_text_color(&s_secTitle, c.muted);
    lv_style_set_text_letter_space(&s_secTitle, 1);
    lv_style_set_pad_left(&s_secTitle, 8);

    fresh(&s_foot);
    lv_style_set_text_font(&s_foot, &lv_font_montserrat_16);
    lv_style_set_text_color(&s_foot, c.muted);
    lv_style_set_pad_left(&s_foot, 8);
    lv_style_set_pad_right(&s_foot, 8);

    fresh(&s_label);
    lv_style_set_text_font(&s_label, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_label, c.text);

    fresh(&s_desc);
    lv_style_set_text_font(&s_desc, &lv_font_montserrat_16);
    lv_style_set_text_color(&s_desc, c.muted);

    // --- Knoepfe: Pillenform -------------------------------------------------
    fresh(&s_btn);
    flat(&s_btn);
    lv_style_set_radius(&s_btn, LV_RADIUS_CIRCLE);
    lv_style_set_height(&s_btn, 52);
    lv_style_set_pad_left(&s_btn, 26);
    lv_style_set_pad_right(&s_btn, 26);
    lv_style_set_pad_top(&s_btn, 0);
    lv_style_set_pad_bottom(&s_btn, 0);
    lv_style_set_bg_opa(&s_btn, LV_OPA_COVER);
    lv_style_set_text_font(&s_btn, &lv_font_montserrat_20);

    fresh(&s_btnPrimary);
    lv_style_set_bg_color(&s_btnPrimary, c.primary);
    lv_style_set_text_color(&s_btnPrimary, c.onPrimary);

    fresh(&s_btnTonal);
    lv_style_set_bg_color(&s_btnTonal, c.surface2);
    lv_style_set_text_color(&s_btnTonal, c.text);

    fresh(&s_btnDanger);
    lv_style_set_bg_color(&s_btnDanger, c.dangerBg);
    lv_style_set_text_color(&s_btnDanger, c.danger);

    // Gedrueckt: abdunkeln statt Schatten/Animation.
    fresh(&s_btnPressed);
    lv_style_set_bg_img_recolor(&s_btnPressed, lv_color_black());
    lv_style_set_bg_img_recolor_opa(&s_btnPressed, LV_OPA_20);
    lv_style_set_img_recolor_opa(&s_btnPressed, LV_OPA_20);
    lv_style_set_transform_width(&s_btnPressed, 0);
    lv_style_set_transform_height(&s_btnPressed, 0);
    lv_style_set_bg_opa(&s_btnPressed, LV_OPA_80);

    // Gesperrt: nur noch ein Umriss in der Farbe der tonalen Knoepfe, Schrift
    // gedaempft - sichtbar schwaecher als jeder bedienbare Knopf, in beiden
    // Schemata.
    // Der Farbfilter des LVGL-Standardthemas wird dabei abgeschaltet: Es laeuft
    // im hellen Modus (lv_conf.h) und mischt gesperrte Objekte zu 50 % mit
    // Hellgrau. Im dunklen Schema wurde ein gesperrter Knopf dadurch HELLER als
    // die bedienbaren und wirkte hervorgehoben, das Symbol darauf grau in grau.
    fresh(&s_btnDisabled);
    lv_style_set_color_filter_opa(&s_btnDisabled, LV_OPA_TRANSP);
    lv_style_set_bg_opa(&s_btnDisabled, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_btnDisabled, 2);
    lv_style_set_border_color(&s_btnDisabled, c.surface2);
    lv_style_set_border_opa(&s_btnDisabled, LV_OPA_COVER);
    lv_style_set_text_color(&s_btnDisabled, c.off);

    // --- Schalter --------------------------------------------------------------
    fresh(&s_swMain);
    lv_style_set_bg_color(&s_swMain, c.switchOff);
    lv_style_set_bg_opa(&s_swMain, LV_OPA_COVER);
    lv_style_set_radius(&s_swMain, LV_RADIUS_CIRCLE);
    flat(&s_swMain);

    fresh(&s_swInd);
    lv_style_set_bg_color(&s_swInd, c.accent);
    lv_style_set_bg_opa(&s_swInd, LV_OPA_COVER);

    fresh(&s_swKnob);
    lv_style_set_bg_color(&s_swKnob, lv_color_white());
    lv_style_set_bg_opa(&s_swKnob, LV_OPA_COVER);
    lv_style_set_pad_all(&s_swKnob, -4);
    flat(&s_swKnob);

    // --- Auswahlliste ----------------------------------------------------------
    fresh(&s_dd);
    lv_style_set_bg_color(&s_dd, c.surface2);
    lv_style_set_bg_opa(&s_dd, LV_OPA_COVER);
    flat(&s_dd);
    lv_style_set_radius(&s_dd, 14);
    lv_style_set_pad_left(&s_dd, 16);
    lv_style_set_pad_right(&s_dd, 16);
    lv_style_set_pad_top(&s_dd, 13);
    lv_style_set_pad_bottom(&s_dd, 13);
    lv_style_set_text_font(&s_dd, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_dd, c.text);

    fresh(&s_ddList);
    lv_style_set_bg_color(&s_ddList, c.surface);
    lv_style_set_bg_opa(&s_ddList, LV_OPA_COVER);
    lv_style_set_border_color(&s_ddList, c.divider);
    lv_style_set_border_width(&s_ddList, 1);
    lv_style_set_shadow_width(&s_ddList, 0);
    lv_style_set_radius(&s_ddList, 14);
    lv_style_set_text_font(&s_ddList, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_ddList, c.text);
    lv_style_set_text_line_space(&s_ddList, 24);   // grosse Beruehrflaechen
    lv_style_set_max_height(&s_ddList, 400);

    fresh(&s_ddSel);
    lv_style_set_bg_color(&s_ddSel, c.primary);
    lv_style_set_bg_opa(&s_ddSel, LV_OPA_COVER);
    lv_style_set_text_color(&s_ddSel, c.onPrimary);

    // --- Regler ---------------------------------------------------------------
    fresh(&s_slMain);
    lv_style_set_bg_color(&s_slMain, c.surface2);
    lv_style_set_bg_opa(&s_slMain, LV_OPA_COVER);
    lv_style_set_radius(&s_slMain, LV_RADIUS_CIRCLE);
    flat(&s_slMain);

    fresh(&s_slInd);
    lv_style_set_bg_color(&s_slInd, c.accent);
    lv_style_set_bg_opa(&s_slInd, LV_OPA_COVER);
    lv_style_set_radius(&s_slInd, LV_RADIUS_CIRCLE);

    fresh(&s_slKnob);
    lv_style_set_bg_color(&s_slKnob, c.accent);
    lv_style_set_bg_opa(&s_slKnob, LV_OPA_COVER);
    lv_style_set_radius(&s_slKnob, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&s_slKnob, 9);
    flat(&s_slKnob);

    fresh(&s_pill);
    lv_style_set_bg_color(&s_pill, c.surface2);
    lv_style_set_bg_opa(&s_pill, LV_OPA_COVER);
    lv_style_set_radius(&s_pill, 12);
    lv_style_set_pad_left(&s_pill, 12);
    lv_style_set_pad_right(&s_pill, 12);
    lv_style_set_pad_top(&s_pill, 8);
    lv_style_set_pad_bottom(&s_pill, 8);
    lv_style_set_min_width(&s_pill, 108);
    lv_style_set_text_font(&s_pill, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_pill, c.text);
    lv_style_set_text_align(&s_pill, LV_TEXT_ALIGN_CENTER);

    // --- Textfeld ---------------------------------------------------------------
    fresh(&s_ta);
    lv_style_set_bg_color(&s_ta, c.surface2);
    lv_style_set_bg_opa(&s_ta, LV_OPA_COVER);
    lv_style_set_border_color(&s_ta, c.surface2);
    lv_style_set_border_width(&s_ta, 2);
    lv_style_set_shadow_width(&s_ta, 0);
    lv_style_set_outline_width(&s_ta, 0);
    lv_style_set_radius(&s_ta, 14);
    lv_style_set_pad_left(&s_ta, 16);
    lv_style_set_pad_right(&s_ta, 16);
    lv_style_set_pad_top(&s_ta, 12);
    lv_style_set_pad_bottom(&s_ta, 12);
    lv_style_set_text_font(&s_ta, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_ta, c.text);

    fresh(&s_taFocus);
    lv_style_set_border_color(&s_taFocus, c.accent);

    fresh(&s_taCursor);
    lv_style_set_border_color(&s_taCursor, c.accent);
    lv_style_set_border_width(&s_taCursor, 2);
    lv_style_set_border_side(&s_taCursor, LV_BORDER_SIDE_LEFT);

    fresh(&s_taPlaceholder);
    lv_style_set_text_color(&s_taPlaceholder, c.muted);

    // --- Segmente -----------------------------------------------------------------
    fresh(&s_segTrack);
    lv_style_set_bg_color(&s_segTrack, c.surface2);
    lv_style_set_bg_opa(&s_segTrack, LV_OPA_COVER);
    flat(&s_segTrack);
    lv_style_set_radius(&s_segTrack, 16);
    lv_style_set_pad_all(&s_segTrack, 5);
    lv_style_set_pad_column(&s_segTrack, 4);

    fresh(&s_segItem);
    lv_style_set_bg_opa(&s_segItem, LV_OPA_TRANSP);
    flat(&s_segItem);
    lv_style_set_radius(&s_segItem, 12);
    lv_style_set_height(&s_segItem, 46);
    lv_style_set_pad_left(&s_segItem, 20);
    lv_style_set_pad_right(&s_segItem, 20);
    lv_style_set_pad_top(&s_segItem, 0);
    lv_style_set_pad_bottom(&s_segItem, 0);
    lv_style_set_text_font(&s_segItem, &lv_font_montserrat_20);
    lv_style_set_text_color(&s_segItem, c.muted);

    fresh(&s_segItemOn);
    lv_style_set_bg_color(&s_segItemOn, c.surface);
    lv_style_set_bg_opa(&s_segItemOn, LV_OPA_COVER);
    lv_style_set_text_color(&s_segItemOn, c.text);

    // --- Chip, Aktionen, Tastatur ------------------------------------------------
    fresh(&s_chip);
    lv_style_set_bg_opa(&s_chip, LV_OPA_COVER);
    flat(&s_chip);
    lv_style_set_radius(&s_chip, LV_RADIUS_CIRCLE);
    lv_style_set_pad_left(&s_chip, 12);
    lv_style_set_pad_right(&s_chip, 12);
    lv_style_set_pad_top(&s_chip, 5);
    lv_style_set_pad_bottom(&s_chip, 5);
    lv_style_set_pad_column(&s_chip, 8);
    lv_style_set_text_font(&s_chip, &lv_font_montserrat_16);

    fresh(&s_dot);
    lv_style_set_radius(&s_dot, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&s_dot, LV_OPA_COVER);
    flat(&s_dot);
    lv_style_set_width(&s_dot, 8);
    lv_style_set_height(&s_dot, 8);

    // Knopfzeile im Fluss einer Ansicht (buttonRow): ohne eigene Raender.
    fresh(&s_btnRow);
    lv_style_set_bg_opa(&s_btnRow, LV_OPA_TRANSP);
    flat(&s_btnRow);
    lv_style_set_pad_all(&s_btnRow, 0);
    lv_style_set_pad_column(&s_btnRow, 12);

    // Leiste unter dem scrollenden Inhalt: Raender wie die Seite, oben eine
    // Trennlinie zum Inhalt, der darueber "verschwindet".
    fresh(&s_actions);
    lv_style_set_bg_opa(&s_actions, LV_OPA_TRANSP);
    flat(&s_actions);
    lv_style_set_border_width(&s_actions, 1);
    lv_style_set_border_side(&s_actions, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&s_actions, c.divider);
    lv_style_set_border_opa(&s_actions, LV_OPA_COVER);
    lv_style_set_pad_left(&s_actions, 36);
    lv_style_set_pad_right(&s_actions, 36);
    lv_style_set_pad_top(&s_actions, 14);
    lv_style_set_pad_bottom(&s_actions, 16);
    lv_style_set_pad_column(&s_actions, 12);

    fresh(&s_kbMain);
    lv_style_set_bg_color(&s_kbMain, c.side);
    lv_style_set_bg_opa(&s_kbMain, LV_OPA_COVER);
    lv_style_set_border_side(&s_kbMain, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&s_kbMain, c.divider);
    lv_style_set_border_width(&s_kbMain, 1);
    lv_style_set_shadow_width(&s_kbMain, 0);

    fresh(&s_kbItems);
    lv_style_set_bg_color(&s_kbItems, c.surface2);
    lv_style_set_bg_opa(&s_kbItems, LV_OPA_COVER);
    lv_style_set_text_color(&s_kbItems, c.text);
    lv_style_set_text_font(&s_kbItems, &lv_font_montserrat_24);
    lv_style_set_radius(&s_kbItems, 10);
    lv_style_set_shadow_width(&s_kbItems, 0);
    lv_style_set_border_width(&s_kbItems, 0);

    fresh(&s_kbItemsOn);
    lv_style_set_bg_color(&s_kbItemsOn, c.primary);
    lv_style_set_text_color(&s_kbItemsOn, c.onPrimary);

    s_stylesInit = true;
}

// ------------------------------------------------------------------------------
// Hilfen
// ------------------------------------------------------------------------------
// Reiner Layout-Container. lv_obj_create() liefert ein KLICKBARES Objekt; ein
// klickbarer Container faengt den Tipp ab, ohne ihn an die klickbare Zeile
// darueber weiterzugeben (z. B. die Textspalte einer Link-Zeile). Deshalb hier
// nicht klickbar; wer klickbar sein soll, setzt das Flag ausdruecklich.
static lv_obj_t* plainBox(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t* text(lv_obj_t* parent, const char* t, lv_style_t* style) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_add_style(l, style, 0);
    lv_label_set_text(l, t ? t : "");
    return l;
}

// Wischgesten nicht an den Bildschirm weiterreichen: Ziehen an einem Regler
// oder Scrollen einer Liste darf nicht "zurueck zum Dashboard" ausloesen.
static void noGesture(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

// Tastenton wie in den uebrigen Ansichten, zentral fuer alle Bedienelemente der
// Bausteine (UiSound). Wird NACH dem Callback der Seite angehaengt: spielt die
// Seite selbst einen eigenen Ton, gewinnt dieser.
static void addClickTone(lv_obj_t* o) {
    UiSound_AttachClick(o);
}

// ------------------------------------------------------------------------------
// Ungespeicherte Aenderungen
// ------------------------------------------------------------------------------
// Jedes Bedienelement mit Wert meldet seine Aenderung hierher. Ausgewertet
// wird das nur auf Seiten mit Leiste (s_guarded) - auf allen anderen wirken
// die Bedienelemente sofort, dort gibt es nichts zu verlieren.
static void dirtyEvent(lv_event_t* e) {
    if (lv_obj_has_flag(lv_event_get_target(e), LV_OBJ_FLAG_USER_1)) return;   // setTransient
    s_dirty = true;
}

static void trackChanges(lv_obj_t* control) {
    lv_obj_add_event_cb(control, dirtyEvent, LV_EVENT_VALUE_CHANGED, nullptr);
}

bool hasUnsavedChanges() { return s_guarded && s_dirty; }
void markDirty()         { s_dirty = true; }
void clearDirty()        { s_dirty = false; }

void setTransient(lv_obj_t* control) {
    if (control) lv_obj_add_flag(control, LV_OBJ_FLAG_USER_1);
}

// ------------------------------------------------------------------------------
// Tastatur
// ------------------------------------------------------------------------------
// Unter der Tastatur (300 px) bleibt die Seite bedienbar: Sie bekommt so lange
// Platz am Ende, und das Feld wird in den sichtbaren Bereich gescrollt.
static const lv_coord_t KB_HEIGHT = 300;

static void keyboardShow(lv_obj_t* ta, bool digits) {
    if (!s_kb) return;
    lv_keyboard_set_textarea(s_kb, ta);
    lv_keyboard_set_mode(s_kb, digits ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_kb);
    if (s_page_obj) {
        lv_obj_set_style_pad_bottom(s_page_obj, KB_HEIGHT + 32, 0);
        lv_obj_update_layout(s_page_obj);
    }
    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
}

static void keyboardHide() {
    if (!s_kb) return;
    lv_keyboard_set_textarea(s_kb, nullptr);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    if (s_page_obj) lv_obj_set_style_pad_bottom(s_page_obj, 32, 0);
}

static void textareaEvent(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED) {
        keyboardShow(ta, lv_event_get_user_data(e) != nullptr);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY) {
        keyboardHide();
    }
}

static void keyboardEvent(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) return;
    lv_obj_t* ta = lv_keyboard_get_textarea(s_kb);
    if (ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    keyboardHide();
}

void setKeyboard(lv_obj_t* kb) {
    s_kb = kb;
    if (!kb) return;
    lv_obj_add_style(kb, &s_kbMain, 0);
    lv_obj_add_style(kb, &s_kbItems, LV_PART_ITEMS);
    lv_obj_add_style(kb, &s_kbItemsOn, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_style(kb, &s_btnPressed, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_size(kb, 1280, KB_HEIGHT);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    noGesture(kb);
    lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_ALL, nullptr);
}

// ------------------------------------------------------------------------------
// Seite, Kopf, Abschnitt, Zeile
// ------------------------------------------------------------------------------
lv_obj_t* page(lv_obj_t* parent) {
    keyboardHide();
    lv_obj_clean(parent);

    // Rahmen: oben die scrollende Spalte (nimmt den Platz, der bleibt), darunter
    // bei Bedarf die feste Leiste (actions). Der Rahmen selbst scrollt nicht.
    lv_obj_t* frame = plainBox(parent);
    lv_obj_set_size(frame, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(frame, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* p = lv_obj_create(frame);
    lv_obj_remove_style_all(p);
    lv_obj_add_style(p, &s_page, 0);
    lv_obj_set_width(p, lv_pct(100));
    lv_obj_set_flex_grow(p, 1);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_ACTIVE);
    s_page_obj = p;
    s_guarded = false;
    s_dirty = false;
    return p;
}

lv_obj_t* pageHost(lv_obj_t* pg) {
    return lv_obj_get_parent(lv_obj_get_parent(pg));   // Seite -> Rahmen -> Gastgeber
}

void header(lv_obj_t* pg, const char* title, const char* sub, const char* backText, lv_event_cb_t backCb) {
    lv_obj_t* h = plainBox(pg);
    lv_obj_set_size(h, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(h, 6, 0);

    if (backText) {
        // Zurueck-Link: grosse, unsichtbare Beruehrflaeche um "‹ Bereich".
        lv_obj_t* b = plainBox(h);
        lv_obj_set_size(b, LV_SIZE_CONTENT, 44);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(b, 12);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(b, 8, 0);
        text(b, LV_SYMBOL_LEFT, &s_back);
        text(b, backText, &s_back);
        if (backCb) lv_obj_add_event_cb(b, backCb, LV_EVENT_CLICKED, nullptr);
        addClickTone(b);
    }
    text(h, title, &s_title);
    if (sub) text(h, sub, &s_sub);
}

lv_obj_t* section(lv_obj_t* pg, const char* title, const char* foot) {
    lv_obj_t* s = plainBox(pg);
    lv_obj_add_style(s, &s_section, 0);
    lv_obj_set_size(s, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    if (title) text(s, title, &s_secTitle);

    lv_obj_t* card = plainBox(s);
    lv_obj_add_style(card, &s_card, 0);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    if (foot) {
        lv_obj_t* f = text(s, foot, &s_foot);
        lv_obj_set_width(f, lv_pct(100));
        lv_label_set_long_mode(f, LV_LABEL_LONG_WRAP);
    }
    return card;
}

Row row(lv_obj_t* card, const char* label, const char* desc) {
    Row r;
    const bool first = lv_obj_get_child_cnt(card) == 0;
    r.obj = plainBox(card);
    lv_obj_add_style(r.obj, &s_row, 0);
    if (first) lv_obj_set_style_border_width(r.obj, 0, 0);
    lv_obj_set_size(r.obj, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r.obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r.obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    r.text = plainBox(r.obj);
    lv_obj_add_style(r.text, &s_textCol, 0);
    lv_obj_set_height(r.text, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(r.text, 1);
    lv_obj_set_flex_flow(r.text, LV_FLEX_FLOW_COLUMN);

    r.label = text(r.text, label, &s_label);
    lv_obj_set_width(r.label, lv_pct(100));
    lv_label_set_long_mode(r.label, LV_LABEL_LONG_WRAP);
    if (desc) setDesc(r, desc);
    return r;
}

void setDesc(const Row& r, const char* t) {
    Row& m = const_cast<Row&>(r);
    if (!m.desc) {
        m.desc = text(m.text, "", &s_desc);
        lv_obj_set_width(m.desc, lv_pct(100));
        lv_label_set_long_mode(m.desc, LV_LABEL_LONG_WRAP);
        lv_obj_move_to_index(m.desc, 1);   // direkt unter den Titel
    }
    lv_label_set_text(m.desc, t ? t : "");
}

Row linkRow(lv_obj_t* card, const char* label, const char* desc, const char* value,
            lv_event_cb_t cb, void* userData, lv_obj_t** valueLabel) {
    Row r = row(card, label, desc);
    lv_obj_add_flag(r.obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(r.obj, &s_rowPressed, LV_STATE_PRESSED);
    lv_obj_t* v = text(r.obj, value ? value : "", &s_desc);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    lv_obj_t* chev = text(r.obj, LV_SYMBOL_RIGHT, &s_desc);
    lv_obj_set_style_text_font(chev, &lv_font_montserrat_20, 0);
    if (cb) lv_obj_add_event_cb(r.obj, cb, LV_EVENT_CLICKED, userData);
    addClickTone(r.obj);
    if (valueLabel) *valueLabel = v;
    return r;
}

// ------------------------------------------------------------------------------
// Chip
// ------------------------------------------------------------------------------
static void chipColors(Chip kind, lv_color_t* fg, lv_color_t* bg) {
    const UiColors& c = UiTheme::c();
    switch (kind) {
        case Chip::Ok:     *fg = c.ok;     *bg = c.okBg;     break;
        case Chip::Warn:   *fg = c.warn;   *bg = c.warnBg;   break;
        case Chip::Danger: *fg = c.danger; *bg = c.dangerBg; break;
        default:           *fg = c.muted;  *bg = c.surface2; break;
    }
}

lv_obj_t* chip(const Row& r, const char* t, Chip kind) {
    lv_obj_t* ch = plainBox(r.text);
    lv_obj_add_style(ch, &s_chip, 0);
    lv_obj_set_size(ch, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ch, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ch, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* dot = plainBox(ch);
    lv_obj_add_style(dot, &s_dot, 0);
    lv_label_create(ch);
    setChip(ch, t, kind);
    return ch;
}

void setChip(lv_obj_t* ch, const char* t, Chip kind) {
    if (!ch) return;
    lv_color_t fg, bg;
    chipColors(kind, &fg, &bg);
    lv_obj_set_style_bg_color(ch, bg, 0);
    lv_obj_set_style_text_color(ch, fg, 0);
    lv_obj_set_style_bg_color(lv_obj_get_child(ch, 0), fg, 0);
    lv_label_set_text(lv_obj_get_child(ch, 1), t ? t : "");
}

// ------------------------------------------------------------------------------
// Bedienelemente
// ------------------------------------------------------------------------------
// ------------------------------------------------------------------------------
// Stile fuer vorhandene Bedienelemente (andere Ansichten)
// ------------------------------------------------------------------------------
void styleSwitch(lv_obj_t* sw) {
    lv_obj_add_style(sw, &s_swMain, 0);
    lv_obj_add_style(sw, &s_swMain, LV_PART_INDICATOR);
    lv_obj_add_style(sw, &s_swInd, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(sw, &s_swKnob, LV_PART_KNOB);
}

void styleSlider(lv_obj_t* sl) {
    lv_obj_add_style(sl, &s_slMain, 0);
    lv_obj_add_style(sl, &s_slInd, LV_PART_INDICATOR);
    lv_obj_add_style(sl, &s_slKnob, LV_PART_KNOB);
}

void styleDropdown(lv_obj_t* dd) {
    lv_obj_add_style(dd, &s_dd, 0);
    lv_obj_add_style(dd, &s_btnPressed, LV_STATE_PRESSED);
    lv_obj_t* list = lv_dropdown_get_list(dd);
    lv_obj_add_style(list, &s_ddList, 0);
    lv_obj_add_style(list, &s_ddSel, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_add_style(list, &s_ddSel, LV_PART_SELECTED | LV_STATE_PRESSED);
    noGesture(list);
}

void styleButton(lv_obj_t* b, Btn kind) {
    lv_obj_add_style(b, &s_btn, 0);
    lv_obj_add_style(b, kind == Btn::Primary ? &s_btnPrimary : (kind == Btn::Danger ? &s_btnDanger : &s_btnTonal), 0);
    lv_obj_add_style(b, &s_btnPressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &s_btnDisabled, LV_STATE_DISABLED);
}

// ------------------------------------------------------------------------------
// Bedienelemente
// ------------------------------------------------------------------------------
lv_obj_t* addSwitch(const Row& r, bool on, lv_event_cb_t cb, void* userData) {
    lv_obj_t* sw = lv_switch_create(r.obj);
    lv_obj_set_size(sw, 72, 40);
    styleSwitch(sw);
    lv_obj_set_ext_click_area(sw, 14);
    noGesture(sw);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    if (cb) lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, userData);
    trackChanges(sw);
    addClickTone(sw);
    return sw;
}

lv_obj_t* addDropdown(const Row& r, const char* options, uint16_t selected, lv_event_cb_t cb, lv_coord_t width) {
    lv_obj_t* dd = lv_dropdown_create(r.obj);
    lv_obj_set_width(dd, width);
    styleDropdown(dd);
    lv_dropdown_set_options(dd, options);
    lv_dropdown_set_selected(dd, selected);
    lv_dropdown_set_symbol(dd, LV_SYMBOL_DOWN);
    noGesture(dd);

    if (cb) lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    trackChanges(dd);
    addClickTone(dd);   // Oeffnen der Liste; die Auswahl darin meldet VALUE_CHANGED
    return dd;
}

lv_obj_t* addSlider(const Row& r, int min, int max, int value, lv_event_cb_t cb, lv_obj_t** valueLabel) {
    lv_obj_t* sl = lv_slider_create(r.obj);
    lv_obj_set_size(sl, 260, 8);
    styleSlider(sl);
    lv_obj_set_ext_click_area(sl, 24);
    lv_slider_set_range(sl, min, max);
    lv_slider_set_value(sl, value, LV_ANIM_OFF);
    noGesture(sl);
    if (cb) lv_obj_add_event_cb(sl, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    trackChanges(sl);
    UiSound_AttachSlider(sl);

    lv_obj_t* pill = text(r.obj, "", &s_pill);
    if (valueLabel) *valueLabel = pill;
    return sl;
}

lv_obj_t* addTextarea(const Row& r, const char* t, const char* placeholder, lv_coord_t width,
                      bool digitsOnly, bool password) {
    lv_obj_t* ta = lv_textarea_create(r.obj);
    lv_obj_set_width(ta, width);
    lv_obj_add_style(ta, &s_ta, 0);
    lv_obj_add_style(ta, &s_taFocus, LV_STATE_FOCUSED);
    lv_obj_add_style(ta, &s_taCursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_style(ta, &s_taPlaceholder, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_textarea_set_one_line(ta, true);
    if (digitsOnly) {
        lv_textarea_set_accepted_chars(ta, "0123456789");
        lv_textarea_set_max_length(ta, 5);
    }
    if (password) lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_text(ta, t ? t : "");
    if (placeholder) lv_textarea_set_placeholder_text(ta, placeholder);
    noGesture(ta);
    lv_obj_add_event_cb(ta, textareaEvent, LV_EVENT_ALL, digitsOnly ? (void*)1 : nullptr);
    trackChanges(ta);   // nach dem Anfangstext: der zaehlt nicht als Aenderung
    addClickTone(ta);
    return ta;
}

lv_obj_t* addValue(const Row& r, const char* t) {
    lv_obj_t* v = text(r.obj, t, &s_desc);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    return v;
}

lv_obj_t* addButton(lv_obj_t* parent, const char* t, Btn kind, lv_event_cb_t cb, void* userData) {
    lv_obj_t* b = lv_btn_create(parent);
    styleButton(b, kind);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    noGesture(b);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, userData);
    addClickTone(b);
    return b;
}

void setButtonKind(lv_obj_t* btn, Btn kind) {
    lv_obj_remove_style(btn, &s_btnPrimary, 0);
    lv_obj_remove_style(btn, &s_btnTonal, 0);
    lv_obj_remove_style(btn, &s_btnDanger, 0);
    lv_obj_add_style(btn, kind == Btn::Primary ? &s_btnPrimary : (kind == Btn::Danger ? &s_btnDanger : &s_btnTonal), 0);
}

// ------------------------------------------------------------------------------
// Segmente
// ------------------------------------------------------------------------------
static void segItemClicked(lv_event_t* e) {
    lv_obj_t* item = lv_event_get_target(e);
    lv_obj_t* seg  = lv_obj_get_parent(item);
    const uint32_t idx = lv_obj_get_index(item);
    if ((uintptr_t)lv_obj_get_user_data(seg) == idx) return;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(seg); i++) {
        lv_obj_t* it = lv_obj_get_child(seg, i);
        if (i == idx) lv_obj_add_state(it, LV_STATE_CHECKED);
        else          lv_obj_clear_state(it, LV_STATE_CHECKED);
    }
    lv_obj_set_user_data(seg, (void*)(uintptr_t)idx);
    lv_event_send(seg, LV_EVENT_VALUE_CHANGED, nullptr);
}

lv_obj_t* segmented(lv_obj_t* parent, const char* const* items, uint8_t count, uint8_t selected,
                    lv_event_cb_t cb, void* userData) {
    lv_obj_t* seg = plainBox(parent);
    lv_obj_add_style(seg, &s_segTrack, 0);
    lv_obj_set_size(seg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_user_data(seg, (void*)(uintptr_t)selected);
    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t* it = lv_btn_create(seg);
        lv_obj_remove_style_all(it);
        lv_obj_add_style(it, &s_segItem, 0);
        lv_obj_add_style(it, &s_segItemOn, LV_STATE_CHECKED);
        lv_obj_set_width(it, LV_SIZE_CONTENT);
        noGesture(it);
        lv_obj_t* l = lv_label_create(it);
        lv_label_set_text(l, items[i]);
        lv_obj_center(l);
        if (i == selected) lv_obj_add_state(it, LV_STATE_CHECKED);
        lv_obj_add_event_cb(it, segItemClicked, LV_EVENT_CLICKED, nullptr);
        addClickTone(it);
    }
    if (cb) lv_obj_add_event_cb(seg, cb, LV_EVENT_VALUE_CHANGED, userData);
    trackChanges(seg);
    return seg;
}

uint8_t segmentedSelected(lv_obj_t* seg) {
    return (uint8_t)(uintptr_t)lv_obj_get_user_data(seg);
}

// ------------------------------------------------------------------------------
// Text und Aktionsleiste
// ------------------------------------------------------------------------------
lv_obj_t* note(lv_obj_t* parent, const char* t) {
    lv_obj_t* l = text(parent, t, &s_desc);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}

lv_obj_t* buttonRow(lv_obj_t* parent) {
    lv_obj_t* a = plainBox(parent);
    lv_obj_add_style(a, &s_btnRow, 0);
    lv_obj_set_size(a, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(a, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return a;
}

lv_obj_t* actions(lv_obj_t* pg) {
    // Nur fuer Seiten aus page(): Dort gibt es den Rahmen, in den die Leiste
    // gehoert. Fuer alles andere ist buttonRow gedacht - wer trotzdem hier
    // landet, bekommt die Zeile an Ort und Stelle statt im falschen Elternobjekt
    // (so standen die Knoepfe der Schnelleinstellungen am 16.11.2026 oben rechts).
    if (pg != s_page_obj) return buttonRow(pg);

    // Im Rahmen, nicht in der scrollenden Spalte: so bleibt die Leiste stehen.
    lv_obj_t* a = plainBox(lv_obj_get_parent(pg));
    lv_obj_add_style(a, &s_actions, 0);
    lv_obj_set_size(a, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(a, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Ab hier ist die Seite fertig aufgebaut: was sich jetzt noch aendert,
    // ist eine Aenderung durch den Nutzer (oder fuer ihn).
    s_guarded = true;
    s_dirty = false;
    return a;
}

// ------------------------------------------------------------------------------
// Rueckfrage
// ------------------------------------------------------------------------------
void closeConfirm() {
    if (s_confirm) lv_obj_del(s_confirm);
    s_confirm = nullptr;
    s_confirmOk = nullptr;
    s_confirmData = nullptr;
}

// Beide Knoepfe schliessen den Kasten; nur "OK" (user_data != nullptr) ruft
// danach die Aktion. Geloescht wird aufgeschoben - der Knopf, dessen Ereignis
// gerade laeuft, gehoert zum Kasten.
static void confirmClicked(lv_event_t* e) {
    void (*onOk)(void*) = lv_event_get_user_data(e) ? s_confirmOk : nullptr;
    void* data = s_confirmData;
    if (s_confirm) lv_obj_del_async(s_confirm);
    s_confirm = nullptr;
    s_confirmOk = nullptr;
    s_confirmData = nullptr;
    if (onOk) onOk(data);
}

void confirm(const char* title, const char* message, const char* cancelText,
             const char* okText, Btn okKind, void (*onOk)(void*), void* userData) {
    closeConfirm();
    initStyles();
    keyboardHide();
    const UiColors& c = UiTheme::c();

    // Unsichtbare Flaeche ueber dem ganzen Bildschirm: faengt Beruehrungen ab,
    // zeichnet aber nichts (kein Abdunkeln - das waere ein Vollbild-Neuzeichnen).
    lv_obj_t* shield = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(shield);
    lv_obj_set_size(shield, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(shield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shield, LV_OBJ_FLAG_CLICKABLE);
    noGesture(shield);
    s_confirm = shield;
    s_confirmOk = onOk;
    s_confirmData = userData;

    lv_obj_t* box = plainBox(shield);
    lv_obj_add_style(box, &s_card, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, c.divider, 0);
    lv_obj_set_style_pad_all(box, 28, 0);
    lv_obj_set_style_pad_row(box, 14, 0);
    lv_obj_set_size(box, 600, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_center(box);

    lv_obj_t* t = text(box, title, &s_label);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_24, 0);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    if (message && message[0]) note(box, message);

    lv_obj_t* bar = plainBox(box);
    lv_obj_set_size(bar, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(bar, 10, 0);
    lv_obj_set_style_pad_column(bar, 12, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    addButton(bar, cancelText, Btn::Tonal, confirmClicked, nullptr);
    addButton(bar, okText, okKind, confirmClicked, (void*)1);
}

lv_obj_t* qrCard(lv_obj_t* pg, const char* data, const char* steps, const char* hint) {
    lv_obj_t* card = section(pg, nullptr);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_column(card, 32, 0);

    lv_obj_t* frame = plainBox(card);
    lv_obj_set_size(frame, 300, 300);
    lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(frame, 16, 0);
    lv_obj_t* qr = lv_qrcode_create(frame, 260, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, data, strlen(data));
    lv_obj_center(qr);

    lv_obj_t* col = plainBox(card);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_t* t = text(col, steps, &s_label);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(t, 8, 0);
    if (hint) note(col, hint);
    return card;
}

}  // namespace SettingsUi
