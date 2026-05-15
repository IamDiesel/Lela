#include "HaColorPicker.h"
#include "ViewHomeAssistant.h"
#include "SharedData.h"

lv_obj_t* HaColorPicker::overlay = nullptr;
std::function<void(String)> HaColorPicker::onColorSelected = nullptr;

bool HaColorPicker::isActive() {
    return overlay != nullptr;
}

void HaColorPicker::hide() {
    if (overlay) {
        lv_obj_del_async(overlay);
        overlay = nullptr;
    }
}

void HaColorPicker::close_event_cb(lv_event_t * e) {
    playToneI2S(600, 100, true);
    hide();
}

void HaColorPicker::tile_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    const char* hex = (const char*)lv_event_get_user_data(e);
    
    if (onColorSelected) {
        onColorSelected(String(hex));
    }
    
    hide();
}

void HaColorPicker::show(String currentColor, std::function<void(String)> callback) {
    if (overlay != nullptr) {
        return;
    }
    
    onColorSelected = callback;

    overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, 200, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_add_event_cb(overlay, close_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 820, 520);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    
    static const char* palette[] = { 
        "#F44336", "#E91E63", "#9C27B0", "#673AB7", 
        "#3F51B5", "#2196F3", "#03A9F4", "#00BCD4", 
        "#009688", "#4CAF50", "#8BC34A", "#CDDC39", 
        "#FFEB3B", "#FFC107", "#FF9800", "#FF5722", 
        "#795548", "#9E9E9E", "#607D8B", "#FFFFFF", 
        "#AAAAAA", "#555555", "#222222", "" 
    };

    for (int i = 0; i < 24; i++) {
        lv_obj_t* tile = lv_btn_create(panel);
        lv_obj_set_size(tile, 90, 90);
        lv_obj_set_style_radius(tile, 10, 0);
        
        if (String(palette[i]) == "") {
            lv_obj_set_style_bg_color(tile, lv_color_hex(0x111111), 0);
            lv_obj_t* l = lv_label_create(tile);
            lv_label_set_text(l, LV_SYMBOL_CLOSE "\nReset");
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(l);
        } else {
            uint32_t c_val = strtol(palette[i] + 1, NULL, 16);
            lv_obj_set_style_bg_color(tile, lv_color_hex(c_val), 0);
        }
        
        lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED, (void*)palette[i]);
    }
}