#pragma once
#include "IEditTab.h"

class EditTabLayout : public IEditTab {
private:
    HAWidget* current_widget;
    
    lv_obj_t* dd_icon_pos;
    lv_obj_t* dd_text_pos;
    lv_obj_t* dd_state_pos;
    lv_obj_t* slider_icon_margin;
    lv_obj_t* slider_text_margin;
    lv_obj_t* slider_state_margin;
    lv_obj_t* lbl_i_m_val;
    lv_obj_t* lbl_t_m_val;
    lv_obj_t* lbl_s_m_val;
    lv_obj_t* cb_snap;

    static int alignToIdx(int align);
    static int idxToAlign(int idx);

public:
    const char* getName() override { return "Layout"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};