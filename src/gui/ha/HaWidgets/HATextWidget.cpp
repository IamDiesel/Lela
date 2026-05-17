#include "HATextWidget.h"
#include "HaWebsocketLogic.h"
#include "ViewHomeAssistant.h" 

lv_obj_t* HATextWidget::keyboard_overlay = nullptr;
lv_obj_t* HATextWidget::keyboard_ta = nullptr;
lv_obj_t* HATextWidget::keyboard_lbl = nullptr;
HATextWidget* HATextWidget::active_text_widget = nullptr;

HATextWidget::HATextWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    lbl_value = lv_label_create(container);
    lv_obj_set_width(lbl_value, lv_pct(90));
    
    // --- FIX: Schriftart und Farbe hinzugefuegt! Vorher war der Text unsichtbar ---
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_value, lv_color_white(), 0);
    
    lv_obj_set_style_text_align(lbl_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_value, LV_LABEL_LONG_DOT); 
    lv_label_set_text(lbl_value, "-");
    
    // Die Ausrichtung (lv_obj_align) machen wir jetzt dynamisch in setAlignments!
    
    updateState("unknown");
}

// --- NEU: Dynamische Ausrichtung anwenden ---
void HATextWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    // Ruft die Basis-Ausrichtung fuer Icon und Name auf
    HAWidget::setAlignments(i_align, t_align, s_align, i_margin, t_margin, s_margin);
    
    // Wendet den konfigurierten "state_align" und "state_margin" auf unseren Text an
    lv_obj_align(lbl_value, (lv_align_t)s_align, 0, s_margin);
}

void HATextWidget::updateState(String state) {
    HAWidget::updateState(state);
    
    if (active_text_widget == this && keyboard_lbl) {
        lv_label_set_text_fmt(keyboard_lbl, "Aktueller Text: %s", state.c_str());
    }
    
    lv_label_set_text(lbl_value, state.c_str());
    
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x03A9F4), 0); 
}

void HATextWidget::close_keyboard() {
    if (keyboard_overlay) {
        lv_obj_del_async(keyboard_overlay);
        keyboard_overlay = nullptr;
        keyboard_ta = nullptr;
        keyboard_lbl = nullptr;
        active_text_widget = nullptr;
    }
}

void HATextWidget::keyboard_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        if (active_text_widget && keyboard_ta) {
            String new_text = lv_textarea_get_text(keyboard_ta);
            HaWebsocketLogic_CallTextSetValue(active_text_widget->getEntityId(), new_text);
        }
        close_keyboard();
    } else if (code == LV_EVENT_CANCEL) {
        close_keyboard();
    }
}

void HATextWidget::onClick() {
    if (HAWidget::editModeActive) return;
    
    if (keyboard_overlay != nullptr) return; 
    
    active_text_widget = this;
    
    keyboard_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(keyboard_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(keyboard_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(keyboard_overlay, 220, 0); 
    lv_obj_clear_flag(keyboard_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    keyboard_lbl = lv_label_create(keyboard_overlay);
    lv_obj_set_style_text_font(keyboard_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(keyboard_lbl, lv_color_white(), 0);
    lv_obj_align(keyboard_lbl, LV_ALIGN_TOP_MID, 0, 20);
    
    String current_val = HaWebsocketLogic_GetState(entity_id);
    lv_label_set_text_fmt(keyboard_lbl, "Aktueller Text: %s", current_val.c_str());
    
    keyboard_ta = lv_textarea_create(keyboard_overlay);
    lv_obj_set_size(keyboard_ta, lv_pct(80), 80);
    lv_obj_align(keyboard_ta, LV_ALIGN_TOP_MID, 0, 60);
    lv_textarea_set_text(keyboard_ta, current_val.c_str());
    
    if (baked_max > 0) {
        lv_textarea_set_max_length(keyboard_ta, (uint32_t)baked_max);
    }

    lv_obj_t* kb = lv_keyboard_create(keyboard_overlay);
    lv_obj_set_size(kb, LV_PCT(100), LV_PCT(55));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, keyboard_ta);
    
    lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_ALL, NULL);
}