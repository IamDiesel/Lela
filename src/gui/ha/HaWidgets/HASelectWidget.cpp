#include "HASelectWidget.h"
#include "HaWebsocketLogic.h"

HASelectWidget::HASelectWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    dropdown = lv_dropdown_create(container);
    lv_obj_set_width(dropdown, lv_pct(85));
    lv_obj_align(dropdown, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_style_bg_color(dropdown, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(dropdown, 0, 0);
    lv_obj_set_style_text_color(dropdown, lv_color_white(), 0);
    
    lv_obj_add_flag(dropdown, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(dropdown, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    updateState("unknown");
}

void HASelectWidget::updateState(String state) {
    HAWidget::updateState(state);
    
    if (lv_obj_has_state(dropdown, LV_STATE_PRESSED)) {
        return;
    }
    
    std::vector<String> opts = HaWebsocketLogic_GetOptions(entity_id);
    
    if (opts.empty() && baked_options.length() > 0) {
        int start = 0;
        while (start < baked_options.length()) {
            int end = baked_options.indexOf(',', start);
            if (end == -1) end = baked_options.length();
            opts.push_back(baked_options.substring(start, end));
            start = end + 1;
        }
    }
    
    if (opts.empty()) {
        // --- FIX: CRASH-SCHUTZ ---
        // Verhindert, dass LVGL mit einem leeren String ("") gefuettert wird!
        String safe_text = state;
        if (safe_text.length() == 0) {
            safe_text = "-";
        }
        
        lv_dropdown_set_options(dropdown, safe_text.c_str());
        lv_dropdown_set_selected(dropdown, 0);
        
        lv_obj_add_state(dropdown, LV_STATE_DISABLED);
        lv_obj_clear_flag(dropdown, LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_set_style_bg_color(dropdown, lv_color_hex(0x333333), 0);
        lv_obj_set_style_text_color(dropdown, lv_color_hex(0xAAAAAA), 0);
        
    } else {
        String opt_str = "";
        uint16_t sel_idx = 0;
        
        for (size_t i = 0; i < opts.size(); i++) {
            opt_str += opts[i];
            if (i < opts.size() - 1) opt_str += "\n";
            if (opts[i] == state) sel_idx = i;
        }
        
        // --- FIX: CRASH-SCHUTZ ---
        if (opt_str.length() == 0) {
            opt_str = "-";
        }
        
        lv_dropdown_set_options(dropdown, opt_str.c_str());
        lv_dropdown_set_selected(dropdown, sel_idx);
        
        lv_obj_clear_state(dropdown, LV_STATE_DISABLED);
        lv_obj_add_flag(dropdown, LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_set_style_bg_color(dropdown, lv_color_hex(0x444444), 0);
        lv_obj_set_style_text_color(dropdown, lv_color_white(), 0);
    }
    
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFD700), 0); 
}

void HASelectWidget::dropdown_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) return; 
    HASelectWidget* w = (HASelectWidget*)lv_event_get_user_data(e);
    lv_obj_t* dd = lv_event_get_target(e);
    
    char buf[64];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
    String selected = String(buf);
    
    String payload = "{\"option\":\"" + selected + "\"}";
    String domain = w->getEntityId().startsWith("select.") ? "select" : "input_select";
    
    HaWebsocketLogic_CallServiceWithData(domain, "select_option", w->getEntityId(), payload);
}

void HASelectWidget::onClick() {}