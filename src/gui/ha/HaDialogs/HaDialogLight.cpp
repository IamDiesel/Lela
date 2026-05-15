#include "HaDialogLight.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"

lv_obj_t* HaDialogLight::overlay = nullptr;
lv_obj_t* HaDialogLight::slider_bri = nullptr;
lv_obj_t* HaDialogLight::slider_white = nullptr;
lv_obj_t* HaDialogLight::color_wheel = nullptr;
String HaDialogLight::current_entity = "";

bool HaDialogLight::isActive() {
    return overlay != nullptr;
}

void HaDialogLight::hide() {
    if (overlay) {
        lv_obj_del_async(overlay);
        overlay = nullptr;
        slider_bri = nullptr;
        slider_white = nullptr;
        color_wheel = nullptr;
    }
}

void HaDialogLight::close_event_cb(lv_event_t * e) {
    playToneI2S(600, 100, true);
    hide();
}

void HaDialogLight::send_state_cb(lv_event_t * e) {
    if (!slider_bri || !color_wheel) return;

    int bri = lv_slider_get_value(slider_bri);
    int w_val = (slider_white != nullptr) ? lv_slider_get_value(slider_white) : -1;
    
    lv_color_t col = lv_colorwheel_get_rgb(color_wheel);
    lv_color32_t c32; 
    c32.full = lv_color_to32(col); 
    
    HaWebsocketLogic_CallLightService(current_entity, bri, c32.ch.red, c32.ch.green, c32.ch.blue, w_val);
}

void HaDialogLight::show(HAWidget* w) {
    if (overlay != nullptr) {
        return;
    }

    playToneI2S(800, 100, true);
    current_entity = w->getEntityId();

    int init_bri = HaWebsocketLogic_GetBrightness(current_entity);
    uint32_t init_rgb = HaWebsocketLogic_GetRGB(current_entity);
    int init_w = HaWebsocketLogic_GetWhite(current_entity);
    bool is_rgbw = HaWebsocketLogic_IsRGBW(current_entity);

    if (init_bri < 0) init_bri = 128; 
    if (init_w < 0) init_w = 0;

    overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0); 
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 500, is_rgbw ? 580 : 500);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, w->getName().c_str());
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);

    // Helligkeit Slider
    slider_bri = lv_slider_create(panel);
    lv_slider_set_range(slider_bri, 1, 255);
    lv_obj_set_size(slider_bri, 350, 30);
    lv_obj_align(slider_bri, LV_ALIGN_TOP_MID, 0, 100);
    lv_slider_set_value(slider_bri, init_bri, LV_ANIM_OFF); 

    lv_obj_t* lbl_bri = lv_label_create(panel);
    lv_label_set_text(lbl_bri, "Helligkeit (0-100%)");
    lv_obj_align_to(lbl_bri, slider_bri, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Weiss-Kanal Slider (falls vorhanden)
    if (is_rgbw) {
        slider_white = lv_slider_create(panel);
        lv_slider_set_range(slider_white, 0, 255);
        lv_obj_set_size(slider_white, 350, 30);
        lv_obj_align(slider_white, LV_ALIGN_TOP_MID, 0, 180);
        lv_slider_set_value(slider_white, init_w, LV_ANIM_OFF); 

        lv_obj_t* lbl_w = lv_label_create(panel);
        lv_label_set_text(lbl_w, "Weiß-Kanal (0-100%)");
        lv_obj_align_to(lbl_w, slider_white, LV_ALIGN_OUT_TOP_MID, 0, -10);
    } else { 
        slider_white = nullptr; 
    }

    // Farbrad
    color_wheel = lv_colorwheel_create(panel, true);
    lv_obj_set_size(color_wheel, 250, 250);
    lv_obj_align(color_wheel, LV_ALIGN_TOP_MID, 0, is_rgbw ? 260 : 180);

    if (init_rgb != 0xFFFFFFFF) {
        lv_color_t init_col;
        init_col.ch.red = (init_rgb >> 16) & 0xFF; 
        init_col.ch.green = (init_rgb >> 8) & 0xFF; 
        init_col.ch.blue = init_rgb & 0xFF;
        lv_colorwheel_set_rgb(color_wheel, init_col);
    }

    // Callbacks binden
    lv_obj_add_event_cb(slider_bri, send_state_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(color_wheel, send_state_cb, LV_EVENT_RELEASED, NULL);
    
    if (slider_white) {
        lv_obj_add_event_cb(slider_white, send_state_cb, LV_EVENT_RELEASED, NULL);
    }

    // Close Button
    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_close);
    
    lv_obj_add_event_cb(btn_close, close_event_cb, LV_EVENT_CLICKED, NULL);
}