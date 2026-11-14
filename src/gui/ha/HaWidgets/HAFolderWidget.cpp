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

HAFolderWidget::~HAFolderWidget() {
    for (auto& c : folder_children) {
        delete c.widget;
    }
    folder_children.clear();
}

void HAFolderWidget::addChild(HAWidget* widget, const HAWidgetDef& def) {
    widget->setInFolder(true);   // Sichtbarkeit steuert ab jetzt der Ordner
    folder_children.push_back({widget, def});
    lv_obj_add_flag(widget->getContainer(), LV_OBJ_FLAG_HIDDEN);
}

void HAFolderWidget::removeChild(HAWidget* widget) {
    // Kind suchen und entfernen; der Ordner besitzt seine Kinder (delete hier).
    auto it = std::find_if(folder_children.begin(), folder_children.end(), [widget](const FolderChildInfo& info) {
        return info.widget == widget;
    });
    if (it != folder_children.end()) {
        if (it->widget && it->widget->getContainer()) {
            lv_obj_del_async(it->widget->getContainer());
        }
        delete it->widget; 
        folder_children.erase(it);
    }
}

std::vector<HAWidgetDef> HAFolderWidget::getChildrenDefs() {
    std::vector<HAWidgetDef> res;
    for (auto& c : folder_children) {
        res.push_back(HaWidgetFactory::createDefFromWidget(c.widget));
    }
    return res;
}

// Setzt HIDDEN nur bei Aenderung. lv_obj_add_flag/clear_flag invalidieren das
// Objekt sonst jedes Mal - bei 500-ms-Takt ein staendiges Neuzeichnen.
static void setHidden(lv_obj_t* obj, bool hidden) {
    if (!obj) return;
    if (hidden == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void HAFolderWidget::checkConditions() {
    // Eigene Bedingungen des Ordners zuerst. Ist er selbst ausgeblendet,
    // bleibt alles darin unsichtbar - die Kinder muessen nicht geprueft werden.
    if (!applyOwnVisibility()) return;

    // Rahmen und Platzhalter nur beim Wechsel des Edit-Modus umstellen.
    const int8_t look = HAWidget::editModeActive ? 1 : 0;
    if (look != applied_edit_look) {
        applied_edit_look = look;
        lv_obj_set_style_bg_opa(container, look ? 50 : 0, 0);
        lv_obj_set_style_border_width(container, look ? 2 : 0, 0);
    }

    if (HAWidget::editModeActive) {
        setHidden(placeholder_lbl, false);
        for (auto& c : folder_children) {
            setHidden(c.widget->getContainer(), true);
        }
        return;
    }
    setHidden(placeholder_lbl, true);

    // Das erste Kind, dessen Bedingungen erfuellt sind, ist sichtbar.
    bool found_true = false;
    for (auto& c : folder_children) {
        if (!found_true && HAWidget::evaluateConditions(c.widget->getConditionsType(), c.widget->getConditions())) {
            setHidden(c.widget->getContainer(), false);
            c.widget->checkConditions(); 
            found_true = true; 
        } else {
            setHidden(c.widget->getContainer(), true);
        }
    }
}

bool HAFolderWidget::needsRefresh() {
    // Alle Kinder abfragen (kein vorzeitiges Ende): jedes merkt sich dabei
    // seinen Stand.
    bool changed = false;
    for (auto& c : folder_children) {
        if (c.widget->needsRefresh()) changed = true;
    }
    return changed;
}

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