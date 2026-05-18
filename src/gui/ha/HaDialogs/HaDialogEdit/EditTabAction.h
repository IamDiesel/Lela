#pragma once
#include "IEditTab.h"

class EditTabAction : public IEditTab {
private:
    lv_obj_t* ta_domain;
    lv_obj_t* ta_service;
    lv_obj_t* ta_target;
    lv_obj_t* roller_search;
    lv_obj_t* current_ta;

public:
    const char* getName() override { return "Aktion"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};