#include "HACoverWidget.h"
#include "HaWebsocketLogic.h"

HACoverWidget::HACoverWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    // --- DER SLIDER FUER DIE EXAKTE POSITION ---
    slider = lv_slider_create(container);
    lv_obj_set_size(slider, lv_pct(85), 15);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, -10);
    lv_slider_set_range(slider, 0, 100);
    
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x3498DB), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    
    lv_obj_add_flag(slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_RELEASED, this);
    
    pos_label = lv_label_create(container);
    lv_obj_set_style_text_font(pos_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pos_label, lv_color_white(), 0);
    lv_obj_align_to(pos_label, slider, LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_label_set_text(pos_label, "-");

    // --- DIE DREI SCHNELL-BUTTONS (Auf, Stop, Ab) ---
    btn_up = lv_btn_create(container);
    lv_obj_set_size(btn_up, 60, 40);
    lv_obj_align(btn_up, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_set_style_bg_color(btn_up, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(btn_up, btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, LV_SYMBOL_UP);
    lv_obj_center(lbl_up);

    btn_stop = lv_btn_create(container);
    lv_obj_set_size(btn_stop, 60, 40);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xAA0000), 0); // Stop ist rot
    lv_obj_add_event_cb(btn_stop, btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_PAUSE);
    lv_obj_center(lbl_stop);

    btn_down = lv_btn_create(container);
    lv_obj_set_size(btn_down, 60, 40);
    lv_obj_align(btn_down, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_set_style_bg_color(btn_down, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(btn_down, btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, LV_SYMBOL_DOWN);
    lv_obj_center(lbl_down);
    
    updateState("unknown");
}

void HACoverWidget::updateState(String state) {
    HAWidget::updateState(state);
    
    // Ruckeln verhindern, wenn der Nutzer gerade zieht
    if (lv_obj_has_state(slider, LV_STATE_PRESSED)) return;
    
    int pos = HaWebsocketLogic_GetPosition(entity_id);
    
    if (pos >= 0 && pos <= 100) {
        lv_slider_set_value(slider, pos, LV_ANIM_OFF);
        lv_label_set_text_fmt(pos_label, "%d%%", pos);
    } else {
        // Falls HA (noch) keine Position meldet, zeigen wir zumindest "open/closed"
        lv_label_set_text(pos_label, state.c_str());
    }
    
    // Optisches Feedback: Wenn der Rollladen offen ist, leuchtet das Icon blau
    if (state == "open" || state == "opening" || pos > 0) {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x3498DB), 0); 
    } else {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x888888), 0); 
    }
}

void HACoverWidget::slider_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) return; 
    
    HACoverWidget* w = (HACoverWidget*)lv_event_get_user_data(e);
    lv_obj_t* slider = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    int val = lv_slider_get_value(slider);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_label_set_text_fmt(w->pos_label, "%d%%", val);
    } 
    else if (code == LV_EVENT_RELEASED) {
        HaWebsocketLogic_CallCoverPosition(w->getEntityId(), val);
    }
}

void HACoverWidget::btn_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) return; 
    
    HACoverWidget* w = (HACoverWidget*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);
    
    if (btn == w->btn_up) {
        HaWebsocketLogic_CallCoverService(w->getEntityId(), "open_cover");
    } else if (btn == w->btn_stop) {
        HaWebsocketLogic_CallCoverService(w->getEntityId(), "stop_cover");
    } else if (btn == w->btn_down) {
        HaWebsocketLogic_CallCoverService(w->getEntityId(), "close_cover");
    }
}

void HACoverWidget::onClick() {
    // Interaktionen laufen über Slider und Buttons, kein Klick auf die Box nötig
}