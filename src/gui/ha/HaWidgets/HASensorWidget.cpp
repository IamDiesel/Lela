#include "HASensorWidget.h"
#include "HaWebsocketLogic.h"
#include "MdiMapper.h"
#include <math.h>
#include <string.h>

// Anzahl beschrifteter Zeitmarken auf der X-Achse.
static const int X_MAJOR_TICKS = 5;

HASensorWidget::HASensorWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, bool showChart, int chart_w, int chart_h, int chart_x, int chart_y, String c_min, String c_max)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi, c_on, c_off) {
    
    this->show_chart = showChart; 
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
    
    this->chart_scale = 10.0f;
    this->last_value = 0; 
    this->last_update_millis = millis();
    resetHistory();
    
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
    if (show_chart) {
        lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
        
        chart = lv_chart_create(container);
        lv_obj_set_size(chart, lv_pct(chart_w_pct), lv_pct(chart_h_pct));
        lv_obj_set_style_pad_left(chart, 35, 0); 
        lv_obj_set_style_pad_bottom(chart, 25, 0); 
        lv_obj_set_style_pad_right(chart, 10, 0); 
        lv_obj_set_style_pad_top(chart, 10, 0);
        lv_obj_align(chart, LV_ALIGN_CENTER, chart_x_ofs, chart_y_ofs);
        
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE); 
        lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
        lv_chart_set_point_count(chart, CHART_POINTS); 
        lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 5, 2, X_MAJOR_TICKS, 1, true, 25);
        lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 4, 1, true, 35);
        
        lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN); 
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
        lv_obj_set_style_line_color(chart, lv_color_hex(0x333333), LV_PART_MAIN); 
        
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
        lv_label_set_recolor(state_label, true); 
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
        lv_label_set_recolor(state_label, true); 
        lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28, 0); 
        lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
        lv_label_set_text(state_label, "--");
        
        setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin); 
    }
}

void HASensorWidget::setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) {
    if (this->show_chart == show && 
        this->chart_w_pct == w_p && 
        this->chart_h_pct == h_p && 
        this->chart_x_ofs == x_ofs && 
        this->chart_y_ofs == y_ofs && 
        this->chart_min == c_min && 
        this->chart_max == c_max) {
        return;
    }
    
    this->show_chart = show; 
    this->chart_w_pct = w_p; 
    this->chart_h_pct = h_p; 
    this->chart_x_ofs = x_ofs; 
    this->chart_y_ofs = y_ofs; 
    this->chart_min = c_min; 
    this->chart_max = c_max;
    
    if (chart) { 
        lv_obj_del_async(chart); 
        chart = nullptr; 
        ser = nullptr; 
    }
    if (state_label) { 
        lv_obj_del_async(state_label); 
        state_label = nullptr; 
    }
    if (unit_label) { 
        lv_obj_del_async(unit_label); 
        unit_label = nullptr; 
    }
    
    resetHistory();
    buildUI(); 
    
    String old = current_state; 
    current_state = "force_update"; 
    updateState(old);
}

void HASensorWidget::hold_timer_cb(lv_timer_t * t) {
    HASensorWidget* w = (HASensorWidget*)t->user_data;
    
    if (!w->first_value_received || !w->show_chart) {
        return;
    }
    
    if (millis() - w->last_update_millis >= 10000) { 
        w->addChartValue(w->last_value, true); 
        w->last_update_millis = millis(); 
    }
}

void HASensorWidget::resetHistory() {
    first_value_received = false;
    time_t now;
    time(&now);
    for (int i = 0; i < CHART_POINTS; i++) {
        history[i] = NAN;
        timestamps[i] = now - ((CHART_POINTS - 1 - i) * 10);
        is_held[i] = true;
    }
}

void HASensorWidget::addChartValue(float val, bool held) {
    if (!show_chart || !chart || !ser) {
        return;
    }
    
    // Verlauf um einen Punkt nach vorne schieben, neuer Wert ans Ende.
    memmove(&history[0],    &history[1],    (CHART_POINTS - 1) * sizeof(history[0]));
    memmove(&timestamps[0], &timestamps[1], (CHART_POINTS - 1) * sizeof(timestamps[0]));
    memmove(&is_held[0],    &is_held[1],    (CHART_POINTS - 1) * sizeof(is_held[0]));

    time_t now; 
    time(&now); 
    history[CHART_POINTS - 1] = val;
    timestamps[CHART_POINTS - 1] = now; 
    is_held[CHART_POINTS - 1] = held;
    first_value_received = true;

    redrawChart();
}

void HASensorWidget::redrawChart() {
    if (!chart || !ser) return;

    // Bereich aus dem SICHTBAREN Verlauf - er schrumpft also auch wieder,
    // wenn ein Ausreisser aus dem Fenster gelaufen ist.
    float lo = INFINITY, hi = -INFINITY;
    for (int i = 0; i < CHART_POINTS; i++) {
        if (isnan(history[i])) continue;
        if (history[i] < lo) lo = history[i];
        if (history[i] > hi) hi = history[i];
    }
    if (lo > hi) return;   // noch kein Wert

    float fixMin, fixMax;
    if (HAWidget::parseNumber(chart_min, fixMin) && HAWidget::parseNumber(chart_max, fixMax) && fixMax > fixMin) {
        lo = fixMin;
        hi = fixMax;
    } else {
        const float range = hi - lo;
        const float pad = (range < 0.5f) ? 0.5f : range * 0.2f;
        lo -= pad;
        hi += pad;
    }

    // Skalierung so waehlen, dass beide Grenzen in int16 passen.
    const float maxAbs = fmaxf(fabsf(lo), fabsf(hi));
    chart_scale = 10.0f;
    while (maxAbs * chart_scale > 30000.0f && chart_scale > 0.0001f) {
        chart_scale /= 10.0f;
    }

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
                       (lv_coord_t)lroundf(lo * chart_scale), (lv_coord_t)lroundf(hi * chart_scale));

    // Alle Punkte neu schreiben (Index 0 = aeltester), damit eine geaenderte
    // Skalierung auch fuer die alten Punkte gilt.
    lv_chart_set_x_start_point(chart, ser, 0);
    lv_coord_t* ys = lv_chart_get_y_array(chart, ser);
    for (int i = 0; i < CHART_POINTS; i++) {
        ys[i] = isnan(history[i]) ? LV_CHART_POINT_NONE : (lv_coord_t)lroundf(history[i] * chart_scale);
    }
    lv_chart_refresh(chart);
}

