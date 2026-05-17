#pragma once
#include "HAWidgetBase.h"

class HACoverWidget : public HAWidget {
public:
    HACoverWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;

private:
    lv_obj_t* slider;
    lv_obj_t* pos_label;
    
    lv_obj_t* btn_up;
    lv_obj_t* btn_stop;
    lv_obj_t* btn_down;
    
    static void slider_event_cb(lv_event_t * e);
    static void btn_event_cb(lv_event_t * e);
};