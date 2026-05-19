#include "ViewQuickSettings.h"
#include "ViewPower.h" // NEU eingebunden
#include "GuiManager.h"
#include "SharedData.h"
#include "LVGL_Driver.h"
#include "UIHelper.h"
#include <M5Unified.h>

static lv_obj_t* quickOverlay = nullptr;
static lv_obj_t* mixer_container = nullptr;

static void brightnessSliderWrapper(lv_event_t * e) {
    brightnessPercent = lv_slider_get_value(lv_event_get_target(e));
    M5.Display.setBrightness((brightnessPercent * 255) / 100);
}

static void qs_slider_release_cb(lv_event_t* e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    float vol = 0.0f;
    if (id == 0 && !muteMaster) vol = volMaster / 100.0f;
    else if (id == 1 && !muteMaster && !muteUI) vol = (volMaster / 100.0f) * (volUI / 100.0f);
    else if (id == 2 && !muteMaster && !muteAlarm) vol = (volMaster / 100.0f) * (volAlarm / 100.0f);
    else if (id == 3 && !muteMaster && !muteBaby) vol = (volMaster / 100.0f) * (volBaby / 100.0f);

    uint8_t chan = (id == 0) ? 0 : (id - 1);
    if (vol > 0.01f) playBingI2S(chan); 
    SharedData::Save();
}

