#include "HaDialogEdit.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "HaConfigLogic.h"
#include "MdiMapper.h"
#include "HaColorPicker.h"
#include "UIHelper.h"

lv_obj_t* HaDialogEdit::overlay = nullptr; 
lv_obj_t* HaDialogEdit::edit_panel = nullptr;          
HAWidget* HaDialogEdit::current_widget = nullptr;
lv_obj_t* HaDialogEdit::kb = nullptr;

lv_obj_t* HaDialogEdit::ta_name = nullptr;
lv_obj_t* HaDialogEdit::ta_icon_search = nullptr; 
lv_obj_t* HaDialogEdit::dd_icon_cat = nullptr;
lv_obj_t* HaDialogEdit::dd_icon = nullptr;
lv_obj_t* HaDialogEdit::cb_snap = nullptr;

lv_obj_t* HaDialogEdit::cb_chart = nullptr; 
lv_obj_t* HaDialogEdit::slider_chart_w = nullptr;
lv_obj_t* HaDialogEdit::slider_chart_h = nullptr;
lv_obj_t* HaDialogEdit::slider_chart_x = nullptr; 
lv_obj_t* HaDialogEdit::slider_chart_y = nullptr; 
lv_obj_t* HaDialogEdit::ta_chart_min = nullptr;
lv_obj_t* HaDialogEdit::ta_chart_max = nullptr;

lv_obj_t* HaDialogEdit::slider_vac_w = nullptr;
lv_obj_t* HaDialogEdit::slider_vac_h = nullptr;
lv_obj_t* HaDialogEdit::slider_vac_gap = nullptr;
lv_obj_t* HaDialogEdit::slider_vac_y = nullptr;

lv_obj_t* HaDialogEdit::dd_icon_pos = nullptr;
lv_obj_t* HaDialogEdit::dd_text_pos = nullptr;
lv_obj_t* HaDialogEdit::dd_state_pos = nullptr;
lv_obj_t* HaDialogEdit::slider_icon_margin = nullptr;
lv_obj_t* HaDialogEdit::slider_text_margin = nullptr;
lv_obj_t* HaDialogEdit::slider_state_margin = nullptr;

lv_obj_t* HaDialogEdit::lbl_i_m_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_t_m_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_s_m_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_c_w_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_c_h_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_c_x_val = nullptr;    
lv_obj_t* HaDialogEdit::lbl_c_y_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_v_w_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_v_h_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_v_gap_val = nullptr;
lv_obj_t* HaDialogEdit::lbl_v_y_val = nullptr;

lv_obj_t* HaDialogEdit::btn_color_on = nullptr;
lv_obj_t* HaDialogEdit::btn_color_off = nullptr;

String HaDialogEdit::selected_color_on = "";
String HaDialogEdit::selected_color_off = "";
String HaDialogEdit::last_search_term = "";

int HaDialogEdit::orig_w = 0; int HaDialogEdit::orig_h = 0;
int HaDialogEdit::orig_i_align = 0; int HaDialogEdit::orig_t_align = 0; int HaDialogEdit::orig_s_align = 0;
int HaDialogEdit::orig_i_margin = 0; int HaDialogEdit::orig_t_margin = 0; int HaDialogEdit::orig_s_margin = 0;
int HaDialogEdit::orig_c_w = 0; int HaDialogEdit::orig_c_h = 0; int HaDialogEdit::orig_c_x = 0; int HaDialogEdit::orig_c_y = 0;

static int idxToAlign(int idx) {
    switch(idx) {
        case 0: return LV_ALIGN_TOP_MID; case 1: return LV_ALIGN_CENTER; case 2: return LV_ALIGN_BOTTOM_MID; 
        case 3: return LV_ALIGN_LEFT_MID; case 4: return LV_ALIGN_RIGHT_MID; default: return LV_ALIGN_CENTER;
    }
}

