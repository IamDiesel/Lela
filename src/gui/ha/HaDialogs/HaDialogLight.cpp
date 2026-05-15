#include "HaDialogLight.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "UIHelper.h"

lv_obj_t* HaDialogLight::overlay = nullptr;
lv_obj_t* HaDialogLight::slider_bri = nullptr;
lv_obj_t* HaDialogLight::slider_white = nullptr;
lv_obj_t* HaDialogLight::slider_temp = nullptr;
lv_obj_t* HaDialogLight::color_wheel = nullptr;
String HaDialogLight::current_entity = "";

lv_obj_t* HaDialogLight::lbl_bri_val = nullptr;
lv_obj_t* HaDialogLight::lbl_w_val = nullptr;
lv_obj_t* HaDialogLight::lbl_temp_val = nullptr;

bool HaDialogLight::isActive() {
    return overlay != nullptr;
}

void HaDialogLight::hide() {
    if (overlay) {
        lv_obj_del_async(overlay);
        overlay = nullptr;
        slider_bri = nullptr;
        slider_white = nullptr;
        slider_temp = nullptr;
        color_wheel = nullptr;
        lbl_bri_val = nullptr;
        lbl_w_val = nullptr;
        lbl_temp_val = nullptr;
    }
}

void HaDialogLight::close_event_cb(lv_event_t * e) {
    playToneI2S(600, 100, true);
    hide();
}

void HaDialogLight::send_state_cb(lv_event_t * e) {
    lv_obj_t* target = lv_event_get_target(e);
    
    int bri = -1;
    if (slider_bri != nullptr) {
        bri = lv_slider_get_value(slider_bri);
    }
    
    if (lbl_bri_val && bri >= 0) {
        lv_label_set_text_fmt(lbl_bri_val, "Helligkeit: %d %%", (int)((bri / 255.0f) * 100.0f));
    }

    if (target == slider_temp) {
        int temp = lv_slider_get_value(slider_temp);
        if (lbl_temp_val) {
            lv_label_set_text_fmt(lbl_temp_val, "Farbtemperatur: %d Mired", temp);
        }
        HaWebsocketLogic_CallLightServiceTemp(current_entity, bri, temp);
        return;
    }

    if (color_wheel && (target == color_wheel || target == slider_white)) {
        lv_color_t col = lv_colorwheel_get_rgb(color_wheel);
        lv_color32_t c32; 
        c32.full = lv_color_to32(col); 
        
        int w_val = -1;
        if (slider_white != nullptr) {
            w_val = lv_slider_get_value(slider_white);
        }
        
        if (lbl_w_val && w_val >= 0) {
            lv_label_set_text_fmt(lbl_w_val, "Weiss-Kanal: %d %%", (int)((w_val / 255.0f) * 100.0f));
        }
        
        HaWebsocketLogic_CallLightService(current_entity, bri, c32.ch.red, c32.ch.green, c32.ch.blue, w_val);
        return;
    }

    HaWebsocketLogic_CallLightService(current_entity, bri, -1, -1, -1, -1);
}

