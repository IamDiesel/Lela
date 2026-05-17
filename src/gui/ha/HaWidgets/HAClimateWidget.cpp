#include "HAClimateWidget.h"
#include "HaWebsocketLogic.h"

HAClimateWidget::HAClimateWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    current_target = 21.0f; // Initialer Fallback-Wert

    // --- LINKE SEITE: Ist-Temperatur ---
    lbl_current_temp = lv_label_create(container);
    lv_obj_set_style_text_font(lbl_current_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_current_temp, lv_color_white(), 0);
    lv_obj_align(lbl_current_temp, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_label_set_text(lbl_current_temp, "--.-°C");

    // --- RECHTE SEITE: Minus-Button ---
    btn_minus = lv_btn_create(container);
    lv_obj_set_size(btn_minus, 45, 45);
    lv_obj_align(btn_minus, LV_ALIGN_RIGHT_MID, -120, -10);
    lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(btn_minus, btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_m = lv_label_create(btn_minus);
    lv_label_set_text(lbl_m, LV_SYMBOL_MINUS);
    lv_obj_center(lbl_m);

    // --- RECHTE SEITE: Soll-Temperatur ---
    lbl_target_temp = lv_label_create(container);
    lv_obj_set_style_text_font(lbl_target_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_target_temp, lv_color_hex(0x3498DB), 0);
    lv_obj_align(lbl_target_temp, LV_ALIGN_RIGHT_MID, -65, -10);
    lv_label_set_text(lbl_target_temp, "--.-");

    // --- RECHTE SEITE: Plus-Button ---
    btn_plus = lv_btn_create(container);
    lv_obj_set_size(btn_plus, 45, 45);
    lv_obj_align(btn_plus, LV_ALIGN_RIGHT_MID, -10, -10);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(btn_plus, btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* lbl_p = lv_label_create(btn_plus);
    lv_label_set_text(lbl_p, LV_SYMBOL_PLUS);
    lv_obj_center(lbl_p);

    // --- UNTEN RECHTS: Modus-Button (Heizen, Kuehlen, Auto) ---
    btn_mode = lv_btn_create(container);
    lv_obj_set_size(btn_mode, 100, 30);
    lv_obj_align(btn_mode, LV_ALIGN_BOTTOM_RIGHT, -35, -5);
    lv_obj_set_style_bg_color(btn_mode, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_mode, btn_event_cb, LV_EVENT_CLICKED, this);
    lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, "mode");
    lv_obj_center(lbl_mode);

    updateState("unknown");
}

void HAClimateWidget::updateState(String state) {
    HAWidget::updateState(state);

    float cur_temp = HaWebsocketLogic_GetCurrentTemperature(entity_id);
    float tgt_temp = HaWebsocketLogic_GetTargetTemperature(entity_id);

    if (cur_temp > -90.0f) {
        lv_label_set_text_fmt(lbl_current_temp, "%.1f°C", cur_temp);
    } else {
        lv_label_set_text(lbl_current_temp, "--.-°C");
    }

    if (tgt_temp > -90.0f) {
        current_target = tgt_temp;
        lv_label_set_text_fmt(lbl_target_temp, "%.1f", current_target);
    }

    // Aktuellen Modus im Button anzeigen
    lv_label_set_text(lbl_mode, state.c_str());

    // Optisches Feedback über die Icon-Farbe
    if (state == "heat") {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xE74C3C), 0); // Rot
    } else if (state == "cool") {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x3498DB), 0); // Blau
    } else if (state == "auto") {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x2ECC71), 0); // Gruen
    } else {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x888888), 0); // Grau
    }
}

void HAClimateWidget::btn_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) return;

    HAClimateWidget* w = (HAClimateWidget*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);

    // Temperatur verstellen
    if (btn == w->btn_plus || btn == w->btn_minus) {
        float step = HaWebsocketLogic_GetStep(w->getEntityId());
        if (step <= 0.0f) step = 0.5f; // Fallback auf 0.5 Grad Schritte

        if (btn == w->btn_plus) w->current_target += step;
        if (btn == w->btn_minus) w->current_target -= step;

        lv_label_set_text_fmt(w->lbl_target_temp, "%.1f", w->current_target);
        HaWebsocketLogic_CallClimateSetTemperature(w->getEntityId(), w->current_target);
    }
    // Modus (hvac_mode) durchschalten
    else if (btn == w->btn_mode) {
        std::vector<String> opts = HaWebsocketLogic_GetOptions(w->getEntityId());

        // Crash-Schutz / Fallback auf gebackene Optionen
        if (opts.empty() && w->baked_options.length() > 0) {
            int start = 0;
            while (start < w->baked_options.length()) {
                int end = w->baked_options.indexOf(',', start);
                if (end == -1) end = w->baked_options.length();
                opts.push_back(w->baked_options.substring(start, end));
                start = end + 1;
            }
        }

        if (opts.size() > 0) {
            String current_mode = lv_label_get_text(w->lbl_mode);
            int idx = -1;
            
            for (size_t i = 0; i < opts.size(); i++) {
                if (opts[i] == current_mode) {
                    idx = i;
                    break;
                }
            }
            
            idx++;
            if (idx >= opts.size()) idx = 0;

            String new_mode = opts[idx];
            lv_label_set_text(w->lbl_mode, new_mode.c_str());
            HaWebsocketLogic_CallClimateSetHvacMode(w->getEntityId(), new_mode);
        }
    }
}

void HAClimateWidget::onClick() {}