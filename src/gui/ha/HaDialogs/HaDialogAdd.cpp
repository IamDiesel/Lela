#include "HaDialogAdd.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "SharedData.h"
#include "HAWidgets.h"
#include "HaEntityCache.h" 
#include <lvgl.h>

static lv_obj_t* add_widget_overlay = nullptr;
static lv_obj_t* dd_widget_type = nullptr;
static lv_obj_t* ta_widget_entity = nullptr;
static lv_obj_t* lbl_preview = nullptr; 
static lv_obj_t* roller_search = nullptr; 

void HaDialogAdd::resetState() {
    add_widget_overlay = nullptr;
    dd_widget_type = nullptr;
    ta_widget_entity = nullptr;
    lbl_preview = nullptr;
    roller_search = nullptr;
}

static void updateAddWidgetPreview() {
    if (!lbl_preview || !dd_widget_type || !ta_widget_entity) return;
    
    char buf[32];
    lv_dropdown_get_selected_str(dd_widget_type, buf, sizeof(buf));
    String e_type = String(buf);
    
    String input_txt = String(lv_textarea_get_text(ta_widget_entity));
    input_txt.trim();
    
    if (input_txt.length() == 0) {
        lv_label_set_text(lbl_preview, "Vorschau: ...");
        lv_obj_set_style_text_color(lbl_preview, lv_color_white(), 0);
        return;
    }
    
    String e_id = ViewHomeAssistant::generateEntityId(e_type, input_txt);
    bool exists = HaEntityCache::EntityExists(e_id); 
    String ha_name = HaEntityCache::GetEntityName(e_id);
    
    String previewText = "ID wird: " + e_id + "\n";
    if (exists) {
        previewText += "Status: Gefunden in HA! " LV_SYMBOL_OK;
        if (ha_name.length() > 0) previewText += " (" + ha_name + ")";
        lv_obj_set_style_text_color(lbl_preview, lv_color_hex(0x2ECC71), 0); 
    } else {
        previewText += "Status: Unbekannt " LV_SYMBOL_CLOSE " (Trotzdem moeglich)";
        lv_obj_set_style_text_color(lbl_preview, lv_color_hex(0xE74C3C), 0); 
    }
    lv_label_set_text(lbl_preview, previewText.c_str());
}

