#include "HaDialogVacuum.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"

static lv_obj_t* vacuum_overlay = nullptr;
static String cur_vacuum_entity = "";

void HaDialogVacuum::showVacuumDialog(HAWidget* w) {
    if (vacuum_overlay != nullptr) {
        return;
    }
    
    cur_vacuum_entity = w->getEntityId();

    vacuum_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(vacuum_overlay, 1280, 720);
    lv_obj_set_style_bg_color(vacuum_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(vacuum_overlay, 220, 0);

    lv_obj_t* panel = lv_obj_create(vacuum_overlay);
    lv_obj_set_size(panel, 650, 480);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);

    String name = HaWebsocketLogic_GetEntityName(cur_vacuum_entity);
    int bat = HaWebsocketLogic_GetBattery(cur_vacuum_entity);
    
    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, name.c_str());
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 10, 10);

    if (bat >= 0) {
        lv_obj_t* lbl_bat = lv_label_create(panel);
        lv_label_set_text_fmt(lbl_bat, LV_SYMBOL_CHARGE " %d%%", bat);
        lv_obj_set_style_text_color(lbl_bat, lv_color_hex(0x2ECC71), 0);
        lv_obj_set_style_text_font(lbl_bat, &lv_font_montserrat_20, 0);
        lv_obj_align(lbl_bat, LV_ALIGN_TOP_RIGHT, -60, 15);
    }

    lv_obj_t* lbl_status = lv_label_create(panel);
    lv_label_set_text_fmt(lbl_status, "Status: %s", HaWebsocketLogic_GetState(cur_vacuum_entity).c_str());
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_LEFT, 10, 50);

    int btn_y = 110;
    
    lv_obj_t* btn_start = lv_btn_create(panel);
    lv_obj_set_size(btn_start, 180, 60);
    lv_obj_align(btn_start, LV_ALIGN_TOP_LEFT, 10, btn_y);
    lv_obj_set_style_bg_color(btn_start, lv_color_hex(0x27AE60), 0);
    
    lv_obj_t* lbl_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_start, LV_SYMBOL_PLAY " Start");
    lv_obj_set_style_text_color(lbl_start, lv_color_white(), 0);
    lv_obj_center(lbl_start);
    
    lv_obj_add_event_cb(btn_start, [](lv_event_t* e){
        HaWebsocketLogic_CallVacuumService(cur_vacuum_entity, "start");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_pause = lv_btn_create(panel);
    lv_obj_set_size(btn_pause, 180, 60);
    lv_obj_align(btn_pause, LV_ALIGN_TOP_MID, 0, btn_y);
    lv_obj_set_style_bg_color(btn_pause, lv_color_hex(0xF39C12), 0);
    
    lv_obj_t* lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, LV_SYMBOL_PAUSE " Pause");
    lv_obj_set_style_text_color(lbl_pause, lv_color_white(), 0);
    lv_obj_center(lbl_pause);
    
    lv_obj_add_event_cb(btn_pause, [](lv_event_t* e){
        HaWebsocketLogic_CallVacuumService(cur_vacuum_entity, "pause");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_stop = lv_btn_create(panel);
    lv_obj_set_size(btn_stop, 180, 60);
    lv_obj_align(btn_stop, LV_ALIGN_TOP_RIGHT, -10, btn_y);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xC0392B), 0);
    
    lv_obj_t* lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_STOP " Stop");
    lv_obj_set_style_text_color(lbl_stop, lv_color_white(), 0);
    lv_obj_center(lbl_stop);
    
    lv_obj_add_event_cb(btn_stop, [](lv_event_t* e){
        HaWebsocketLogic_CallVacuumService(cur_vacuum_entity, "stop");
    }, LV_EVENT_CLICKED, NULL);

    btn_y += 80;
    
    lv_obj_t* btn_locate = lv_btn_create(panel);
    lv_obj_set_size(btn_locate, 280, 60);
    lv_obj_align(btn_locate, LV_ALIGN_TOP_LEFT, 10, btn_y);
    lv_obj_set_style_bg_color(btn_locate, lv_color_hex(0x8E44AD), 0);
    
    lv_obj_t* lbl_locate = lv_label_create(btn_locate);
    lv_label_set_text(lbl_locate, LV_SYMBOL_BELL " Lokalisieren");
    lv_obj_set_style_text_color(lbl_locate, lv_color_white(), 0);
    lv_obj_center(lbl_locate);
    
    lv_obj_add_event_cb(btn_locate, [](lv_event_t* e){
        HaWebsocketLogic_CallVacuumService(cur_vacuum_entity, "locate");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_home = lv_btn_create(panel);
    lv_obj_set_size(btn_home, 280, 60);
    lv_obj_align(btn_home, LV_ALIGN_TOP_RIGHT, -10, btn_y);
    lv_obj_set_style_bg_color(btn_home, lv_color_hex(0x2980B9), 0);
    
    lv_obj_t* lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME " Zurueck zur Basis");
    lv_obj_set_style_text_color(lbl_home, lv_color_white(), 0);
    lv_obj_center(lbl_home);
    
    lv_obj_add_event_cb(btn_home, [](lv_event_t* e){
        HaWebsocketLogic_CallVacuumService(cur_vacuum_entity, "return_to_base");
    }, LV_EVENT_CLICKED, NULL);

    btn_y += 90;
    
    lv_obj_t* lbl_fan = lv_label_create(panel);
    lv_label_set_text(lbl_fan, "Saugkraft:");
    lv_obj_set_style_text_color(lbl_fan, lv_color_white(), 0); 
    lv_obj_align(lbl_fan, LV_ALIGN_TOP_LEFT, 10, btn_y + 10);

    lv_obj_t* dd_fan = lv_dropdown_create(panel);
    lv_dropdown_set_options(dd_fan, "low\nmedium\nhigh\nmax");
    lv_obj_set_width(dd_fan, 450);
    lv_obj_align(dd_fan, LV_ALIGN_TOP_RIGHT, -10, btn_y);
    
    String cur_speed = HaWebsocketLogic_GetFanSpeed(cur_vacuum_entity);
    int sel_idx = 0;
    if (cur_speed == "medium") {
        sel_idx = 1;
    } else if (cur_speed == "high") {
        sel_idx = 2;
    } else if (cur_speed == "max") {
        sel_idx = 3;
    }
    lv_dropdown_set_selected(dd_fan, sel_idx);

    lv_obj_add_event_cb(dd_fan, [](lv_event_t* e) {
        lv_obj_t* dd = lv_event_get_target(e);
        char buf[32];
        lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
        HaWebsocketLogic_CallVacuumSetFanSpeed(cur_vacuum_entity, String(buf));
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl_close, lv_color_white(), 0);
    lv_obj_center(lbl_close);
    
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
        lv_obj_del_async(vacuum_overlay);
        vacuum_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);
}