#include "HaDialogTab.h"
#include "ViewHomeAssistant.h"
#include "HaConfigLogic.h"
#include "HaWebsocketLogic.h"
#include "SharedData.h"
#include "UIHelper.h"

lv_obj_t* HaDialogTab::kb_overlay = nullptr;
lv_obj_t* HaDialogTab::ta_new_tab = nullptr;
int HaDialogTab::editingTabIndex = -1;
lv_obj_t* HaDialogTab::confirm_overlay = nullptr;

void HaDialogTab::resetState() {
    kb_overlay = nullptr;
    ta_new_tab = nullptr;
    editingTabIndex = -1;
    confirm_overlay = nullptr;
}

void HaDialogTab::createOverlay(const char* placeholder_or_text, bool is_placeholder) {
    kb_overlay = UIHelper::createModalOverlay(ViewHomeAssistant::screen, 200, &kb_overlay);
    
    ta_new_tab = lv_textarea_create(kb_overlay);
    lv_obj_set_size(ta_new_tab, 600, 80);
    lv_obj_align(ta_new_tab, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_text_font(ta_new_tab, &lv_font_montserrat_36, 0);
    
    if (is_placeholder) {
        lv_textarea_set_placeholder_text(ta_new_tab, placeholder_or_text);
    } else {
        lv_textarea_set_text(ta_new_tab, placeholder_or_text);
    }
    
    lv_obj_t* kb = lv_keyboard_create(kb_overlay);
    lv_obj_set_size(kb, 1280, 400);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_new_tab);
    lv_obj_add_state(ta_new_tab, LV_STATE_FOCUSED);

    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* keyboard = lv_event_get_target(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            lv_obj_clear_state(lv_keyboard_get_textarea(keyboard), LV_STATE_FOCUSED);
        }
    }, LV_EVENT_ALL, NULL);

    lv_obj_t * btn_kb_cancel = lv_btn_create(kb_overlay);
    lv_obj_set_size(btn_kb_cancel, 80, 80);
    lv_obj_align_to(btn_kb_cancel, ta_new_tab, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_set_style_bg_color(btn_kb_cancel, lv_color_hex(0xAA0000), 0);
    
    lv_obj_add_event_cb(btn_kb_cancel, [](lv_event_t* e) {
        lv_obj_del_async(kb_overlay); 
        kb_overlay = nullptr; 
        lv_tabview_set_act(ViewHomeAssistant::tabview, ViewHomeAssistant::currentActiveTab, LV_ANIM_OFF);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_kb_cancel = lv_label_create(btn_kb_cancel); 
    lv_label_set_text(lbl_kb_cancel, LV_SYMBOL_CLOSE); 
    lv_obj_center(lbl_kb_cancel);
    
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
}

void HaDialogTab::showAddTabDialog() {
    if (kb_overlay != nullptr) return;   // kein zweites Overlay uebereinander
    playToneI2S(800, 100, true);
    editingTabIndex = -1; 
    createOverlay("Name des neuen Raums...", true);
}

void HaDialogTab::showRenameTabDialog() {
    if (kb_overlay != nullptr) return; 
    playToneI2S(800, 100, true); 
    
    editingTabIndex = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
    if (editingTabIndex >= (int)HaConfigLogic::dashboards.size()) {
        editingTabIndex = 0; 
    }
    
    createOverlay(HaConfigLogic::dashboards[editingTabIndex].name.c_str(), false);
}

void HaDialogTab::deleteCurrentTab() {
    if (confirm_overlay != nullptr || kb_overlay != nullptr) return;

    uint16_t act_tab = lv_tabview_get_tab_act(ViewHomeAssistant::tabview);
    if (act_tab >= HaConfigLogic::dashboards.size()) return;

    playToneI2S(600, 100, true); 

    // Rueckfrage, denn ein geloeschter Tab nimmt alle seine Widgets mit und
    // laesst sich nicht wiederherstellen.
    confirm_overlay = UIHelper::createModalOverlay(ViewHomeAssistant::screen, 200, &confirm_overlay);

    lv_obj_t* panel = lv_obj_create(confirm_overlay);
    lv_obj_set_size(panel, 560, 260);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    const String question = "Tab \"" + HaConfigLogic::dashboards[act_tab].name + "\" loeschen?";
    UIHelper::createLabel(panel, question.c_str(), &lv_font_montserrat_24, LV_ALIGN_TOP_MID, 0, 25, 0xFFFFFF);
    UIHelper::createLabel(panel, "Alle Widgets darin gehen verloren.", &lv_font_montserrat_16, LV_ALIGN_TOP_MID, 0, 75, 0xAAAAAA);

    // Index im user_data mitgeben: er ist der Tab, nach dem gefragt wurde,
    // auch falls sich der aktive Tab bis zur Bestaetigung aendern sollte.
    UIHelper::createButton(panel, 220, 60, LV_ALIGN_BOTTOM_RIGHT, -20, -25, 0xAA0000, "Ja, loeschen",
                           confirm_delete_cb, (void*)(uintptr_t)act_tab);
    UIHelper::createButton(panel, 220, 60, LV_ALIGN_BOTTOM_LEFT, 20, -25, 0x333333, "Abbrechen",
                           [](lv_event_t* e) { closeConfirm(); });
}

void HaDialogTab::closeConfirm() {
    if (confirm_overlay) {
        lv_obj_del_async(confirm_overlay);
        confirm_overlay = nullptr;
    }
}

void HaDialogTab::confirm_delete_cb(lv_event_t* e) {
    const int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    closeConfirm();

    if (idx < 0 || idx >= (int)HaConfigLogic::dashboards.size()) return;

    playToneI2S(600, 100, true);
    HaConfigLogic::DeleteTab(idx); 
    HaConfigLogic::Save();
    HaWebsocketLogic_UpdateTrackedEntities();
    
    ViewHomeAssistant::currentActiveTab = 0; 
    ViewHomeAssistant::pendingHaReload = true; 
}

void HaDialogTab::kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        playToneI2S(800, 100, true);
        const char * txt = lv_textarea_get_text(ta_new_tab);
        
        if (strlen(txt) > 0) {
            if (editingTabIndex == -1) {
                HaConfigLogic::AddTab(String(txt)); 
                ViewHomeAssistant::currentActiveTab = HaConfigLogic::dashboards.size() - 1; 
            } else {
                // Ungespeicherte Aenderungen am Layout zuerst uebernehmen -
                // Save() und der Neuaufbau wuerden sie sonst verwerfen.
                ViewHomeAssistant::captureActiveTabLayout();
                HaConfigLogic::RenameTab(editingTabIndex, String(txt)); 
                ViewHomeAssistant::currentActiveTab = editingTabIndex; 
            }
            
            HaConfigLogic::Save(); 
            HaWebsocketLogic_UpdateTrackedEntities(); 
        }
        
        lv_obj_del_async(kb_overlay); 
        kb_overlay = nullptr; 
        ViewHomeAssistant::pendingHaReload = true; 
        
    } else if (code == LV_EVENT_CANCEL) {
        playToneI2S(600, 100, true);
        lv_obj_del_async(kb_overlay); 
        kb_overlay = nullptr;
        
        if (editingTabIndex == -1) {
            lv_tabview_set_act(ViewHomeAssistant::tabview, ViewHomeAssistant::currentActiveTab, LV_ANIM_OFF);
        }
    }
}