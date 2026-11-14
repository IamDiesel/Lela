#include "UIHelper.h"
#include "HaWebsocketLogic.h"
#include "HaTheme.h"
#include "UiTheme.h"
#include "UiSound.h"

// Farben der Bausteine: Alle Farbangaben der Aufrufer sind DUNKEL-WERTE (so
// war die HA-App gestaltet) und werden hier auf das aktuelle Schema
// abgebildet (HaTheme::themed). Groessen und Lage bleiben unveraendert.

lv_obj_t* UIHelper::createLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, uint32_t color) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    if (font) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    
    // Ohne Angabe (0xFFFFFFFF) erbt das Label die Textfarbe des Dialogs.
    // Eine Angabe ist ein Dunkel-Wert und folgt dem Schema (Weiss -> fast Schwarz).
    if (color != 0xFFFFFFFF) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(HaTheme::themed(color)), 0);
    }
    
    lv_obj_align(lbl, align, x, y);
    return lbl;
}

lv_obj_t* UIHelper::createButton(lv_obj_t* parent, int w, int h, int align, int x, int y, uint32_t bg_color, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, align, x, y);
    // Graue Knoepfe -> Bedienflaeche des Schemas, bunte (Bestaetigen, Stopp,
    // Loeschen) -> ihre Farbe im Schema, Schrift jeweils passend.
    const bool neutral = HaTheme::isNeutral(bg_color);
    const uint32_t bg = neutral ? (lv_color_to32(HaTheme::control()) & 0xFFFFFF) : HaTheme::themed(bg_color);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    const lv_color_t fg = neutral ? UiTheme::c().text : HaTheme::onColor(bg);
    lv_obj_set_style_text_color(btn, fg, 0);
    
    if (text && strlen(text) > 0) {
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, fg, 0);
        lv_obj_center(lbl);
    }
    
    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    UiSound_AttachClick(btn);   // nach dem Aufrufer: dessen eigener Ton gewinnt
    return btn;
}

void UIHelper::themePanel(lv_obj_t* panel) {
    const UiColors& c = UiTheme::c();
    lv_obj_set_style_bg_color(panel, c.surface, 0);
    lv_obj_set_style_text_color(panel, c.text, 0);   // erben alle Labels ohne eigene Farbe
    lv_obj_set_style_border_color(panel, c.divider, 0);
}