static int alignToIdx(int align) {
    switch(align) {
        case LV_ALIGN_TOP_MID: return 0; case LV_ALIGN_CENTER: return 1; case LV_ALIGN_BOTTOM_MID: return 2; 
        case LV_ALIGN_LEFT_MID: return 3; case LV_ALIGN_RIGHT_MID: return 4;
    }
    return 1; 
}

void HaDialogEdit::resetState() {
    overlay = nullptr; edit_panel = nullptr; current_widget = nullptr; kb = nullptr;
    ta_name = nullptr; ta_icon_search = nullptr; dd_icon_cat = nullptr; dd_icon = nullptr; cb_snap = nullptr;
    cb_chart = nullptr; slider_chart_w = nullptr; slider_chart_h = nullptr; slider_chart_x = nullptr; slider_chart_y = nullptr;
    ta_chart_min = nullptr; ta_chart_max = nullptr;
    slider_vac_w = nullptr; slider_vac_h = nullptr; slider_vac_gap = nullptr; slider_vac_y = nullptr;
    dd_icon_pos = nullptr; dd_text_pos = nullptr; dd_state_pos = nullptr;
    slider_icon_margin = nullptr; slider_text_margin = nullptr; slider_state_margin = nullptr;
    lbl_i_m_val = nullptr; lbl_t_m_val = nullptr; lbl_s_m_val = nullptr;
    lbl_c_w_val = nullptr; lbl_c_h_val = nullptr; lbl_c_x_val = nullptr; lbl_c_y_val = nullptr;
    lbl_v_w_val = nullptr; lbl_v_h_val = nullptr; lbl_v_gap_val = nullptr; lbl_v_y_val = nullptr;
    btn_color_on = nullptr; btn_color_off = nullptr;
}

