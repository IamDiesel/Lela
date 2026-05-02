#include "HaDialogAdd.h"
#include "ViewHomeAssistant.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "SharedData.h"
#include "HAWidgets.h"
#include <lvgl.h>

static lv_obj_t* add_widget_overlay = nullptr;
static lv_obj_t* dd_widget_type = nullptr;
static lv_obj_t* ta_widget_entity = nullptr;
static lv_obj_t* lbl_preview = nullptr; 

void HaDialogAdd::resetState() {
    add_widget_overlay = nullptr;
    dd_widget_type = nullptr;
    ta_widget_entity = nullptr;
    lbl_preview = nullptr;
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
    bool exists = HaWebsocketLogic_EntityExists(e_id);
    String ha_name = HaWebsocketLogic_GetEntityName(e_id);
    
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
    
    // FIX: Neue Optionen inkl. Media Player, Cover und Fan
    lv_dropdown_set_options(dd_widget_type, "light\nswitch\ninput_boolean\nmedia_player\ncover\nfan\ninput_button\nbutton\nscene\nscript\nautomation\nsensor\nbinary_sensor\nweather\nperson\ndevice_tracker");
    
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
    lv_obj_align(lbl_preview, LV_ALIGN_TOP_MID, 0, 230);
    lv_obj_set_style_text_align(lbl_preview, LV_TEXT_ALIGN_CENTER, 0);

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
        if(code == LV_EVENT_VALUE_CHANGED) updateAddWidgetPreview();
    }, LV_EVENT_ALL, kb);
    
    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = lv_event_get_target(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            lv_obj_clear_state(lv_keyboard_get_textarea(keyboard), LV_STATE_FOCUSED);
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
            String ha_name = HaWebsocketLogic_GetEntityName(e_id);
            if (ha_name.length() > 0) friendly_name = ha_name;
            else if (input_txt.indexOf('.') != -1) friendly_name = formatEntityName(e_id, "");
            
            String cached_icon = HaWebsocketLogic_GetCachedIcon(e_id);
            
            // FIX: Zuweisung des richtigen Widget-Typs (Fallbacks landen sicher beim Light-Widget, was dank des letzten Updates nun ein generisches Toggle ist!)
            String w_type = "light";
            if (e_type == "sensor" || e_type == "binary_sensor" || e_type == "weather" || e_type == "person" || e_type == "device_tracker") w_type = "sensor";
            else if (e_type == "button" || e_type == "input_button" || e_type == "script" || e_type == "scene" || e_type == "automation") w_type = "action";
            else if (e_type == "media_player") w_type = "media_player";
            
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
                
                HaConfigLogic::dashboards[act_tab].widgets.push_back(def);
                HaConfigLogic::Save();
                HaWebsocketLogic_UpdateTrackedEntities();
                
                ViewHomeAssistant::pendingHaReload = true; 
            } else {
                lv_obj_del_async(add_widget_overlay);
            }
            add_widget_overlay = nullptr;
            lbl_preview = nullptr;
            
        } else if(code == LV_EVENT_CANCEL) {
            playToneI2S(600, 100, true);
            lv_obj_del_async(add_widget_overlay);
            add_widget_overlay = nullptr;
            lbl_preview = nullptr;
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
    }, LV_EVENT_CLICKED, NULL);
}