void UIHelper::themeDropdown(lv_obj_t* dd) {
    const UiColors& c = UiTheme::c();
    lv_obj_set_style_bg_color(dd, HaTheme::control(), 0);
    lv_obj_set_style_text_color(dd, c.text, 0);
    lv_obj_set_style_border_width(dd, 0, 0);
    lv_obj_t* list = lv_dropdown_get_list(dd);
    lv_obj_set_style_bg_color(list, c.surface, 0);
    lv_obj_set_style_text_color(list, c.text, 0);
    lv_obj_set_style_border_color(list, c.divider, 0);
    lv_obj_set_style_bg_color(list, c.primary, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(list, c.onPrimary, LV_PART_SELECTED | LV_STATE_CHECKED);
}

void UIHelper::themeTextarea(lv_obj_t* ta) {
    const UiColors& c = UiTheme::c();
    lv_obj_set_style_bg_color(ta, HaTheme::control(), 0);
    lv_obj_set_style_text_color(ta, c.text, 0);
    lv_obj_set_style_border_color(ta, c.divider, 0);
    lv_obj_set_style_border_color(ta, c.accent, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ta, c.muted, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(ta, c.accent, LV_PART_CURSOR | LV_STATE_FOCUSED);
}

void UIHelper::themeCheckbox(lv_obj_t* cbx) {
    const UiColors& c = UiTheme::c();
    lv_obj_set_style_text_color(cbx, c.text, 0);
    lv_obj_set_style_bg_color(cbx, HaTheme::control(), LV_PART_INDICATOR);
    lv_obj_set_style_border_color(cbx, c.muted, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cbx, c.accent, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(cbx, c.accent, LV_PART_INDICATOR | LV_STATE_CHECKED);
}

lv_obj_t* UIHelper::createSlider(lv_obj_t* parent, int w, int h, int align, int x, int y, int min_val, int max_val, int val, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, w, h);
    lv_obj_align(slider, align, x, y);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, HaTheme::track(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UiTheme::c().accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, UiTheme::c().accent, LV_PART_KNOB);
    UiSound_AttachSlider(slider);   // Klick beim Anfassen/Loslassen wie in den Einstellungen
    
    if (cb) {
        lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return slider;
}

lv_obj_t* UIHelper::createDropdown(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* options, int selected, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* dd = lv_dropdown_create(parent);
    lv_obj_set_size(dd, w, h);
    lv_obj_align(dd, align, x, y);
    themeDropdown(dd);
    
    if (options && strlen(options) > 0) {
        lv_dropdown_set_options(dd, options);
    }
    lv_dropdown_set_selected(dd, selected);
    
    if (cb) {
        lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    UiSound_AttachClick(dd);
    return dd;
}

lv_obj_t* UIHelper::createTextarea(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* text, const char* placeholder, bool one_line) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, h);
    lv_obj_align(ta, align, x, y);
    themeTextarea(ta);
    
    if (text) {
        lv_textarea_set_text(ta, text);
    }
    if (placeholder) {
        lv_textarea_set_placeholder_text(ta, placeholder);
    }
    lv_textarea_set_one_line(ta, one_line);
    return ta;
}

lv_obj_t* UIHelper::createCheckbox(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, bool checked, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cb_obj = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_obj, text);
    themeCheckbox(cb_obj);
    
    if (font) {
        lv_obj_set_style_text_font(cb_obj, font, 0);
    }
    lv_obj_align(cb_obj, align, x, y);
    
    if (checked) {
        lv_obj_add_state(cb_obj, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(cb_obj, LV_STATE_CHECKED);
    }
    
    if (cb) {
        lv_obj_add_event_cb(cb_obj, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    UiSound_AttachClick(cb_obj);
    return cb_obj;
}

void UIHelper::trackOverlay(lv_obj_t* obj, lv_obj_t** slot) {
    if (!obj || !slot) return;
    lv_obj_add_event_cb(obj, [](lv_event_t* e) {
        lv_obj_t** s = (lv_obj_t**)lv_event_get_user_data(e);
        if (s && *s == lv_event_get_target(e)) *s = nullptr;
    }, LV_EVENT_DELETE, slot);
}

// Anzahl der lebenden modalen Overlays. Jeder Dialog laeuft ueber
// createModalOverlay() und loescht sein Overlay beim Schliessen; das
// DELETE-Ereignis zaehlt wieder herunter - auch wenn der ganze Bildschirm
// geloescht wird.
static int s_openModals = 0;

bool UIHelper::isModalOpen() {
    return s_openModals > 0;
}

lv_obj_t* UIHelper::createModalOverlay(lv_obj_t* parent, lv_opa_t opa, lv_obj_t** slot, bool black) {
    lv_obj_t* overlay = lv_obj_create(parent);
    s_openModals++;
    lv_obj_add_event_cb(overlay, [](lv_event_t*) {
        if (s_openModals > 0) s_openModals--;
    }, LV_EVENT_DELETE, nullptr);
    lv_obj_set_size(overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(overlay, black ? lv_color_black() : UiTheme::c().bg, 0);
    lv_obj_set_style_bg_opa(overlay, opa, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (slot) trackOverlay(overlay, slot);
    return overlay;
}

void UIHelper::showEntitySuggestions(lv_obj_t* roller, lv_obj_t* anchor, const String& query, bool onlyWithLocation) {
    if (!roller) return;

    std::vector<String> hits;
    if (query.length() >= 2) hits = HaWebsocketLogic_SearchEntities(query, 5, onlyWithLocation);

    if (hits.empty()) {
        lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    String opts;
    for (size_t k = 0; k < hits.size(); k++) {
        if (k > 0) opts += "\n";
        opts += hits[k];
    }
    lv_roller_set_options(roller, opts.c_str(), LV_ROLLER_MODE_NORMAL);
    if (anchor) lv_obj_align_to(roller, anchor, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_clear_flag(roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(roller);
}
