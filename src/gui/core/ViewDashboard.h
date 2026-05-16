#pragma once
#include "lvgl.h"

class ViewDashboard {
private:
    static void btn_cat_event_cb(lv_event_t * e);
    static void btn_baby_event_cb(lv_event_t * e);
    static void btn_ha_event_cb(lv_event_t * e); 

public:
    static lv_obj_t* build();
    static void update(); 
};