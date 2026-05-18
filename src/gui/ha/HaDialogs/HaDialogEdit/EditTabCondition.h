#pragma once
#include "IEditTab.h"

class EditTabCondition : public IEditTab {
private:
    lv_obj_t* dd_cond_type;
    lv_obj_t* ta_cond1_entity;
    lv_obj_t* dd_cond1_op;
    lv_obj_t* ta_cond1_val;
    lv_obj_t* ta_cond2_entity;
    lv_obj_t* dd_cond2_op;
    lv_obj_t* ta_cond2_val;
    
    lv_obj_t* roller_cond_search; 

public:
    const char* getName() override { return "Bedingung"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};