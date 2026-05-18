#include "EditTabSize.h"
#include "UIHelper.h"

void EditTabSize::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    this->current_widget = w;
    
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_LEFT_MID, 40, -50, 0xAA0000, LV_SYMBOL_MINUS, [](lv_event_t* e){ 
        EditTabSize* tab = (EditTabSize*)lv_event_get_user_data(e);
        if(tab->current_widget) { 
            int nw = tab->current_widget->getW() - 40; 
            if (nw < 100) nw = 100; 
            tab->current_widget->setSize(nw, tab->current_widget->getH()); 
        }
    }, this);
    
    UIHelper::createLabel(parent, "Breite", &lv_font_montserrat_24, LV_ALIGN_CENTER, 0, -50);
    
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_RIGHT_MID, -40, -50, 0x27AE60, LV_SYMBOL_PLUS, [](lv_event_t* e){ 
        EditTabSize* tab = (EditTabSize*)lv_event_get_user_data(e);
        if(tab->current_widget) {
            tab->current_widget->setSize(tab->current_widget->getW() + 40, tab->current_widget->getH()); 
        }
    }, this);

    UIHelper::createButton(parent, 80, 80, LV_ALIGN_LEFT_MID, 40, 50, 0xAA0000, LV_SYMBOL_MINUS, [](lv_event_t* e){ 
        EditTabSize* tab = (EditTabSize*)lv_event_get_user_data(e);
        if(tab->current_widget) { 
            int nh = tab->current_widget->getH() - 40; 
            if (nh < 80) nh = 80; 
            tab->current_widget->setSize(tab->current_widget->getW(), nh); 
        }
    }, this);
    
    UIHelper::createLabel(parent, "Hoehe", &lv_font_montserrat_24, LV_ALIGN_CENTER, 0, 50);
    
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_RIGHT_MID, -40, 50, 0x27AE60, LV_SYMBOL_PLUS, [](lv_event_t* e){ 
        EditTabSize* tab = (EditTabSize*)lv_event_get_user_data(e);
        if(tab->current_widget) {
            tab->current_widget->setSize(tab->current_widget->getW(), tab->current_widget->getH() + 40); 
        }
    }, this);
}

void EditTabSize::saveConfig(HAWidget* w) {
    // Groesse wird live beim Klicken auf die Buttons angewendet, hier muss nichts mehr gespeichert werden!
}