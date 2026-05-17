#include "HaEditToolbar.h"
#include "ViewHomeAssistant.h"
#include "HaDialogTab.h"
#include "HaDialogImport.h"
#include "HaDialogAdd.h"
#include "HaConfigLogic.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"

lv_obj_t* HaEditToolbar::btn_import = nullptr;
lv_obj_t* HaEditToolbar::btn_delete_tab = nullptr; 
lv_obj_t* HaEditToolbar::btn_rename_tab = nullptr; 
lv_obj_t* HaEditToolbar::btn_add_widget = nullptr; 
lv_obj_t* HaEditToolbar::btn_tab_left = nullptr;
lv_obj_t* HaEditToolbar::btn_tab_right = nullptr;

void HaEditToolbar::resetState() {
    btn_import = nullptr;
    btn_delete_tab = nullptr; 
    btn_rename_tab = nullptr; 
    btn_add_widget = nullptr; 
    btn_tab_left = nullptr;
    btn_tab_right = nullptr;
}

void HaEditToolbar::show() {
    lv_obj_clear_flag(lv_tabview_get_tab_btns(ViewHomeAssistant::tabview), LV_OBJ_FLAG_CLICKABLE); 
    
    lv_obj_set_style_bg_color(ViewHomeAssistant::btn_edit, lv_color_hex(0x27AE60), 0); 
    lv_label_set_text(ViewHomeAssistant::label_edit, LV_SYMBOL_SAVE " SPEICHERN");

    btn_tab_left = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_tab_left, 80, 50);
    lv_obj_align(btn_tab_left, LV_ALIGN_TOP_LEFT, 200, 15);
    lv_obj_set_style_bg_color(btn_tab_left, lv_color_hex(0x2980B9), 0);
    
    lv_obj_add_event_cb(btn_tab_left, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
        if (act_tab > 0 && act_tab < HaConfigLogic::dashboards.size()) {
            HaConfigLogic::MoveTabLeft(act_tab); 
            HaConfigLogic::Save();
            ViewHomeAssistant::currentActiveTab = act_tab - 1; 
            ViewHomeAssistant::pendingHaReload = true;
        }
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_tl = lv_label_create(btn_tab_left); 
    lv_label_set_text(lbl_tl, LV_SYMBOL_LEFT); 
    lv_obj_center(lbl_tl);

    btn_tab_right = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_tab_right, 80, 50);
    lv_obj_align(btn_tab_right, LV_ALIGN_TOP_LEFT, 290, 15);
    lv_obj_set_style_bg_color(btn_tab_right, lv_color_hex(0x2980B9), 0);
    
    lv_obj_add_event_cb(btn_tab_right, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
        if (act_tab >= 0 && act_tab < HaConfigLogic::dashboards.size() - 1) {
            HaConfigLogic::MoveTabRight(act_tab); 
            HaConfigLogic::Save();
            ViewHomeAssistant::currentActiveTab = act_tab + 1; 
            ViewHomeAssistant::pendingHaReload = true;
        }
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_tr = lv_label_create(btn_tab_right); 
    lv_label_set_text(lbl_tr, LV_SYMBOL_RIGHT); 
    lv_obj_center(lbl_tr);

    ViewHomeAssistant::trash_btn = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(ViewHomeAssistant::trash_btn, 100, 100);
    lv_obj_align(ViewHomeAssistant::trash_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    lv_obj_set_style_bg_color(ViewHomeAssistant::trash_btn, lv_color_hex(0xAA0000), 0); 
    lv_obj_set_style_radius(ViewHomeAssistant::trash_btn, LV_RADIUS_CIRCLE, 0);
    
    lv_obj_t* trash_lbl = lv_label_create(ViewHomeAssistant::trash_btn);
    lv_label_set_text(trash_lbl, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_font(trash_lbl, &lv_font_montserrat_36, 0);
    lv_obj_center(trash_lbl);

    btn_import = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_import, 200, 50);
    lv_obj_align(btn_import, LV_ALIGN_TOP_RIGHT, -240, 15);
    lv_obj_set_style_bg_color(btn_import, lv_color_hex(0x8E44AD), 0);
    
    lv_obj_add_event_cb(btn_import, [](lv_event_t* e) {
        if (HaDialogImport::isAnyOverlayActive()) return; 
        playToneI2S(800, 100, true);
        uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
        if (act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
        HaDialogImport::showLoadingPopup("Lade Dashboards..."); 
        HaWebsocketLogic_RequestDashboardList(act_tab);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_imp = lv_label_create(btn_import); 
    lv_label_set_text(lbl_imp, LV_SYMBOL_DOWNLOAD " HA IMPORT"); 
    lv_obj_center(lbl_imp);

    btn_delete_tab = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_delete_tab, 220, 50);
    lv_obj_align(btn_delete_tab, LV_ALIGN_TOP_RIGHT, -460, 15); 
    lv_obj_set_style_bg_color(btn_delete_tab, lv_color_hex(0xAA0000), 0); 
    
    lv_obj_add_event_cb(btn_delete_tab, [](lv_event_t* e) {
        HaDialogTab::deleteCurrentTab();
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_del = lv_label_create(btn_delete_tab); 
    lv_label_set_text(lbl_del, LV_SYMBOL_TRASH " TAB LOESCHEN"); 
    lv_obj_center(lbl_del);

    btn_rename_tab = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_rename_tab, 220, 50);
    lv_obj_align(btn_rename_tab, LV_ALIGN_TOP_RIGHT, -700, 15);
    lv_obj_set_style_bg_color(btn_rename_tab, lv_color_hex(0x2980B9), 0);
    
    lv_obj_add_event_cb(btn_rename_tab, [](lv_event_t* e) {
        HaDialogTab::showRenameTabDialog();
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_ren = lv_label_create(btn_rename_tab); 
    lv_label_set_text(lbl_ren, LV_SYMBOL_EDIT " UMBENENNEN"); 
    lv_obj_center(lbl_ren);

    btn_add_widget = lv_btn_create(ViewHomeAssistant::screen);
    lv_obj_set_size(btn_add_widget, 250, 50);
    lv_obj_align(btn_add_widget, LV_ALIGN_BOTTOM_LEFT, 20, -30);
    lv_obj_set_style_bg_color(btn_add_widget, lv_color_hex(0x8E44AD), 0);
    
    lv_obj_add_event_cb(btn_add_widget, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        HaDialogAdd::showAddWidgetDialog();
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_addw = lv_label_create(btn_add_widget); 
    lv_label_set_text(lbl_addw, LV_SYMBOL_PLUS " WIDGET ADD"); 
    lv_obj_center(lbl_addw);
}

void HaEditToolbar::hide() {
    lv_obj_add_flag(lv_tabview_get_tab_btns(ViewHomeAssistant::tabview), LV_OBJ_FLAG_CLICKABLE); 
    
    lv_obj_set_style_bg_color(ViewHomeAssistant::btn_edit, lv_color_hex(0x333333), 0);
    lv_label_set_text(ViewHomeAssistant::label_edit, LV_SYMBOL_EDIT " BEARBEITEN");

    if (btn_tab_left) { lv_obj_del_async(btn_tab_left); btn_tab_left = nullptr; }
    if (btn_tab_right) { lv_obj_del_async(btn_tab_right); btn_tab_right = nullptr; }
    if (btn_import) { lv_obj_del_async(btn_import); btn_import = nullptr; }
    if (btn_delete_tab) { lv_obj_del_async(btn_delete_tab); btn_delete_tab = nullptr; }
    if (btn_rename_tab) { lv_obj_del_async(btn_rename_tab); btn_rename_tab = nullptr; }
    if (btn_add_widget) { lv_obj_del_async(btn_add_widget); btn_add_widget = nullptr; }
    
    if (ViewHomeAssistant::trash_btn) { 
        lv_obj_del_async(ViewHomeAssistant::trash_btn); 
        ViewHomeAssistant::trash_btn = nullptr; 
    }
}