#pragma once
#include "lvgl.h"
#include "HAWidgets.h"

class IEditTab {
public:
    virtual ~IEditTab() = default;
    
    // Name des Tabs in der LVGL Tabview
    virtual const char* getName() = 0;
    
    // Wird aufgerufen, wenn der Tab gebaut wird
    virtual void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) = 0;
    
    // Wird beim Klick auf "Uebernehmen" aufgerufen
    virtual void saveConfig(HAWidget* w) = 0;
};