static void create_mixer_row(lv_obj_t* parent, const char* label_txt, int* vol_var, bool* mute_var, int max_vol, int id) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 45);
    lv_obj_set_style_bg_opa(row, 0, 0); 
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_size(slider, 300, 15);
    lv_obj_align(slider, LV_ALIGN_LEFT_MID, 120, 0);
    lv_slider_set_range(slider, 0, max_vol);
    lv_slider_set_value(slider, *vol_var, LV_ANIM_OFF);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(slider, [](lv_event_t* e) {
        int* v = (int*)lv_event_get_user_data(e);
        *v = lv_slider_get_value(lv_event_get_target(e));
    }, LV_EVENT_VALUE_CHANGED, vol_var);
    lv_obj_add_event_cb(slider, qs_slider_release_cb, LV_EVENT_RELEASED, (void*)(intptr_t)id);

    lv_obj_t* btn_mute = lv_btn_create(row);
    lv_obj_set_size(btn_mute, 60, 45);
    lv_obj_align(btn_mute, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_mute, lv_color_hex(*mute_var ? 0xAA0000 : 0x555555), 0);
    lv_obj_t* l_m = lv_label_create(btn_mute);
    lv_label_set_text(l_m, *mute_var ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    lv_obj_center(l_m);
    lv_obj_add_event_cb(btn_mute, [](lv_event_t* e) {
        playToneI2S(800, 15, true); 
        bool* m = (bool*)lv_event_get_user_data(e);
        *m = !(*m);
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_set_style_bg_color(btn, lv_color_hex(*m ? 0xAA0000 : 0x555555), 0);
        lv_label_set_text(lv_obj_get_child(btn, 0), *m ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
        SharedData::Save();
    }, LV_EVENT_CLICKED, mute_var);
}

bool ViewQuickSettings::isActive() {
    return quickOverlay != nullptr;
}

void ViewQuickSettings::hide() {
    if (quickOverlay != nullptr) { 
        lv_obj_del_async(quickOverlay); 
        quickOverlay = nullptr; 
        mixer_container = nullptr;
    }
}

void ViewQuickSettings::toggle(GuiManager* guiManager) {
    if (quickOverlay != nullptr) { 
        hide();
        return; 
    }
    
    quickOverlay = lv_obj_create(lv_disp_get_layer_sys(NULL)); 
    lv_obj_set_size(quickOverlay, 1280, 720); 
    lv_obj_set_style_bg_opa(quickOverlay, 0, 0); 
    lv_obj_set_style_border_width(quickOverlay, 0, 0);
    lv_obj_clear_flag(quickOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(quickOverlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(quickOverlay, [](lv_event_t* e){
        ViewQuickSettings::hide();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(quickOverlay, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, guiManager);

    lv_obj_t* panel = lv_obj_create(quickOverlay);
    lv_obj_set_width(panel, 580);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(panel, 240, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 15, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_row(panel, 15, 0);
    
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE); 

    lv_obj_add_event_cb(panel, [](lv_event_t* e){}, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, "Quick Settings");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);

    // Helligkeit
    lv_obj_t* row_br = lv_obj_create(panel);
    lv_obj_set_size(row_br, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(row_br, 0, 0); lv_obj_set_style_border_width(row_br, 0, 0);
    lv_obj_set_style_pad_all(row_br, 0, 0);
    
    lv_obj_t* br_icon = lv_label_create(row_br);
    lv_label_set_text(br_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(br_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(br_icon, lv_color_white(), 0);
    lv_obj_align(br_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* slider_br = lv_slider_create(row_br);
    lv_obj_set_size(slider_br, 450, 20);
    lv_obj_align(slider_br, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_slider_set_range(slider_br, 5, 100);
    lv_slider_set_value(slider_br, brightnessPercent, LV_ANIM_OFF);
    lv_obj_add_flag(slider_br, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_add_event_cb(slider_br, brightnessSliderWrapper, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(slider_br, [](lv_event_t* e) {
        playToneI2S(1000, 15, true);
        GuiManager * manager = (GuiManager *)lv_event_get_user_data(e);
        manager->preferences.begin("catmat", false);
        manager->preferences.putInt("bright", brightnessPercent);
        manager->preferences.end();
    }, LV_EVENT_RELEASED, guiManager);

    // Master Lautstaerke
    lv_obj_t* row_vol = lv_obj_create(panel);
    lv_obj_set_size(row_vol, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(row_vol, 0, 0); lv_obj_set_style_border_width(row_vol, 0, 0);
    lv_obj_set_style_pad_all(row_vol, 0, 0);

    lv_obj_t* vol_icon = lv_label_create(row_vol);
    lv_label_set_text(vol_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(vol_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(vol_icon, lv_color_white(), 0);
    lv_obj_align(vol_icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* slider_master = lv_slider_create(row_vol);
    lv_obj_set_size(slider_master, 200, 20);
    lv_obj_align(slider_master, LV_ALIGN_LEFT_MID, 60, 0);
    lv_slider_set_range(slider_master, 0, 100);
    lv_slider_set_value(slider_master, volMaster, LV_ANIM_OFF);
    lv_obj_add_flag(slider_master, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(slider_master, [](lv_event_t* e) { volMaster = lv_slider_get_value(lv_event_get_target(e)); }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_master, qs_slider_release_cb, LV_EVENT_RELEASED, (void*)(intptr_t)0);

    lv_obj_t* btn_mute_master = lv_btn_create(row_vol);
    lv_obj_set_size(btn_mute_master, 60, 45);
    lv_obj_align(btn_mute_master, LV_ALIGN_LEFT_MID, 280, 0);
    lv_obj_set_style_bg_color(btn_mute_master, lv_color_hex(muteMaster ? 0xAA0000 : 0x555555), 0);
    lv_obj_t* l_m = lv_label_create(btn_mute_master);
    lv_label_set_text(l_m, muteMaster ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    lv_obj_center(l_m);
    lv_obj_add_event_cb(btn_mute_master, [](lv_event_t* e) {
        playToneI2S(800, 15, true); 
        muteMaster = !muteMaster;
        lv_obj_t* btn = lv_event_get_target(e);
        lv_obj_set_style_bg_color(btn, lv_color_hex(muteMaster ? 0xAA0000 : 0x555555), 0);
        lv_label_set_text(lv_obj_get_child(btn, 0), muteMaster ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
        SharedData::Save();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_mixer = lv_btn_create(row_vol);
    lv_obj_set_size(btn_mixer, 130, 45);
    lv_obj_align(btn_mixer, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_mixer, lv_color_hex(0x8E44AD), 0);
    lv_obj_t* l_mix = lv_label_create(btn_mixer);
    lv_label_set_text(l_mix, LV_SYMBOL_SETTINGS " Mixer");
    lv_obj_center(l_mix);

    // Mixer Container (Versteckt)
    mixer_container = lv_obj_create(panel);
    lv_obj_set_width(mixer_container, LV_PCT(100));
    lv_obj_set_height(mixer_container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(mixer_container, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(mixer_container, 0, 0);
    lv_obj_set_flex_flow(mixer_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(mixer_container, 0, 0);
    lv_obj_set_style_pad_row(mixer_container, 15, 0);
    lv_obj_add_flag(mixer_container, LV_OBJ_FLAG_HIDDEN);
    
    create_mixer_row(mixer_container, "UI", &volUI, &muteUI, 100, 1);
    create_mixer_row(mixer_container, "Alarm", &volAlarm, &muteAlarm, 100, 2);
    create_mixer_row(mixer_container, "Baby", &volBaby, &muteBaby, 150, 3); 

    lv_obj_add_event_cb(btn_mixer, [](lv_event_t* e) {
        playToneI2S(800, 15, true); 
        lv_obj_t* c = (lv_obj_t*)lv_event_get_user_data(e);
        if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(c, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, mixer_container);

    // Action Buttons
    lv_obj_t * row_btns = lv_obj_create(panel);
    lv_obj_set_size(row_btns, LV_PCT(100), 70);
    lv_obj_set_style_bg_opa(row_btns, 0, 0); lv_obj_set_style_border_width(row_btns, 0, 0);
    lv_obj_set_style_pad_all(row_btns, 0, 0);

    lv_obj_t * btn_off = lv_btn_create(row_btns); 
    lv_obj_set_size(btn_off, 140, 60); 
    lv_obj_align(btn_off, LV_ALIGN_LEFT_MID, 40, 0); 
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0x555555), 0); 
    // NEU: Ruft ViewPower anstelle der internen Funktion auf
    lv_obj_add_event_cb(btn_off, ViewPower::btn_screen_off_event_cb, LV_EVENT_CLICKED, guiManager); 
    lv_obj_add_flag(btn_off, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_t * lbl_off = lv_label_create(btn_off); 
    lv_label_set_text(lbl_off, LV_SYMBOL_EYE_CLOSE); 
    lv_obj_set_style_text_font(lbl_off, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_off);

    lv_obj_t * btn_sd = lv_btn_create(row_btns); 
    lv_obj_set_size(btn_sd, 140, 60); 
    lv_obj_align(btn_sd, LV_ALIGN_RIGHT_MID, -40, 0); 
    lv_obj_set_style_bg_color(btn_sd, lv_color_hex(0xAA0000), 0); 
    // NEU: Ruft ViewPower anstelle der internen Funktion auf
    lv_obj_add_event_cb(btn_sd, ViewPower::btn_shutdown_event_cb, LV_EVENT_CLICKED, NULL); 
    lv_obj_add_flag(btn_sd, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_t * lbl_sd = lv_label_create(btn_sd); 
    lv_label_set_text(lbl_sd, LV_SYMBOL_POWER); 
    lv_obj_set_style_text_font(lbl_sd, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_sd);
}