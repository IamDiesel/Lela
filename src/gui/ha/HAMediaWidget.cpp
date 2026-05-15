#include "HAMediaWidget.h"
#include "HaWebsocketLogic.h"
#include "MdiMapper.h"

// =========================================================
// MEDIA WIDGET
// =========================================================

HAMediaWidget::HAMediaWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    
    if (String(mdi).length() == 0) {
        lv_label_set_text(icon_label, getSafeIcon("mdi:television").c_str()); 
    } else {
        lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    }
    
    updateState("unknown");
}

void HAMediaWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) {
        return; 
    }
    
    HAWidget::updateState(state); 
    lv_obj_set_style_text_opa(icon_label, 255, 0); 
    
    uint32_t c_on_val = 0x00A0FF; 
    if (color_on.length() > 0 && color_on.startsWith("#")) {
        c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    }
    
    uint32_t c_off_val = 0x555555; 
    if (color_off.length() > 0 && color_off.startsWith("#")) {
        c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    }
    
    if (state != "off" && state != "unavailable" && state != "unknown") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAMediaWidget::onClick() { 
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    HaWebsocketLogic_CallService("media_player", "toggle", entity_id); 
}

// =========================================================
// MEDIA ITEM WIDGET
// =========================================================

HAMediaItemWidget::HAMediaItemWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, String m_c_type, String m_c_id)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    
    this->media_content_type = m_c_type; 
    this->media_content_id = m_c_id;
    
    if (String(mdi).length() == 0) {
        lv_label_set_text(icon_label, getSafeIcon("mdi:play").c_str()); 
    } else {
        lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    }
    
    updateState("unknown");
}

void HAMediaItemWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) {
        return; 
    }
    
    HAWidget::updateState(state); 
    lv_obj_set_style_text_opa(icon_label, 255, 0); 
    
    uint32_t c_on_val = 0x00A0FF; 
    if (color_on.length() > 0 && color_on.startsWith("#")) {
        c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    }
    
    uint32_t c_off_val = 0x555555; 
    if (color_off.length() > 0 && color_off.startsWith("#")) {
        c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    }
    
    if (state == "playing") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAMediaItemWidget::onClick() { 
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    HaWebsocketLogic_CallPlayMedia(entity_id, media_content_type, media_content_id); 
}

String HAMediaItemWidget::getMediaContentType() { 
    return media_content_type; 
}

String HAMediaItemWidget::getMediaContentId() { 
    return media_content_id; 
}