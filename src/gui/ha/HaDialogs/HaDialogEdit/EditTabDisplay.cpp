#include "EditTabDisplay.h"
#include "UIHelper.h"
#include "HaDialogEdit.h"
#include "MdiMapper.h"
#include "HaColorPicker.h"
#include "SharedData.h"

void EditTabDisplay::updateColorBtn(lv_obj_t* btn, String hexColor, const char* prefix) {
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

void EditTabDisplay::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    UIHelper::createLabel(parent, "Name:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 10);
    ta_name = UIHelper::createTextarea(parent, 700, 40, LV_ALIGN_TOP_LEFT, 100, 0, w->getName().c_str());
    HaDialogEdit::bindKeyboard(ta_name, LV_KEYBOARD_MODE_TEXT_LOWER);

    UIHelper::createLabel(parent, "Suche:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 60);
    ta_icon_search = UIHelper::createTextarea(parent, 700, 40, LV_ALIGN_TOP_LEFT, 100, 50, last_search_term.c_str(), "z.B. arrow");
    HaDialogEdit::bindKeyboard(ta_icon_search, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    lv_obj_add_event_cb(ta_icon_search, [](lv_event_t* e) {
        EditTabDisplay* tab = (EditTabDisplay*)lv_event_get_user_data(e);
        tab->last_search_term = String(lv_textarea_get_text(tab->ta_icon_search)); 
        tab->last_search_term.toLowerCase();
        
        if (tab->last_search_term.length() < 2) {
            if (tab->dd_icon_cat && lv_obj_is_valid(tab->dd_icon_cat) && tab->dd_icon && lv_obj_is_valid(tab->dd_icon)) {
                lv_dropdown_set_options(tab->dd_icon, icon_categories[lv_dropdown_get_selected(tab->dd_icon_cat)].options);
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
                
                if (search_target.indexOf(tab->last_search_term) != -1) { 
                    results += "\n" + icon_name; count++; 
                }
                pos = (end == -1) ? opts.length() : end + 1;
                if (count >= 50) break;
            }
            if (count >= 50) break;
        }
        
        if (count == 0) results += "\nKeine Treffer";
        if (tab->dd_icon && lv_obj_is_valid(tab->dd_icon)) {
            lv_dropdown_set_options(tab->dd_icon, results.c_str()); 
            lv_dropdown_set_selected(tab->dd_icon, count > 0 ? 1 : 0);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    UIHelper::createLabel(parent, "Bereich:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 110);
    String cat_opts = "";
    for(int i = 0; i < num_icon_categories; i++) { 
        cat_opts += icon_categories[i].name; 
        if(i < num_icon_categories - 1) cat_opts += "\n"; 
    }
    dd_icon_cat = UIHelper::createDropdown(parent, 700, 40, LV_ALIGN_TOP_LEFT, 100, 100, cat_opts.c_str(), 0, [](lv_event_t* e) {
        EditTabDisplay* tab = (EditTabDisplay*)lv_event_get_user_data(e);
        if (tab->dd_icon && lv_obj_is_valid(tab->dd_icon) && tab->dd_icon_cat && lv_obj_is_valid(tab->dd_icon_cat)) {
            lv_dropdown_set_options(tab->dd_icon, icon_categories[lv_dropdown_get_selected(tab->dd_icon_cat)].options); 
            lv_dropdown_set_selected(tab->dd_icon, 0); 
        }
        if (tab->last_search_term.length() > 0 && tab->ta_icon_search && lv_obj_is_valid(tab->ta_icon_search)) { 
            tab->last_search_term = ""; 
            lv_textarea_set_text(tab->ta_icon_search, ""); 
        }
    });
    lv_obj_add_event_cb(dd_icon_cat, NULL, LV_EVENT_VALUE_CHANGED, this);

    UIHelper::createLabel(parent, "Icon:", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, 160);
    dd_icon = UIHelper::createDropdown(parent, 700, 40, LV_ALIGN_TOP_LEFT, 100, 150, "", 0);
    
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
        lv_event_send(ta_icon_search, LV_EVENT_VALUE_CHANGED, NULL);
    }

    UIHelper::createLabel(parent, "Personalisierte Farben:", nullptr, LV_ALIGN_TOP_LEFT, 10, 205, 0x00A0FF);
    
    selected_color_on = w->getColorOn(); 
    selected_color_off = w->getColorOff();

    btn_color_on = UIHelper::createButton(parent, 350, 50, LV_ALIGN_TOP_LEFT, 10, 230, 0x555555, "Laden...", [](lv_event_t* e){ 
        EditTabDisplay* tab = (EditTabDisplay*)lv_event_get_user_data(e);
        playToneI2S(800, 100, true); 
        HaColorPicker::show(tab->selected_color_on, [tab](String color) {
            tab->selected_color_on = color; 
            updateColorBtn(tab->btn_color_on, tab->selected_color_on, "Aktiv:");
        });
    }, this);
    
    btn_color_off = UIHelper::createButton(parent, 350, 50, LV_ALIGN_TOP_LEFT, 380, 230, 0x555555, "Laden...", [](lv_event_t* e){ 
        EditTabDisplay* tab = (EditTabDisplay*)lv_event_get_user_data(e);
        playToneI2S(800, 100, true); 
        HaColorPicker::show(tab->selected_color_off, [tab](String color) {
            tab->selected_color_off = color; 
            updateColorBtn(tab->btn_color_off, tab->selected_color_off, "Inaktiv:");
        });
    }, this);

    updateColorBtn(btn_color_on, selected_color_on, "Aktiv:"); 
    updateColorBtn(btn_color_off, selected_color_off, "Inaktiv:");
}

void EditTabDisplay::saveConfig(HAWidget* w) {
    if (ta_name && lv_obj_is_valid(ta_name)) {
        w->setName(String(lv_textarea_get_text(ta_name)));
    }

    if (dd_icon && lv_obj_is_valid(dd_icon)) {
        char buf[64]; 
        lv_dropdown_get_selected_str(dd_icon, buf, sizeof(buf));
        String sel_icon = String(buf);
        if (sel_icon != "--- Unveraendert ---") {
            if (sel_icon.startsWith("Standard") || sel_icon.startsWith("---") || sel_icon.startsWith("Keine Treffer")) {
                w->setMdiIcon("");
            } else {
                int space_idx = sel_icon.indexOf(' ');
                if (space_idx != -1) w->setMdiIcon(sel_icon.substring(space_idx + 1));
                else w->setMdiIcon(sel_icon);
            }
        }
    }
    w->setColors(selected_color_on, selected_color_off);
}