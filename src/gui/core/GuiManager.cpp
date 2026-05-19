#include "GuiManager.h"
#include "ViewDashboard.h"
#include "ViewCatMat.h"
#include "ViewBaby.h"
#include "ViewSettings.h"
#include "LVGL_Driver.h" 
#include "SharedData.h" 
#include <WiFi.h>       
#include <M5Unified.h>
#include "ViewHomeAssistant.h" 
#include "HAWidgets.h"
#include "HaWebsocketLogic.h"

GuiManager gui;

lv_obj_t * splash_status_label = nullptr;

static const lv_point_t walk_pts[] = {{0,0}, {12,36}, {42,36}, {66,24}, {72,9}, {84,9}, {96,27}, {84,42}, {84,72}, {72,72}, {72,54}, {42,54}, {42,72}, {30,72}, {30,54}, {12,36}};
static const lv_point_t sit_pts[] = {{66, 72}, {30, 72}, {30, 48}, {36, 24}, {30, 6}, {39, 15}, {57, 15}, {66, 6}, {60, 24}, {66, 48}, {66, 72}};

static void anim_cat_walk_cb(void * var, int32_t v) { 
    lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0); 
}

static void anim_bar_cb(void * var, int32_t v) { 
    lv_bar_set_value((lv_obj_t*)var, v, LV_ANIM_OFF); 
}

static void anim_zzz_y_cb(void * var, int32_t v) { 
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0); 
}

static void anim_zzz_opa_cb(void * var, int32_t v) { 
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0); 
}

static void anim_walk_ready_cb(lv_anim_t * a) {
    lv_obj_t * cat = (lv_obj_t *)a->var;
    lv_line_set_points(cat, sit_pts, 11);
}

static void splash_timer_cb(lv_timer_t * timer) {
    lv_anim_del_all(); 
    splash_status_label = nullptr; 
    gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE);
}

static void btn_shutdown_event_cb(lv_event_t * e) { 
    playToneI2S(600, 100, true);
    intentionalDisconnect = true; 
    
    WiFi.disconnect(true, true); 
    WiFi.mode(WIFI_OFF); 
    
    lv_obj_t * sd_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sd_scr, lv_color_hex(0x000000), 0);
    
    lv_obj_t * lbl = lv_label_create(sd_scr); 
    lv_label_set_text(lbl, "Geraet wird heruntergefahren...\n\n(Power-Taste lange drücken \nzum Aufwecken)"); 
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl); 
    
    lv_scr_load_anim(sd_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    lv_timer_handler(); 
    delay(1500); 
    
    M5.Power.powerOff(); 
}

static lv_obj_t * screen_off_overlay = nullptr;

static void screen_off_click_cb(lv_event_t * e) {
    M5.Display.setBrightness((brightnessPercent * 255) / 100);
    
    if (screen_off_overlay != nullptr) {
        lv_obj_del_async(screen_off_overlay); 
        screen_off_overlay = nullptr;
    }
}

static void btn_screen_off_event_cb(lv_event_t * e) {
    if (screen_off_overlay != nullptr) {
        return;
    }
    
    screen_off_overlay = lv_obj_create(lv_disp_get_layer_sys(NULL));
    lv_obj_set_size(screen_off_overlay, 1280, 720);
    lv_obj_set_style_bg_color(screen_off_overlay, lv_color_black(), 0); 
    lv_obj_set_style_bg_opa(screen_off_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen_off_overlay, 0, 0);
    
    lv_obj_add_flag(screen_off_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_off_overlay, screen_off_click_cb, LV_EVENT_PRESSED, NULL);

    M5.Display.setBrightness(0);

    GuiManager * manager = (GuiManager *)lv_event_get_user_data(e);
    manager->toggleQuickOverlay(); 
}

static void alarm_wake_timer_cb(lv_timer_t * timer) {
    if (screen_off_overlay != nullptr) {
        if (alarmActive || babyAlarmActive || disconnectAlarmActive) {
            M5.Display.setBrightness((brightnessPercent * 255) / 100);
            lv_obj_del_async(screen_off_overlay);
            screen_off_overlay = nullptr;
        }
    }
}

