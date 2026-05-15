#pragma once
#include <Arduino.h>
#include <time.h> 
#include "LVGL_Driver.h"

String formatEntityName(String entity_id, String name);

class HAWidget {
protected:
    int tab_index;      
    String type;         
    String entity_id;
    String widget_name;  
    String mdi_icon;     
    String color_on;     
    String color_off;    
    
    int icon_align;
    int text_align;
    int state_align; 
    
    int icon_margin; 
    int text_margin; 
    int state_margin; 
    
    bool snap_to_grid; 

    // Variablen-Recycling: Fuer Sensoren (Diagramme) UND Vacuum (Button Layout)
    bool show_chart = false;
    int chart_w_pct = 60;
    int chart_h_pct = 40;
    int chart_x_ofs = 10;
    int chart_y_ofs = 10;
    String chart_min = "";
    String chart_max = "";

    lv_obj_t* icon_label;
    lv_obj_t* name_label;
    String current_state;

    static void widget_event_cb(lv_event_t * e);

public:
    lv_obj_t* container;
    static bool editModeActive;
    
    static void (*onEditRequested)(HAWidget*);
    static void (*onDeleteRequested)(HAWidget*);
    static void (*onLightControlRequested)(HAWidget* w); 
    static void (*onMediaControlRequested)(HAWidget* w); 
    static void (*onVacuumControlRequested)(HAWidget* w); 

    HAWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off);
    virtual ~HAWidget();

    virtual void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin);
    void setName(String n);
    void setMdiIcon(String mdi);
    void setColors(String on, String off);
    void setSize(int w, int h);
    void setPosition(int x, int y, bool snap);
    void setSnapToGrid(bool snap) { this->snap_to_grid = snap; }
    
    virtual void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) {
        show_chart = show; chart_w_pct = w_p; chart_h_pct = h_p; chart_x_ofs = x_ofs; chart_y_ofs = y_ofs; chart_min = c_min; chart_max = c_max;
    }

    String getEntityId() { return entity_id; }
    String getType() { return type; }
    String getName() { return widget_name; }
    String getMdiIcon() { return mdi_icon; }
    String getColorOn() { return color_on; }
    String getColorOff() { return color_off; }
    int getX() { return lv_obj_get_x(container); }
    int getY() { return lv_obj_get_y(container); }
    int getW() { return lv_obj_get_width(container); }
    int getH() { return lv_obj_get_height(container); }
    int getTabIndex() { return tab_index; }
    
    int getIconAlign() { return icon_align; }
    int getTextAlign() { return text_align; }
    int getStateAlign() { return state_align; }
    int getIconMargin() { return icon_margin; }
    int getTextMargin() { return text_margin; }
    int getStateMargin() { return state_margin; }
    bool getSnapToGrid() { return snap_to_grid; }
    
    virtual bool getShowChart() { return show_chart; }
    virtual int getChartWPct() { return chart_w_pct; }
    virtual int getChartHPct() { return chart_h_pct; }
    virtual int getChartXOfs() { return chart_x_ofs; }
    virtual int getChartYOfs() { return chart_y_ofs; }
    virtual String getChartMin() { return chart_min; }
    virtual String getChartMax() { return chart_max; }
    virtual String getMediaContentType() { return ""; }
    virtual String getMediaContentId() { return ""; }

    virtual void updateState(String state);
    virtual void onClick() = 0; 
};

class HALightWidget : public HAWidget {
public:
    HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};

class HASensorWidget : public HAWidget {
private:
    lv_obj_t* chart;
    lv_chart_series_t * ser;
    lv_obj_t* state_label;
    lv_obj_t* unit_label;
    
    float current_min, current_max;
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

class HAActionWidget : public HAWidget {
public:
    HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};

class HAMediaWidget : public HAWidget {
public:
    HAMediaWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};

class HAMediaItemWidget : public HAWidget {
private:
    String media_content_type;
    String media_content_id;
public:
    HAMediaItemWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, String m_c_type, String m_c_id);
    void updateState(String state) override;
    void onClick() override;
    String getMediaContentType() override { return media_content_type; }
    String getMediaContentId() override { return media_content_id; }
};

class HAVacuumWidget : public HAWidget {
private:
    lv_obj_t* btn_play_pause;
    lv_obj_t* lbl_play_pause;
    lv_obj_t* btn_stop;
    lv_obj_t* lbl_stop;
public:
    HAVacuumWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
    void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) override;
};