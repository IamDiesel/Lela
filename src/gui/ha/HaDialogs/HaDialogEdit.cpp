#include "HaDialogEdit.h"
#include "ViewHomeAssistant.h"
#include "SharedData.h"
#include "UIHelper.h"

// --- Alle modularen Tabs ---
#include "EditTabSize.h"
#include "EditTabDisplay.h"
#include "EditTabLayout.h"
#include "EditTabAction.h"
#include "EditTabCondition.h"

lv_obj_t* HaDialogEdit::overlay = nullptr; 
lv_obj_t* HaDialogEdit::edit_panel = nullptr;          
HAWidget* HaDialogEdit::current_widget = nullptr;
lv_obj_t* HaDialogEdit::kb = nullptr;

std::vector<IEditTab*> HaDialogEdit::active_tabs;

int HaDialogEdit::orig_w = 0; int HaDialogEdit::orig_h = 0;
int HaDialogEdit::orig_i_align = 0; int HaDialogEdit::orig_t_align = 0; int HaDialogEdit::orig_s_align = 0;
int HaDialogEdit::orig_i_margin = 0; int HaDialogEdit::orig_t_margin = 0; int HaDialogEdit::orig_s_margin = 0;
int HaDialogEdit::orig_c_w = 0; int HaDialogEdit::orig_c_h = 0; int HaDialogEdit::orig_c_x = 0; int HaDialogEdit::orig_c_y = 0;

void HaDialogEdit::resetState() {
    for (auto tab : active_tabs) { delete tab; }
    active_tabs.clear();

    overlay = nullptr; 
    edit_panel = nullptr; 
    current_widget = nullptr; 
    kb = nullptr;
}

void HaDialogEdit::btn_save_event_cb(lv_event_t * e) {
    if (!current_widget) return;
    
    // Lasse jeden Tab seine eigenen Werte speichern!
    for (auto tab : active_tabs) {
        tab->saveConfig(current_widget);
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

void HaDialogEdit::bindKeyboard(lv_obj_t* ta, int mode) {
    if (!ta) return;
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* target = lv_event_get_target(e);
        int m = (int)(intptr_t)lv_event_get_user_data(e);
        
        if(code == LV_EVENT_FOCUSED) {
            if (kb && lv_obj_is_valid(kb)) {
                lv_keyboard_set_textarea(kb, target);
                lv_keyboard_set_mode(kb, m);
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
                if (current_widget->getY() > 300) lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
                else lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
            }
        }
    }, LV_EVENT_ALL, (void*)(intptr_t)mode);
}

void HaDialogEdit::showWidgetEditDialog(HAWidget* w) {
    if (overlay != nullptr) return; 
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
    lv_obj_set_size(edit_panel, 860, 580); 
    lv_obj_set_style_bg_color(edit_panel, lv_color_hex(0x222222), 0);
    lv_obj_add_flag(edit_panel, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_clear_flag(edit_panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    if (w->getY() > 300) lv_obj_align(edit_panel, LV_ALIGN_TOP_MID, 0, 10);
    else lv_obj_align(edit_panel, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* tv = lv_tabview_create(edit_panel, LV_DIR_TOP, 50);
    lv_obj_set_size(tv, 860, 500); 
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

    // --- Die saubere neue Modul-Architektur ---
    active_tabs.push_back(new EditTabSize());
    active_tabs.push_back(new EditTabDisplay());
    active_tabs.push_back(new EditTabLayout());
    active_tabs.push_back(new EditTabAction());
    active_tabs.push_back(new EditTabCondition());
    
    for (auto tab : active_tabs) {
        lv_obj_t* t = lv_tabview_add_tab(tv, tab->getName());
        tab->buildUI(t, w, kb);
    }

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