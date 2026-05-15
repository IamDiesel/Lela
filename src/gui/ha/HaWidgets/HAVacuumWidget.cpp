#include "HAVacuumWidget.h"
#include "HaWebsocketLogic.h"

HAVacuumWidget::HAVacuumWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {

    this->icon_align = LV_ALIGN_CENTER;
    this->text_align = LV_ALIGN_TOP_MID;
    this->icon_margin = -10;
    this->text_margin = 10;
    
    setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin);

    lv_obj_set_style_text_opa(icon_label, 80, 0); 

    btn_play_pause = lv_btn_create(container);
    lv_obj_set_style_bg_color(btn_play_pause, lv_color_hex(0x2980B9), 0);
    lbl_play_pause = lv_label_create(btn_play_pause);
    lv_label_set_text(lbl_play_pause, LV_SYMBOL_PLAY);
    lv_obj_center(lbl_play_pause);
    
    lv_obj_add_event_cb(btn_play_pause, [](lv_event_t* e) {
        HAVacuumWidget* w = (HAVacuumWidget*)lv_event_get_user_data(e);
        if (w->current_state == "cleaning") {
            HaWebsocketLogic_CallVacuumService(w->getEntityId(), "pause");
        } else {
            HaWebsocketLogic_CallVacuumService(w->getEntityId(), "start");
        }
    }, LV_EVENT_CLICKED, this);

    btn_stop = lv_btn_create(container);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xC0392B), 0);
    lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_STOP);
    lv_obj_center(lbl_stop);
    
    lv_obj_add_event_cb(btn_stop, [](lv_event_t* e) {
        HAVacuumWidget* w = (HAVacuumWidget*)lv_event_get_user_data(e);
        HaWebsocketLogic_CallVacuumService(w->getEntityId(), "stop");
    }, LV_EVENT_CLICKED, this);

    if (chart_w_pct <= 0 || chart_w_pct == 95) { 
        chart_w_pct = 60; 
        chart_h_pct = 40; 
        chart_x_ofs = 10; 
        chart_y_ofs = 10; 
    }
    
    setChartConfig(false, chart_w_pct, chart_h_pct, chart_x_ofs, chart_y_ofs, "", "");
    updateState("unknown");
}

void HAVacuumWidget::setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) {
    HAWidget::setChartConfig(show, w_p, h_p, x_ofs, y_ofs, c_min, c_max);
    
    lv_obj_set_size(btn_play_pause, w_p, h_p);
    lv_obj_set_size(btn_stop, w_p, h_p);

    lv_obj_align(btn_play_pause, LV_ALIGN_BOTTOM_MID, -(w_p/2 + x_ofs/2), -y_ofs);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_MID, (w_p/2 + x_ofs/2), -y_ofs);
}

void HAVacuumWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_color(icon_label, 0).full != 0) {
        return; 
    }
    
    HAWidget::updateState(state);
    
    if (state == "cleaning") {
        lv_label_set_text(lbl_play_pause, LV_SYMBOL_PAUSE);
    } else {
        lv_label_set_text(lbl_play_pause, LV_SYMBOL_PLAY);
    }

    uint32_t c_on_val = 0x1ABC9C; 
    if (color_on.length() > 0 && color_on.startsWith("#")) {
        c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    }
    
    uint32_t c_off_val = 0x555555; 
    if (color_off.length() > 0 && color_off.startsWith("#")) {
        c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    }

    if (state == "cleaning" || state == "returning" || state == "error") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAVacuumWidget::onClick() {
    // Interaktion verläuft hier primär über die separaten Buttons oder den Longpress
}