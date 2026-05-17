#pragma once
#include "HAWidgetBase.h"

class HANumberWidget : public HAWidget {
public:
    HANumberWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;

private:
    lv_obj_t* slider;
    lv_obj_t* val_label;
    
    static void slider_event_cb(lv_event_t * e);
    int getScaleFactor();
};