#pragma once
#include "IEditTab.h"

class EditTabDisplay : public IEditTab {
private:
    lv_obj_t* ta_name;
    lv_obj_t* ta_icon_search;
    lv_obj_t* dd_icon_cat;
    lv_obj_t* dd_icon;
    lv_obj_t* btn_color_on;
    lv_obj_t* btn_color_off;
    
    String selected_color_on;
    String selected_color_off;
    String last_search_term;

    static void updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix);

public:
    const char* getName() override { return "Anzeige"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;
};