void HaDialogEdit::updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix) {
    if (!btn || !lv_obj_is_valid(btn)) return;
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (!label || !lv_obj_is_valid(label)) return;
    
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

void HaDialogEdit::icon_search_event_cb(lv_event_t * e) {
    if (!ta_icon_search || !lv_obj_is_valid(ta_icon_search)) return;
    last_search_term = String(lv_textarea_get_text(ta_icon_search)); 
    last_search_term.toLowerCase();
    
    if (last_search_term.length() < 2) {
        if (dd_icon_cat && lv_obj_is_valid(dd_icon_cat) && dd_icon && lv_obj_is_valid(dd_icon)) {
            lv_dropdown_set_options(dd_icon, icon_categories[lv_dropdown_get_selected(dd_icon_cat)].options);
        }
        return;
    }
    
    String results = "--- Suchergebnisse ---"; 
    int count = 0;
    
    for (int c = 1; c < num_icon_categories; c++) {
        String opts = String(icon_categories[c].options); 
        int pos = 0;
        while ((pos = opts.indexOf("mdi:", pos)) != -1) {
            int end = opts.indexOf('\n', pos);
            String icon_name = (end == -1) ? opts.substring(pos) : opts.substring(pos, end);
            String search_target = icon_name; search_target.toLowerCase();
            
            if (search_target.indexOf(last_search_term) != -1) { 
                results += "\n" + icon_name; count++; 
            }
            pos = (end == -1) ? opts.length() : end + 1;
            if (count >= 50) break;
        }
        if (count >= 50) break;
    }
    
    if (count == 0) results += "\nKeine Treffer";
    if (dd_icon && lv_obj_is_valid(dd_icon)) {
        lv_dropdown_set_options(dd_icon, results.c_str()); 
        lv_dropdown_set_selected(dd_icon, count > 0 ? 1 : 0);
    }
}

void HaDialogEdit::dd_icon_cat_event_cb(lv_event_t * e) {
    if (dd_icon && lv_obj_is_valid(dd_icon) && dd_icon_cat && lv_obj_is_valid(dd_icon_cat)) {
        lv_dropdown_set_options(dd_icon, icon_categories[lv_dropdown_get_selected(dd_icon_cat)].options); 
        lv_dropdown_set_selected(dd_icon, 0); 
    }
    if (last_search_term.length() > 0 && ta_icon_search && lv_obj_is_valid(ta_icon_search)) { 
        last_search_term = ""; 
        lv_textarea_set_text(ta_icon_search, ""); 
    }
}

void HaDialogEdit::layout_change_cb(lv_event_t* e) {
    if (!current_widget) return;
    
    if (lbl_i_m_val && lv_obj_is_valid(lbl_i_m_val) && slider_icon_margin && lv_obj_is_valid(slider_icon_margin)) 
        lv_label_set_text_fmt(lbl_i_m_val, "%d px", (int)lv_slider_get_value(slider_icon_margin));
        
    if (lbl_t_m_val && lv_obj_is_valid(lbl_t_m_val) && slider_text_margin && lv_obj_is_valid(slider_text_margin)) 
        lv_label_set_text_fmt(lbl_t_m_val, "%d px", (int)lv_slider_get_value(slider_text_margin));
        
    if (lbl_s_m_val && lv_obj_is_valid(lbl_s_m_val) && slider_state_margin && lv_obj_is_valid(slider_state_margin)) 
        lv_label_set_text_fmt(lbl_s_m_val, "%d px", (int)lv_slider_get_value(slider_state_margin));

    if (dd_icon_pos && lv_obj_is_valid(dd_icon_pos) && dd_text_pos && lv_obj_is_valid(dd_text_pos) && 
        slider_icon_margin && lv_obj_is_valid(slider_icon_margin) && slider_text_margin && lv_obj_is_valid(slider_text_margin)) {
        
        current_widget->setAlignments(
            idxToAlign(lv_dropdown_get_selected(dd_icon_pos)), 
            idxToAlign(lv_dropdown_get_selected(dd_text_pos)),
            (dd_state_pos && lv_obj_is_valid(dd_state_pos)) ? idxToAlign(lv_dropdown_get_selected(dd_state_pos)) : LV_ALIGN_CENTER,
            lv_slider_get_value(slider_icon_margin), 
            lv_slider_get_value(slider_text_margin),
            (slider_state_margin && lv_obj_is_valid(slider_state_margin)) ? lv_slider_get_value(slider_state_margin) : 0
        );
    }
}

void HaDialogEdit::btn_save_event_cb(lv_event_t * e) {
    if (!current_widget) return;
    
    if (ta_name && lv_obj_is_valid(ta_name)) {
        current_widget->setName(String(lv_textarea_get_text(ta_name)));
    }

    if (dd_icon && lv_obj_is_valid(dd_icon)) {
        char buf[64]; 
        lv_dropdown_get_selected_str(dd_icon, buf, sizeof(buf));
        String sel_icon = String(buf);
        if (sel_icon != "--- Unveraendert ---") {
            if (sel_icon.startsWith("Standard") || sel_icon.startsWith("---") || sel_icon.startsWith("Keine Treffer")) {
                current_widget->setMdiIcon("");
            } else {
                current_widget->setMdiIcon(sel_icon);
            }
        }
    }

    current_widget->setColors(selected_color_on, selected_color_off);
    if (cb_snap && lv_obj_is_valid(cb_snap)) {
        current_widget->setSnapToGrid(lv_obj_has_state(cb_snap, LV_STATE_CHECKED));
    }
    
    if (current_widget->getType() == "sensor" && cb_chart && lv_obj_is_valid(cb_chart) && 
        slider_chart_w && lv_obj_is_valid(slider_chart_w) && 
        slider_chart_h && lv_obj_is_valid(slider_chart_h) && 
        slider_chart_x && lv_obj_is_valid(slider_chart_x) && 
        slider_chart_y && lv_obj_is_valid(slider_chart_y) && 
        ta_chart_min && lv_obj_is_valid(ta_chart_min) && 
        ta_chart_max && lv_obj_is_valid(ta_chart_max)) {
        
        current_widget->setChartConfig(
            lv_obj_has_state(cb_chart, LV_STATE_CHECKED),
            lv_slider_get_value(slider_chart_w),
            lv_slider_get_value(slider_chart_h),
            lv_slider_get_value(slider_chart_x),
            lv_slider_get_value(slider_chart_y),
            String(lv_textarea_get_text(ta_chart_min)),
            String(lv_textarea_get_text(ta_chart_max))
        );
    } else if (current_widget->getType() == "vacuum" && slider_vac_w && lv_obj_is_valid(slider_vac_w) && 
               slider_vac_h && lv_obj_is_valid(slider_vac_h) && 
               slider_vac_gap && lv_obj_is_valid(slider_vac_gap) && 
               slider_vac_y && lv_obj_is_valid(slider_vac_y)) {
               
        current_widget->setChartConfig(false,
            lv_slider_get_value(slider_vac_w),
            lv_slider_get_value(slider_vac_h),
            lv_slider_get_value(slider_vac_gap),
            lv_slider_get_value(slider_vac_y),
            "", ""
        );
    }

    if (overlay && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay); 
    }
    resetState();
}

