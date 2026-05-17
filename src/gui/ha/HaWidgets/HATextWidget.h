#pragma once
#include "HAWidgetBase.h"

class HATextWidget : public HAWidget {
public:
    HATextWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;
    
    // --- NEU: Override um Position/Margin des Textes konfigurierbar zu machen ---
    void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) override;

private:
    lv_obj_t* lbl_value;
    
    // Statische Pointer fuer das Fullscreen-Overlay
    static lv_obj_t* keyboard_overlay;
    static lv_obj_t* keyboard_ta;
    static lv_obj_t* keyboard_lbl;
    static HATextWidget* active_text_widget;

    static void keyboard_event_cb(lv_event_t * e);
    static void close_keyboard();
};