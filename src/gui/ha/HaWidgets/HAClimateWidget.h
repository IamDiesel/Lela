#pragma once
#include "HAWidgetBase.h"

class HAClimateWidget : public HAWidget {
public:
    HAClimateWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;

private:
    lv_obj_t* lbl_current_temp;
    lv_obj_t* lbl_target_temp;
    lv_obj_t* btn_plus;
    lv_obj_t* btn_minus;
    lv_obj_t* btn_mode;
    lv_obj_t* lbl_mode;

    float current_target;

    static void btn_event_cb(lv_event_t * e);
};