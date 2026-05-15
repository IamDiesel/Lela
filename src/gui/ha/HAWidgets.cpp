#include "HAWidgets.h"
#include "HaWebsocketLogic.h"
#include "MdiMapper.h"

bool HAWidget::editModeActive = false;
void (*HAWidget::onEditRequested)(HAWidget*) = nullptr;
void (*HAWidget::onDeleteRequested)(HAWidget*) = nullptr;
void (*HAWidget::onLightControlRequested)(HAWidget*) = nullptr; 
void (*HAWidget::onMediaControlRequested)(HAWidget*) = nullptr; 
void (*HAWidget::onVacuumControlRequested)(HAWidget*) = nullptr; // <-- NEU: Vacuum Callback

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
    if (entity_id.startsWith("switch.") || entity_id.startsWith("input_boolean.") || entity_id.startsWith("button.") || entity_id.startsWith("input_button.")) return getSafeIcon("mdi:power"); 
    if (entity_id.startsWith("script.") || entity_id.startsWith("automation.") || entity_id.startsWith("scene.")) return getSafeIcon("mdi:play");
    if (entity_id.startsWith("light.")) return getSafeIcon("mdi:lightbulb"); 
    //if (entity_id.startsWith("switch.") || entity_id.startsWith("input_boolean.")) return getSafeIcon("mdi:power"); 
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
            case LV_ALIGN_CENTER: oy = margin; break; 
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
                    int nx = lv_obj_get_x(obj); int ny = lv_obj_get_y(obj);
                    nx = round(nx / 10.0) * 10; ny = round(ny / 10.0) * 10;
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
            // --- NEU: Longpress Check für Vacuum ---
            else if (widget->getType() == "vacuum" && HAWidget::onVacuumControlRequested) HAWidget::onVacuumControlRequested(widget); 
        }
    }
}

// =========================================================
// LIGHT WIDGET
// =========================================================
HALightWidget::HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HALightWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) return; 
    
    HAWidget::updateState(state);
    lv_obj_set_style_text_opa(icon_label, 255, 0); 

    uint32_t c_on_val = 0xFFD700; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    
    if (state == "on" || state == "open") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HALightWidget::onClick() {
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    int dotIdx = entity_id.indexOf('.'); String domain = (dotIdx != -1) ? entity_id.substring(0, dotIdx) : "homeassistant";
    HaWebsocketLogic_CallService(domain, "toggle", entity_id);
}


// =========================================================
// SENSOR WIDGET (MIT FIX FÜR X-ACHSE & RENDER KOLLAPS)
// =========================================================

HASensorWidget::HASensorWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, bool showChart, int chart_w, int chart_h, int chart_x, int chart_y, String c_min, String c_max)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    
    this->showChart = showChart;
    this->chart_w_pct = chart_w;
    this->chart_h_pct = chart_h;
    this->chart_x_ofs = chart_x;
    this->chart_y_ofs = chart_y;
    this->chart_min = c_min;
    this->chart_max = c_max;
    
    this->chart = nullptr;
    this->state_label = nullptr;
    this->unit_label = nullptr;
    this->ser = nullptr;
    this->current_min = 0;
    this->current_max = 100;
    this->first_value_received = false;
    this->last_value = 0;
    this->last_update_millis = millis();
    
    // BACKFILL: Array direkt mit fiktiven Werten (Jetzt - 10s Schritte) füllen, 
    // damit die X-Achse sofort etwas zum Zeichnen hat!
    time_t now; time(&now);
    for(int i=0; i<50; i++) {
        timestamps[i] = now - ((49 - i) * 10);
        is_held[i] = true; 
    }
    
    hold_timer = lv_timer_create(hold_timer_cb, 10000, this);
    
    buildUI();
    updateState("unknown");
}

HASensorWidget::~HASensorWidget() {
    if (hold_timer) {
        lv_timer_del(hold_timer);
        hold_timer = nullptr;
    }
}