void GuiManager::init() {
    currentScreen = (ScreenID)99; 

    lv_timer_create(alarm_wake_timer_cb, 500, NULL);

    lv_obj_t * splash_screen = lv_obj_create(NULL); 
    if (!splash_screen) {
        return; 
    }

    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x111111), 0); 
    lv_obj_set_style_border_width(splash_screen, 0, 0);
    lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0); 

    lv_obj_t * splash_title = lv_label_create(splash_screen); 
    lv_label_set_text(splash_title, "Lela OS 2"); 
    lv_obj_set_style_text_font(splash_title, &lv_font_montserrat_36, 0); 
    lv_obj_set_style_text_color(splash_title, lv_color_hex(0x00A0FF), 0); 
    lv_obj_align(splash_title, LV_ALIGN_TOP_MID, 0, 60); 

    lv_obj_t * splash_bar = lv_bar_create(splash_screen); 
    lv_obj_set_size(splash_bar, 600, 20); 
    lv_obj_align(splash_bar, LV_ALIGN_CENTER, 0, 10); 
    lv_bar_set_range(splash_bar, 0, 100); 
    lv_obj_set_style_bg_color(splash_bar, lv_color_hex(0x00A0FF), LV_PART_INDICATOR);

    splash_status_label = lv_label_create(splash_screen);
    lv_obj_set_style_text_font(splash_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(splash_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(splash_status_label, "Starte Hintergrunddienste...");
    lv_obj_align(splash_status_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t * splash_mat = lv_obj_create(splash_screen); 
    lv_obj_set_size(splash_mat, 180, 12); 
    lv_obj_set_style_bg_color(splash_mat, lv_color_hex(0x555555), 0); 
    lv_obj_set_style_border_width(splash_mat, 0, 0); 
    lv_obj_set_style_radius(splash_mat, 6, 0); 
    lv_obj_align(splash_mat, LV_ALIGN_CENTER, 180, 0); 

    lv_color_t baby_col = lv_color_hex(0x4FA5D6); 
    lv_obj_t * baby_cont = lv_obj_create(splash_screen);
    lv_obj_set_size(baby_cont, 180, 210); 
    lv_obj_align(baby_cont, LV_ALIGN_BOTTOM_MID, 0, -90); 
    lv_obj_set_style_bg_opa(baby_cont, 0, 0); 
    lv_obj_set_style_border_width(baby_cont, 0, 0);
    lv_obj_set_style_pad_all(baby_cont, 0, 0);

    static const lv_point_t body_pts[] = {{45, 90}, {135, 90}, {150, 135}, {135, 180}, {45, 180}, {30, 135}, {45, 90}};
    lv_obj_t * l_body = lv_line_create(baby_cont); 
    lv_line_set_points(l_body, body_pts, 7); 
    lv_obj_set_style_line_width(l_body, 9, 0); 
    lv_obj_set_style_line_color(l_body, baby_col, 0); 
    lv_obj_set_style_line_rounded(l_body, true, 0);
    
    static const lv_point_t head_pts[] = {{60, 45}, {120, 45}, {135, 66}, {120, 90}, {60, 90}, {45, 66}, {60, 45}};
    lv_obj_t * l_head = lv_line_create(baby_cont); 
    lv_line_set_points(l_head, head_pts, 7); 
    lv_obj_set_style_line_width(l_head, 6, 0); 
    lv_obj_set_style_line_color(l_head, lv_color_white(), 0);
    
    static const lv_point_t eye_l_pts[] = {{69, 66}, {75, 72}, {81, 66}};
    static const lv_point_t eye_r_pts[] = {{99, 66}, {105, 72}, {111, 66}};
    lv_obj_t * l_e_l = lv_line_create(baby_cont); 
    lv_line_set_points(l_e_l, eye_l_pts, 3); 
    lv_obj_set_style_line_width(l_e_l, 3, 0); 
    lv_obj_set_style_line_color(l_e_l, lv_color_white(), 0);
    lv_obj_t * l_e_r = lv_line_create(baby_cont); 
    lv_line_set_points(l_e_r, eye_r_pts, 3); 
    lv_obj_set_style_line_width(l_e_r, 3, 0); 
    lv_obj_set_style_line_color(l_e_r, lv_color_white(), 0);
    
    static const lv_point_t paci_pts[] = {{84, 78}, {96, 78}, {96, 84}, {84, 84}, {84, 78}}; 
    static const lv_point_t paci_handle[] = {{87, 84}, {93, 84}, {93, 90}, {87, 90}, {87, 84}}; 
    lv_obj_t * l_paci = lv_line_create(baby_cont); 
    lv_line_set_points(l_paci, paci_pts, 5); 
    lv_obj_set_style_line_width(l_paci, 6, 0); 
    lv_obj_set_style_line_color(l_paci, lv_color_hex(0xE67E22), 0); 
    lv_obj_t * l_paci_h = lv_line_create(baby_cont); 
    lv_line_set_points(l_paci_h, paci_handle, 5); 
    lv_obj_set_style_line_width(l_paci_h, 6, 0); 
    lv_obj_set_style_line_color(l_paci_h, lv_color_white(), 0);
    
    lv_obj_t * lbl_zzz = lv_label_create(splash_screen); 
    lv_label_set_text(lbl_zzz, "zzzZZZ"); 
    lv_obj_set_style_text_color(lbl_zzz, lv_color_white(), 0); 
    lv_obj_align_to(lbl_zzz, baby_cont, LV_ALIGN_TOP_MID, 30, 15); 
    
    lv_anim_t a_zzz_y; 
    lv_anim_init(&a_zzz_y); 
    lv_anim_set_var(&a_zzz_y, lbl_zzz); 
    lv_anim_set_values(&a_zzz_y, 0, -90); 
    lv_anim_set_time(&a_zzz_y, 1500); 
    lv_anim_set_exec_cb(&a_zzz_y, anim_zzz_y_cb); 
    lv_anim_set_path_cb(&a_zzz_y, lv_anim_path_ease_out); 
    lv_anim_set_delay(&a_zzz_y, 500); 
    lv_anim_set_repeat_count(&a_zzz_y, 3); 
    lv_anim_start(&a_zzz_y);
    
    lv_anim_t a_zzz_opa; 
    lv_anim_init(&a_zzz_opa); 
    lv_anim_set_var(&a_zzz_opa, lbl_zzz); 
    lv_anim_set_values(&a_zzz_opa, 255, 0); 
    lv_anim_set_time(&a_zzz_opa, 1500); 
    lv_anim_set_exec_cb(&a_zzz_opa, anim_zzz_opa_cb); 
    lv_anim_set_path_cb(&a_zzz_opa, lv_anim_path_linear); 
    lv_anim_set_delay(&a_zzz_opa, 500); 
    lv_anim_set_repeat_count(&a_zzz_opa, 3); 
    lv_anim_start(&a_zzz_opa);

    lv_obj_t * cat_walker = lv_line_create(splash_screen); 
    lv_line_set_points(cat_walker, walk_pts, 16); 
    lv_obj_set_style_line_width(cat_walker, 9, 0); 
    lv_obj_set_style_line_color(cat_walker, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_set_style_line_rounded(cat_walker, true, 0); 
    
    // --- DER FIX: Offset von -66 auf -42 geaendert, damit die Katze perfekt sitzt! ---
    lv_obj_align(cat_walker, LV_ALIGN_CENTER, -240, -42); 

    lv_anim_t a_walk; 
    lv_anim_init(&a_walk); 
    lv_anim_set_var(&a_walk, cat_walker); 
    lv_anim_set_values(&a_walk, 0, 405); 
    lv_anim_set_time(&a_walk, 2500); 
    lv_anim_set_exec_cb(&a_walk, anim_cat_walk_cb); 
    lv_anim_set_path_cb(&a_walk, lv_anim_path_linear); 
    lv_anim_set_ready_cb(&a_walk, anim_walk_ready_cb);
    lv_anim_start(&a_walk);

    lv_anim_t a_bar; 
    lv_anim_init(&a_bar); 
    lv_anim_set_var(&a_bar, splash_bar); 
    lv_anim_set_values(&a_bar, 0, 100); 
    lv_anim_set_time(&a_bar, 4500); 
    lv_anim_set_exec_cb(&a_bar, anim_bar_cb); 
    lv_anim_start(&a_bar);

    lv_scr_load(splash_screen);

    lv_timer_t * t = lv_timer_create(splash_timer_cb, 4600, NULL);
    lv_timer_set_repeat_count(t, 1);
}

ScreenID GuiManager::getCurrentScreen() const { 
    return currentScreen; 
}

void GuiManager::switchScreen(ScreenID newScreen, lv_scr_load_anim_t anim_type) {
    if (currentScreen == newScreen) {
        return;
    }

    if (currentScreen == SCREEN_BABY) {
        requestBabyStream = false;
    }

    if (newScreen == SCREEN_HA) {
        HaWebsocketLogic_Start();
    } else if (currentScreen == SCREEN_HA && newScreen != SCREEN_HA) {
        ViewHomeAssistant::clearWidgets();
        HaWebsocketLogic_Stop(); 
    }

    lv_obj_t* nextScr = nullptr;
    switch(newScreen) {
        case SCREEN_DASHBOARD: 
            nextScr = ViewDashboard::build(); 
            break;
        case SCREEN_CATMAT:    
            nextScr = ViewCatMat::build(); 
            break;
        case SCREEN_BABY:      
            nextScr = ViewBaby::build(); 
            break;
        case SCREEN_SETTINGS:  
            nextScr = ViewSettings::build(); 
            break;
        case SCREEN_HA:        
            nextScr = ViewHomeAssistant::build(); 
            break;
    }
    
    if (nextScr != nullptr) {
        lv_scr_load_anim(nextScr, anim_type, 0, 0, true);
        currentScreen = newScreen;
    }
}

void GuiManager::toggleQuickOverlay() {
    if (quickOverlay != nullptr) { 
        lv_obj_del_async(quickOverlay); 
        quickOverlay = nullptr; 
        return; 
    }
    
    quickOverlay = lv_obj_create(lv_disp_get_layer_sys(NULL)); 
    
    lv_obj_set_size(quickOverlay, 1280, 720); 
    lv_obj_set_style_bg_opa(quickOverlay, 0, 0); 
    lv_obj_set_style_border_width(quickOverlay, 0, 0);
    lv_obj_clear_flag(quickOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(quickOverlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(quickOverlay, [](lv_event_t* e){
        GuiManager * manager = (GuiManager *)lv_event_get_user_data(e);
        manager->toggleQuickOverlay();
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(quickOverlay, gestureEventWrapper, LV_EVENT_GESTURE, this);

    lv_obj_t* panel = lv_obj_create(quickOverlay);
    lv_obj_set_width(panel, 600);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(panel, 240, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 15, 0);
    
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_row(panel, 15, 0);

    lv_obj_add_event_cb(panel, [](lv_event_t* e){}, LV_EVENT_CLICKED, NULL);

    static bool mixerExpanded = false;

    lv_obj_t* title_vol = lv_label_create(panel); 
    lv_label_set_text(title_vol, "System-Lautstaerke"); 
    lv_obj_set_style_text_font(title_vol, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_vol, lv_color_white(), 0);

    lv_obj_t* row_master = lv_obj_create(panel);
    lv_obj_set_size(row_master, 560, 50);
    lv_obj_set_style_bg_opa(row_master, 0, 0);
    lv_obj_set_style_border_width(row_master, 0, 0);
    lv_obj_set_style_pad_all(row_master, 0, 0);
    lv_obj_clear_flag(row_master, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_mute_master = lv_btn_create(row_master);
    lv_obj_set_size(btn_mute_master, 50, 50);
    lv_obj_align(btn_mute_master, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_mute_master, lv_color_hex(muteMaster ? 0xAA0000 : 0x555555), 0);
    lv_obj_t* lbl_mute_master = lv_label_create(btn_mute_master);
    lv_label_set_text(lbl_mute_master, muteMaster ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    lv_obj_center(lbl_mute_master);
    lv_obj_add_event_cb(btn_mute_master, [](lv_event_t* e){
        playToneI2S(800, 15, true);
        muteMaster = !muteMaster;
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_set_style_bg_color(btn, lv_color_hex(muteMaster ? 0xAA0000 : 0x555555), 0);
        lv_label_set_text(lv_obj_get_child(btn, 0), muteMaster ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
        SharedData::Save();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* slider_master = lv_slider_create(row_master);
    lv_obj_set_size(slider_master, 370, 20);
    lv_obj_align(slider_master, LV_ALIGN_LEFT_MID, 65, 0);
    lv_slider_set_range(slider_master, 0, 100);
    lv_slider_set_value(slider_master, volMaster, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_master, [](lv_event_t* e){
        volMaster = lv_slider_get_value(lv_event_get_target(e));
        SharedData::Save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(slider_master, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* btn_mixer = lv_btn_create(row_master);
    lv_obj_set_size(btn_mixer, 100, 50);
    lv_obj_align(btn_mixer, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_mixer, lv_color_hex(0x00A0FF), 0);
    lv_obj_t* lbl_mixer = lv_label_create(btn_mixer);
    lv_label_set_text(lbl_mixer, "Mixer " LV_SYMBOL_DOWN);
    lv_obj_center(lbl_mixer);

    lv_obj_t* container_slaves = lv_obj_create(panel);
    lv_obj_set_size(container_slaves, 560, 160);
    lv_obj_set_style_bg_color(container_slaves, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(container_slaves, 0, 0);
    lv_obj_set_style_radius(container_slaves, 10, 0);
    lv_obj_set_style_pad_all(container_slaves, 5, 0);
    lv_obj_clear_flag(container_slaves, LV_OBJ_FLAG_SCROLLABLE);
    if (!mixerExpanded) lv_obj_add_flag(container_slaves, LV_OBJ_FLAG_HIDDEN);

    auto create_slave_row = [](lv_obj_t* parent, int y_ofs, const char* name, int* vol_var, bool* mute_var) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_set_size(row, 540, 45);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y_ofs);
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* btn_mute = lv_btn_create(row);
        lv_obj_set_size(btn_mute, 45, 45);
        lv_obj_align(btn_mute, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(btn_mute, lv_color_hex(*mute_var ? 0xAA0000 : 0x555555), 0);
        lv_obj_t* lbl_mute = lv_label_create(btn_mute);
        lv_label_set_text(lbl_mute, *mute_var ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
        lv_obj_center(lbl_mute);
        
        lv_obj_add_event_cb(btn_mute, [](lv_event_t* e){
            bool* m = (bool*)lv_event_get_user_data(e);
            *m = !(*m);
            playToneI2S(800, 15, true);
            lv_obj_t* btn = lv_event_get_target(e);
            lv_obj_set_style_bg_color(btn, lv_color_hex(*m ? 0xAA0000 : 0x555555), 0);
            lv_label_set_text(lv_obj_get_child(btn, 0), *m ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
            SharedData::Save();
        }, LV_EVENT_CLICKED, mute_var);

        lv_obj_t* lbl_name = lv_label_create(row);
        lv_label_set_text(lbl_name, name);
        lv_obj_set_style_text_color(lbl_name, lv_color_white(), 0);
        lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 55, 0);

        lv_obj_t* slider = lv_slider_create(row);
        lv_obj_set_size(slider, 300, 20);
        lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, *vol_var, LV_ANIM_OFF);
        lv_obj_add_flag(slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
        
        lv_obj_add_event_cb(slider, [](lv_event_t* e){
            int* v = (int*)lv_event_get_user_data(e);
            *v = lv_slider_get_value(lv_event_get_target(e));
            SharedData::Save();
        }, LV_EVENT_VALUE_CHANGED, vol_var);
    };

    create_slave_row(container_slaves, 5, "UI", &volUI, &muteUI);
    create_slave_row(container_slaves, 55, "Alarm", &volAlarm, &muteAlarm);
    create_slave_row(container_slaves, 105, "Baby", &volBaby, &muteBaby);

    lv_obj_add_event_cb(btn_mixer, [](lv_event_t* e){
        playToneI2S(800, 15, true);
        lv_obj_t* container = (lv_obj_t*)lv_event_get_user_data(e);
        mixerExpanded = !mixerExpanded;
        if (mixerExpanded) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, container_slaves);

    lv_obj_t* title_br = lv_label_create(panel); 
    lv_label_set_text(title_br, "Helligkeit"); 
    lv_obj_set_style_text_font(title_br, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_br, lv_color_white(), 0);

    lv_obj_t* row_br = lv_obj_create(panel);
    lv_obj_set_size(row_br, 560, 40);
    lv_obj_set_style_bg_opa(row_br, 0, 0);
    lv_obj_set_style_border_width(row_br, 0, 0);
    lv_obj_set_style_pad_all(row_br, 0, 0);
    lv_obj_clear_flag(row_br, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* slider_br = lv_slider_create(row_br); 
    lv_obj_set_size(slider_br, 450, 20); 
    lv_obj_align(slider_br, LV_ALIGN_CENTER, 0, 0); 
    lv_slider_set_range(slider_br, 1, 100); 
    lv_slider_set_value(slider_br, brightnessPercent, LV_ANIM_OFF); 
    lv_obj_add_flag(slider_br, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_add_event_cb(slider_br, [](lv_event_t* e) {
        lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
        brightnessPercent = lv_slider_get_value(slider);
        M5.Display.setBrightness((brightnessPercent * 255) / 100); 
    }, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_add_event_cb(slider_br, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        GuiManager * manager = (GuiManager *)lv_event_get_user_data(e);
        manager->preferences.begin("catmat", false);
        manager->preferences.putInt("bright", brightnessPercent);
        manager->preferences.end();
    }, LV_EVENT_RELEASED, this);

    lv_obj_t* bottom_btns = lv_obj_create(panel);
    lv_obj_set_size(bottom_btns, 560, 70);
    lv_obj_set_style_bg_opa(bottom_btns, 0, 0);
    lv_obj_set_style_border_width(bottom_btns, 0, 0);
    lv_obj_set_style_pad_all(bottom_btns, 0, 0);
    lv_obj_clear_flag(bottom_btns, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_off = lv_btn_create(bottom_btns); 
    lv_obj_set_size(btn_off, 140, 60); 
    lv_obj_align(btn_off, LV_ALIGN_LEFT_MID, 80, 0); 
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0x555555), 0); 
    lv_obj_add_event_cb(btn_off, btn_screen_off_event_cb, LV_EVENT_CLICKED, this); 
    lv_obj_add_flag(btn_off, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_t * lbl_off = lv_label_create(btn_off); 
    lv_label_set_text(lbl_off, LV_SYMBOL_EYE_CLOSE); 
    lv_obj_set_style_text_font(lbl_off, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_off);

    lv_obj_t * btn_sd = lv_btn_create(bottom_btns); 
    lv_obj_set_size(btn_sd, 140, 60); 
    lv_obj_align(btn_sd, LV_ALIGN_RIGHT_MID, -80, 0); 
    lv_obj_set_style_bg_color(btn_sd, lv_color_hex(0xAA0000), 0); 
    lv_obj_add_event_cb(btn_sd, btn_shutdown_event_cb, LV_EVENT_CLICKED, NULL); 
    lv_obj_add_flag(btn_sd, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_t * lbl_sd = lv_label_create(btn_sd); 
    lv_label_set_text(lbl_sd, LV_SYMBOL_POWER); 
    lv_obj_set_style_text_font(lbl_sd, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_sd);
}

void GuiManager::volumeSliderWrapper(lv_event_t * e) {
    // Unused
}

void GuiManager::gestureEventWrapper(lv_event_t * e) { 
    GuiManager * manager = (GuiManager *)lv_event_get_user_data(e); 
    manager->handleGesture(e); 
}

void GuiManager::handleGesture(lv_event_t * e) {
    if (vidFSMode) {
        return;
    }
    
    if (currentScreen == SCREEN_HA && HAWidget::editModeActive) {
        return; 
    }
    
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    
    if (quickOverlay != nullptr && dir == LV_DIR_TOP) { 
        toggleQuickOverlay(); 
        return; 
    }
    
    if (quickOverlay == nullptr) {
        if (currentScreen == SCREEN_DASHBOARD) {
            if (dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_CATMAT, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_RIGHT) {
                switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_TOP) {
                switchScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_BOTTOM) {
                toggleQuickOverlay();
            }
        } 
        else if (currentScreen == SCREEN_CATMAT) { 
            if (dir == LV_DIR_RIGHT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                toggleQuickOverlay(); 
            }
        }
        else if (currentScreen == SCREEN_BABY) { 
            if (dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                toggleQuickOverlay(); 
            }
        }
        else if (currentScreen == SCREEN_SETTINGS) { 
            if (dir == LV_DIR_RIGHT || dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
        }
        else if (currentScreen == SCREEN_HA) { 
            if (dir == LV_DIR_RIGHT || dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                toggleQuickOverlay(); 
            }
        }
    }
}
