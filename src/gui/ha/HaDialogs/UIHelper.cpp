#include "UIHelper.h"
#include "HaWebsocketLogic.h"

lv_obj_t* UIHelper::createLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, uint32_t color) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    if (font) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    
    // Setze die Farbe nur, wenn sie explizit ueberschrieben wurde (nicht 0xFFFFFFFF)
    if (color != 0xFFFFFFFF) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    }
    
    lv_obj_align(lbl, align, x, y);
    return lbl;
}

lv_obj_t* UIHelper::createButton(lv_obj_t* parent, int w, int h, int align, int x, int y, uint32_t bg_color, const char* text, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, align, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), 0);
    
    if (text && strlen(text) > 0) {
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        // Buttons haben dunkle Hintergruende, daher erzwingen wir hier weisse Schrift
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);
    }
    
    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

lv_obj_t* UIHelper::createSlider(lv_obj_t* parent, int w, int h, int align, int x, int y, int min_val, int max_val, int val, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, w, h);
    lv_obj_align(slider, align, x, y);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    
    if (cb) {
        lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return slider;
}

lv_obj_t* UIHelper::createDropdown(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* options, int selected, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* dd = lv_dropdown_create(parent);
    lv_obj_set_size(dd, w, h);
    lv_obj_align(dd, align, x, y);
    
    if (options && strlen(options) > 0) {
        lv_dropdown_set_options(dd, options);
    }
    lv_dropdown_set_selected(dd, selected);
    
    if (cb) {
        lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return dd;
}

lv_obj_t* UIHelper::createTextarea(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* text, const char* placeholder, bool one_line) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, h);
    lv_obj_align(ta, align, x, y);
    
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
    if (black) lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, opa, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (slot) trackOverlay(overlay, slot);
    return overlay;
}

void UIHelper::showEntitySuggestions(lv_obj_t* roller, lv_obj_t* anchor, const String& query) {
    if (!roller) return;

    std::vector<String> hits;
    if (query.length() >= 2) hits = HaWebsocketLogic_SearchEntities(query, 5);

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
