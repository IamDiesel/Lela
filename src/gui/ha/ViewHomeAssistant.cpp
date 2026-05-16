#include "ViewHomeAssistant.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "ViewTopbar.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"

#include "HaDialogEdit.h"
#include "HaDialogAdd.h"
#include "HaDialogImport.h"
#include "HaDialogMedia.h" 
#include "HaDialogVacuum.h" 
#include "HaDialogLight.h" 
#include "HaColorPicker.h" 
#include <algorithm> 

lv_obj_t* ViewHomeAssistant::screen = nullptr;
lv_obj_t* ViewHomeAssistant::tabview = nullptr;
lv_obj_t* ViewHomeAssistant::btn_edit = nullptr;
lv_obj_t* ViewHomeAssistant::label_edit = nullptr;
lv_obj_t* ViewHomeAssistant::trash_btn = nullptr;
std::vector<HAWidget*> ViewHomeAssistant::widgets;
std::vector<lv_obj_t*> ViewHomeAssistant::tab_pages; 
volatile bool ViewHomeAssistant::pendingHaReload = false;

int ViewHomeAssistant::editingTabIndex = -1;
uint16_t ViewHomeAssistant::currentActiveTab = 0;

static lv_obj_t* btn_back = nullptr;
static lv_obj_t* btn_import = nullptr;
static lv_obj_t* btn_delete_tab = nullptr; 
static lv_obj_t* btn_rename_tab = nullptr; 
static lv_obj_t* btn_add_widget = nullptr; 
static lv_obj_t* btn_tab_left = nullptr;
static lv_obj_t* btn_tab_right = nullptr;
static lv_obj_t* kb_overlay = nullptr;
static lv_obj_t* ta_new_tab = nullptr;

// =========================================================
// 1. SAUBERE HELPER-FUNKTIONEN (ALS KLASSEN-MITGLIEDER)
// =========================================================

