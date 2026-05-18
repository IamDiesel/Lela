#include "EditTabLayout.h"
#include "UIHelper.h"

int EditTabLayout::idxToAlign(int idx) {
    switch(idx) {
        case 0: return LV_ALIGN_TOP_MID; case 1: return LV_ALIGN_CENTER; case 2: return LV_ALIGN_BOTTOM_MID; 
        case 3: return LV_ALIGN_LEFT_MID; case 4: return LV_ALIGN_RIGHT_MID; default: return LV_ALIGN_CENTER;
    }
}

int EditTabLayout::alignToIdx(int align) {
    switch(align) {
        case LV_ALIGN_TOP_MID: return 0; case LV_ALIGN_CENTER: return 1; case LV_ALIGN_BOTTOM_MID: return 2; 
        case LV_ALIGN_LEFT_MID: return 3; case LV_ALIGN_RIGHT_MID: return 4;
    }
    return 1; 
}

void EditTabLayout::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    current_widget = w;
    int y = 15;
    
    UIHelper::createButton(parent, 150, 40, LV_ALIGN_TOP_LEFT, 10, y, 0x2980B9, "Nach vorne", [](lv_event_t* e){ 
        EditTabLayout* tab = (EditTabLayout*)lv_event_get_user_data(e);
        if(tab->current_widget && tab->current_widget->container && lv_obj_is_valid(tab->current_widget->container)) 
            lv_obj_move_foreground(tab->current_widget->container); 
    }, this);
    
    UIHelper::createButton(parent, 150, 40, LV_ALIGN_TOP_LEFT, 170, y, 0x8E44AD, "Nach hinten", [](lv_event_t* e){ 
        EditTabLayout* tab = (EditTabLayout*)lv_event_get_user_data(e);
        if(tab->current_widget && tab->current_widget->container && lv_obj_is_valid(tab->current_widget->container)) 
            lv_obj_move_background(tab->current_widget->container); 
    }, this);
    
    auto layout_change_cb = [](lv_event_t* e) {
        EditTabLayout* tab = (EditTabLayout*)lv_event_get_user_data(e);
        if (!tab->current_widget) return;
        
        if (tab->lbl_i_m_val && tab->slider_icon_margin) 
            lv_label_set_text_fmt(tab->lbl_i_m_val, "%d px", (int)lv_slider_get_value(tab->slider_icon_margin));
        if (tab->lbl_t_m_val && tab->slider_text_margin) 
            lv_label_set_text_fmt(tab->lbl_t_m_val, "%d px", (int)lv_slider_get_value(tab->slider_text_margin));
        if (tab->lbl_s_m_val && tab->slider_state_margin) 
            lv_label_set_text_fmt(tab->lbl_s_m_val, "%d px", (int)lv_slider_get_value(tab->slider_state_margin));

        if (tab->dd_icon_pos && tab->dd_text_pos && tab->slider_icon_margin && tab->slider_text_margin) {
            tab->current_widget->setAlignments(
                idxToAlign(lv_dropdown_get_selected(tab->dd_icon_pos)), 
                idxToAlign(lv_dropdown_get_selected(tab->dd_text_pos)),
                tab->dd_state_pos ? idxToAlign(lv_dropdown_get_selected(tab->dd_state_pos)) : LV_ALIGN_CENTER,
                lv_slider_get_value(tab->slider_icon_margin), 
                lv_slider_get_value(tab->slider_text_margin),
                tab->slider_state_margin ? lv_slider_get_value(tab->slider_state_margin) : 0
            );
        }
    };

    y += 60;
    UIHelper::createLabel(parent, "Icon:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
    dd_icon_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(w->getIconAlign()));
    lv_obj_add_event_cb(dd_icon_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
    
    UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
    slider_icon_margin = UIHelper::createSlider(parent, 300, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, w->getIconMargin());
    lv_obj_add_event_cb(slider_icon_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
    lbl_i_m_val = UIHelper::createLabel(parent, (String(w->getIconMargin()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 680, y+10);

    y += 60;
    UIHelper::createLabel(parent, "Text:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
    dd_text_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(w->getTextAlign()));
    lv_obj_add_event_cb(dd_text_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
    
    UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
    slider_text_margin = UIHelper::createSlider(parent, 300, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, w->getTextMargin());
    lv_obj_add_event_cb(slider_text_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
    lbl_t_m_val = UIHelper::createLabel(parent, (String(w->getTextMargin()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 680, y+10);

    y += 60;
    if (w->getType() == "sensor") {
        UIHelper::createLabel(parent, "Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
        dd_state_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(w->getStateAlign()));
        lv_obj_add_event_cb(dd_state_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
        
        UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
        slider_state_margin = UIHelper::createSlider(parent, 300, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, w->getStateMargin());
        lv_obj_add_event_cb(slider_state_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, this);
        lbl_s_m_val = UIHelper::createLabel(parent, (String(w->getStateMargin()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 680, y+10);
    } else {
        dd_state_pos = nullptr; slider_state_margin = nullptr; lbl_s_m_val = nullptr;
    }

    cb_snap = UIHelper::createCheckbox(parent, "Am Raster einrasten (Snap 10px)", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+60, w->getSnapToGrid());
}

void EditTabLayout::saveConfig(HAWidget* w) {
    if (cb_snap && lv_obj_is_valid(cb_snap)) {
        w->setSnapToGrid(lv_obj_has_state(cb_snap, LV_STATE_CHECKED));
    }
}