#pragma once
#include "IEditTab.h"

class EditTabSize : public IEditTab {
private:
    HAWidget* current_widget;
public:
    const char* getName() override { return "Groesse"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};