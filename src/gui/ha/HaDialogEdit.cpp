#include "HaDialogEdit.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "HaConfigLogic.h"
#include "MdiMapper.h"

static lv_obj_t* action_overlay = nullptr; 
static lv_obj_t* edit_panel = nullptr;          
static lv_obj_t* edit_name_preview = nullptr;   

lv_obj_t* HaDialogEdit::ta_name = nullptr;
lv_obj_t* HaDialogEdit::ta_entity = nullptr;
lv_obj_t* HaDialogEdit::ta_icon_search = nullptr; 
lv_obj_t* HaDialogEdit::dd_icon_cat = nullptr;
lv_obj_t* HaDialogEdit::dd_icon = nullptr;
lv_obj_t* HaDialogEdit::dd_type = nullptr;
lv_obj_t* HaDialogEdit::cb_snap = nullptr;

lv_obj_t* HaDialogEdit::cb_chart = nullptr; 
lv_obj_t* HaDialogEdit::slider_chart_w = nullptr;
lv_obj_t* HaDialogEdit::slider_chart_h = nullptr;
lv_obj_t* HaDialogEdit::slider_chart_x = nullptr; 
lv_obj_t* HaDialogEdit::slider_chart_y = nullptr; 
lv_obj_t* HaDialogEdit::lbl_c_w_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_c_h_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_c_x_val = nullptr;    
lv_obj_t* HaDialogEdit::lbl_c_y_val = nullptr;    
lv_obj_t* HaDialogEdit::ta_chart_min = nullptr;
lv_obj_t* HaDialogEdit::ta_chart_max = nullptr;

lv_obj_t* HaDialogEdit::slider_icon_margin = nullptr;
lv_obj_t* HaDialogEdit::slider_text_margin = nullptr;
lv_obj_t* HaDialogEdit::lbl_i_m_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_t_m_val = nullptr;

lv_obj_t* HaDialogEdit::dd_state_pos = nullptr;
lv_obj_t* HaDialogEdit::slider_state_margin = nullptr;
lv_obj_t* HaDialogEdit::lbl_s_m_val = nullptr;

lv_obj_t* HaDialogEdit::kb = nullptr;
HAWidget* HaDialogEdit::current_widget = nullptr;
lv_obj_t* HaDialogEdit::overlay = nullptr;

static lv_obj_t* light_control_overlay = nullptr;
static lv_obj_t* cur_light_slider = nullptr;
static lv_obj_t* cur_light_w_slider = nullptr; 
static lv_obj_t* cur_light_cw = nullptr;
static String cur_light_entity = "";

lv_obj_t* HaDialogEdit::btn_color_on = nullptr;
lv_obj_t* HaDialogEdit::btn_color_off = nullptr;
lv_obj_t* HaDialogEdit::color_picker_overlay = nullptr;
bool HaDialogEdit::picking_color_on = true;
String HaDialogEdit::selected_color_on = "";
String HaDialogEdit::selected_color_off = "";

static String last_search_term = "";
static String search_results_options = "";

static int orig_w = 0, orig_h = 0, orig_i_align = 0, orig_t_align = 0, orig_s_align = 0;
static int orig_i_margin = 0, orig_t_margin = 0, orig_s_margin = 0;

static lv_obj_t* dd_icon_pos = nullptr;
static lv_obj_t* dd_text_pos = nullptr;

static int idxToAlign(int idx) {
    switch(idx) {
        case 0: return LV_ALIGN_TOP_MID;                 
        case 1: return LV_ALIGN_CENTER;                  
        case 2: return LV_ALIGN_BOTTOM_MID;        
        case 3: return LV_ALIGN_LEFT_MID;              
        case 4: return LV_ALIGN_RIGHT_MID;                
        default: return LV_ALIGN_CENTER;
    }
}
static int alignToIdx(int align) {
    switch(align) {
        case LV_ALIGN_TOP_MID: return 0;
        case LV_ALIGN_CENTER: return 1;
        case LV_ALIGN_BOTTOM_MID: return 2;
        case LV_ALIGN_LEFT_MID: return 3;
        case LV_ALIGN_RIGHT_MID: return 4;
    }
    return 1; 
}

void HaDialogEdit::icon_search_event_cb(lv_event_t * e) {
    if (!ta_icon_search) return;
    last_search_term = String(lv_textarea_get_text(ta_icon_search));
    last_search_term.toLowerCase();

    if (last_search_term.length() < 2) {
        if (dd_icon_cat && dd_icon) {
            int cat_idx = lv_dropdown_get_selected(dd_icon_cat);
            lv_dropdown_set_options(dd_icon, icon_categories[cat_idx].options);
        }
        return;
    }

    search_results_options = "--- Suchergebnisse ---";
    int count = 0;
    
    for (int c = 1; c < num_icon_categories; c++) {
        String opts = String(icon_categories[c].options);
        int pos = 0;
        while ((pos = opts.indexOf("mdi:", pos)) != -1) {
            int end = opts.indexOf('\n', pos);
            String icon_name = (end == -1) ? opts.substring(pos) : opts.substring(pos, end);
            String search_target = icon_name; search_target.toLowerCase();
            
            if (search_target.indexOf(last_search_term) != -1) {
                search_results_options += "\n" + icon_name;
                count++;
            }
            pos = (end == -1) ? opts.length() : end + 1;
            if (count >= 50) break;
        }
        if (count >= 50) break;
    }

    if (count == 0) search_results_options += "\nKeine Treffer";
    lv_dropdown_set_options(dd_icon, search_results_options.c_str());
    lv_dropdown_set_selected(dd_icon, count > 0 ? 1 : 0);
}

