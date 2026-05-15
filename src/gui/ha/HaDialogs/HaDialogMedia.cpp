#include "HaDialogMedia.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h" // Fuer das Hinzufuegen von Widgets
#include "SharedData.h"

static lv_obj_t* media_control_overlay = nullptr;
static lv_obj_t* cur_media_slider = nullptr;
static String cur_media_entity = "";

static lv_obj_t* browser_overlay = nullptr;
static lv_obj_t* browser_list = nullptr;
static lv_obj_t* browser_spinner = nullptr;
static lv_obj_t* browser_lbl_error = nullptr;

void HaDialogMedia::resetState() {
    media_control_overlay = nullptr;
    cur_media_slider = nullptr;
    browser_overlay = nullptr;
    browser_list = nullptr;
    browser_spinner = nullptr;
    browser_lbl_error = nullptr;
}

static void buildBrowserList() {
    if (!browser_list) return;
    lv_obj_clean(browser_list); 

    // Der neue "Home/Root" Button ganz oben
    lv_obj_t* row_back = lv_obj_create(browser_list);
    lv_obj_set_width(row_back, lv_pct(100));
    lv_obj_set_height(row_back, 80);
    lv_obj_set_style_bg_color(row_back, lv_color_hex(0x2980B9), 0); 
    lv_obj_set_style_border_width(row_back, 0, 0);
    lv_obj_clear_flag(row_back, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t* lbl_back = lv_label_create(row_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_HOME " Hauptverzeichnis (Root)");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_back, lv_color_white(), 0);
    lv_obj_center(lbl_back);

    lv_obj_add_flag(row_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_back, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        lv_obj_add_flag(browser_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
        HaWebsocketLogic_RequestBrowseMedia(cur_media_entity); 
    }, LV_EVENT_CLICKED, NULL);

    // Listenelemente
    for (int i = 0; i < currentMediaFolder.size(); i++) {
        const auto& item = currentMediaFolder[i];
        
        lv_obj_t* row = lv_obj_create(browser_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 80); 
        lv_obj_set_style_bg_color(row, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        const char* icon = LV_SYMBOL_FILE;
        if (item.can_expand) icon = LV_SYMBOL_DIRECTORY;
        else if (item.can_play) icon = LV_SYMBOL_PLAY;

        // Der linke Bereich (Klickbar für Ordner oeffnen / Abspielen)
        lv_obj_t* btn_main = lv_btn_create(row);
        lv_obj_set_size(btn_main, item.can_play ? 580 : 680, 80); 
        lv_obj_align(btn_main, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_opa(btn_main, 0, 0); 
        lv_obj_set_style_shadow_width(btn_main, 0, 0);
        
        lv_obj_t* lbl_main = lv_label_create(btn_main);
        lv_label_set_text_fmt(lbl_main, "%s  %s", icon, item.title.c_str());
        lv_obj_set_style_text_font(lbl_main, &lv_font_montserrat_24, 0);
        lv_obj_align(lbl_main, LV_ALIGN_LEFT_MID, 10, 0);
        lv_label_set_long_mode(lbl_main, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_main, 540); 
        
        lv_obj_set_user_data(btn_main, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn_main, [](lv_event_t* e) {
            playToneI2S(1000, 50, true);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx >= 0 && idx < currentMediaFolder.size()) {
                const auto& selItem = currentMediaFolder[idx];
                if (selItem.can_expand) {
                    lv_obj_add_flag(browser_list, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
                    HaWebsocketLogic_RequestBrowseMedia(cur_media_entity, selItem.media_content_type, selItem.media_content_id);
                } 
                else if (selItem.can_play) {
                    HaWebsocketLogic_CallPlayMedia(cur_media_entity, selItem.media_content_type, selItem.media_content_id);
                    lv_obj_del_async(browser_overlay);
                    browser_overlay = nullptr;
                }
            }
        }, LV_EVENT_CLICKED, NULL);

        // Das Widget-Plus (Nur anzeigen, wenn es abspielbar ist)
        if (item.can_play) {
            lv_obj_t* btn_add = lv_btn_create(row);
            lv_obj_set_size(btn_add, 80, 60);
            lv_obj_align(btn_add, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_bg_color(btn_add, lv_color_hex(0x8E44AD), 0); 
            lv_obj_t* lbl_add = lv_label_create(btn_add);
            lv_label_set_text(lbl_add, LV_SYMBOL_PLUS);
            lv_obj_set_style_text_font(lbl_add, &lv_font_montserrat_28, 0);
            lv_obj_center(lbl_add);

            lv_obj_set_user_data(btn_add, (void*)(intptr_t)i);
            lv_obj_add_event_cb(btn_add, [](lv_event_t* e) {
                // Nur ein Bestätigungston! Der Browser bleibt offen.
                playToneI2S(1500, 80, true); 
                int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                
                if (idx >= 0 && idx < currentMediaFolder.size()) {
                    const auto& selItem = currentMediaFolder[idx];
                    
                    uint16_t act_tab = ViewHomeAssistant::currentActiveTab;
                    if (act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
                    
                    HAWidgetDef def;
                    def.entity_id = cur_media_entity; // Es zielt auf den Player
                    def.type = "media_item"; // Der neue Widget Typ
                    def.x = 20; def.y = 20; def.w = 160; def.h = 105;
                    def.name = selItem.title; 
                    def.mdi_icon = ""; // Wird automatisch Play
                    def.color_on = "";
                    def.color_off = "";
                    def.media_content_type = selItem.media_content_type;
                    def.media_content_id = selItem.media_content_id;
                    
                    HaConfigLogic::dashboards[act_tab].widgets.push_back(def);
                    HaConfigLogic::Save();
                    HaWebsocketLogic_UpdateTrackedEntities();
                    
                    // Dashboard im Hintergrund neuladen, waehrend Browser offen bleibt
                    ViewHomeAssistant::pendingHaReload = true; 
                }
            }, LV_EVENT_CLICKED, NULL);
        }
    }
}

void HaDialogMedia::update() {
    if (browser_overlay != nullptr) {
        if (pendingMediaBrowserUpdate) {
            pendingMediaBrowserUpdate = false;
            lv_obj_add_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(browser_list, LV_OBJ_FLAG_HIDDEN);
            buildBrowserList();
        }
        if (pendingMediaBrowserError) {
            pendingMediaBrowserError = false;
            lv_obj_add_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(browser_lbl_error, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void showBrowserOverlay() {
    if (browser_overlay != nullptr) return;
    
    HaWebsocketLogic_RequestBrowseMedia(cur_media_entity);

    browser_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(browser_overlay, 1280, 720);
    lv_obj_set_style_bg_color(browser_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(browser_overlay, 230, 0); 
    lv_obj_add_flag(browser_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* panel = lv_obj_create(browser_overlay);
    lv_obj_set_size(panel, 800, 600);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    
    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, "Medien Browser");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);

    // NEU: Separater Schliessen-Button (oben rechts)
    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
        lv_obj_del_async(browser_overlay); 
        browser_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);

    browser_spinner = lv_spinner_create(panel, 1000, 60);
    lv_obj_set_size(browser_spinner, 80, 80);
    lv_obj_align(browser_spinner, LV_ALIGN_CENTER, 0, 0);

    browser_lbl_error = lv_label_create(panel);
    lv_label_set_text(browser_lbl_error, "Keine Medien verfuegbar.");
    lv_obj_set_style_text_color(browser_lbl_error, lv_color_hex(0xAA0000), 0);
    lv_obj_align(browser_lbl_error, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(browser_lbl_error, LV_OBJ_FLAG_HIDDEN);

    // Die Liste (wird durch buildBrowserList befuellt)
    browser_list = lv_list_create(panel);
    lv_obj_set_size(browser_list, 750, 480);
    lv_obj_align(browser_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(browser_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_all(browser_list, 10, 0);
    lv_obj_add_flag(browser_list, LV_OBJ_FLAG_HIDDEN);
}

void HaDialogMedia::showMediaControlDialog(HAWidget* w) {
    if (media_control_overlay != nullptr) return; 

    playToneI2S(800, 100, true);
    cur_media_entity = w->getEntityId();

    String title = HaWebsocketLogic_GetMediaTitle(cur_media_entity);
    String artist = HaWebsocketLogic_GetMediaArtist(cur_media_entity);
    float vol = HaWebsocketLogic_GetMediaVolume(cur_media_entity);
    std::vector<String> sources = HaWebsocketLogic_GetMediaSourceList(cur_media_entity);
    String current_source = HaWebsocketLogic_GetMediaSource(cur_media_entity);
    
    if (title.length() == 0) title = "Kein Titel aktiv";
    if (artist.length() == 0) artist = "Unbekannt";
    if (vol < 0) vol = 0.5f; 

    media_control_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(media_control_overlay, 1280, 720);
    lv_obj_set_style_bg_color(media_control_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(media_control_overlay, 180, 0); 
    lv_obj_add_flag(media_control_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(media_control_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* panel = lv_obj_create(media_control_overlay);
    
    lv_obj_set_size(panel, 650, sources.size() > 0 ? 580 : 480);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* lbl_header = lv_label_create(panel);
    lv_label_set_text(lbl_header, w->getName().c_str());
    lv_obj_set_style_text_font(lbl_header, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_header, lv_color_white(), 0);
    lv_obj_align(lbl_header, LV_ALIGN_TOP_MID, 0, 10);

    // FIX: Power Button Oben Links platziert
    lv_obj_t* btn_power = lv_btn_create(panel);
    lv_obj_set_size(btn_power, 60, 50);
    lv_obj_align(btn_power, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_power, lv_color_hex(0xE74C3C), 0);
    lv_obj_t* lbl_power = lv_label_create(btn_power);
    lv_label_set_text(lbl_power, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(lbl_power, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_power);
    lv_obj_add_event_cb(btn_power, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        HaWebsocketLogic_CallService("media_player", "toggle", cur_media_entity);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
        lv_obj_del_async(media_control_overlay); 
        media_control_overlay = nullptr;
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title.c_str());
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x00A0FF), 0);
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_title, 450); 
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 70);

    lv_obj_t* lbl_artist = lv_label_create(panel);
    lv_label_set_text(lbl_artist, artist.c_str());
    lv_obj_set_style_text_font(lbl_artist, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_artist, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_artist, LV_ALIGN_TOP_MID, 0, 110);

    lv_obj_t* btn_prev = lv_btn_create(panel);
    lv_obj_set_size(btn_prev, 90, 70);
    lv_obj_align(btn_prev, LV_ALIGN_TOP_MID, -165, 160);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x333333), 0);
    lv_obj_t* lbl_prev = lv_label_create(btn_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(lbl_prev, &lv_font_montserrat_28, 0);
    lv_obj_center(lbl_prev);
    lv_obj_add_event_cb(btn_prev, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        HaWebsocketLogic_CallMediaService(cur_media_entity, "media_previous_track");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_play = lv_btn_create(panel);
    lv_obj_set_size(btn_play, 100, 70);
    lv_obj_align(btn_play, LV_ALIGN_TOP_MID, -55, 160);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x27AE60), 0);
    lv_obj_t* lbl_play = lv_label_create(btn_play);
    lv_label_set_text(lbl_play, LV_SYMBOL_PLAY " / " LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(lbl_play, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_play);
    lv_obj_add_event_cb(btn_play, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        HaWebsocketLogic_CallMediaService(cur_media_entity, "media_play_pause");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_stop = lv_btn_create(panel);
    lv_obj_set_size(btn_stop, 100, 70);
    lv_obj_align(btn_stop, LV_ALIGN_TOP_MID, 55, 160);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0xE67E22), 0); 
    lv_obj_t* lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(lbl_stop, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_stop);
    lv_obj_add_event_cb(btn_stop, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        HaWebsocketLogic_CallMediaService(cur_media_entity, "media_stop");
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_next = lv_btn_create(panel);
    lv_obj_set_size(btn_next, 90, 70);
    lv_obj_align(btn_next, LV_ALIGN_TOP_MID, 165, 160);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x333333), 0);
    lv_obj_t* lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(lbl_next, &lv_font_montserrat_28, 0);
    lv_obj_center(lbl_next);
    lv_obj_add_event_cb(btn_next, [](lv_event_t* e) {
        playToneI2S(1000, 50, true);
        HaWebsocketLogic_CallMediaService(cur_media_entity, "media_next_track");
    }, LV_EVENT_CLICKED, NULL);

    int y_offset = 260;
    
    if (sources.size() > 0) {
        lv_obj_t* dd_source = lv_dropdown_create(panel);
        lv_obj_set_size(dd_source, 500, 45);
        lv_obj_align(dd_source, LV_ALIGN_TOP_MID, 0, y_offset);
        lv_obj_set_style_text_font(dd_source, &lv_font_montserrat_20, 0);
        
        String opts = "";
        int sel_idx = 0;
        for (int i = 0; i < sources.size(); i++) {
            opts += sources[i];
            if (i < sources.size() - 1) opts += "\n";
            if (sources[i] == current_source) sel_idx = i;
        }
        lv_dropdown_set_options(dd_source, opts.c_str());
        lv_dropdown_set_selected(dd_source, sel_idx);

        lv_obj_add_event_cb(dd_source, [](lv_event_t* e){
            lv_obj_t* dd = lv_event_get_target(e);
            char buf[128];
            lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
            HaWebsocketLogic_CallMediaSelectSource(cur_media_entity, String(buf));
        }, LV_EVENT_VALUE_CHANGED, NULL);

        y_offset += 65;
    }

    lv_obj_t* btn_browse = lv_btn_create(panel);
    lv_obj_set_size(btn_browse, 500, 50);
    lv_obj_align(btn_browse, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_set_style_bg_color(btn_browse, lv_color_hex(0x2980B9), 0);
    lv_obj_t* lbl_browse = lv_label_create(btn_browse);
    lv_label_set_text(lbl_browse, LV_SYMBOL_DIRECTORY " Medien durchsuchen");
    lv_obj_set_style_text_font(lbl_browse, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_browse);
    lv_obj_add_event_cb(btn_browse, [](lv_event_t* e) {
        playToneI2S(800, 100, true);
        showBrowserOverlay();
    }, LV_EVENT_CLICKED, NULL);

    y_offset += 75;

    lv_obj_t* lbl_vol = lv_label_create(panel);
    lv_label_set_text(lbl_vol, "Lautstaerke");
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_vol, LV_ALIGN_TOP_MID, 0, y_offset);

    cur_media_slider = lv_slider_create(panel);
    lv_slider_set_range(cur_media_slider, 0, 100);
    lv_obj_set_size(cur_media_slider, 500, 25);
    lv_obj_align(cur_media_slider, LV_ALIGN_TOP_MID, 0, y_offset + 30);
    lv_slider_set_value(cur_media_slider, (int)(vol * 100), LV_ANIM_OFF); 
    lv_obj_clear_flag(cur_media_slider, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_add_event_cb(cur_media_slider, [](lv_event_t* e) {
        lv_obj_t* sl = lv_event_get_target(e);
        float new_vol = lv_slider_get_value(sl) / 100.0f;
        HaWebsocketLogic_CallMediaVolumeService(cur_media_entity, new_vol);
    }, LV_EVENT_RELEASED, NULL);
}