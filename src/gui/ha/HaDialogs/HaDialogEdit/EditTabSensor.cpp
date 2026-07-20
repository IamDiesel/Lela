#include "EditTabSensor.h"
#include "UIHelper.h"
#include "HaDialogEdit.h" 

void EditTabSensor::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    this->current_widget = w;

    if (w->getType() != "sensor") {
        UIHelper::createLabel(parent, "Keine Diagramm-Optionen\nfür diesen Widget-Typ verfügbar.", 
                              &lv_font_montserrat_20, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    int cy = 10;
    
    // 1. Chart Aktivieren Checkbox
    cb_chart = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_chart, "Als Diagramm (Verlauf) anzeigen");
    lv_obj_set_style_text_font(cb_chart, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(cb_chart, 10, cy);
    if (w->getShowChart()) lv_obj_add_state(cb_chart, LV_STATE_CHECKED);
    cy += 50;

    auto chart_slider_cb = [](lv_event_t* e) {
        EditTabSensor* tab = (EditTabSensor*)lv_event_get_user_data(e);
        if(!tab) return;
        lv_label_set_text_fmt(tab->lbl_c_w_val, "%d %%", (int)lv_slider_get_value(tab->slider_chart_w));
        lv_label_set_text_fmt(tab->lbl_c_h_val, "%d %%", (int)lv_slider_get_value(tab->slider_chart_h));
        lv_label_set_text_fmt(tab->lbl_c_x_val, "%d px", (int)lv_slider_get_value(tab->slider_chart_x));
        lv_label_set_text_fmt(tab->lbl_c_y_val, "%d px", (int)lv_slider_get_value(tab->slider_chart_y));
    };

    // --- NEUE MAßE FÜR BESSERE TOUCH-BEDIENUNG ---
    int slider_w = 350;    // Vorher: 250 -> Jetzt 40% breiter!
    int val_lbl_x = 490;   // Vorher: 400 -> Nach rechts gerückt, damit es neben den Slider passt

    // Breite
    UIHelper::createLabel(parent, "Breite:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    slider_chart_w = UIHelper::createSlider(parent, slider_w, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 50, 100, w->getChartWPct());
    lbl_c_w_val = UIHelper::createLabel(parent, (String(w->getChartWPct()) + " %").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, val_lbl_x, cy);
    lv_obj_add_event_cb(slider_chart_w, chart_slider_cb, LV_EVENT_VALUE_CHANGED, this);
    cy += 40;

    // Höhe
    UIHelper::createLabel(parent, "Hoehe:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    slider_chart_h = UIHelper::createSlider(parent, slider_w, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 20, 100, w->getChartHPct());
    lbl_c_h_val = UIHelper::createLabel(parent, (String(w->getChartHPct()) + " %").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, val_lbl_x, cy);
    lv_obj_add_event_cb(slider_chart_h, chart_slider_cb, LV_EVENT_VALUE_CHANGED, this);
    cy += 40;

    // X-Offset
    UIHelper::createLabel(parent, "X-Pos:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    slider_chart_x = UIHelper::createSlider(parent, slider_w, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, -100, 100, w->getChartXOfs());
    lbl_c_x_val = UIHelper::createLabel(parent, (String(w->getChartXOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, val_lbl_x, cy);
    lv_obj_add_event_cb(slider_chart_x, chart_slider_cb, LV_EVENT_VALUE_CHANGED, this);
    cy += 40;

    // Y-Offset
    UIHelper::createLabel(parent, "Y-Pos:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    slider_chart_y = UIHelper::createSlider(parent, slider_w, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, -200, 100, w->getChartYOfs());
    lbl_c_y_val = UIHelper::createLabel(parent, (String(w->getChartYOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, val_lbl_x, cy);
    lv_obj_add_event_cb(slider_chart_y, chart_slider_cb, LV_EVENT_VALUE_CHANGED, this);
    cy += 50;

    // 3. Min/Max Felder (bleiben unverändert, da Textfelder)
    UIHelper::createLabel(parent, "Min. Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy+10);
    ta_chart_min = UIHelper::createTextarea(parent, 150, 40, LV_ALIGN_TOP_LEFT, 120, cy, w->getChartMin().c_str(), "Auto");
    
    UIHelper::createLabel(parent, "Max. Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 300, cy+10);
    ta_chart_max = UIHelper::createTextarea(parent, 150, 40, LV_ALIGN_TOP_LEFT, 420, cy, w->getChartMax().c_str(), "Auto");

    HaDialogEdit::bindKeyboard(ta_chart_min, LV_KEYBOARD_MODE_NUMBER);
    HaDialogEdit::bindKeyboard(ta_chart_max, LV_KEYBOARD_MODE_NUMBER);
}

void EditTabSensor::saveConfig(HAWidget* w) {
    if (!cb_chart || !lv_obj_is_valid(cb_chart)) return;

    w->setChartConfig(
        lv_obj_has_state(cb_chart, LV_STATE_CHECKED),
        lv_slider_get_value(slider_chart_w),
        lv_slider_get_value(slider_chart_h),
        lv_slider_get_value(slider_chart_x),
        lv_slider_get_value(slider_chart_y),
        String(lv_textarea_get_text(ta_chart_min)),
        String(lv_textarea_get_text(ta_chart_max))
    );
}