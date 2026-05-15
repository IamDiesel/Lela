#include "HaDialogImport.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "HaConfigLogic.h"
#include <lvgl.h>

static lv_obj_t* import_overlay = nullptr;
static lv_obj_t* dd_dashboards = nullptr;
static lv_obj_t* view_overlay = nullptr;
static lv_obj_t* dd_views = nullptr;
static lv_obj_t* error_overlay = nullptr;
static lv_obj_t* loading_overlay = nullptr;
static lv_obj_t* loading_label = nullptr;

void HaDialogImport::resetState() {
    import_overlay = nullptr;
    dd_dashboards = nullptr;
    view_overlay = nullptr;
    dd_views = nullptr;
    error_overlay = nullptr;
    loading_overlay = nullptr;
    loading_label = nullptr;
}

bool HaDialogImport::isAnyOverlayActive() {
    return import_overlay != nullptr || view_overlay != nullptr || error_overlay != nullptr || loading_overlay != nullptr;
}

void HaDialogImport::showLoadingPopup(const char* text) {
    if (loading_overlay == nullptr) {
        loading_overlay = lv_obj_create(ViewHomeAssistant::screen);
        lv_obj_set_size(loading_overlay, 1280, 720);
        lv_obj_set_style_bg_color(loading_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(loading_overlay, 200, 0);
        lv_obj_clear_flag(loading_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

        lv_obj_t* panel = lv_obj_create(loading_overlay);
        lv_obj_set_size(panel, 400, 200);
        lv_obj_center(panel);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

        lv_obj_t* spinner = lv_spinner_create(panel, 1000, 60);
        lv_obj_set_size(spinner, 80, 80);
        lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 20);

        loading_label = lv_label_create(panel);
        lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
        lv_obj_align(loading_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    }
    lv_label_set_text(loading_label, text);
}

void HaDialogImport::hideLoadingPopup() {
    if (loading_overlay != nullptr) {
        lv_obj_del_async(loading_overlay); 
        loading_overlay = nullptr;
        loading_label = nullptr;
    }
}

void HaDialogImport::showDashboardSelectionPopup() {
    playToneI2S(800, 100, true);
    import_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(import_overlay, 1280, 720);
    lv_obj_set_style_bg_color(import_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(import_overlay, 200, 0);
    lv_obj_clear_flag(import_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(import_overlay);
    lv_obj_set_size(panel, 600, 400);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "1. Waehle das Dashboard:");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);

    dd_dashboards = lv_roller_create(panel);
    String options = "";
    if (availableDashboardTitles.empty()) options = "Keine Dashboards gefunden";
    else {
        for (int i = 0; i < availableDashboardTitles.size(); i++) {
            options += availableDashboardTitles[i];
            if (i < availableDashboardTitles.size() - 1) options += "\n";
        }
    }
    
    lv_roller_set_options(dd_dashboards, options.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(dd_dashboards, 4);
    lv_obj_set_width(dd_dashboards, 500);
    lv_obj_set_style_text_font(dd_dashboards, &lv_font_montserrat_24, 0);
    lv_obj_align(dd_dashboards, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, 200, 60);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x27AE60), 0);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t sel = lv_roller_get_selected(dd_dashboards);
        if (sel < availableDashboardUrls.size()) {
            showLoadingPopup("Lade Raeume..."); 
            HaWebsocketLogic_RequestDashboardViews(availableDashboardUrls[sel]); 
        }
        lv_obj_del_async(import_overlay); 
        import_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Weiter");
    lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_ok);

    lv_obj_t* btn_cancel = lv_btn_create(panel);
    lv_obj_set_size(btn_cancel, 200, 60);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) {
        playToneI2S(600, 100, true);
        lv_obj_del_async(import_overlay); 
        import_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Abbrechen");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_cancel);
}

void HaDialogImport::showViewSelectionPopup() {
    playToneI2S(800, 100, true);
    view_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(view_overlay, 1280, 720);
    lv_obj_set_style_bg_color(view_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(view_overlay, 200, 0);
    lv_obj_clear_flag(view_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(view_overlay);
    lv_obj_set_size(panel, 600, 400);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "2. Waehle den HA-Tab (View):");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);

    dd_views = lv_roller_create(panel);
    String options = "";
    if (availableViewTitles.empty()) options = "Keine Tabs gefunden";
    else {
        for (int i = 0; i < availableViewTitles.size(); i++) {
            options += availableViewTitles[i];
            if (i < availableViewTitles.size() - 1) options += "\n";
        }
    }
    
    lv_roller_set_options(dd_views, options.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(dd_views, 4);
    lv_obj_set_width(dd_views, 500);
    lv_obj_set_style_text_font(dd_views, &lv_font_montserrat_24, 0);
    lv_obj_align(dd_views, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, 200, 60);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x8E44AD), 0); 
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        uint16_t sel = lv_roller_get_selected(dd_views);
        if (sel < availableViewTitles.size()) {
            showLoadingPopup("Importiere Widgets..."); 
            
            uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
            if(act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
            ViewHomeAssistant::currentActiveTab = act_tab;
            
            HaWebsocketLogic_RequestDashboardCards(currentImportUrl, sel); 
        }
        lv_obj_del_async(view_overlay); 
        view_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Importieren");
    lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_ok);

    lv_obj_t* btn_cancel = lv_btn_create(panel);
    lv_obj_set_size(btn_cancel, 200, 60);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) {
        playToneI2S(600, 100, true);
        lv_obj_del_async(view_overlay); 
        view_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Abbrechen");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_cancel);
}

void HaDialogImport::showImportErrorPopup() {
    playToneI2S(600, 200, true);
    error_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(error_overlay, 1280, 720);
    lv_obj_set_style_bg_color(error_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(error_overlay, 200, 0);
    lv_obj_clear_flag(error_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* panel = lv_obj_create(error_overlay);
    lv_obj_set_size(panel, 640, 300);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xAA0000), 0); 
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_t* lbl = lv_label_create(panel);
    
    // FIX: Nutze die rein globale Variable ohne lokales 'extern'
    lv_label_set_text(lbl, ::importErrorMessage.c_str());
    
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* btn_ok = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, 200, 60);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
        lv_obj_del_async(error_overlay); 
        error_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Verstanden");
    lv_obj_center(lbl_ok);
}

void HaDialogImport::checkPendingEvents() {
    if (pendingDashboardList) { pendingDashboardList = false; hideLoadingPopup(); showDashboardSelectionPopup(); }
    if (pendingViewList) { pendingViewList = false; hideLoadingPopup(); showViewSelectionPopup(); }
    if (pendingImportError) { pendingImportError = false; hideLoadingPopup(); showImportErrorPopup(); }
}