void HaDialogEdit::dd_icon_cat_event_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    int cat_idx = lv_dropdown_get_selected(dd);
    lv_dropdown_set_options(dd_icon, icon_categories[cat_idx].options);
    lv_dropdown_set_selected(dd_icon, 0); 
    
    if (last_search_term.length() > 0) {
        last_search_term = "";
        lv_textarea_set_text(ta_icon_search, "");
    }
}

void HaDialogEdit::resetState() {
    action_overlay = nullptr; edit_panel = nullptr; edit_name_preview = nullptr;
    light_control_overlay = nullptr; cur_light_slider = nullptr; cur_light_w_slider = nullptr;
    cur_light_cw = nullptr; color_picker_overlay = nullptr;
    dd_state_pos = nullptr; slider_state_margin = nullptr; lbl_s_m_val = nullptr;
    cb_chart = nullptr; slider_chart_w = nullptr; slider_chart_h = nullptr;
    slider_chart_x = nullptr; slider_chart_y = nullptr;
    ta_chart_min = nullptr; ta_chart_max = nullptr; 
    lbl_c_w_val = nullptr; lbl_c_h_val = nullptr; lbl_c_x_val = nullptr; lbl_c_y_val = nullptr;
}

void HaDialogEdit::updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix) {
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if(hexColor.length() > 0 && hexColor.startsWith("#")) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(strtol(hexColor.substring(1).c_str(), NULL, 16)), 0);
        lv_label_set_text_fmt(label, "%s %s", prefix, hexColor.c_str());
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x555555), 0);
        lv_label_set_text_fmt(label, "%s Standard", prefix);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
    }
}

void HaDialogEdit::close_color_picker_cb(lv_event_t * e) {
    playToneI2S(600, 100, true);
    if(color_picker_overlay) { lv_obj_del_async(color_picker_overlay); color_picker_overlay = nullptr; }
}

void HaDialogEdit::color_tile_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    const char* hex = (const char*)lv_event_get_user_data(e);
    if(picking_color_on) {
        selected_color_on = String(hex); updateColorBtn(btn_color_on, selected_color_on, "Aktiv:");
    } else {
        selected_color_off = String(hex); updateColorBtn(btn_color_off, selected_color_off, "Inaktiv:");
    }
    close_color_picker_cb(NULL);
}

