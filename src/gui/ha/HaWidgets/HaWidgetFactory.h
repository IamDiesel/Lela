#pragma once
#include <Arduino.h>
#include "lvgl.h"
#include "HaConfigLogic.h"
#include "HAWidgetBase.h"

class HaWidgetFactory {
public:
    // Baut ein fertiges Widget aus dem Datenmodell (JSON)
    static HAWidget* createWidget(lv_obj_t* parent, int tab_idx, const HAWidgetDef& wDef);
    
    // Liest ein Widget auf dem Bildschirm aus und wandelt es zurück ins Datenmodell
    static HAWidgetDef createDefFromWidget(HAWidget* w);
};