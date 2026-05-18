#include "HAFolderWidget.h"
#include "HaWebsocketLogic.h"
#include "HaWidgetFactory.h"
#include "HaDialogFolder.h"

HAFolderWidget::HAFolderWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    
    lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(name_label, LV_OBJ_FLAG_HIDDEN);
    
    placeholder_lbl = lv_label_create(container);
    lv_label_set_text(placeholder_lbl, LV_SYMBOL_DIRECTORY " Ordner");
    lv_obj_set_style_text_color(placeholder_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(placeholder_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
}

// --- FIX: C++ Objekte loeschen, wenn der Ordner geloescht wird ---
HAFolderWidget::~HAFolderWidget() {
    for (auto& c : folder_children) {
        delete c.widget;
    }
    folder_children.clear();
}

void HAFolderWidget::addChild(HAWidget* widget, const HAWidgetDef& def) {
    folder_children.push_back({widget, def});
    lv_obj_add_flag(widget->getContainer(), LV_OBJ_FLAG_HIDDEN);
}

void HAFolderWidget::removeChild(HAWidget* widget) {
    auto it = std::remove_if(folder_children.begin(), folder_children.end(), [widget](const FolderChildInfo& info) {
        return info.widget == widget;
    });
    if (it != folder_children.end()) {
        if (it->widget && it->widget->getContainer()) {
            lv_obj_del_async(it->widget->getContainer());
        }
        delete it->widget; // C++ Objekt aufraeumen
        folder_children.erase(it, folder_children.end());
    }
}

std::vector<HAWidgetDef> HAFolderWidget::getChildrenDefs() {
    std::vector<HAWidgetDef> res;
    for (auto& c : folder_children) {
        res.push_back(HaWidgetFactory::createDefFromWidget(c.widget));
    }
    return res;
}

bool HAFolderWidget::evaluate(const HAWidgetDef& def) {
    if (def.conditions.empty()) return true; 
    
    bool result = (def.conditions_type == "AND"); 
    
    for (const auto& c : def.conditions) {
        String state = HaWebsocketLogic_GetState(c.entity);
        bool cond_met = false;
        
        if (c.op == "==") cond_met = (state == c.value);
        else if (c.op == "!=") cond_met = (state != c.value);
        else {
            float s_f = state.toFloat();
            float v_f = c.value.toFloat();
            if (c.op == ">") cond_met = (s_f > v_f);
            else if (c.op == "<") cond_met = (s_f < v_f);
            else if (c.op == ">=") cond_met = (s_f >= v_f);
            else if (c.op == "<=") cond_met = (s_f <= v_f);
        }
        
        if (def.conditions_type == "AND") {
            result = result && cond_met;
            if (!result) break; 
        } else {
            result = result || cond_met;
            if (result) break; 
        }
    }
    return result;
}

void HAFolderWidget::checkConditions() {
    if (HAWidget::editModeActive) {
        lv_obj_clear_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(container, 50, 0); 
        lv_obj_set_style_border_width(container, 2, 0);
        for(auto& c : folder_children) {
            lv_obj_add_flag(c.widget->getContainer(), LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    
    lv_obj_add_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    
    bool found_true = false;
    for (auto& c : folder_children) {
        if (!found_true && evaluate(c.def)) {
            lv_obj_clear_flag(c.widget->getContainer(), LV_OBJ_FLAG_HIDDEN);
            c.widget->checkConditions(); 
            found_true = true; 
        } else {
            lv_obj_add_flag(c.widget->getContainer(), LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// --- FIX: Die Websocket-Updates muessen in den Ordner weitergereicht werden! ---
void HAFolderWidget::updateState(String state) { 
    for (auto& c : folder_children) {
        String child_state = HaWebsocketLogic_GetState(c.widget->getEntityId());
        if (child_state.length() > 0) {
            c.widget->updateState(child_state);
        }
    }
}

void HAFolderWidget::onClick() {
    if (HAWidget::editModeActive) {
        HaDialogFolder::show(this);
    }
}