void HaDialogEdit::openColorPicker() {
    if(color_picker_overlay) return;
    color_picker_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(color_picker_overlay, 1280, 720);
    lv_obj_set_style_bg_color(color_picker_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(color_picker_overlay, 200, 0);
    lv_obj_add_flag(color_picker_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(color_picker_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(color_picker_overlay, close_color_picker_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* panel = lv_obj_create(color_picker_overlay);
    lv_obj_set_size(panel, 820, 520);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    
    static const char* palette[] = { "#F44336", "#E91E63", "#9C27B0", "#673AB7", "#3F51B5", "#2196F3", "#03A9F4", "#00BCD4", "#009688", "#4CAF50", "#8BC34A", "#CDDC39", "#FFEB3B", "#FFC107", "#FF9800", "#FF5722", "#795548", "#9E9E9E", "#607D8B", "#FFFFFF", "#AAAAAA", "#555555", "#222222", "" };

    for(int i=0; i<24; i++) {
        lv_obj_t* tile = lv_btn_create(panel);
        lv_obj_set_size(tile, 90, 90); lv_obj_set_style_radius(tile, 10, 0);
        if (String(palette[i]) == "") {
            lv_obj_set_style_bg_color(tile, lv_color_hex(0x111111), 0);
            lv_obj_t* l = lv_label_create(tile); lv_label_set_text(l, LV_SYMBOL_CLOSE "\nReset");
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(l);
        } else {
            lv_obj_set_style_bg_color(tile, lv_color_hex(strtol(palette[i]+1, NULL, 16)), 0);
        }
        lv_obj_add_event_cb(tile, color_tile_event_cb, LV_EVENT_CLICKED, (void*)palette[i]);
    }
}

void HaDialogEdit::showLightControlDialog(HAWidget* w) {
    if (light_control_overlay != nullptr || action_overlay != nullptr) return; 

    playToneI2S(800, 100, true);
    cur_light_entity = w->getEntityId();

    int init_bri = HaWebsocketLogic_GetBrightness(cur_light_entity);
    uint32_t init_rgb = HaWebsocketLogic_GetRGB(cur_light_entity);
    int init_w = HaWebsocketLogic_GetWhite(cur_light_entity);
    bool is_rgbw = HaWebsocketLogic_IsRGBW(cur_light_entity);

    if (init_bri < 0) init_bri = 128; 
    if (init_w < 0) init_w = 0;

    light_control_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(light_control_overlay, 1280, 720);
    lv_obj_set_style_bg_color(light_control_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(light_control_overlay, 180, 0); 
    lv_obj_add_flag(light_control_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(light_control_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(light_control_overlay);
    lv_obj_set_size(panel, 500, is_rgbw ? 580 : 500);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, w->getName().c_str());
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);

    cur_light_slider = lv_slider_create(panel);
    lv_slider_set_range(cur_light_slider, 1, 255);
    lv_obj_set_size(cur_light_slider, 350, 30);
    lv_obj_align(cur_light_slider, LV_ALIGN_TOP_MID, 0, 100);
    lv_slider_set_value(cur_light_slider, init_bri, LV_ANIM_OFF); 

    lv_obj_t* lbl_bri = lv_label_create(panel);
    lv_label_set_text(lbl_bri, "Helligkeit (0-100%)");
    lv_obj_align_to(lbl_bri, cur_light_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    if (is_rgbw) {
        cur_light_w_slider = lv_slider_create(panel);
        lv_slider_set_range(cur_light_w_slider, 0, 255);
        lv_obj_set_size(cur_light_w_slider, 350, 30);
        lv_obj_align(cur_light_w_slider, LV_ALIGN_TOP_MID, 0, 180);
        lv_slider_set_value(cur_light_w_slider, init_w, LV_ANIM_OFF); 

        lv_obj_t* lbl_w = lv_label_create(panel);
        lv_label_set_text(lbl_w, "Weiß-Kanal (0-100%)");
        lv_obj_align_to(lbl_w, cur_light_w_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);
    } else { cur_light_w_slider = nullptr; }

    cur_light_cw = lv_colorwheel_create(panel, true);
    lv_obj_set_size(cur_light_cw, 250, 250);
    lv_obj_align(cur_light_cw, LV_ALIGN_TOP_MID, 0, is_rgbw ? 260 : 180);

    if (init_rgb != 0xFFFFFFFF) {
        lv_color_t init_col;
        init_col.ch.red = (init_rgb >> 16) & 0xFF; init_col.ch.green = (init_rgb >> 8) & 0xFF; init_col.ch.blue = init_rgb & 0xFF;
        lv_colorwheel_set_rgb(cur_light_cw, init_col);
    }

    auto send_ha_state = [](lv_event_t* e) {
        int bri = lv_slider_get_value(cur_light_slider);
        int w_val = (cur_light_w_slider != nullptr) ? lv_slider_get_value(cur_light_w_slider) : -1;
        lv_color_t col = lv_colorwheel_get_rgb(cur_light_cw);
        lv_color32_t c32; c32.full = lv_color_to32(col); 
        HaWebsocketLogic_CallLightService(cur_light_entity, bri, c32.ch.red, c32.ch.green, c32.ch.blue, w_val);
    };

    lv_obj_add_event_cb(cur_light_slider, send_ha_state, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(cur_light_cw, send_ha_state, LV_EVENT_RELEASED, NULL);
    if (cur_light_w_slider) lv_obj_add_event_cb(cur_light_w_slider, send_ha_state, LV_EVENT_RELEASED, NULL);

    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e) { lv_obj_del_async(light_control_overlay); light_control_overlay = nullptr; }, LV_EVENT_CLICKED, NULL);
}

void HaDialogEdit::showWidgetEditDialog(HAWidget* w) {
    if (action_overlay != nullptr || light_control_overlay != nullptr) return; 
    current_widget = w;
    
    orig_w = w->getW(); 
    orig_h = w->getH(); 
    orig_i_align = w->getIconAlign(); 
    orig_t_align = w->getTextAlign();
    orig_s_align = w->getStateAlign();
    
    orig_i_margin = w->getIconMargin();
    orig_t_margin = w->getTextMargin();
    orig_s_margin = w->getStateMargin();
    
    playToneI2S(800, 100, true);
    action_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(action_overlay, 1280, 720);
    lv_obj_set_style_bg_opa(action_overlay, 80, 0); 
    lv_obj_add_flag(action_overlay, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(action_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    edit_panel = lv_obj_create(action_overlay);
    lv_obj_set_size(edit_panel, 680, 560); 
    lv_obj_set_style_bg_color(edit_panel, lv_color_hex(0x222222), 0);
    lv_obj_add_flag(edit_panel, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(edit_panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    if (w->getY() > 300) lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
    else lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* tv = lv_tabview_create(edit_panel, LV_DIR_TOP, 50);
    lv_obj_set_size(tv, 680, 480); 
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* tab_size = lv_tabview_add_tab(tv, "Groesse");
    lv_obj_t* tab_disp = lv_tabview_add_tab(tv, "Anzeige");
    lv_obj_t* tab_layout = lv_tabview_add_tab(tv, "Layout");
    
    lv_obj_t* tab_chart = nullptr;
    if (w->getType() == "sensor") {
        tab_chart = lv_tabview_add_tab(tv, "Diagramm");
    }
    lv_obj_set_style_text_font(lv_tabview_get_tab_btns(tv), &lv_font_montserrat_24, 0);

    // --- TAB GROESSE ---
    lv_obj_t* btn_w_minus = lv_btn_create(tab_size);
    lv_obj_set_size(btn_w_minus, 80, 80); lv_obj_align(btn_w_minus, LV_ALIGN_LEFT_MID, 20, -50);
    lv_obj_set_style_bg_color(btn_w_minus, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(lv_label_create(btn_w_minus), LV_SYMBOL_MINUS); lv_obj_center(lv_obj_get_child(btn_w_minus, 0));
    lv_obj_add_event_cb(btn_w_minus, [](lv_event_t* e){ HAWidget* wid = (HAWidget*)lv_event_get_user_data(e); int nw = wid->getW() - 40; if (nw < 100) nw = 100; wid->setSize(nw, wid->getH()); }, LV_EVENT_CLICKED, w);

    lv_obj_t* lbl_w = lv_label_create(tab_size); lv_label_set_text(lbl_w, "Breite");
    lv_obj_set_style_text_font(lbl_w, &lv_font_montserrat_24, 0); lv_obj_align(lbl_w, LV_ALIGN_CENTER, 0, -50);

    lv_obj_t* btn_w_plus = lv_btn_create(tab_size);
    lv_obj_set_size(btn_w_plus, 80, 80); lv_obj_align(btn_w_plus, LV_ALIGN_RIGHT_MID, -20, -50);
    lv_obj_set_style_bg_color(btn_w_plus, lv_color_hex(0x27AE60), 0);
    lv_label_set_text(lv_label_create(btn_w_plus), LV_SYMBOL_PLUS); lv_obj_center(lv_obj_get_child(btn_w_plus, 0));
    lv_obj_add_event_cb(btn_w_plus, [](lv_event_t* e){ HAWidget* wid = (HAWidget*)lv_event_get_user_data(e); wid->setSize(wid->getW() + 40, wid->getH()); }, LV_EVENT_CLICKED, w);

    lv_obj_t* btn_h_minus = lv_btn_create(tab_size);
    lv_obj_set_size(btn_h_minus, 80, 80); lv_obj_align(btn_h_minus, LV_ALIGN_LEFT_MID, 20, 50);
    lv_obj_set_style_bg_color(btn_h_minus, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(lv_label_create(btn_h_minus), LV_SYMBOL_MINUS); lv_obj_center(lv_obj_get_child(btn_h_minus, 0));
    lv_obj_add_event_cb(btn_h_minus, [](lv_event_t* e){ HAWidget* wid = (HAWidget*)lv_event_get_user_data(e); int nh = wid->getH() - 40; if (nh < 80) nh = 80; wid->setSize(wid->getW(), nh); }, LV_EVENT_CLICKED, w);

    lv_obj_t* lbl_h = lv_label_create(tab_size); lv_label_set_text(lbl_h, "Hoehe");
    lv_obj_set_style_text_font(lbl_h, &lv_font_montserrat_24, 0); lv_obj_align(lbl_h, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t* btn_h_plus = lv_btn_create(tab_size);
    lv_obj_set_size(btn_h_plus, 80, 80); lv_obj_align(btn_h_plus, LV_ALIGN_RIGHT_MID, -20, 50);
    lv_obj_set_style_bg_color(btn_h_plus, lv_color_hex(0x27AE60), 0);
    lv_label_set_text(lv_label_create(btn_h_plus), LV_SYMBOL_PLUS); lv_obj_center(lv_obj_get_child(btn_h_plus, 0));
    lv_obj_add_event_cb(btn_h_plus, [](lv_event_t* e){ HAWidget* wid = (HAWidget*)lv_event_get_user_data(e); wid->setSize(wid->getW(), wid->getH() + 40); }, LV_EVENT_CLICKED, w);

    // --- TAB ANZEIGE ---
    lv_obj_t* lbl_n = lv_label_create(tab_disp); lv_label_set_text(lbl_n, "Name:");
    lv_obj_set_style_text_font(lbl_n, &lv_font_montserrat_16, 0); lv_obj_set_pos(lbl_n, 10, 10);
    ta_name = lv_textarea_create(tab_disp); lv_obj_set_size(ta_name, 500, 40); lv_obj_set_pos(ta_name, 100, 0);
    lv_textarea_set_text(ta_name, w->getName().c_str());

    lv_obj_t* lbl_search = lv_label_create(tab_disp); lv_label_set_text(lbl_search, "Suche:");
    lv_obj_set_style_text_font(lbl_search, &lv_font_montserrat_16, 0); lv_obj_set_pos(lbl_search, 10, 60);
    ta_icon_search = lv_textarea_create(tab_disp); lv_obj_set_size(ta_icon_search, 500, 40); lv_obj_set_pos(ta_icon_search, 100, 50);
    lv_textarea_set_placeholder_text(ta_icon_search, "z.B. arrow");
    lv_textarea_set_text(ta_icon_search, last_search_term.c_str());

    lv_obj_t* lbl_cat = lv_label_create(tab_disp); lv_label_set_text(lbl_cat, "Bereich:");
    lv_obj_set_style_text_font(lbl_cat, &lv_font_montserrat_16, 0); lv_obj_set_pos(lbl_cat, 10, 110);
    dd_icon_cat = lv_dropdown_create(tab_disp); lv_obj_set_size(dd_icon_cat, 500, 40); lv_obj_set_pos(dd_icon_cat, 100, 100);
    
    String cat_options = "";
    for(int i = 0; i < num_icon_categories; i++) { cat_options += icon_categories[i].name; if(i < num_icon_categories - 1) cat_options += "\n"; }
    lv_dropdown_set_options(dd_icon_cat, cat_options.c_str());

    lv_obj_t* lbl_i = lv_label_create(tab_disp); lv_label_set_text(lbl_i, "Icon:");
    lv_obj_set_style_text_font(lbl_i, &lv_font_montserrat_16, 0); lv_obj_set_pos(lbl_i, 10, 160);
    dd_icon = lv_dropdown_create(tab_disp); lv_obj_set_size(dd_icon, 500, 40); lv_obj_set_pos(dd_icon, 100, 150);
    
    String c_icon = w->getMdiIcon(); int sel_cat = 0, sel_icon = 0;
    if (c_icon == "") { sel_cat = 0; sel_icon = 1; } 
    else {
        for (int c = 0; c < num_icon_categories; c++) {
            String opts = String(icon_categories[c].options);
            int pos = 0, idx = 0; bool found = false;
            while (pos != -1) {
                int next = opts.indexOf('\n', pos);
                String token = (next == -1) ? opts.substring(pos) : opts.substring(pos, next);
                if (token == c_icon) { sel_cat = c; sel_icon = idx; found = true; break; }
                idx++; pos = (next == -1) ? -1 : next + 1;
            }
            if (found) break;
        }
    }
    
    lv_dropdown_set_selected(dd_icon_cat, sel_cat);
    lv_dropdown_set_options(dd_icon, icon_categories[sel_cat].options);
    lv_dropdown_set_selected(dd_icon, sel_icon);
    
    lv_obj_add_event_cb(dd_icon_cat, dd_icon_cat_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ta_icon_search, icon_search_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (last_search_term.length() >= 2) icon_search_event_cb(NULL);

    lv_obj_t* lbl_c1 = lv_label_create(tab_disp); lv_label_set_text(lbl_c1, "Personalisierte Farben:");
    lv_obj_set_style_text_color(lbl_c1, lv_color_hex(0x00A0FF), 0); lv_obj_set_pos(lbl_c1, 10, 205);

    btn_color_on = lv_btn_create(tab_disp); lv_obj_set_size(btn_color_on, 280, 50); lv_obj_set_pos(btn_color_on, 10, 230);
    lv_label_set_text(lv_label_create(btn_color_on), "Laden..."); lv_obj_center(lv_obj_get_child(btn_color_on, 0));
    lv_obj_add_event_cb(btn_color_on, [](lv_event_t* e){ playToneI2S(800, 100, true); picking_color_on = true; openColorPicker(); }, LV_EVENT_CLICKED, NULL);

    btn_color_off = lv_btn_create(tab_disp); lv_obj_set_size(btn_color_off, 280, 50); lv_obj_set_pos(btn_color_off, 320, 230);
    lv_label_set_text(lv_label_create(btn_color_off), "Laden..."); lv_obj_center(lv_obj_get_child(btn_color_off, 0));
    lv_obj_add_event_cb(btn_color_off, [](lv_event_t* e){ playToneI2S(800, 100, true); picking_color_on = false; openColorPicker(); }, LV_EVENT_CLICKED, NULL);

    selected_color_on = w->getColorOn(); selected_color_off = w->getColorOff();
    updateColorBtn(btn_color_on, selected_color_on, "Aktiv:"); updateColorBtn(btn_color_off, selected_color_off, "Inaktiv:");

    // --- TAB LAYOUT ---
    int y_offs = 15;
    
    lv_obj_t* btn_fwd = lv_btn_create(tab_layout);
    lv_obj_set_size(btn_fwd, 150, 40); lv_obj_set_pos(btn_fwd, 10, y_offs);
    lv_obj_set_style_bg_color(btn_fwd, lv_color_hex(0x2980B9), 0);
    lv_label_set_text(lv_label_create(btn_fwd), "Nach vorne"); lv_obj_center(lv_obj_get_child(btn_fwd, 0));
    lv_obj_add_event_cb(btn_fwd, [](lv_event_t* e){ if(current_widget) lv_obj_move_foreground(current_widget->container); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_bwd = lv_btn_create(tab_layout);
    lv_obj_set_size(btn_bwd, 150, 40); lv_obj_set_pos(btn_bwd, 170, y_offs);
    lv_obj_set_style_bg_color(btn_bwd, lv_color_hex(0x8E44AD), 0);
    lv_label_set_text(lv_label_create(btn_bwd), "Nach hinten"); lv_obj_center(lv_obj_get_child(btn_bwd, 0));
    lv_obj_add_event_cb(btn_bwd, [](lv_event_t* e){ if(current_widget) lv_obj_move_background(current_widget->container); }, LV_EVENT_CLICKED, NULL);
    y_offs += 60;

    lv_obj_t* lbl_i_pos = lv_label_create(tab_layout); lv_label_set_text(lbl_i_pos, "Icon:");
    lv_obj_set_style_text_font(lbl_i_pos, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_i_pos, 10, y_offs + 10);
    
    dd_icon_pos = lv_dropdown_create(tab_layout); 
    lv_obj_set_size(dd_icon_pos, 140, 40); lv_obj_set_pos(dd_icon_pos, 80, y_offs);
    lv_dropdown_set_options(dd_icon_pos, "Oben\nMitte\nUnten\nLinks\nRechts");
    lv_dropdown_set_selected(dd_icon_pos, alignToIdx(orig_i_align));

    lv_obj_t* lbl_i_m = lv_label_create(tab_layout); lv_label_set_text(lbl_i_m, "Abstand:");
    lv_obj_set_style_text_font(lbl_i_m, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_i_m, 240, y_offs + 10);
    
    slider_icon_margin = lv_slider_create(tab_layout);
    lv_obj_set_size(slider_icon_margin, 200, 20); lv_obj_set_pos(slider_icon_margin, 350, y_offs + 10);
    lv_slider_set_range(slider_icon_margin, -50, 100);
    lv_slider_set_value(slider_icon_margin, orig_i_margin, LV_ANIM_OFF);

    lbl_i_m_val = lv_label_create(tab_layout);
    lv_label_set_text_fmt(lbl_i_m_val, "%d px", orig_i_margin);
    lv_obj_set_style_text_font(lbl_i_m_val, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_i_m_val, 570, y_offs + 10);

    y_offs += 60;

    lv_obj_t* lbl_t_pos = lv_label_create(tab_layout); lv_label_set_text(lbl_t_pos, "Text:");
    lv_obj_set_style_text_font(lbl_t_pos, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_t_pos, 10, y_offs + 10);
    
    dd_text_pos = lv_dropdown_create(tab_layout); 
    lv_obj_set_size(dd_text_pos, 140, 40); lv_obj_set_pos(dd_text_pos, 80, y_offs);
    lv_dropdown_set_options(dd_text_pos, "Oben\nMitte\nUnten\nLinks\nRechts");
    lv_dropdown_set_selected(dd_text_pos, alignToIdx(orig_t_align));

    lv_obj_t* lbl_t_m = lv_label_create(tab_layout); lv_label_set_text(lbl_t_m, "Abstand:");
    lv_obj_set_style_text_font(lbl_t_m, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_t_m, 240, y_offs + 10);
    
    slider_text_margin = lv_slider_create(tab_layout);
    lv_obj_set_size(slider_text_margin, 200, 20); lv_obj_set_pos(slider_text_margin, 350, y_offs + 10);
    lv_slider_set_range(slider_text_margin, -50, 100);
    lv_slider_set_value(slider_text_margin, orig_t_margin, LV_ANIM_OFF);

    lbl_t_m_val = lv_label_create(tab_layout);
    lv_label_set_text_fmt(lbl_t_m_val, "%d px", orig_t_margin);
    lv_obj_set_style_text_font(lbl_t_m_val, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_t_m_val, 570, y_offs + 10);

    y_offs += 60;
    
    if (w->getType() == "sensor") {
        lv_obj_t* lbl_s_pos = lv_label_create(tab_layout); lv_label_set_text(lbl_s_pos, "Wert:");
        lv_obj_set_style_text_font(lbl_s_pos, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_s_pos, 10, y_offs + 10);
        
        dd_state_pos = lv_dropdown_create(tab_layout); 
        lv_obj_set_size(dd_state_pos, 140, 40); lv_obj_set_pos(dd_state_pos, 80, y_offs);
        lv_dropdown_set_options(dd_state_pos, "Oben\nMitte\nUnten\nLinks\nRechts");
        lv_dropdown_set_selected(dd_state_pos, alignToIdx(orig_s_align));

        lv_obj_t* lbl_s_m = lv_label_create(tab_layout); lv_label_set_text(lbl_s_m, "Abstand:");
        lv_obj_set_style_text_font(lbl_s_m, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_s_m, 240, y_offs + 10);
        
        slider_state_margin = lv_slider_create(tab_layout);
        lv_obj_set_size(slider_state_margin, 200, 20); lv_obj_set_pos(slider_state_margin, 350, y_offs + 10);
        lv_slider_set_range(slider_state_margin, -50, 100);
        lv_slider_set_value(slider_state_margin, orig_s_margin, LV_ANIM_OFF);

        lbl_s_m_val = lv_label_create(tab_layout);
        lv_label_set_text_fmt(lbl_s_m_val, "%d px", orig_s_margin);
        lv_obj_set_style_text_font(lbl_s_m_val, &lv_font_montserrat_20, 0);
        lv_obj_set_pos(lbl_s_m_val, 570, y_offs + 10);

        y_offs += 60;
    }

    auto layout_change_cb = [](lv_event_t* e) {
        if (!current_widget) return;
        lv_label_set_text_fmt(lbl_i_m_val, "%d px", (int)lv_slider_get_value(slider_icon_margin));
        lv_label_set_text_fmt(lbl_t_m_val, "%d px", (int)lv_slider_get_value(slider_text_margin));
        
        if (lbl_s_m_val && slider_state_margin) {
            lv_label_set_text_fmt(lbl_s_m_val, "%d px", (int)lv_slider_get_value(slider_state_margin));
        }

        current_widget->setAlignments(
            idxToAlign(lv_dropdown_get_selected(dd_icon_pos)), 
            idxToAlign(lv_dropdown_get_selected(dd_text_pos)),
            dd_state_pos ? idxToAlign(lv_dropdown_get_selected(dd_state_pos)) : LV_ALIGN_CENTER,
            lv_slider_get_value(slider_icon_margin),
            lv_slider_get_value(slider_text_margin),
            slider_state_margin ? lv_slider_get_value(slider_state_margin) : 0
        );
    };
    
    lv_obj_add_event_cb(dd_icon_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(dd_text_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_icon_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_text_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    if (w->getType() == "sensor") {
        lv_obj_add_event_cb(dd_state_pos, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(slider_state_margin, layout_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    cb_snap = lv_checkbox_create(tab_layout);
    lv_checkbox_set_text(cb_snap, "Am Raster einrasten (Snap 10px)");
    lv_obj_set_style_text_font(cb_snap, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(cb_snap, 10, y_offs);
    if (w->getSnapToGrid()) lv_obj_add_state(cb_snap, LV_STATE_CHECKED);
    else lv_obj_clear_state(cb_snap, LV_STATE_CHECKED);

    // --- TAB DIAGRAMM (NEU) ---
    if (tab_chart) {
        int cy = 10;
        
        cb_chart = lv_checkbox_create(tab_chart);
        lv_checkbox_set_text(cb_chart, "Als Diagramm (Verlauf) anzeigen");
        lv_obj_set_style_text_font(cb_chart, &lv_font_montserrat_20, 0);
        lv_obj_set_pos(cb_chart, 10, cy);
        if (w->getShowChart()) lv_obj_add_state(cb_chart, LV_STATE_CHECKED);
        else lv_obj_clear_state(cb_chart, LV_STATE_CHECKED);
        cy += 40;

        auto chart_slider_cb = [](lv_event_t* e) {
            lv_label_set_text_fmt(lbl_c_w_val, "%d %%", (int)lv_slider_get_value(slider_chart_w));
            lv_label_set_text_fmt(lbl_c_h_val, "%d %%", (int)lv_slider_get_value(slider_chart_h));
            lv_label_set_text_fmt(lbl_c_x_val, "%d px", (int)lv_slider_get_value(slider_chart_x));
            lv_label_set_text_fmt(lbl_c_y_val, "%d px", (int)lv_slider_get_value(slider_chart_y));
        };

        // Breite
        lv_obj_t* lbl_cw = lv_label_create(tab_chart); lv_label_set_text(lbl_cw, "Breite:");
        lv_obj_set_style_text_font(lbl_cw, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_cw, 10, cy);
        
        slider_chart_w = lv_slider_create(tab_chart);
        lv_obj_set_size(slider_chart_w, 250, 20); lv_obj_set_pos(slider_chart_w, 120, cy+2);
        lv_slider_set_range(slider_chart_w, 50, 100);
        lv_slider_set_value(slider_chart_w, w->getChartWPct(), LV_ANIM_OFF);
        
        lbl_c_w_val = lv_label_create(tab_chart); lv_label_set_text_fmt(lbl_c_w_val, "%d %%", w->getChartWPct());
        lv_obj_set_style_text_font(lbl_c_w_val, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_c_w_val, 400, cy);
        lv_obj_add_event_cb(slider_chart_w, chart_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        cy += 40;

        // Höhe
        lv_obj_t* lbl_ch = lv_label_create(tab_chart); lv_label_set_text(lbl_ch, "Hoehe:");
        lv_obj_set_style_text_font(lbl_ch, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_ch, 10, cy);
        
        slider_chart_h = lv_slider_create(tab_chart);
        lv_obj_set_size(slider_chart_h, 250, 20); lv_obj_set_pos(slider_chart_h, 120, cy+2);
        lv_slider_set_range(slider_chart_h, 20, 100);
        lv_slider_set_value(slider_chart_h, w->getChartHPct(), LV_ANIM_OFF);
        
        lbl_c_h_val = lv_label_create(tab_chart); lv_label_set_text_fmt(lbl_c_h_val, "%d %%", w->getChartHPct());
        lv_obj_set_style_text_font(lbl_c_h_val, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_c_h_val, 400, cy);
        lv_obj_add_event_cb(slider_chart_h, chart_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        cy += 40;
        
        // Offset X
        lv_obj_t* lbl_cx = lv_label_create(tab_chart); lv_label_set_text(lbl_cx, "X-Pos:");
        lv_obj_set_style_text_font(lbl_cx, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_cx, 10, cy);
        
        slider_chart_x = lv_slider_create(tab_chart);
        lv_obj_set_size(slider_chart_x, 250, 20); lv_obj_set_pos(slider_chart_x, 120, cy+2);
        lv_slider_set_range(slider_chart_x, -100, 100);
        lv_slider_set_value(slider_chart_x, w->getChartXOfs(), LV_ANIM_OFF);
        
        lbl_c_x_val = lv_label_create(tab_chart); lv_label_set_text_fmt(lbl_c_x_val, "%d px", w->getChartXOfs());
        lv_obj_set_style_text_font(lbl_c_x_val, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_c_x_val, 400, cy);
        lv_obj_add_event_cb(slider_chart_x, chart_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        cy += 40;
        
        // Offset Y
        lv_obj_t* lbl_cy = lv_label_create(tab_chart); lv_label_set_text(lbl_cy, "Y-Pos:");
        lv_obj_set_style_text_font(lbl_cy, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_cy, 10, cy);
        
        // ==========================================
        // FIX: Y-Offset Bereich auf -200 erweitert!
        // ==========================================
        slider_chart_y = lv_slider_create(tab_chart);
        lv_obj_set_size(slider_chart_y, 250, 20); lv_obj_set_pos(slider_chart_y, 120, cy+2);
        lv_slider_set_range(slider_chart_y, -200, 100);
        lv_slider_set_value(slider_chart_y, w->getChartYOfs(), LV_ANIM_OFF);
        
        lbl_c_y_val = lv_label_create(tab_chart); lv_label_set_text_fmt(lbl_c_y_val, "%d px", w->getChartYOfs());
        lv_obj_set_style_text_font(lbl_c_y_val, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_c_y_val, 400, cy);
        lv_obj_add_event_cb(slider_chart_y, chart_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        cy += 50;

        // Min/Max Werte
        lv_obj_t* lbl_cmin = lv_label_create(tab_chart); lv_label_set_text(lbl_cmin, "Min. Wert:");
        lv_obj_set_style_text_font(lbl_cmin, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_cmin, 10, cy+10);
        ta_chart_min = lv_textarea_create(tab_chart); 
        lv_obj_set_size(ta_chart_min, 150, 40); lv_obj_set_pos(ta_chart_min, 120, cy);
        lv_textarea_set_placeholder_text(ta_chart_min, "Auto");
        lv_textarea_set_text(ta_chart_min, w->getChartMin().c_str());
        lv_textarea_set_one_line(ta_chart_min, true);

        lv_obj_t* lbl_cmax = lv_label_create(tab_chart); lv_label_set_text(lbl_cmax, "Max. Wert:");
        lv_obj_set_style_text_font(lbl_cmax, &lv_font_montserrat_20, 0); lv_obj_set_pos(lbl_cmax, 300, cy+10);
        ta_chart_max = lv_textarea_create(tab_chart); 
        lv_obj_set_size(ta_chart_max, 150, 40); lv_obj_set_pos(ta_chart_max, 420, cy);
        lv_textarea_set_placeholder_text(ta_chart_max, "Auto");
        lv_textarea_set_text(ta_chart_max, w->getChartMax().c_str());
        lv_textarea_set_one_line(ta_chart_max, true);
    }

    // --- KEYBOARD ---
    kb = lv_keyboard_create(action_overlay);
    lv_obj_set_size(kb, 1280, 350); lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); 

    auto ta_focus_cb = [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* ta = lv_event_get_target(e);
        if(code == LV_EVENT_FOCUSED) {
            lv_keyboard_set_textarea(kb, ta);
            if (ta == ta_chart_min || ta == ta_chart_max) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
            else lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
            
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
        }
        if(code == LV_EVENT_DEFOCUSED) {
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            if (current_widget && current_widget->getY() > 300) lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
            else lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
            if (ta == ta_icon_search) lv_event_send(ta_icon_search, LV_EVENT_VALUE_CHANGED, NULL);
        }
    };
    
    lv_obj_add_event_cb(ta_name, ta_focus_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_icon_search, ta_focus_cb, LV_EVENT_ALL, NULL);
    if (ta_chart_min) lv_obj_add_event_cb(ta_chart_min, ta_focus_cb, LV_EVENT_ALL, NULL);
    if (ta_chart_max) lv_obj_add_event_cb(ta_chart_max, ta_focus_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = lv_event_get_target(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            lv_obj_t* ta = lv_keyboard_get_textarea(keyboard);
            if (ta) { lv_obj_clear_state(ta, LV_STATE_FOCUSED); lv_event_send(ta, LV_EVENT_DEFOCUSED, NULL); }
        }
    }, LV_EVENT_ALL, NULL);

    // --- GLOBALE BUTTONS ---
    lv_obj_t* btn_ok = lv_btn_create(edit_panel);
    lv_obj_set_size(btn_ok, 180, 50); lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x27AE60), 0);
    lv_label_set_text(lv_label_create(btn_ok), "Uebernehmen"); lv_obj_center(lv_obj_get_child(btn_ok, 0));

    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
        if (!current_widget) return;
        current_widget->setName(String(lv_textarea_get_text(ta_name)));

        char buf[64]; lv_dropdown_get_selected_str(dd_icon, buf, sizeof(buf));
        String sel_icon = String(buf);
        if (sel_icon != "--- Unveraendert ---") {
            if (sel_icon.startsWith("Standard") || sel_icon.startsWith("---") || sel_icon.startsWith("Keine Treffer")) current_widget->setMdiIcon("");
            else current_widget->setMdiIcon(sel_icon);
        }

        current_widget->setColors(selected_color_on, selected_color_off);
        current_widget->setSnapToGrid(lv_obj_has_state(cb_snap, LV_STATE_CHECKED));
        
        if (current_widget->getType() == "sensor" && cb_chart) {
            current_widget->setChartConfig(
                lv_obj_has_state(cb_chart, LV_STATE_CHECKED),
                lv_slider_get_value(slider_chart_w),
                lv_slider_get_value(slider_chart_h),
                lv_slider_get_value(slider_chart_x),
                lv_slider_get_value(slider_chart_y),
                String(lv_textarea_get_text(ta_chart_min)),
                String(lv_textarea_get_text(ta_chart_max))
            );
        }

        lv_obj_del_async(action_overlay); action_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* btn_cancel = lv_btn_create(edit_panel);
    lv_obj_set_size(btn_cancel, 180, 50); lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -220, -15);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x555555), 0);
    lv_label_set_text(lv_label_create(btn_cancel), "Abbrechen"); lv_obj_center(lv_obj_get_child(btn_cancel, 0));
    
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) {
        if (current_widget) { 
            current_widget->setSize(orig_w, orig_h); 
            current_widget->setAlignments(orig_i_align, orig_t_align, orig_s_align, orig_i_margin, orig_t_margin, orig_s_margin); 
        }
        lv_obj_del_async(action_overlay); action_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
}

void HaDialogEdit::handleWidgetDeleteDrop(HAWidget* w) {
    if (!ViewHomeAssistant::trash_btn || action_overlay != nullptr || light_control_overlay != nullptr) return; 
    
    lv_area_t trash_area, w_area, res;
    lv_obj_get_coords(ViewHomeAssistant::trash_btn, &trash_area);
    lv_obj_get_coords(w->container, &w_area);
    
    if (_lv_area_intersect(&res, &trash_area, &w_area)) {
        playToneI2S(600, 100, true);
        action_overlay = lv_obj_create(ViewHomeAssistant::screen);
        lv_obj_set_size(action_overlay, 1280, 720);
        lv_obj_set_style_bg_color(action_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(action_overlay, 200, 0);
        lv_obj_clear_flag(action_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

        lv_obj_t* panel = lv_obj_create(action_overlay);
        lv_obj_set_size(panel, 500, 250); lv_obj_center(panel);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);

        lv_obj_t* lbl = lv_label_create(panel); lv_label_set_text(lbl, "Widget loeschen?");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0); lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 30);

        lv_obj_t* btn_del = lv_btn_create(panel);
        lv_obj_set_size(btn_del, 200, 60); lv_obj_align(btn_del, LV_ALIGN_BOTTOM_RIGHT, -20, -30);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xAA0000), 0);
        lv_label_set_text(lv_label_create(btn_del), "Ja, loeschen"); lv_obj_center(lv_obj_get_child(btn_del, 0));
        lv_obj_add_event_cb(btn_del, [](lv_event_t* e) {
            HAWidget* wid = (HAWidget*)lv_event_get_user_data(e);
            lv_obj_add_flag(wid->container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_del_async(action_overlay); action_overlay = nullptr;
        }, LV_EVENT_CLICKED, w);

        lv_obj_t* btn_cancel = lv_btn_create(panel);
        lv_obj_set_size(btn_cancel, 200, 60); lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -30);
        lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x333333), 0);
        lv_label_set_text(lv_label_create(btn_cancel), "Abbrechen"); lv_obj_center(lv_obj_get_child(btn_cancel, 0));
        lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) { lv_obj_del_async(action_overlay); action_overlay = nullptr; }, LV_EVENT_CLICKED, NULL);
    }
}