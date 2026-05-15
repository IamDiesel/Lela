#pragma once
#include "HAWidgetBase.h"

class HAVacuumWidget : public HAWidget {
private:
    lv_obj_t* btn_play_pause;
    lv_obj_t* lbl_play_pause;
    lv_obj_t* btn_stop;
    lv_obj_t* lbl_stop;
public:
    HAVacuumWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;
    void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) override;
};