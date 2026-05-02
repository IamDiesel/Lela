#include "HAWidgets.h"
#include "HaWebsocketLogic.h"
#include "MdiMapper.h"

bool HAWidget::editModeActive = false;
void (*HAWidget::onEditRequested)(HAWidget*) = nullptr;
void (*HAWidget::onDeleteRequested)(HAWidget*) = nullptr;
void (*HAWidget::onLightControlRequested)(HAWidget*) = nullptr; 
void (*HAWidget::onMediaControlRequested)(HAWidget*) = nullptr; 

static bool widget_is_dragging = false;

String formatEntityName(String entity_id, String name) {
    if (name.length() > 0) return name; 
    int dotIdx = entity_id.indexOf('.');
    String res = (dotIdx != -1) ? entity_id.substring(dotIdx + 1) : entity_id;
    res.replace("_", " ");
    bool capitalizeNext = true;
    for (int i = 0; i < res.length(); i++) {
        if (res[i] == ' ') capitalizeNext = true;
        else if (capitalizeNext) { res[i] = toupper(res[i]); capitalizeNext = false; }
    }
    return res;
}

String getSafeIcon(String mdi) {
    String res = getAutoIcon(mdi);
    return res != "" ? res : String(LV_SYMBOL_DUMMY);
}

String getIconForEntity(String entity_id, String mdi_icon) {
    if (mdi_icon.length() == 1) { return mdi_icon; }
    if (mdi_icon.length() > 0) {
        String l = mdi_icon; l.replace(" ", "");
        int start = 0;
        while(start < l.length()) {
            int end = l.indexOf(',', start);
            if (end == -1) end = l.length();
            String single_mdi = l.substring(start, end);
            String found = getAutoIcon(single_mdi);
            if (found != "") return found;
            start = end + 1;
        }
    }
    if (entity_id.startsWith("light.")) return getSafeIcon("mdi:lightbulb"); 
    if (entity_id.startsWith("switch.") || entity_id.startsWith("input_boolean.")) return getSafeIcon("mdi:power"); 
    if (entity_id.startsWith("media_player.")) return getSafeIcon("mdi:television");
    if (entity_id.startsWith("camera.")) return getSafeIcon("mdi:cctv");
    if (entity_id.startsWith("climate.") || entity_id.indexOf("temp") != -1) return getSafeIcon("mdi:thermometer");
    if (entity_id.startsWith("vacuum.")) return getSafeIcon("mdi:robot-vacuum");
    if (entity_id.indexOf("battery") != -1) return getSafeIcon("mdi:battery");
    if (entity_id.indexOf("solar") != -1) return getSafeIcon("mdi:solar-panel");
    if (entity_id.indexOf("katze") != -1 || entity_id.indexOf("cat") != -1) return getSafeIcon("mdi:cat");
    if (entity_id.indexOf("kaffee") != -1 || entity_id.indexOf("coffee") != -1) return getSafeIcon("mdi:coffee");
    return String(LV_SYMBOL_DUMMY); 
}

HAWidget::HAWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off) {
    this->tab_index = tab_idx; this->type = type; this->entity_id = entity; this->mdi_icon = mdi;
    this->color_on = c_on; this->color_off = c_off; this->widget_name = formatEntityName(entity_id, String(name));
    this->current_state = "unknown";
    
    this->icon_align = LV_ALIGN_TOP_MID; this->text_align = LV_ALIGN_BOTTOM_MID; this->state_align = LV_ALIGN_CENTER;
    this->icon_margin = 5; this->text_margin = 5; this->state_margin = 0;
    this->snap_to_grid = true; 

    container = lv_obj_create(parent);
    lv_obj_set_size(container, w, h); lv_obj_set_pos(container, x, y);
    lv_obj_set_style_radius(container, 25, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x222222), 0); 
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x555555), 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLL_CHAIN); 
    lv_obj_add_flag(container, LV_OBJ_FLAG_PRESS_LOCK);     
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(container, widget_event_cb, LV_EVENT_ALL, this);

    icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_text_font(icon_label, &lela_icons, 0);
    lv_label_set_text(icon_label, LV_SYMBOL_DUMMY);

    name_label = lv_label_create(container);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT); 
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(name_label, widget_name.c_str());
    lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
    
    setSize(w, h); 
}

