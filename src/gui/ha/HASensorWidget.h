#pragma once
#include "HAWidgetBase.h"

class HASensorWidget : public HAWidget {
private:
    lv_obj_t* chart;
    lv_chart_series_t * ser;
    lv_obj_t* state_label;
    lv_obj_t* unit_label;
    
    float current_min;
    float current_max;
    bool first_value_received;
    
    time_t timestamps[50];
    bool is_held[50];
    lv_timer_t* hold_timer;
    float last_value;
    uint32_t last_update_millis;

    static void chart_draw_event_cb(lv_event_t * e);
    static void hold_timer_cb(lv_timer_t * t);
    
    void addChartValue(float val, bool held);
    void buildUI();

public:
    HASensorWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off, bool showChart, int chart_w, int chart_h, int chart_x, int chart_y, String c_min, String c_max);
    ~HASensorWidget();
    
    void updateState(String state) override;
    void onClick() override;
    void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) override;
    void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) override;
};