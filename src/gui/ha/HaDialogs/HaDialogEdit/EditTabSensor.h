#pragma once
#include "IEditTab.h"
#include "HAWidgets.h"

class EditTabSensor : public IEditTab {
private:
    HAWidget* current_widget = nullptr;
    
    // UI Elemente exklusiv für diesen Tab
    lv_obj_t* cb_chart = nullptr;
    lv_obj_t* slider_chart_w = nullptr;
    lv_obj_t* slider_chart_h = nullptr;
    lv_obj_t* slider_chart_x = nullptr;
    lv_obj_t* slider_chart_y = nullptr;
    
    lv_obj_t* lbl_c_w_val = nullptr;
    lv_obj_t* lbl_c_h_val = nullptr;
    lv_obj_t* lbl_c_x_val = nullptr;
    lv_obj_t* lbl_c_y_val = nullptr;
    
    lv_obj_t* ta_chart_min = nullptr;
    lv_obj_t* ta_chart_max = nullptr;

public:
    const char* getName() override { return "Diagramm"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};