void HASensorWidget::buildUI() {
    if (showChart) {
        lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
        
        chart = lv_chart_create(container);
        lv_obj_set_size(chart, lv_pct(chart_w_pct), lv_pct(chart_h_pct));
        
        // ==========================================
        // DER LAYOUT FIX: Realistische Padding-Werte
        // ==========================================
        // Links Platz für z.B. "22.5", Unten für "14:30"
        lv_obj_set_style_pad_left(chart, 35, 0); 
        lv_obj_set_style_pad_bottom(chart, 25, 0); 
        lv_obj_set_style_pad_right(chart, 10, 0);
        lv_obj_set_style_pad_top(chart, 10, 0);
        
        lv_obj_align(chart, LV_ALIGN_CENTER, chart_x_ofs, chart_y_ofs);
        
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
        lv_chart_set_point_count(chart, 50); 
        
        // draw_size an das neue Padding angepasst, Y hat 4 Ticks, X hat 5 Ticks
        lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 5, 2, 5, 1, true, 25);
        lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 4, 1, true, 35);

        lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
        lv_obj_set_style_line_color(chart, lv_color_hex(0x333333), LV_PART_MAIN); 
        
        // Ticks explizit mit Farbe und Schrift ausstatten
        lv_obj_set_style_line_width(chart, 2, LV_PART_TICKS);
        lv_obj_set_style_line_color(chart, lv_color_hex(0x666666), LV_PART_TICKS);
        lv_obj_set_style_text_color(chart, lv_color_hex(0xAAAAAA), LV_PART_TICKS);
        lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);

        lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(chart, LV_OPA_30, LV_PART_ITEMS); 
        lv_obj_set_style_size(chart, 6, LV_PART_INDICATOR); 

        lv_obj_add_event_cb(chart, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, this);

        ser = lv_chart_add_series(chart, lv_color_hex(0x3498DB), LV_CHART_AXIS_PRIMARY_Y);
        
        state_label = lv_label_create(container);
        lv_obj_set_style_text_font(state_label, &lv_font_montserrat_16, 0); 
        lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
        lv_label_set_text(state_label, "--");
        lv_obj_align(state_label, LV_ALIGN_TOP_RIGHT, -10, 20);
        
        unit_label = lv_label_create(container);
        lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0); 
        lv_obj_set_style_text_color(unit_label, lv_color_hex(0x888888), 0);
        lv_label_set_text(unit_label, "");
        lv_obj_align(unit_label, LV_ALIGN_TOP_LEFT, 15, 20);
        
    } else {
        lv_obj_clear_flag(icon_label, LV_OBJ_FLAG_HIDDEN); 
        state_label = lv_label_create(container);
        lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28, 0); 
        lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
        lv_label_set_text(state_label, "--");
        setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin); 
    }
}

void HASensorWidget::setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) {
    if (this->showChart == show && this->chart_w_pct == w_p && this->chart_h_pct == h_p && 
        this->chart_x_ofs == x_ofs && this->chart_y_ofs == y_ofs &&
        this->chart_min == c_min && this->chart_max == c_max) return;
        
    this->showChart = show;
    this->chart_w_pct = w_p;
    this->chart_h_pct = h_p;
    this->chart_x_ofs = x_ofs;
    this->chart_y_ofs = y_ofs;
    this->chart_min = c_min;
    this->chart_max = c_max;
    
    if (chart) { lv_obj_del_async(chart); chart = nullptr; ser = nullptr; }
    if (state_label) { lv_obj_del_async(state_label); state_label = nullptr; }
    if (unit_label) { lv_obj_del_async(unit_label); unit_label = nullptr; }
    
    first_value_received = false;
    time_t now; time(&now);
    for(int i=0; i<50; i++) {
        timestamps[i] = now - ((49 - i) * 10);
        is_held[i] = true;
    }
    
    buildUI();
    
    String old = current_state;
    current_state = "force_update";
    updateState(old);
}

void HASensorWidget::hold_timer_cb(lv_timer_t * t) {
    HASensorWidget* w = (HASensorWidget*)t->user_data;
    if (!w->first_value_received || !w->showChart) return;
    
    if (millis() - w->last_update_millis >= 10000) {
        w->addChartValue(w->last_value, true); 
        w->last_update_millis = millis(); 
    }
}