void HaDialogEdit::btn_cancel_event_cb(lv_event_t * e) {
    if (current_widget) { 
        current_widget->setSize(orig_w, orig_h); 
        current_widget->setAlignments(orig_i_align, orig_t_align, orig_s_align, orig_i_margin, orig_t_margin, orig_s_margin); 
        if (current_widget->getType() == "vacuum") {
            current_widget->setChartConfig(false, orig_c_w, orig_c_h, orig_c_x, orig_c_y, "", "");
        }
    }
    
    if (overlay && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay); 
    }
    resetState();
}

void HaDialogEdit::bindKeyboardToTextarea(lv_obj_t* ta) {
    if (!ta) return;
    
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* target = lv_event_get_target(e);
        if (!target || !lv_obj_is_valid(target)) return;
        
        if(code == LV_EVENT_FOCUSED) {
            if (kb && lv_obj_is_valid(kb)) {
                lv_keyboard_set_textarea(kb, target);
                if (target == ta_chart_min || target == ta_chart_max) {
                    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
                } else {
                    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
                }
                lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            }
            if (edit_panel && lv_obj_is_valid(edit_panel)) {
                lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
            }
        }
        
        if(code == LV_EVENT_DEFOCUSED) {
            if (kb && lv_obj_is_valid(kb)) {
                lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            }
            if (edit_panel && lv_obj_is_valid(edit_panel) && current_widget && current_widget->container && lv_obj_is_valid(current_widget->container)) {
                if (current_widget->getY() > 300) {
                    lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
                } else {
                    lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
                }
            }
            if (target == ta_icon_search && ta_icon_search && lv_obj_is_valid(ta_icon_search)) {
                lv_event_send(ta_icon_search, LV_EVENT_VALUE_CHANGED, NULL);
            }
        }
    }, LV_EVENT_ALL, NULL);
}

void HaDialogEdit::buildSizeTab(lv_obj_t* parent, HAWidget* w) {
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_LEFT_MID, 20, -50, 0xAA0000, LV_SYMBOL_MINUS, [](lv_event_t* e){ 
        if(current_widget) { int nw = current_widget->getW() - 40; if (nw < 100) nw = 100; current_widget->setSize(nw, current_widget->getH()); }
    });
    UIHelper::createLabel(parent, "Breite", &lv_font_montserrat_24, LV_ALIGN_CENTER, 0, -50);
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_RIGHT_MID, -20, -50, 0x27AE60, LV_SYMBOL_PLUS, [](lv_event_t* e){ 
        if(current_widget) current_widget->setSize(current_widget->getW() + 40, current_widget->getH()); 
    });

    UIHelper::createButton(parent, 80, 80, LV_ALIGN_LEFT_MID, 20, 50, 0xAA0000, LV_SYMBOL_MINUS, [](lv_event_t* e){ 
        if(current_widget) { int nh = current_widget->getH() - 40; if (nh < 80) nh = 80; current_widget->setSize(current_widget->getW(), nh); }
    });
    UIHelper::createLabel(parent, "Hoehe", &lv_font_montserrat_24, LV_ALIGN_CENTER, 0, 50);
    UIHelper::createButton(parent, 80, 80, LV_ALIGN_RIGHT_MID, -20, 50, 0x27AE60, LV_SYMBOL_PLUS, [](lv_event_t* e){ 
        if(current_widget) current_widget->setSize(current_widget->getW(), current_widget->getH() + 40); 
    });
}