void HaDialogAdd::showAddWidgetDialog() {
    if (add_widget_overlay != nullptr) return; 
    
    add_widget_overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(add_widget_overlay, 1280, 720);
    lv_obj_set_style_bg_color(add_widget_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(add_widget_overlay, 220, 0);
    lv_obj_add_flag(add_widget_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(add_widget_overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* panel = lv_obj_create(add_widget_overlay);
    lv_obj_set_size(panel, 600, 450);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "Neues Widget Manuell Erstellen");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);

    dd_widget_type = lv_dropdown_create(panel);
    lv_dropdown_set_options(dd_widget_type, 
        "light\nswitch\ninput_boolean\n"
        "media_player\nvacuum\n"
        "select\ninput_select\n"
        "number\ninput_number\n"
        "cover\nfan\n"
        "input_button\nbutton\nscene\nscript\nautomation\n"
        "sensor\nbinary_sensor\nweather\nperson\ndevice_tracker"
    );
    lv_obj_set_width(dd_widget_type, 400);
    lv_obj_set_style_text_font(dd_widget_type, &lv_font_montserrat_24, 0);
    lv_obj_align(dd_widget_type, LV_ALIGN_TOP_MID, 0, 60);

    ta_widget_entity = lv_textarea_create(panel);
    lv_obj_set_size(ta_widget_entity, 400, 80);
    lv_textarea_set_placeholder_text(ta_widget_entity, "Name (z.B. Deckenlampe)");
    lv_obj_set_style_text_font(ta_widget_entity, &lv_font_montserrat_24, 0);
    lv_obj_align(ta_widget_entity, LV_ALIGN_TOP_MID, 0, 140);

    lbl_preview = lv_label_create(panel);
    lv_label_set_text(lbl_preview, "Vorschau: ...");
    lv_obj_set_style_text_font(lbl_preview, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_preview, LV_ALIGN_TOP_MID, 0, 240);
    lv_obj_set_style_text_align(lbl_preview, LV_TEXT_ALIGN_CENTER, 0);

    roller_search = lv_roller_create(panel);
    lv_obj_set_size(roller_search, 400, 110);
    lv_obj_align(roller_search, LV_ALIGN_TOP_MID, 0, 230); 
    lv_obj_set_style_text_font(roller_search, &lv_font_montserrat_16, 0);
    lv_roller_set_visible_row_count(roller_search, 3);
    lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN); 
    
    // --- FIX: Der unsinnige sizeof(buf) Fehler ist behoben ---
    lv_obj_add_event_cb(roller_search, [](lv_event_t* e) {
        char buf[128];
        lv_roller_get_selected_str(roller_search, buf, sizeof(buf));
        String selected = String(buf);
        int dotIdx = selected.indexOf('.');
        if (dotIdx != -1) selected = selected.substring(dotIdx + 1);
        lv_textarea_set_text(ta_widget_entity, selected.c_str());
        lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN); 
        updateAddWidgetPreview();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    // ---------------------------------------------------------

    lv_obj_add_event_cb(dd_widget_type, [](lv_event_t* e){ updateAddWidgetPreview(); }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* kb = lv_keyboard_create(add_widget_overlay);
    lv_obj_set_size(kb, 1280, 380);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_widget_entity);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); 

    lv_obj_add_event_cb(ta_widget_entity, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = (lv_obj_t*)lv_event_get_user_data(e);
        if(code == LV_EVENT_FOCUSED) lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        if(code == LV_EVENT_DEFOCUSED) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        
        if(code == LV_EVENT_VALUE_CHANGED) {
            updateAddWidgetPreview();
            String txt = lv_textarea_get_text(ta_widget_entity);
            if (txt.length() >= 2 && roller_search) {
                std::vector<String> hits = HaEntityCache::SearchEntities(txt, 5);
                if (hits.size() > 0) {
                    String opts = "";
                    for (size_t k = 0; k < hits.size(); k++) {
                        opts += hits[k];
                        if (k < hits.size() - 1) opts += "\n";
                    }
                    lv_roller_set_options(roller_search, opts.c_str(), LV_ROLLER_MODE_NORMAL);
                    lv_obj_clear_flag(roller_search, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_move_foreground(roller_search); 
                } else {
                    lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (roller_search) {
                lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }, LV_EVENT_ALL, kb);
    
    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = lv_event_get_target(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            lv_obj_clear_state(lv_keyboard_get_textarea(keyboard), LV_STATE_FOCUSED);
            if (roller_search) lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_ALL, NULL);
    
    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        if(code == LV_EVENT_READY) {
            playToneI2S(800, 100, true);
            
            char buf[32];
            lv_dropdown_get_selected_str(dd_widget_type, buf, sizeof(buf));
            String e_type = String(buf);
            String input_txt = String(lv_textarea_get_text(ta_widget_entity));
            input_txt.trim();
            
            String e_id = ViewHomeAssistant::generateEntityId(e_type, input_txt);
            String friendly_name = input_txt;
            String ha_name = HaEntityCache::GetEntityName(e_id); 
            if (ha_name.length() > 0) friendly_name = ha_name;
            else if (input_txt.indexOf('.') != -1) friendly_name = formatEntityName(e_id, "");
            
            String cached_icon = HaEntityCache::GetCachedIcon(e_id); 
            
            String w_type = "light";
            if (e_type == "sensor" || e_type == "binary_sensor" || e_type == "weather" || e_type == "person" || e_type == "device_tracker") w_type = "sensor";
            else if (e_type == "button" || e_type == "input_button" || e_type == "script" || e_type == "scene" || e_type == "automation") w_type = "action";
            else if (e_type == "media_player") w_type = "media_player";
            else if (e_type == "vacuum") w_type = "vacuum";
            else if (e_type == "select" || e_type == "input_select") w_type = "select";
            else if (e_type == "number" || e_type == "input_number") w_type = "number";
            
            if (input_txt.length() > 0) {
                uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
                if(act_tab >= HaConfigLogic::dashboards.size()) act_tab = 0;
                ViewHomeAssistant::currentActiveTab = act_tab;

                HAWidgetDef def;
                def.entity_id = e_id;
                def.type = w_type;
                def.x = 20; def.y = 20; def.w = 160; def.h = 105;
                def.name = friendly_name; 
                def.mdi_icon = cached_icon; 
                def.color_on = "";
                def.color_off = "";
                
                if (w_type == "vacuum") {
                    def.show_chart = false;
                    def.chart_w_pct = 60;  
                    def.chart_h_pct = 40;   
                    def.chart_x_ofs = 10;   
                    def.chart_y_ofs = 10;   
                }

                if (w_type == "select") {
                    std::vector<String> opts = HaEntityCache::GetGlobalOptions(e_id);
                    String opt_str = "";
                    for (size_t k = 0; k < opts.size(); k++) {
                        opt_str += opts[k];
                        if (k < opts.size() - 1) opt_str += ",";
                    }
                    def.select_options = opt_str;
                } 
                else if (w_type == "number") {
                    def.slider_min = HaEntityCache::GetGlobalMin(e_id);
                    def.slider_max = HaEntityCache::GetGlobalMax(e_id);
                    def.slider_step = HaEntityCache::GetGlobalStep(e_id);
                }
                
                HaConfigLogic::dashboards[act_tab].widgets.push_back(def);
                HaConfigLogic::Save();
                HaWebsocketLogic_UpdateTrackedEntities();
                ViewHomeAssistant::pendingHaReload = true; 
            } else {
                lv_obj_del_async(add_widget_overlay);
            }
            add_widget_overlay = nullptr;
            lbl_preview = nullptr;
            roller_search = nullptr;
        } else if(code == LV_EVENT_CANCEL) {
            playToneI2S(600, 100, true);
            lv_obj_del_async(add_widget_overlay);
            add_widget_overlay = nullptr;
            lbl_preview = nullptr;
            roller_search = nullptr;
        }
    }, LV_EVENT_ALL, NULL);
    
    lv_obj_t* btn_close = lv_btn_create(panel);
    lv_obj_set_size(btn_close, 50, 50);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xAA0000), 0);
    lv_obj_t* lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_center(lbl_close);
    lv_obj_add_event_cb(btn_close, [](lv_event_t* e) {
        lv_obj_del_async(add_widget_overlay); 
        add_widget_overlay = nullptr;
        lbl_preview = nullptr;
        roller_search = nullptr;
    }, LV_EVENT_CLICKED, NULL);
}