void HASensorWidget::addChartValue(float val, bool held) {
    if (!showChart || !chart || !ser) return;
    
    for(int i = 0; i < 49; i++) {
        timestamps[i] = timestamps[i+1];
        is_held[i] = is_held[i+1];
    }
    
    time_t now; time(&now);
    timestamps[49] = now;
    is_held[49] = held;

    if (!first_value_received) {
        current_min = val;
        current_max = val;
        first_value_received = true;
    } else {
        if (val < current_min) current_min = val;
        if (val > current_max) current_max = val;
    }

    // ==========================================
    // FLOAT FIX: Mit 10 multiplizieren für Kommastellen
    // ==========================================
    if (chart_min.length() > 0 && chart_max.length() > 0) {
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)(chart_min.toFloat() * 10.0f), (lv_coord_t)(chart_max.toFloat() * 10.0f));
    } else {
        float range = current_max - current_min;
        if (range < 0.5f) {
            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)((current_min - 0.5f) * 10.0f), (lv_coord_t)((current_max + 0.5f) * 10.0f));
        } else {
            float padding = range * 0.2f;
            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)((current_min - padding) * 10.0f), (lv_coord_t)((current_max + padding) * 10.0f));
        }
    }
    
    lv_chart_set_next_value(chart, ser, (lv_coord_t)(val * 10.0f));
}

void HASensorWidget::chart_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    HASensorWidget* w = (HASensorWidget*)lv_event_get_user_data(e);

    // X-Achse (NTP Uhrzeit - gekürzt auf HH:MM gegen Überlappung)
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X) {
        if (dsc->text) {
            int pt_idx = dsc->value; 
            if (pt_idx >= 0 && pt_idx < 50) {
                time_t t = w->timestamps[pt_idx];
                if (t > 1000000) { // Check ob NTP geladen ist
                    struct tm * tm_info = localtime(&t);
                    snprintf(dsc->text, dsc->text_length, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
                } else {
                    snprintf(dsc->text, dsc->text_length, "--:--"); 
                }
            }
        }
    }
    // Y-Achse (Durch 10 teilen, um Float wiederherzustellen)
    else if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
        if (dsc->text) {
            snprintf(dsc->text, dsc->text_length, "%.1f", (float)dsc->value / 10.0f);
        }
    }
    // Die Linienpunkte (Echte vs. Gehaltene Werte)
    else if (dsc->part == LV_PART_INDICATOR) {
        int pt_idx = dsc->id; 
        if (pt_idx >= 0 && pt_idx < 50) {
            if (w->is_held[pt_idx]) {
                dsc->rect_dsc->bg_opa = LV_OPA_TRANSP;
                dsc->rect_dsc->border_opa = LV_OPA_TRANSP;
            } else {
                dsc->rect_dsc->bg_opa = LV_OPA_COVER;
                dsc->rect_dsc->border_opa = LV_OPA_COVER;
            }
        }
    }
}

void HASensorWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    HAWidget::setAlignments(i_align, t_align, s_align, i_margin, t_margin, s_margin);
    
    if (showChart) return; 
    
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
    
    if (!showChart) {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0);
    } else if (ser) {
        lv_chart_set_series_color(chart, ser, lv_color_hex(c_on_val));
        lv_obj_set_style_bg_color(chart, lv_color_hex(c_on_val), LV_PART_ITEMS);
    }
    
    String unit = HaWebsocketLogic_GetUnit(entity_id);
    String display_text = state;
    
    if (unit.length() > 0 && state != "unavailable" && state != "unknown") {
        display_text += " " + unit;
        if (!showChart) {
            lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);
            lv_label_set_text(icon_label, unit.c_str());
        } else if (unit_label) {
            lv_label_set_text_fmt(unit_label, "[%s]", unit.c_str());
        }
    } else {
        if (!showChart) {
            lv_obj_set_style_text_font(icon_label, &lela_icons, 0);
            lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str());
        }
    }
    
    lv_label_set_text(state_label, display_text.c_str());
    
    if (showChart) {
        if (state != "unavailable" && state != "unknown") {
            float val = state.toFloat(); 
            last_value = val;
            last_update_millis = millis();
            addChartValue(val, false); 
        }
    }
}

void HASensorWidget::onClick() { }

// =========================================================
// ACTION WIDGET
// =========================================================
HAActionWidget::HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str()); updateState("unknown");
}

void HAActionWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) return; 
    
    this->current_state = state;
    lv_obj_set_style_text_opa(icon_label, 255, 0); 

    uint32_t c_on_val = 0x9B59B6; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0);
}

