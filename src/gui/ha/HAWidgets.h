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

    HAWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    virtual ~HAWidget();

    virtual void updateState(String state);

    int getTabIndex() { return tab_index; }
    String getType() { return type; }
    String getEntityId() { return entity_id; }
    String getName() { return widget_name; }
    String getMdiIcon() { return mdi_icon; } 
    String getColorOn() { return color_on; }   
    String getColorOff() { return color_off; } 
    
    int getIconAlign() { return icon_align; } 
    int getTextAlign() { return text_align; } 
    int getStateAlign() { return state_align; } 
    
    int getIconMargin() { return icon_margin; } 
    int getTextMargin() { return text_margin; } 
    int getStateMargin() { return state_margin; } 
    
    bool getSnapToGrid() { return snap_to_grid; } 
    
    virtual bool getShowChart() { return false; }
    virtual int getChartWPct() { return 95; }
    virtual int getChartHPct() { return 50; }
    virtual int getChartXOfs() { return 0; }
    virtual int getChartYOfs() { return -15; }
    virtual String getChartMin() { return ""; }
    virtual String getChartMax() { return ""; }
    virtual void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) {}
    
    virtual String getMediaContentType() { return ""; }
    virtual String getMediaContentId() { return ""; }

    int getX() { return lv_obj_get_x(container); }
    int getY() { return lv_obj_get_y(container); }
    int getW() { return lv_obj_get_width(container); }
    int getH() { return lv_obj_get_height(container); }

    void setSize(int w, int h);
    void setName(String n);            
    void setMdiIcon(String mdi);       
    void setColors(String on, String off); 
    void setSnapToGrid(bool snap) { snap_to_grid = snap; } 
    
    virtual void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin); 

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
    lv_obj_t* state_label;
    
    bool showChart;
    int chart_w_pct;
    int chart_h_pct;
    int chart_x_ofs;
    int chart_y_ofs;
    String chart_min;
    String chart_max;
    
    lv_obj_t* chart;
    lv_chart_series_t* ser;
    lv_obj_t* unit_label; 
    
    uint32_t timestamps[50];
    bool is_held[50];
    float last_value;
    uint32_t last_update_millis;
    lv_timer_t* hold_timer;
    
    float current_min;
    float current_max;
    bool first_value_received;
    
    void buildUI(); 
    void addChartValue(float val, bool held);
    static void chart_draw_event_cb(lv_event_t * e);
    static void hold_timer_cb(lv_timer_t * t);
    
public:
    HASensorWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off, bool showChart = false, int chart_w = 95, int chart_h = 50, int chart_x = 0, int chart_y = -15, String c_min = "", String c_max = "");
    virtual ~HASensorWidget() override; 
    
    void updateState(String state) override;
    void onClick() override;
    void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) override; 
    
    bool getShowChart() override { return showChart; }
    int getChartWPct() override { return chart_w_pct; }
    int getChartHPct() override { return chart_h_pct; }
    int getChartXOfs() override { return chart_x_ofs; }
    int getChartYOfs() override { return chart_y_ofs; }
    String getChartMin() override { return chart_min; }
    String getChartMax() override { return chart_max; }
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
    HAMediaItemWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off, String m_c_type, String m_c_id);
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
    void onClick() override; // <-- DIESE ZEILE HAT GEFEHLT
};