void ViewHomeAssistant::helper_loadWidgets() {
    clearWidgets();
    HaConfigLogic::Load();
    HaWebsocketLogic_UpdateTrackedEntities();
    
    tab_pages.clear();
    
    for (int i = 0; i < HaConfigLogic::dashboards.size(); i++) {
        lv_obj_t * tab = lv_tabview_add_tab(tabview, HaConfigLogic::dashboards[i].name.c_str());
        lv_obj_set_scroll_dir(tab, LV_DIR_VER);
        tab_pages.push_back(tab); 
        
        for (const auto& wDef : HaConfigLogic::dashboards[i].widgets) {
            HAWidget* new_widget = nullptr;
            
            if (wDef.type == "sensor") {
                new_widget = new HASensorWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str(), wDef.show_chart, wDef.chart_w_pct, wDef.chart_h_pct, wDef.chart_x_ofs, wDef.chart_y_ofs, wDef.chart_min, wDef.chart_max);
            } else if (wDef.type == "action") {
                new_widget = new HAActionWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str());
            } else if (wDef.type == "media_player") {
                new_widget = new HAMediaWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str());
            } else if (wDef.type == "media_item") {
                new_widget = new HAMediaItemWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str(), wDef.media_content_type, wDef.media_content_id);
            } else if (wDef.type == "vacuum") {
                new_widget = new HAVacuumWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()); 
            } else {
                new_widget = new HALightWidget(tab, i, wDef.type, wDef.entity_id, wDef.x, wDef.y, wDef.w, wDef.h, wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str());
            }
            
            new_widget->setAlignments(wDef.icon_align, wDef.text_align, wDef.state_align, wDef.icon_margin, wDef.text_margin, wDef.state_margin);
            new_widget->setSnapToGrid(wDef.snap_to_grid); 
            new_widget->setChartConfig(wDef.show_chart, wDef.chart_w_pct, wDef.chart_h_pct, wDef.chart_x_ofs, wDef.chart_y_ofs, wDef.chart_min, wDef.chart_max);
            
            widgets.push_back(new_widget);
        }
    }
    lv_tabview_add_tab(tabview, "+");
    lv_obj_add_event_cb(tabview, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void ViewHomeAssistant::helper_saveWidgets() {
    uint16_t act_tab = lv_tabview_get_tab_act(tabview);
    if (act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
    currentActiveTab = act_tab;
    
    std::stable_sort(widgets.begin(), widgets.end(), [](HAWidget* a, HAWidget* b) {
        int idxA = (a && a->container && lv_obj_is_valid(a->container)) ? lv_obj_get_index(a->container) : 999999;
        int idxB = (b && b->container && lv_obj_is_valid(b->container)) ? lv_obj_get_index(b->container) : 999999;
        return idxA < idxB;
    });

    HaConfigLogic::dashboards[act_tab].widgets.clear();
    HaConfigLogic::dashboards[act_tab].widgets.reserve(widgets.size());

    for (HAWidget* w : widgets) {
        if (!w || !w->container || !lv_obj_is_valid(w->container) || lv_obj_has_flag(w->container, LV_OBJ_FLAG_HIDDEN)) continue; 
        
        if (w->getTabIndex() == act_tab) {
            HAWidgetDef def;
            def.entity_id = w->getEntityId();
            def.type = w->getType();
            def.x = w->getX(); 
            def.y = w->getY(); 
            def.w = w->getW(); 
            def.h = w->getH();
            def.name = w->getName(); 
            def.mdi_icon = w->getMdiIcon(); 
            def.color_on = w->getColorOn(); 
            def.color_off = w->getColorOff(); 
            def.icon_align = w->getIconAlign();
            def.text_align = w->getTextAlign();
            def.state_align = w->getStateAlign();
            def.icon_margin = w->getIconMargin(); 
            def.text_margin = w->getTextMargin(); 
            def.state_margin = w->getStateMargin(); 
            def.snap_to_grid = w->getSnapToGrid(); 
            def.show_chart = w->getShowChart();
            def.chart_w_pct = w->getChartWPct();
            def.chart_h_pct = w->getChartHPct();
            def.chart_x_ofs = w->getChartXOfs();
            def.chart_y_ofs = w->getChartYOfs();
            def.chart_min = w->getChartMin();
            def.chart_max = w->getChartMax();
            def.media_content_type = w->getMediaContentType();
            def.media_content_id = w->getMediaContentId();
            
            HaConfigLogic::dashboards[act_tab].widgets.push_back(def);
        }
    }
    HaConfigLogic::Save();
    HaWebsocketLogic_UpdateTrackedEntities(); 
    pendingHaReload = true;
}

void ViewHomeAssistant::helper_buildEditUI() {
    lv_obj_clear_flag(lv_tabview_get_tab_btns(tabview), LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x27AE60), 0); 
    lv_label_set_text(label_edit, LV_SYMBOL_SAVE " SPEICHERN");

    if (btn_back) {
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0xAA0000), 0);
        lv_label_set_text(lv_obj_get_child(btn_back, 0), LV_SYMBOL_CLOSE " VERWERFEN");
    }

    btn_tab_left = lv_btn_create(screen);
    lv_obj_set_size(btn_tab_left, 80, 50);
    lv_obj_align(btn_tab_left, LV_ALIGN_TOP_LEFT, 200, 15);
    lv_obj_set_style_bg_color(btn_tab_left, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_tab_left, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t act_tab = lv_tabview_get_tab_act(tabview);
        if (act_tab > 0 && act_tab < HaConfigLogic::dashboards.size()) {
            HaConfigLogic::MoveTabLeft(act_tab); HaConfigLogic::Save();
            currentActiveTab = act_tab - 1; pendingHaReload = true;
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_tl = lv_label_create(btn_tab_left); lv_label_set_text(lbl_tl, LV_SYMBOL_LEFT); lv_obj_center(lbl_tl);

    btn_tab_right = lv_btn_create(screen);
    lv_obj_set_size(btn_tab_right, 80, 50);
    lv_obj_align(btn_tab_right, LV_ALIGN_TOP_LEFT, 290, 15);
    lv_obj_set_style_bg_color(btn_tab_right, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_tab_right, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t act_tab = lv_tabview_get_tab_act(tabview);
        if (act_tab >= 0 && act_tab < HaConfigLogic::dashboards.size() - 1) {
            HaConfigLogic::MoveTabRight(act_tab); HaConfigLogic::Save();
            currentActiveTab = act_tab + 1; pendingHaReload = true;
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_tr = lv_label_create(btn_tab_right); lv_label_set_text(lbl_tr, LV_SYMBOL_RIGHT); lv_obj_center(lbl_tr);

    trash_btn = lv_btn_create(screen);
    lv_obj_set_size(trash_btn, 100, 100);
    lv_obj_align(trash_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    lv_obj_set_style_bg_color(trash_btn, lv_color_hex(0xAA0000), 0); 
    lv_obj_set_style_radius(trash_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_t* trash_lbl = lv_label_create(trash_btn);
    lv_label_set_text(trash_lbl, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_font(trash_lbl, &lv_font_montserrat_36, 0);
    lv_obj_center(trash_lbl);

    btn_import = lv_btn_create(screen);
    lv_obj_set_size(btn_import, 200, 50);
    lv_obj_align(btn_import, LV_ALIGN_TOP_RIGHT, -240, 15);
    lv_obj_set_style_bg_color(btn_import, lv_color_hex(0x8E44AD), 0);
    lv_obj_add_event_cb(btn_import, btn_import_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_imp = lv_label_create(btn_import); lv_label_set_text(lbl_imp, LV_SYMBOL_DOWNLOAD " HA IMPORT"); lv_obj_center(lbl_imp);

    btn_delete_tab = lv_btn_create(screen);
    lv_obj_set_size(btn_delete_tab, 220, 50);
    lv_obj_align(btn_delete_tab, LV_ALIGN_TOP_RIGHT, -460, 15); 
    lv_obj_set_style_bg_color(btn_delete_tab, lv_color_hex(0xAA0000), 0); 
    lv_obj_add_event_cb(btn_delete_tab, btn_delete_tab_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_del = lv_label_create(btn_delete_tab); lv_label_set_text(lbl_del, LV_SYMBOL_TRASH " TAB LOESCHEN"); lv_obj_center(lbl_del);

    btn_rename_tab = lv_btn_create(screen);
    lv_obj_set_size(btn_rename_tab, 220, 50);
    lv_obj_align(btn_rename_tab, LV_ALIGN_TOP_RIGHT, -700, 15);
    lv_obj_set_style_bg_color(btn_rename_tab, lv_color_hex(0x2980B9), 0);
    lv_obj_add_event_cb(btn_rename_tab, btn_rename_tab_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ren = lv_label_create(btn_rename_tab); lv_label_set_text(lbl_ren, LV_SYMBOL_EDIT " UMBENENNEN"); lv_obj_center(lbl_ren);

    btn_add_widget = lv_btn_create(screen);
    lv_obj_set_size(btn_add_widget, 250, 50);
    lv_obj_align(btn_add_widget, LV_ALIGN_BOTTOM_LEFT, 20, -30);
    lv_obj_set_style_bg_color(btn_add_widget, lv_color_hex(0x8E44AD), 0);
    lv_obj_add_event_cb(btn_add_widget, btn_add_widget_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_addw = lv_label_create(btn_add_widget); lv_label_set_text(lbl_addw, LV_SYMBOL_PLUS " WIDGET ADD"); lv_obj_center(lbl_addw);
}

void ViewHomeAssistant::helper_destroyEditUI() {
    lv_obj_add_flag(lv_tabview_get_tab_btns(tabview), LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x333333), 0);
    lv_label_set_text(label_edit, LV_SYMBOL_EDIT " BEARBEITEN");

    if (btn_back) {
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0);
        lv_label_set_text(lv_obj_get_child(btn_back, 0), LV_SYMBOL_LEFT " ZURUECK");
    }

    if (btn_tab_left) { lv_obj_del_async(btn_tab_left); btn_tab_left = nullptr; }
    if (btn_tab_right) { lv_obj_del_async(btn_tab_right); btn_tab_right = nullptr; }
    if (btn_import) { lv_obj_del_async(btn_import); btn_import = nullptr; }
    if (btn_delete_tab) { lv_obj_del_async(btn_delete_tab); btn_delete_tab = nullptr; }
    if (btn_rename_tab) { lv_obj_del_async(btn_rename_tab); btn_rename_tab = nullptr; }
    if (btn_add_widget) { lv_obj_del_async(btn_add_widget); btn_add_widget = nullptr; }
    if (trash_btn) { lv_obj_del_async(trash_btn); trash_btn = nullptr; }
}

// =========================================================
// 2. HAUPT-KLASSEN FUNKTIONEN
// =========================================================

String ViewHomeAssistant::generateEntityId(String type, String input) {
    input.trim();
    if (input.indexOf('.') != -1) {
        String id = input; id.toLowerCase(); return id;
    }
    String formatted = input;
    formatted.toLowerCase(); formatted.replace(" ", "_"); formatted.replace("ä", "ae");
    formatted.replace("ö", "oe"); formatted.replace("ü", "ue"); formatted.replace("ß", "ss");
    return type + "." + formatted;
}

void ViewHomeAssistant::btn_back_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    if (HAWidget::editModeActive) {
        HAWidget::editModeActive = false; pendingHaReload = true; 
    } else {
        gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

void ViewHomeAssistant::btn_import_event_cb(lv_event_t * e) {
    if (HaDialogImport::isAnyOverlayActive()) return; 
    playToneI2S(800, 100, true);
    uint16_t act_tab = lv_tabview_get_tab_act(tabview);
    if (act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
    HaDialogImport::showLoadingPopup("Lade Dashboards..."); 
    HaWebsocketLogic_RequestDashboardList(act_tab);
}

void ViewHomeAssistant::btn_delete_tab_event_cb(lv_event_t * e) {
    playToneI2S(600, 100, true); 
    uint16_t act_tab = lv_tabview_get_tab_act(tabview);
    if (act_tab < HaConfigLogic::dashboards.size()) {
        HaConfigLogic::DeleteTab(act_tab); HaConfigLogic::Save();
        HaWebsocketLogic_UpdateTrackedEntities();
        currentActiveTab = 0; pendingHaReload = true; 
    }
}

void ViewHomeAssistant::btn_rename_tab_event_cb(lv_event_t * e) {
    if (kb_overlay != nullptr) return; 
    playToneI2S(800, 100, true); 
    editingTabIndex = lv_tabview_get_tab_act(tabview);
    if (editingTabIndex >= (int)HaConfigLogic::dashboards.size()) editingTabIndex = 0; 
    
    kb_overlay = lv_obj_create(screen);
    lv_obj_set_size(kb_overlay, 1280, 720);
    lv_obj_set_style_bg_color(kb_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(kb_overlay, 200, 0);
    lv_obj_add_flag(kb_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(kb_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 
    
    ta_new_tab = lv_textarea_create(kb_overlay);
    lv_obj_set_size(ta_new_tab, 600, 80);
    lv_obj_align(ta_new_tab, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_text_font(ta_new_tab, &lv_font_montserrat_36, 0);
    lv_textarea_set_text(ta_new_tab, HaConfigLogic::dashboards[editingTabIndex].name.c_str());
    
    lv_obj_t* kb = lv_keyboard_create(kb_overlay);
    lv_obj_set_size(kb, 1280, 400);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_new_tab);
    lv_obj_add_state(ta_new_tab, LV_STATE_FOCUSED);

    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = lv_event_get_target(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) lv_obj_clear_state(lv_keyboard_get_textarea(keyboard), LV_STATE_FOCUSED);
    }, LV_EVENT_ALL, NULL);

    lv_obj_t * btn_kb_cancel = lv_btn_create(kb_overlay);
    lv_obj_set_size(btn_kb_cancel, 80, 80);
    lv_obj_align_to(btn_kb_cancel, ta_new_tab, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_set_style_bg_color(btn_kb_cancel, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(btn_kb_cancel, [](lv_event_t* e) { lv_obj_del_async(kb_overlay); kb_overlay = nullptr; }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_kb_cancel = lv_label_create(btn_kb_cancel); lv_label_set_text(lbl_kb_cancel, LV_SYMBOL_CLOSE); lv_obj_center(lbl_kb_cancel);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
}

void ViewHomeAssistant::btn_add_widget_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    HaDialogAdd::showAddWidgetDialog();
}

void ViewHomeAssistant::kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        playToneI2S(800, 100, true);
        const char * txt = lv_textarea_get_text(ta_new_tab);
        if (strlen(txt) > 0) {
            if (editingTabIndex == -1) {
                HaConfigLogic::AddTab(String(txt)); currentActiveTab = HaConfigLogic::dashboards.size() - 1; 
            } else {
                HaConfigLogic::RenameTab(editingTabIndex, String(txt)); currentActiveTab = editingTabIndex; 
            }
            HaConfigLogic::Save(); HaWebsocketLogic_UpdateTrackedEntities(); 
        }
        lv_obj_del_async(kb_overlay); kb_overlay = nullptr; pendingHaReload = true; 
    } else if (code == LV_EVENT_CANCEL) {
        playToneI2S(600, 100, true);
        lv_obj_del_async(kb_overlay); kb_overlay = nullptr;
        if (editingTabIndex == -1) lv_tabview_set_act(tabview, currentActiveTab, LV_ANIM_OFF);
    }
}

void ViewHomeAssistant::tabview_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) return; 
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    
    if (tab_id == HaConfigLogic::dashboards.size()) {
        playToneI2S(800, 100, true);
        editingTabIndex = -1; 
        
        kb_overlay = lv_obj_create(screen);
        lv_obj_set_size(kb_overlay, 1280, 720);
        lv_obj_set_style_bg_color(kb_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(kb_overlay, 200, 0);
        lv_obj_add_flag(kb_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(kb_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 
        
        ta_new_tab = lv_textarea_create(kb_overlay);
        lv_obj_set_size(ta_new_tab, 600, 80);
        lv_obj_align(ta_new_tab, LV_ALIGN_TOP_MID, 0, 100);
        lv_obj_set_style_text_font(ta_new_tab, &lv_font_montserrat_36, 0);
        lv_textarea_set_placeholder_text(ta_new_tab, "Name des neuen Raums...");
        
        lv_obj_t* kb = lv_keyboard_create(kb_overlay);
        lv_obj_set_size(kb, 1280, 400);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(kb, ta_new_tab);
        lv_obj_add_state(ta_new_tab, LV_STATE_FOCUSED);

        lv_obj_add_event_cb(kb, [](lv_event_t* e){
            lv_event_code_t code = lv_event_get_code(e);
            lv_obj_t* keyboard = lv_event_get_target(e);
            if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) lv_obj_clear_state(lv_keyboard_get_textarea(keyboard), LV_STATE_FOCUSED);
        }, LV_EVENT_ALL, NULL);

        lv_obj_t * btn_kb_cancel = lv_btn_create(kb_overlay);
        lv_obj_set_size(btn_kb_cancel, 80, 80);
        lv_obj_align_to(btn_kb_cancel, ta_new_tab, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
        lv_obj_set_style_bg_color(btn_kb_cancel, lv_color_hex(0xAA0000), 0);
        lv_obj_add_event_cb(btn_kb_cancel, [](lv_event_t* e) {
            lv_obj_del_async(kb_overlay); kb_overlay = nullptr; lv_tabview_set_act(tabview, currentActiveTab, LV_ANIM_OFF);
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t * lbl_kb_cancel = lv_label_create(btn_kb_cancel); lv_label_set_text(lbl_kb_cancel, LV_SYMBOL_CLOSE); lv_obj_center(lbl_kb_cancel);
        
        lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
        lv_tabview_set_act(tv, currentActiveTab, LV_ANIM_OFF); 
    } else {
        if (currentActiveTab != tab_id) currentActiveTab = tab_id;
    }
}

void ViewHomeAssistant::btn_edit_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
    HAWidget::editModeActive = !HAWidget::editModeActive;
    
    if (HAWidget::editModeActive) {
        helper_buildEditUI();
    } else {
        helper_destroyEditUI();
        helper_saveWidgets();
    }
}

lv_obj_t* ViewHomeAssistant::build() {
    trash_btn = nullptr; btn_import = nullptr; btn_delete_tab = nullptr;
    btn_rename_tab = nullptr; btn_add_widget = nullptr; btn_tab_left = nullptr;
    btn_tab_right = nullptr; kb_overlay = nullptr; 

    HaDialogEdit::resetState(); HaDialogAdd::resetState();
    HaDialogImport::resetState(); HaDialogMedia::resetState(); 
    
    if (HaColorPicker::isActive()) HaColorPicker::hide();
    if (HaDialogLight::isActive()) HaDialogLight::hide();

    pendingHaReload = false; HAWidget::editModeActive = false; 
    
    HaWebsocketLogic_Start();
    
    HAWidget::onEditRequested = HaDialogEdit::showWidgetEditDialog;
    HAWidget::onDeleteRequested = HaDialogEdit::handleWidgetDeleteDrop;
    HAWidget::onLightControlRequested = HaDialogLight::show;
    HAWidget::onMediaControlRequested = HaDialogMedia::showMediaControlDialog;
    HAWidget::onVacuumControlRequested = HaDialogVacuum::showVacuumDialog; 
    
    screen = lv_obj_create(NULL);
    if (!screen) return nullptr;

    lv_obj_set_style_bg_color(screen, isDarkMode ? lv_color_hex(0x111111) : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(screen);

    tabview = lv_tabview_create(screen, LV_DIR_TOP, 60);
    lv_obj_set_size(tabview, 1280, 640); 
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_scroll_dir(lv_tabview_get_content(tabview), LV_DIR_VER);

    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, isDarkMode ? lv_color_hex(0x222222) : lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    helper_loadWidgets();

    btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 160, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 15);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back); lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " ZURUECK"); lv_obj_center(lbl_back);

    btn_edit = lv_btn_create(screen);
    lv_obj_set_size(btn_edit, 200, 50);
    lv_obj_align(btn_edit, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(btn_edit, btn_edit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x333333), 0);
    label_edit = lv_label_create(btn_edit); lv_label_set_text(label_edit, LV_SYMBOL_EDIT " BEARBEITEN"); lv_obj_center(label_edit);

    if (HaConfigLogic::dashboards.size() > 0) {
        if (currentActiveTab >= HaConfigLogic::dashboards.size()) currentActiveTab = 0;
        lv_tabview_set_act(tabview, currentActiveTab, LV_ANIM_OFF);
    }

    return screen;
}

void ViewHomeAssistant::update() {
    HaDialogImport::checkPendingEvents();
    HaDialogMedia::update();
    
    static uint32_t reloadWait = 0;
    if (pendingHaReload) { 
        if (reloadWait == 0) reloadWait = millis();
        if (millis() - reloadWait > 150) {
            pendingHaReload = false; reloadWait = 0;
            clearWidgets(); 
            lv_obj_t* new_scr = ViewHomeAssistant::build();
            lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
        }
        return; 
    } else {
        reloadWait = 0;
    }

    if (gui.getCurrentScreen() != SCREEN_HA) return;
    ViewTopbar_Update();

    static uint32_t lastWidgetUpdate = 0;
    static uint32_t localCacheVersion = 0;
    uint32_t currentVersion = HaEntityCache::GetCacheVersion();

    if (currentVersion != localCacheVersion && (millis() - lastWidgetUpdate > 100)) {
        localCacheVersion = currentVersion;
        lastWidgetUpdate = millis();
        for (HAWidget* w : widgets) {
            String state = HaWebsocketLogic_GetState(w->getEntityId());
            if (state.length() > 0) w->updateState(state);
        }
    }
}

void ViewHomeAssistant::clearWidgets() {
    for (HAWidget* w : widgets) {
        if (w->container && lv_obj_is_valid(w->container)) lv_obj_remove_event_cb(w->container, nullptr); 
        w->container = nullptr; 
        delete w; 
    }
    widgets.clear();
}