void HaDialogEdit::buildDisplayTab(lv_obj_t* parent, HAWidget* w) {
    UIHelper::createLabel(parent, "Name:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 10);
    ta_name = UIHelper::createTextarea(parent, 500, 40, LV_ALIGN_TOP_LEFT, 100, 0, w->getName().c_str());
    bindKeyboardToTextarea(ta_name);

    UIHelper::createLabel(parent, "Suche:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 60);
    ta_icon_search = UIHelper::createTextarea(parent, 500, 40, LV_ALIGN_TOP_LEFT, 100, 50, last_search_term.c_str(), "z.B. arrow");
    bindKeyboardToTextarea(ta_icon_search);
    lv_obj_add_event_cb(ta_icon_search, icon_search_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    UIHelper::createLabel(parent, "Bereich:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 110);
    String cat_opts = "";
    for(int i = 0; i < num_icon_categories; i++) { 
        cat_opts += icon_categories[i].name; 
        if(i < num_icon_categories - 1) cat_opts += "\n"; 
    }
    dd_icon_cat = UIHelper::createDropdown(parent, 500, 40, LV_ALIGN_TOP_LEFT, 100, 100, cat_opts.c_str(), 0, dd_icon_cat_event_cb);

    UIHelper::createLabel(parent, "Icon:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 160);
    dd_icon = UIHelper::createDropdown(parent, 500, 40, LV_ALIGN_TOP_LEFT, 100, 150, "", 0);
    
    int sel_cat = 0, sel_icon = 1;
    if (w->getMdiIcon() != "") {
        for (int c = 0; c < num_icon_categories; c++) {
            String opts = String(icon_categories[c].options);
            int pos = 0, idx = 0; bool found = false;
            while (pos != -1) {
                int next = opts.indexOf('\n', pos);
                if (((next == -1) ? opts.substring(pos) : opts.substring(pos, next)) == w->getMdiIcon()) { 
                    sel_cat = c; sel_icon = idx; found = true; break; 
                }
                idx++; pos = (next == -1) ? -1 : next + 1;
            }
            if (found) break;
        }
    }
    lv_dropdown_set_selected(dd_icon_cat, sel_cat);
    lv_dropdown_set_options(dd_icon, icon_categories[sel_cat].options);
    lv_dropdown_set_selected(dd_icon, sel_icon);
    
    if (last_search_term.length() >= 2) {
        icon_search_event_cb(NULL);
    }

    UIHelper::createLabel(parent, "Personalisierte Farben:", nullptr, LV_ALIGN_TOP_LEFT, 10, 205, 0x00A0FF);
    btn_color_on = UIHelper::createButton(parent, 280, 50, LV_ALIGN_TOP_LEFT, 10, 230, 0x555555, "Laden...", [](lv_event_t* e){ 
        playToneI2S(800, 100, true); HaColorPicker::show(selected_color_on, [](String color) {
            selected_color_on = color; updateColorBtn(btn_color_on, selected_color_on, "Aktiv:");
        });
    });
    
    btn_color_off = UIHelper::createButton(parent, 280, 50, LV_ALIGN_TOP_LEFT, 320, 230, 0x555555, "Laden...", [](lv_event_t* e){ 
        playToneI2S(800, 100, true); HaColorPicker::show(selected_color_off, [](String color) {
            selected_color_off = color; updateColorBtn(btn_color_off, selected_color_off, "Inaktiv:");
        });
    });

    selected_color_on = w->getColorOn(); 
    selected_color_off = w->getColorOff();
    updateColorBtn(btn_color_on, selected_color_on, "Aktiv:"); 
    updateColorBtn(btn_color_off, selected_color_off, "Inaktiv:");
}

void HaDialogEdit::buildLayoutTab(lv_obj_t* parent, HAWidget* w) {
    int y = 15;
    UIHelper::createButton(parent, 150, 40, LV_ALIGN_TOP_LEFT, 10, y, 0x2980B9, "Nach vorne", [](lv_event_t* e){ 
        if(current_widget && current_widget->container && lv_obj_is_valid(current_widget->container)) lv_obj_move_foreground(current_widget->container); 
    });
    UIHelper::createButton(parent, 150, 40, LV_ALIGN_TOP_LEFT, 170, y, 0x8E44AD, "Nach hinten", [](lv_event_t* e){ 
        if(current_widget && current_widget->container && lv_obj_is_valid(current_widget->container)) lv_obj_move_background(current_widget->container); 
    });
    
    y += 60;
    UIHelper::createLabel(parent, "Icon:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
    dd_icon_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(orig_i_align), layout_change_cb);
    UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
    slider_icon_margin = UIHelper::createSlider(parent, 200, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, orig_i_margin, layout_change_cb);
    lbl_i_m_val = UIHelper::createLabel(parent, (String(orig_i_margin) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 570, y+10);

    y += 60;
    UIHelper::createLabel(parent, "Text:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
    dd_text_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(orig_t_align), layout_change_cb);
    UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
    slider_text_margin = UIHelper::createSlider(parent, 200, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, orig_t_margin, layout_change_cb);
    lbl_t_m_val = UIHelper::createLabel(parent, (String(orig_t_margin) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 570, y+10);

    y += 60;
    if (w->getType() == "sensor") {
        UIHelper::createLabel(parent, "Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+10);
        dd_state_pos = UIHelper::createDropdown(parent, 140, 40, LV_ALIGN_TOP_LEFT, 80, y, "Oben\nMitte\nUnten\nLinks\nRechts", alignToIdx(orig_s_align), layout_change_cb);
        UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 240, y+10);
        slider_state_margin = UIHelper::createSlider(parent, 200, 20, LV_ALIGN_TOP_LEFT, 350, y+10, -50, 100, orig_s_margin, layout_change_cb);
        lbl_s_m_val = UIHelper::createLabel(parent, (String(orig_s_margin) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 570, y+10);
    }

    cb_snap = UIHelper::createCheckbox(parent, "Am Raster einrasten (Snap 10px)", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, y+60, w->getSnapToGrid());
}

void HaDialogEdit::buildChartTab(lv_obj_t* parent, HAWidget* w) {
    int cy = 10;
    cb_chart = UIHelper::createCheckbox(parent, "Als Diagramm anzeigen", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy, w->getShowChart());
    cy += 40;

    auto s_cb = [](lv_event_t* e) {
        lv_obj_t* slider = lv_event_get_target(e);
        if (!slider || !lv_obj_is_valid(slider)) return;
        int val = lv_slider_get_value(slider);
        
        lv_obj_t* lbl = (lv_obj_t*)lv_event_get_user_data(e);
        if (lbl && lv_obj_is_valid(lbl)) {
            if (slider == slider_chart_w || slider == slider_chart_h) lv_label_set_text_fmt(lbl, "%d %%", val);
            else lv_label_set_text_fmt(lbl, "%d px", val);
        }
    };

    UIHelper::createLabel(parent, "Breite:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_c_w_val = UIHelper::createLabel(parent, (String(w->getChartWPct()) + " %").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_chart_w = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 50, 100, w->getChartWPct(), s_cb, lbl_c_w_val);
    cy += 40;

    UIHelper::createLabel(parent, "Hoehe:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_c_h_val = UIHelper::createLabel(parent, (String(w->getChartHPct()) + " %").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_chart_h = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 20, 100, w->getChartHPct(), s_cb, lbl_c_h_val);
    cy += 40;

    UIHelper::createLabel(parent, "X-Pos:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_c_x_val = UIHelper::createLabel(parent, (String(w->getChartXOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_chart_x = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, -100, 100, w->getChartXOfs(), s_cb, lbl_c_x_val);
    cy += 40;

    UIHelper::createLabel(parent, "Y-Pos:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_c_y_val = UIHelper::createLabel(parent, (String(w->getChartYOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_chart_y = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, -200, 100, w->getChartYOfs(), s_cb, lbl_c_y_val);
    cy += 50;

    UIHelper::createLabel(parent, "Min. Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy+10);
    ta_chart_min = UIHelper::createTextarea(parent, 150, 40, LV_ALIGN_TOP_LEFT, 120, cy, w->getChartMin().c_str(), "Auto");
    bindKeyboardToTextarea(ta_chart_min);

    UIHelper::createLabel(parent, "Max. Wert:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 300, cy+10);
    ta_chart_max = UIHelper::createTextarea(parent, 150, 40, LV_ALIGN_TOP_LEFT, 420, cy, w->getChartMax().c_str(), "Auto");
    bindKeyboardToTextarea(ta_chart_max);
}

void HaDialogEdit::buildVacuumTab(lv_obj_t* parent, HAWidget* w) {
    int cy = 10;
    auto v_cb = [](lv_event_t* e) {
        lv_obj_t* slider = lv_event_get_target(e);
        if (!slider || !lv_obj_is_valid(slider)) return;
        int val = lv_slider_get_value(slider);
        
        lv_obj_t* lbl = (lv_obj_t*)lv_event_get_user_data(e);
        if (lbl && lv_obj_is_valid(lbl)) {
            lv_label_set_text_fmt(lbl, "%d px", val);
        }
        
        if (current_widget && slider_vac_w && lv_obj_is_valid(slider_vac_w) && 
            slider_vac_h && lv_obj_is_valid(slider_vac_h) && 
            slider_vac_gap && lv_obj_is_valid(slider_vac_gap) && 
            slider_vac_y && lv_obj_is_valid(slider_vac_y)) {
            current_widget->setChartConfig(false, 
                lv_slider_get_value(slider_vac_w), 
                lv_slider_get_value(slider_vac_h), 
                lv_slider_get_value(slider_vac_gap), 
                lv_slider_get_value(slider_vac_y), "", "");
        }
    };

    UIHelper::createLabel(parent, "Breite:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_v_w_val = UIHelper::createLabel(parent, (String(w->getChartWPct()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_vac_w = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 30, 100, w->getChartWPct(), v_cb, lbl_v_w_val);
    cy += 50;

    UIHelper::createLabel(parent, "Hoehe:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_v_h_val = UIHelper::createLabel(parent, (String(w->getChartHPct()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_vac_h = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 20, 80, w->getChartHPct(), v_cb, lbl_v_h_val);
    cy += 50;

    UIHelper::createLabel(parent, "Abstand:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_v_gap_val = UIHelper::createLabel(parent, (String(w->getChartXOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_vac_gap = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, 0, 80, w->getChartXOfs(), v_cb, lbl_v_gap_val);
    cy += 50;

    UIHelper::createLabel(parent, "Y-Pos:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, cy);
    lbl_v_y_val = UIHelper::createLabel(parent, (String(w->getChartYOfs()) + " px").c_str(), &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 400, cy);
    slider_vac_y = UIHelper::createSlider(parent, 250, 20, LV_ALIGN_TOP_LEFT, 120, cy+2, -50, 100, w->getChartYOfs(), v_cb, lbl_v_y_val);
}

void HaDialogEdit::showWidgetEditDialog(HAWidget* w) {
    if (overlay != nullptr || HaColorPicker::isActive()) return; 
    current_widget = w;
    
    orig_w = w->getW(); orig_h = w->getH(); 
    orig_i_align = w->getIconAlign(); orig_t_align = w->getTextAlign(); orig_s_align = w->getStateAlign();
    orig_i_margin = w->getIconMargin(); orig_t_margin = w->getTextMargin(); orig_s_margin = w->getStateMargin();
    orig_c_w = w->getChartWPct(); orig_c_h = w->getChartHPct(); orig_c_x = w->getChartXOfs(); orig_c_y = w->getChartYOfs();
    
    playToneI2S(800, 100, true);
    
    overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_style_bg_opa(overlay, 80, 0); 
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    edit_panel = lv_obj_create(overlay);
    lv_obj_set_size(edit_panel, 680, 560); 
    lv_obj_set_style_bg_color(edit_panel, lv_color_hex(0x222222), 0);
    lv_obj_add_flag(edit_panel, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(edit_panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    if (w->getY() > 300) lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
    else lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* tv = lv_tabview_create(edit_panel, LV_DIR_TOP, 50);
    lv_obj_set_size(tv, 680, 480); 
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(lv_tabview_get_tab_btns(tv), &lv_font_montserrat_24, 0);

    kb = lv_keyboard_create(overlay);
    lv_obj_set_size(kb, 1280, 350); 
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); 
    
    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL) {
            lv_obj_t* ta = lv_keyboard_get_textarea(lv_event_get_target(e));
            if (ta && lv_obj_is_valid(ta)) { 
                lv_obj_clear_state(ta, LV_STATE_FOCUSED); 
                lv_event_send(ta, LV_EVENT_DEFOCUSED, NULL); 
            }
        }
    }, LV_EVENT_ALL, NULL);

    buildSizeTab(lv_tabview_add_tab(tv, "Groesse"), w);
    buildDisplayTab(lv_tabview_add_tab(tv, "Anzeige"), w);
    buildLayoutTab(lv_tabview_add_tab(tv, "Layout"), w);
    
    if (w->getType() == "sensor") buildChartTab(lv_tabview_add_tab(tv, "Diagramm"), w);
    else if (w->getType() == "vacuum") buildVacuumTab(lv_tabview_add_tab(tv, "Buttons"), w);

    UIHelper::createButton(edit_panel, 180, 50, LV_ALIGN_BOTTOM_RIGHT, -20, -15, 0x27AE60, "Uebernehmen", btn_save_event_cb);
    UIHelper::createButton(edit_panel, 180, 50, LV_ALIGN_BOTTOM_RIGHT, -220, -15, 0x555555, "Abbrechen", btn_cancel_event_cb);
}

void HaDialogEdit::handleWidgetDeleteDrop(HAWidget* w) {
    if (!ViewHomeAssistant::trash_btn || overlay != nullptr) return; 
    
    lv_area_t trash_area, w_area, res;
    lv_obj_get_coords(ViewHomeAssistant::trash_btn, &trash_area);
    lv_obj_get_coords(w->container, &w_area);
    
    if (_lv_area_intersect(&res, &trash_area, &w_area)) {
        playToneI2S(600, 100, true);
        
        overlay = lv_obj_create(ViewHomeAssistant::screen);
        lv_obj_set_size(overlay, 1280, 720);
        lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(overlay, 200, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

        lv_obj_t* panel = lv_obj_create(overlay);
        lv_obj_set_size(panel, 500, 250); lv_obj_center(panel);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);

        UIHelper::createLabel(panel, "Widget loeschen?", &lv_font_montserrat_24, LV_ALIGN_TOP_MID, 0, 30);
        
        UIHelper::createButton(panel, 200, 60, LV_ALIGN_BOTTOM_RIGHT, -20, -30, 0xAA0000, "Ja, loeschen", [](lv_event_t* e) {
            HAWidget* wid = (HAWidget*)lv_event_get_user_data(e);
            if (wid && wid->container && lv_obj_is_valid(wid->container)) {
                lv_obj_add_flag(wid->container, LV_OBJ_FLAG_HIDDEN);
            }
            if (overlay && lv_obj_is_valid(overlay)) {
                lv_obj_del_async(overlay); 
            }
            resetState(); 
        }, w);

        UIHelper::createButton(panel, 200, 60, LV_ALIGN_BOTTOM_LEFT, 20, -30, 0x333333, "Abbrechen", [](lv_event_t* e) { 
            if (overlay && lv_obj_is_valid(overlay)) {
                lv_obj_del_async(overlay); 
            }
            resetState(); 
        });
    }
}