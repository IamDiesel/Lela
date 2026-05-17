#include "HANumberWidget.h"
#include "HaWebsocketLogic.h"

HANumberWidget::HANumberWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    slider = lv_slider_create(container);
    lv_obj_set_size(slider, lv_pct(80), 10);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 10);
    
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x03A9F4), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    
    lv_obj_add_flag(slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // Zwei Events: Value Changed fuers Label, Released fuer den Websocket
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_RELEASED, this);
    
    val_label = lv_label_create(container);
    lv_obj_set_style_text_font(val_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(val_label, lv_color_white(), 0);
    lv_obj_align_to(val_label, slider, LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_label_set_text(val_label, "-");
    
    updateState("unknown");
}

int HANumberWidget::getScaleFactor() {
    float step = HaWebsocketLogic_GetStep(entity_id);
    if (step > 0 && step < 0.1f) return 100;
    if (step > 0 && step < 1.0f) return 10;
    return 1;
}

void HANumberWidget::updateState(String state) {
    HAWidget::updateState(state);
    
    // Verhindert ruckelnde Slider, wenn der Nutzer ihn gerade gedrueckt haelt!
    if (lv_obj_has_state(slider, LV_STATE_PRESSED)) {
        return; 
    }
    
    float val = state.toFloat();
    float min_v = HaWebsocketLogic_GetMin(entity_id);
    float max_v = HaWebsocketLogic_GetMax(entity_id);
    
    int scale = getScaleFactor();
    
    lv_slider_set_range(slider, (int)(min_v * scale), (int)(max_v * scale));
    lv_slider_set_value(slider, (int)(val * scale), LV_ANIM_OFF);
    
    if (scale > 1) {
        lv_label_set_text_fmt(val_label, "%.1f", val);
    } else {
        lv_label_set_text_fmt(val_label, "%d", (int)val);
    }
    
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x03A9F4), 0); 
}

void HANumberWidget::slider_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) {
        return; 
    }
    
    HANumberWidget* w = (HANumberWidget*)lv_event_get_user_data(e);
    lv_obj_t* slider = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    int scale = w->getScaleFactor();
    float val = (float)lv_slider_get_value(slider) / scale;
    
    // Live-Update des Labels waehrend des Ziehens
    if (code == LV_EVENT_VALUE_CHANGED) {
        if (scale > 1) {
            lv_label_set_text_fmt(w->val_label, "%.1f", val);
        } else {
            lv_label_set_text_fmt(w->val_label, "%d", (int)val);
        }
    } 
    // Erst beim LOSLASSEN an Home Assistant senden!
    else if (code == LV_EVENT_RELEASED) {
        String payload = "{\"value\":" + String(val) + "}";
        String domain = w->getEntityId().startsWith("number.") ? "number" : "input_number";
        
        HaWebsocketLogic_CallServiceWithData(domain, "set_value", w->getEntityId(), payload);
    }
}

void HANumberWidget::onClick() {
    // Bleibt leer, da die Interaktion ueber den Slider laeuft
}