HAWidget::~HAWidget() { 
    if (container && lv_obj_is_valid(container)) { lv_obj_remove_event_cb(container, widget_event_cb); lv_obj_del_async(container); }
}

void HAWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    icon_align = i_align; text_align = t_align; state_align = s_align;
    icon_margin = i_margin; text_margin = t_margin; state_margin = s_margin;
    
    auto apply = [](lv_obj_t* obj, int align_val, int margin) {
        if (!obj) return;
        int ox = 0, oy = 0;
        switch(align_val) {
            case LV_ALIGN_TOP_MID: oy = margin; break;
            case LV_ALIGN_BOTTOM_MID: oy = -margin; break;
            case LV_ALIGN_LEFT_MID: ox = margin; break;
            case LV_ALIGN_RIGHT_MID: ox = -margin; break;
            case LV_ALIGN_CENTER: oy = margin; break; // Bei CENTER wird der Margin zum Vertikalen Nudge-Offset!
        }
        lv_obj_align(obj, align_val, ox, oy);
    };
    apply(icon_label, icon_align, icon_margin); 
    apply(name_label, text_align, text_margin);
}

void HAWidget::setName(String n) { widget_name = formatEntityName(entity_id, n); lv_label_set_text(name_label, widget_name.c_str()); }
void HAWidget::setMdiIcon(String mdi) { mdi_icon = mdi; lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str()); }
void HAWidget::setColors(String on, String off) { color_on = on; color_off = off; String old = current_state; current_state = "force_update"; updateState(old); }
void HAWidget::setSize(int w, int h) { lv_obj_set_size(container, w, h); lv_obj_set_size(name_label, w - 20, 45); setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin); }
void HAWidget::updateState(String state) { this->current_state = state; }

void HAWidget::widget_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    HAWidget * widget = (HAWidget *)lv_event_get_user_data(e);
    if (!widget) return;

    if (HAWidget::editModeActive) {
        if (code == LV_EVENT_PRESSED) {
            widget_is_dragging = false;
            lv_obj_move_foreground(obj); 
        }
        else if (code == LV_EVENT_LONG_PRESSED) widget_is_dragging = true;
        else if (code == LV_EVENT_PRESSING) {
            if (widget_is_dragging) {
                lv_indev_t * indev = lv_indev_get_act();
                if (indev == NULL) return;
                lv_point_t vect; lv_indev_get_vect(indev, &vect);
                lv_coord_t x = lv_obj_get_x(obj) + vect.x; lv_coord_t y = lv_obj_get_y(obj) + vect.y;
                if (x < 0) x = 0; if (y < 0) y = 0;
                lv_obj_set_pos(obj, x, y);
            }
        }
        else if (code == LV_EVENT_SHORT_CLICKED) {
            if (onEditRequested) onEditRequested(widget);
        }
        else if (code == LV_EVENT_RELEASED) {
            if (widget_is_dragging) {
                widget_is_dragging = false;
                
                if (widget->getSnapToGrid()) {
                    int nx = lv_obj_get_x(obj);
                    int ny = lv_obj_get_y(obj);
                    nx = round(nx / 10.0) * 10;
                    ny = round(ny / 10.0) * 10;
                    lv_obj_set_pos(obj, nx, ny);
                }
                
                if (onDeleteRequested) onDeleteRequested(widget);
            }
        }
    } else {
        if (code == LV_EVENT_SHORT_CLICKED) widget->onClick();
        else if (code == LV_EVENT_LONG_PRESSED) {
            if (widget->getType() == "light" && widget->getEntityId().startsWith("light.") && HAWidget::onLightControlRequested) HAWidget::onLightControlRequested(widget);
            else if ((widget->getType() == "media_player" || widget->getType() == "media_item") && HAWidget::onMediaControlRequested) HAWidget::onMediaControlRequested(widget);
        }
    }
}