void HASensorWidget::chart_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    HASensorWidget* w = (HASensorWidget*)lv_event_get_user_data(e);
    
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X) {
        if (dsc->text) {
            // LVGL liefert hier die Nummer der Hauptmarke (0..X_MAJOR_TICKS-1),
            // nicht den Punktindex. Frueher wurden deshalb die 5 aeltesten
            // Punkte beschriftet. Marke -> gleichmaessig verteilter Punkt.
            const int tick = dsc->value;
            const int pt_idx = (tick * (CHART_POINTS - 1)) / (X_MAJOR_TICKS - 1);
            if (pt_idx >= 0 && pt_idx < CHART_POINTS) {
                time_t t = w->timestamps[pt_idx];
                if (t > 1000000) { 
                    struct tm * tm_info = localtime(&t); 
                    snprintf(dsc->text, dsc->text_length, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min); 
                } else {
                    snprintf(dsc->text, dsc->text_length, "--:--"); 
                }
            }
        }
    } else if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
        if (dsc->text) {
            const float v = (float)dsc->value / w->chart_scale;
            if (w->chart_scale >= 10.0f) snprintf(dsc->text, dsc->text_length, "%.1f", v);
            else                         snprintf(dsc->text, dsc->text_length, "%.0f", v);
        }
    } else if (dsc->part == LV_PART_INDICATOR) {
        int pt_idx = dsc->id; 
        if (pt_idx >= 0 && pt_idx < CHART_POINTS) {
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
    
    if (show_chart) {
        return; 
    }
    
    auto apply = [](lv_obj_t* obj, int align_val, int margin) {
        if (!obj) {
            return; 
        }
        int ox = 0;
        int oy = 0;
        switch(align_val) { 
            case LV_ALIGN_TOP_MID: 
                oy = margin; 
                break; 
            case LV_ALIGN_BOTTOM_MID: 
                oy = -margin; 
                break; 
            case LV_ALIGN_LEFT_MID: 
                ox = margin; 
                break; 
            case LV_ALIGN_RIGHT_MID: 
                ox = -margin; 
                break; 
            case LV_ALIGN_CENTER: 
                oy = margin; 
                break; 
        }
        lv_obj_align(obj, align_val, ox, oy);
    };
    
    apply(state_label, state_align, state_margin);
}

void HASensorWidget::updateState(String state) {
    if (this->current_state == state) {
        return; 
    }
    
    HAWidget::updateState(state);
    
    const uint32_t c_on_val = colorOnOr(0x3498DB);
    const uint32_t c_off_val = colorOffOr(0x888888);   // Grau fuer "Aus"

    uint32_t active_color = c_on_val;
    String display_text = state;
    
    // binary_sensor: Farbe nach aktiv/inaktiv und deutsche Klartexte
    if (entity_id.startsWith("binary_sensor.")) {
        // Logik: Ist der Sensor in einem aktiven Zustand?
        bool isActive = (state == "on" || state == "open" || state == "home" || state == "true" || state == "1");
        active_color = isActive ? c_on_val : c_off_val;
        
        // Uebersetzung in lesbare deutsche Texte
        if (state == "on") display_text = "An";
        else if (state == "off") display_text = "Aus";
        else if (state == "open") display_text = "Offen";
        else if (state == "closed") display_text = "Zu";
        else if (state == "home") display_text = "Zuhause";
        else if (state == "not_home") display_text = "Abwesend";
    }
    
    if (!show_chart) {
        lv_obj_set_style_text_color(icon_label, lv_color_hex(active_color), 0);
    } else if (ser) { 
        lv_chart_set_series_color(chart, ser, lv_color_hex(active_color)); 
        lv_obj_set_style_bg_color(chart, lv_color_hex(active_color), LV_PART_ITEMS); 
    }
    
    String unit = HaWebsocketLogic_GetUnit(entity_id); 
    
    if (unit.length() > 0 && state != "unavailable" && state != "unknown") {
        display_text += " #888888 " + unit + "#"; 
        
        if (!show_chart) { 
            lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0); 
            lv_obj_set_style_text_opa(icon_label, 60, 0); 
            lv_label_set_text(icon_label, unit.c_str()); 
        } else if (unit_label) {
            lv_label_set_text_fmt(unit_label, "[%s]", unit.c_str());
        }
    } else {
        if (!show_chart) { 
            lv_obj_set_style_text_font(icon_label, &lela_icons, 0); 
            lv_obj_set_style_text_opa(icon_label, 255, 0); 
            lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str()); 
        }
    }
    
    lv_label_set_text(state_label, display_text.c_str());
    
    // Nur echte Zahlen ins Diagramm ("on", "unavailable" usw. wurden frueher
    // als 0 eingetragen).
    float val;
    if (show_chart && HAWidget::parseNumber(state, val)) {
        last_value = val; 
        last_update_millis = millis(); 
        addChartValue(val, false); 
    }
}

// Klick: nur die konfigurierte Aktion (Anzeige-Widget hat keine eigene).
void HASensorWidget::onClick() {
    runTapAction();
}