void HaDialogLight::show(HAWidget* w) {
    if (overlay != nullptr) {
        return;
    }

    playToneI2S(800, 100, true);
    current_entity = w->getEntityId();

    bool s_bri = HaWebsocketLogic_SupportsBrightness(current_entity);
    bool s_col = HaWebsocketLogic_SupportsColor(current_entity);
    bool s_tmp = HaWebsocketLogic_SupportsColorTemp(current_entity);
    bool is_rgbw = HaWebsocketLogic_IsRGBW(current_entity); 

    int init_bri = HaWebsocketLogic_GetBrightness(current_entity);
    uint32_t init_rgb = HaWebsocketLogic_GetRGB(current_entity);
    int init_w = HaWebsocketLogic_GetWhite(current_entity);
    int init_temp = HaWebsocketLogic_GetColorTemp(current_entity);

    if (init_bri < 0) {
        init_bri = 128; 
    }
    if (init_w < 0) {
        init_w = 0;
    }
    if (init_temp < 153) {
        init_temp = 300; 
    }

    overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0); 
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(overlay);
    
    int panel_h = 95; 
    if (s_bri) {
        panel_h += 90;
    }
    if (s_tmp) {
        panel_h += 90;
    }
    if (is_rgbw) {
        panel_h += 90;
    }
    if (s_col) {
        panel_h += 240;
    }
    
    lv_obj_set_size(panel, 500, panel_h);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE); 

    UIHelper::createLabel(panel, w->getName().c_str(), &lv_font_montserrat_24, LV_ALIGN_TOP_MID, 0, 10, 0xFFFFFF);

    int cur_y = 60;

    if (s_bri) {
        lbl_bri_val = UIHelper::createLabel(panel, (String("Helligkeit: ") + String((int)((init_bri / 255.0f) * 100.0f)) + " %").c_str(), &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, cur_y, 0xFFFFFF);
        cur_y += 30;
        slider_bri = UIHelper::createSlider(panel, 350, 30, LV_ALIGN_TOP_MID, 0, cur_y, 1, 255, init_bri, send_state_cb);
        cur_y += 60;
    }

    if (s_tmp) {
        lbl_temp_val = UIHelper::createLabel(panel, (String("Farbtemperatur: ") + String(init_temp) + " Mired").c_str(), &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, cur_y, 0xFFFFFF);
        cur_y += 30;
        slider_temp = UIHelper::createSlider(panel, 350, 30, LV_ALIGN_TOP_MID, 0, cur_y, 153, 500, init_temp, send_state_cb);
        cur_y += 60;
    } else {
        slider_temp = nullptr;
        lbl_temp_val = nullptr;
    }

    if (is_rgbw) {
        lbl_w_val = UIHelper::createLabel(panel, (String("Weiss-Kanal: ") + String((int)((init_w / 255.0f) * 100.0f)) + " %").c_str(), &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, cur_y, 0xFFFFFF);
        cur_y += 30;
        slider_white = UIHelper::createSlider(panel, 350, 30, LV_ALIGN_TOP_MID, 0, cur_y, 0, 255, init_w, send_state_cb);
        cur_y += 60;
    } else { 
        slider_white = nullptr; 
        lbl_w_val = nullptr;
    }

    if (s_col) {
        UIHelper::createLabel(panel, "RGB Farbe", &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, cur_y, 0xFFFFFF);
        cur_y += 30;
        
        color_wheel = lv_colorwheel_create(panel, true);
        lv_obj_set_size(color_wheel, 200, 200);
        
        // Verschiebe das Farbrad etwas nach links, um Platz für die Buttons zu machen
        lv_obj_align(color_wheel, LV_ALIGN_TOP_MID, -70, cur_y);
        
        // Startmodus: Hue (ohne es hart zu sperren, Longpress in der Mitte ist also noch erlaubt)
        lv_colorwheel_set_mode(color_wheel, LV_COLORWHEEL_MODE_HUE);

        // --- NEU: Buttons fuer direkte Modus-Auswahl ---
        UIHelper::createButton(panel, 130, 50, LV_ALIGN_TOP_MID, 120, cur_y + 15, 0x444444, "Farbton", [](lv_event_t* e){
            if (color_wheel) {
                lv_colorwheel_set_mode(color_wheel, LV_COLORWHEEL_MODE_HUE);
            }
        });
        
        UIHelper::createButton(panel, 130, 50, LV_ALIGN_TOP_MID, 120, cur_y + 75, 0x444444, "Saettigung", [](lv_event_t* e){
            if (color_wheel) {
                lv_colorwheel_set_mode(color_wheel, LV_COLORWHEEL_MODE_SATURATION);
            }
        });
        
        UIHelper::createButton(panel, 130, 50, LV_ALIGN_TOP_MID, 120, cur_y + 135, 0x444444, "Intensitaet", [](lv_event_t* e){
            if (color_wheel) {
                lv_colorwheel_set_mode(color_wheel, LV_COLORWHEEL_MODE_VALUE);
            }
        });

        if (init_rgb != 0xFFFFFFFF) {
            lv_color_t init_col;
            init_col.ch.red = (init_rgb >> 16) & 0xFF; 
            init_col.ch.green = (init_rgb >> 8) & 0xFF; 
            init_col.ch.blue = init_rgb & 0xFF;
            lv_colorwheel_set_rgb(color_wheel, init_col);
        }
        
        lv_obj_add_event_cb(color_wheel, send_state_cb, LV_EVENT_RELEASED, NULL);
    } else {
        color_wheel = nullptr;
    }

    UIHelper::createButton(panel, 50, 50, LV_ALIGN_TOP_RIGHT, 0, 0, 0xAA0000, LV_SYMBOL_CLOSE, close_event_cb);
}