#pragma once
#include "HAWidgetBase.h"

class HASelectWidget : public HAWidget {
public:
    HASelectWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;

private:
    lv_obj_t* dropdown;
    static void dropdown_event_cb(lv_event_t * e);
};