HALightWidget::HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HALightWidget::updateState(String state) {
    if (this->current_state == state) return; 
    HAWidget::updateState(state);
    uint32_t c_on_val = 0xFFD700; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    if (state == "on" || state == "open") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HALightWidget::onClick() {
    int dotIdx = entity_id.indexOf('.'); String domain = (dotIdx != -1) ? entity_id.substring(0, dotIdx) : "homeassistant";
    HaWebsocketLogic_CallService(domain, "toggle", entity_id);
}


// --- SENSOR WIDGET: JETZT KOMPLETT VARIABEL ---
HASensorWidget::HASensorWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x3498DB), 0); 
    lv_obj_set_style_text_opa(icon_label, 80, 0); 

    state_label = lv_label_create(container);
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28, 0); 
    lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
    lv_label_set_text(state_label, "--");
    
    setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin); 
    updateState("unknown");
}

void HASensorWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    // Ruft die Base-Klasse fuer Icon und Text auf
    HAWidget::setAlignments(i_align, t_align, s_align, i_margin, t_margin, s_margin);
    
    // Wendet zusaetzlich das neue dynamische System auf den Sensorwert an
    auto apply = [](lv_obj_t* obj, int align_val, int margin) {
        if (!obj) return;
        int ox = 0, oy = 0;
        switch(align_val) {
            case LV_ALIGN_TOP_MID: oy = margin; break;
            case LV_ALIGN_BOTTOM_MID: oy = -margin; break;
            case LV_ALIGN_LEFT_MID: ox = margin; break;
            case LV_ALIGN_RIGHT_MID: ox = -margin; break;
            case LV_ALIGN_CENTER: oy = margin; break; 
        }
        lv_obj_align(obj, align_val, ox, oy);
    };
    apply(state_label, state_align, state_margin);
}

void HASensorWidget::updateState(String state) {
    if (this->current_state == state) return; 
    HAWidget::updateState(state);
    
    uint32_t c_on_val = 0x3498DB; 
    if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0);
    
    String unit = HaWebsocketLogic_GetUnit(entity_id);

    if (unit.length() > 0 && state != "unavailable" && state != "unknown") {
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(icon_label, unit.c_str());
    } else {
        lv_obj_set_style_text_font(icon_label, &lela_icons, 0);
        lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str());
    }
    
    lv_label_set_text(state_label, state.c_str());
}

void HASensorWidget::onClick() { }

HAActionWidget::HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str()); updateState("unknown");
}

void HAActionWidget::updateState(String state) {
    this->current_state = state;
    uint32_t c_on_val = 0x9B59B6; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0);
}

void HAActionWidget::onClick() {
    String domain = entity_id.substring(0, entity_id.indexOf('.')); String service = "turn_on"; 
    if (domain == "button" || domain == "input_button") service = "press"; else if (domain == "automation") service = "trigger";
    HaWebsocketLogic_CallService(domain, service, entity_id);
}

HAMediaWidget::HAMediaWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    if (String(mdi).length() == 0) lv_label_set_text(icon_label, getSafeIcon("mdi:television").c_str()); else lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HAMediaWidget::updateState(String state) {
    if (this->current_state == state) return; HAWidget::updateState(state);
    uint32_t c_on_val = 0x00A0FF; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    if (state != "off" && state != "unavailable" && state != "unknown") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAMediaWidget::onClick() { HaWebsocketLogic_CallService("media_player", "toggle", entity_id); }

HAMediaItemWidget::HAMediaItemWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, String m_c_type, String m_c_id)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    this->media_content_type = m_c_type; this->media_content_id = m_c_id;
    if (String(mdi).length() == 0) lv_label_set_text(icon_label, getSafeIcon("mdi:play").c_str()); else lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HAMediaItemWidget::updateState(String state) {
    if (this->current_state == state) return; HAWidget::updateState(state);
    uint32_t c_on_val = 0x00A0FF; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    if (state == "playing") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAMediaItemWidget::onClick() { HaWebsocketLogic_CallPlayMedia(entity_id, media_content_type, media_content_id); }