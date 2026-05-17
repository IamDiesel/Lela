#pragma once
#include <Arduino.h>
#include <time.h> 
#include "LVGL_Driver.h"

String formatEntityName(String entity_id, String name);
String getSafeIcon(String mdi);
String getIconForEntity(String entity_id, String mdi_icon);

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

    bool show_chart = false;
    int chart_w_pct = 60;
    int chart_h_pct = 40;
    int chart_x_ofs = 10;
    int chart_y_ofs = 10;
    String chart_min = "";
    String chart_max = "";

    String tap_domain = "";
    String tap_service = "";
    String tap_target = "";

    // --- NEU: Speicherplaetze fuer eingebackene Konfigurationen ---
    String baked_options = "";
    float baked_min = 0.0f;
    float baked_max = 100.0f;
    float baked_step = 1.0f;

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
    void setSnapToGrid(bool snap);
    
    void setTapAction(String domain, String service, String target);
    String getTapDomain();
    String getTapService();
    String getTapTarget();

    // --- NEU: Getter/Setter fuer Fabrik & Widgets ---
    void setBakedOptions(String opts) { baked_options = opts; }
    String getBakedOptions() { return baked_options; }
    void setBakedLimits(float mn, float mx, float stp) { baked_min = mn; baked_max = mx; baked_step = stp; }
    float getBakedMin() { return baked_min; }
    float getBakedMax() { return baked_max; }
    float getBakedStep() { return baked_step; }
    
    virtual void setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max);

    String getEntityId();
    String getType();
    String getName();
    String getMdiIcon();
    String getColorOn();
    String getColorOff();
    int getX();
    int getY();
    int getW();
    int getH();
    int getTabIndex();
    
    int getIconAlign();
    int getTextAlign();
    int getStateAlign();
    int getIconMargin();
    int getTextMargin();
    int getStateMargin();
    bool getSnapToGrid();
    
    virtual bool getShowChart();
    virtual int getChartWPct();
    virtual int getChartHPct();
    virtual int getChartXOfs();
    virtual int getChartYOfs();
    virtual String getChartMin();
    virtual String getChartMax();
    virtual String getMediaContentType();
    virtual String getMediaContentId();

    virtual void updateState(String state);
    virtual void onClick() = 0; 
};

class HALightWidget : public HAWidget {
public:
    HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};

class HAActionWidget : public HAWidget {
public:
    HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    void updateState(String state) override;
    void onClick() override;
};