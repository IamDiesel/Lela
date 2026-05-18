#include "HaDialogFolder.h"
#include "HaDialogAdd.h"
#include "HaDialogEdit.h"
#include "ViewHomeAssistant.h"
#include "HaConfigLogic.h"
#include "UIHelper.h"
#include "SharedData.h"
#include "HaWidgetFactory.h" 

lv_obj_t* HaDialogFolder::overlay = nullptr;
HAFolderWidget* HaDialogFolder::current_folder = nullptr;

void HaDialogFolder::show(HAFolderWidget* folder) {
    if (overlay != nullptr) return;
    current_folder = folder;

    playToneI2S(800, 100, true);

    overlay = lv_obj_create(ViewHomeAssistant::screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, 200, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 860, 520); // <--- DEUTLICH BREITER ---
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    UIHelper::createLabel(panel, "Ordner Inhalt verwalten", &lv_font_montserrat_24, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* list = lv_obj_create(panel);
    lv_obj_set_size(list, 820, 340); // <--- LISTENPLATZ ERWEITERT ---
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x333333), 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    auto& children = folder->getFolderChildren();
    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 60);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        String title = child.def.name + " (" + child.def.type + ")";
        if (child.def.conditions.size() > 0) {
             title += " [" + String(child.def.conditions.size()) + " Bed.]";
        }
        UIHelper::createLabel(row, title.c_str(), &lv_font_montserrat_20, LV_ALIGN_LEFT_MID, 10, 0);

        UIHelper::createButton(row, 100, 40, LV_ALIGN_RIGHT_MID, -230, 0, 0x2980B9, "Edit", [](lv_event_t* e){
            HAWidget* w = (HAWidget*)lv_event_get_user_data(e);
            HaDialogFolder::resetState();
            HaDialogEdit::showWidgetEditDialog(w); 
        }, child.widget);

        UIHelper::createButton(row, 100, 40, LV_ALIGN_RIGHT_MID, -120, 0, 0x27AE60, "Kopie", [](lv_event_t* e){
            HAWidget* w = (HAWidget*)lv_event_get_user_data(e);
            if (current_folder) {
                HAWidgetDef def = HaWidgetFactory::createDefFromWidget(w);
                def.name += " (Kopie)";
                
                HAWidget* new_child = HaWidgetFactory::createWidget(current_folder->getContainer(), current_folder->getTabIndex(), def);
                if (new_child) {
                    lv_obj_set_pos(new_child->getContainer(), 0, 0);
                    lv_obj_set_size(new_child->getContainer(), LV_PCT(100), LV_PCT(100));
                    current_folder->addChild(new_child, def);
                    
                    ViewHomeAssistant::helper_saveWidgets();
                    ViewHomeAssistant::pendingHaReload = false;
                    
                    HAFolderWidget* tf = current_folder;
                    HaDialogFolder::resetState();
                    HaDialogFolder::show(tf);
                }
            }
        }, child.widget);

        UIHelper::createButton(row, 100, 40, LV_ALIGN_RIGHT_MID, -10, 0, 0xAA0000, "Loesch.", [](lv_event_t* e){
            HAWidget* w = (HAWidget*)lv_event_get_user_data(e);
            if (current_folder) {
                current_folder->removeChild(w);
                
                ViewHomeAssistant::helper_saveWidgets();
                ViewHomeAssistant::pendingHaReload = false;
                
                HAFolderWidget* tf = current_folder;
                HaDialogFolder::resetState();
                HaDialogFolder::show(tf);
            } else {
                HaDialogFolder::resetState();
            }
        }, child.widget);
    }

    UIHelper::createButton(panel, 260, 50, LV_ALIGN_BOTTOM_LEFT, 20, -20, 0x27AE60, LV_SYMBOL_PLUS " Hinzufuegen", [](lv_event_t* e){
        HaDialogAdd::showAddWidgetDialog(current_folder); 
        HaDialogFolder::resetState();
    });

    UIHelper::createButton(panel, 260, 50, LV_ALIGN_BOTTOM_LEFT, 300, -20, 0x8E44AD, LV_SYMBOL_SETTINGS " Ordner Layout", [](lv_event_t* e){
        HAWidget* w = current_folder;
        HaDialogFolder::resetState();
        if (w) HaDialogEdit::showWidgetEditDialog(w);
    });

    UIHelper::createButton(panel, 260, 50, LV_ALIGN_BOTTOM_LEFT, 580, -20, 0x555555, "Schliessen", [](lv_event_t* e){
        HaDialogFolder::resetState();
    });
}

void HaDialogFolder::resetState() {
    if (overlay) {
        lv_obj_del_async(overlay);
        overlay = nullptr;
    }
    current_folder = nullptr;
}