void HAActionWidget::onClick() {
    lv_obj_set_style_text_opa(icon_label, 127, 0); 

    String domain = entity_id.substring(0, entity_id.indexOf('.')); String service = "turn_on"; 
    if (domain == "button" || domain == "input_button") service = "press"; else if (domain == "automation") service = "trigger";
    HaWebsocketLogic_CallService(domain, service, entity_id);
}

// =========================================================
// MEDIA WIDGET
// =========================================================
HAMediaWidget::HAMediaWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    if (String(mdi).length() == 0) lv_label_set_text(icon_label, getSafeIcon("mdi:television").c_str()); else lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HAMediaWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) return; 
    
    HAWidget::updateState(state);
    lv_obj_set_style_text_opa(icon_label, 255, 0); 

    uint32_t c_on_val = 0x00A0FF; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    if (state != "off" && state != "unavailable" && state != "unknown") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
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
    this->media_content_type = m_c_type; this->media_content_id = m_c_id;
    if (String(mdi).length() == 0) lv_label_set_text(icon_label, getSafeIcon("mdi:play").c_str()); else lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HAMediaItemWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) return; 
    
    HAWidget::updateState(state);
    lv_obj_set_style_text_opa(icon_label, 255, 0); 

    uint32_t c_on_val = 0x00A0FF; if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);
    if (state == "playing") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAMediaItemWidget::onClick() { 
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    HaWebsocketLogic_CallPlayMedia(entity_id, media_content_type, media_content_id); 
}

// =========================================================
// VACUUM WIDGET (NEU)
// =========================================================
HAVacuumWidget::HAVacuumWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {

    // Text & Icon linksbündig ausrichten (wegen Doppelbreite)
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 20, -15);
    lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 20, 20);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_LEFT, 0);

    // Play/Pause Button
    btn_play_pause = lv_btn_create(container);
    lv_obj_set_size(btn_play_pause, 55, 55);
    lv_obj_align(btn_play_pause, LV_ALIGN_RIGHT_MID, -85, 0);
    lv_obj_set_style_bg_color(btn_play_pause, lv_color_hex(0x2980B9), 0);
    lbl_play_pause = lv_label_create(btn_play_pause);
    lv_label_set_text(lbl_play_pause, LV_SYMBOL_PLAY);
    lv_obj_center(lbl_play_pause);

    lv_obj_add_event_cb(btn_play_pause, [](lv_event_t* e){
        HAVacuumWidget* w = (HAVacuumWidget*)lv_event_get_user_data(e);
        if(w->current_state == "cleaning") HaWebsocketLogic_CallVacuumService(w->entity_id, "pause");
        else HaWebsocketLogic_CallVacuumService(w->entity_id, "start");
    }, LV_EVENT_CLICKED, this);

    // Stop Button
    btn_stop = lv_btn_create(container);
    lv_obj_set_size(btn_stop, 55, 55);
    lv_obj_align(btn_stop, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xC0392B), 0);
    lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_STOP);
    lv_obj_center(lbl_stop);

    lv_obj_add_event_cb(btn_stop, [](lv_event_t* e){
        HAVacuumWidget* w = (HAVacuumWidget*)lv_event_get_user_data(e);
        HaWebsocketLogic_CallVacuumService(w->entity_id, "stop");
    }, LV_EVENT_CLICKED, this);

    updateState("unknown");
}

void HAVacuumWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) return; 
    
    HAWidget::updateState(state);
    lv_obj_set_style_text_opa(icon_label, 255, 0); 

    if(state == "cleaning") lv_label_set_text(lbl_play_pause, LV_SYMBOL_PAUSE);
    else lv_label_set_text(lbl_play_pause, LV_SYMBOL_PLAY);

    uint32_t c_on_val = 0x1ABC9C; 
    if (color_on.length() > 0 && color_on.startsWith("#")) c_on_val = strtol(color_on.substring(1).c_str(), NULL, 16);
    uint32_t c_off_val = 0x555555; 
    if (color_off.length() > 0 && color_off.startsWith("#")) c_off_val = strtol(color_off.substring(1).c_str(), NULL, 16);

    if (state == "cleaning" || state == "returning" || state == "error") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HAVacuumWidget::onClick() {
    // Bleibt absichtlich leer - Interaktion verläuft hier über Buttons oder den Longpress (Popup)
}