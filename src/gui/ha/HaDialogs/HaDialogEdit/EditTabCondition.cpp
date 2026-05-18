#include "EditTabCondition.h"
#include "UIHelper.h"
#include "HaDialogEdit.h"
#include "HaEntityCache.h"

void EditTabCondition::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    UIHelper::createLabel(parent, "Sichtbarkeit:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 10);
    dd_cond_type = UIHelper::createDropdown(parent, 250, 40, LV_ALIGN_TOP_LEFT, 160, 5, "AND (Alle erfuellt)\nOR (Eine erfuellt)", w->getConditionsType() == "OR" ? 1 : 0);

    String c1_ent = "", c1_op = "==", c1_val = "";
    if (w->getConditions().size() > 0) {
        c1_ent = w->getConditions()[0].entity; c1_op = w->getConditions()[0].op; c1_val = w->getConditions()[0].value;
    }
    
    UIHelper::createLabel(parent, "Bedingung 1:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 60);
    ta_cond1_entity = UIHelper::createTextarea(parent, 360, 40, LV_ALIGN_TOP_LEFT, 10, 90, c1_ent.c_str(), "Entitaet (z.B. switch.tv)");
    HaDialogEdit::bindKeyboard(ta_cond1_entity, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    int op1_idx = 0;
    if(c1_op=="!=") op1_idx=1; else if(c1_op==">") op1_idx=2; else if(c1_op=="<") op1_idx=3; else if(c1_op==">=") op1_idx=4; else if(c1_op=="<=") op1_idx=5;
    dd_cond1_op = UIHelper::createDropdown(parent, 80, 40, LV_ALIGN_TOP_LEFT, 380, 90, "==\n!=\n>\n<\n>=\n<=", op1_idx);
    
    ta_cond1_val = UIHelper::createTextarea(parent, 320, 40, LV_ALIGN_TOP_LEFT, 470, 90, c1_val.c_str(), "Wert (z.B. on)");
    HaDialogEdit::bindKeyboard(ta_cond1_val, LV_KEYBOARD_MODE_TEXT_LOWER);

    String c2_ent = "", c2_op = "==", c2_val = "";
    if (w->getConditions().size() > 1) {
        c2_ent = w->getConditions()[1].entity; c2_op = w->getConditions()[1].op; c2_val = w->getConditions()[1].value;
    }
    
    UIHelper::createLabel(parent, "Bedingung 2:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 150);
    ta_cond2_entity = UIHelper::createTextarea(parent, 360, 40, LV_ALIGN_TOP_LEFT, 10, 180, c2_ent.c_str(), "Entitaet (Optional)");
    HaDialogEdit::bindKeyboard(ta_cond2_entity, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    int op2_idx = 0;
    if(c2_op=="!=") op2_idx=1; else if(c2_op==">") op2_idx=2; else if(c2_op=="<") op2_idx=3; else if(c2_op==">=") op2_idx=4; else if(c2_op=="<=") op2_idx=5;
    dd_cond2_op = UIHelper::createDropdown(parent, 80, 40, LV_ALIGN_TOP_LEFT, 380, 180, "==\n!=\n>\n<\n>=\n<=", op2_idx);
    
    ta_cond2_val = UIHelper::createTextarea(parent, 320, 40, LV_ALIGN_TOP_LEFT, 470, 180, c2_val.c_str(), "Wert");
    HaDialogEdit::bindKeyboard(ta_cond2_val, LV_KEYBOARD_MODE_TEXT_LOWER);

    roller_cond_search = lv_roller_create(parent);
    lv_obj_set_size(roller_cond_search, 380, 110);
    lv_obj_set_style_text_font(roller_cond_search, &lv_font_montserrat_16, 0);
    lv_roller_set_visible_row_count(roller_cond_search, 3);
    lv_obj_add_flag(roller_cond_search, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(roller_cond_search, [](lv_event_t* e) {
        EditTabCondition* tab = (EditTabCondition*)lv_event_get_user_data(e);
        lv_obj_t* ta = (lv_obj_t*)lv_obj_get_user_data(tab->roller_cond_search);
        if (ta && lv_obj_is_valid(ta)) {
            char buf[128];
            lv_roller_get_selected_str(tab->roller_cond_search, buf, sizeof(buf));
            lv_textarea_set_text(ta, buf);
            lv_obj_add_flag(tab->roller_cond_search, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    auto cond_search_cb = [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* ta = lv_event_get_target(e);
        EditTabCondition* tab = (EditTabCondition*)lv_event_get_user_data(e);
        
        if (code == LV_EVENT_FOCUSED) {
            lv_obj_set_user_data(tab->roller_cond_search, ta);
        } else if (code == LV_EVENT_VALUE_CHANGED) {
            String txt = lv_textarea_get_text(ta);
            if (txt.length() >= 2 && tab->roller_cond_search) {
                std::vector<String> hits = HaEntityCache::SearchEntities(txt, 5);
                if (hits.size() > 0) {
                    String opts = "";
                    for (size_t k = 0; k < hits.size(); k++) {
                        opts += hits[k];
                        if (k < hits.size() - 1) opts += "\n";
                    }
                    lv_roller_set_options(tab->roller_cond_search, opts.c_str(), LV_ROLLER_MODE_NORMAL);
                    lv_obj_align_to(tab->roller_cond_search, ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
                    lv_obj_clear_flag(tab->roller_cond_search, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_move_foreground(tab->roller_cond_search); 
                } else {
                    lv_obj_add_flag(tab->roller_cond_search, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (tab->roller_cond_search) {
                lv_obj_add_flag(tab->roller_cond_search, LV_OBJ_FLAG_HIDDEN);
            }
        }
    };
    
    lv_obj_add_event_cb(ta_cond1_entity, cond_search_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(ta_cond2_entity, cond_search_cb, LV_EVENT_ALL, this);
}

void EditTabCondition::saveConfig(HAWidget* w) {
    if (dd_cond_type && lv_obj_is_valid(dd_cond_type) && ta_cond1_entity && lv_obj_is_valid(ta_cond1_entity)) {
        String type = (lv_dropdown_get_selected(dd_cond_type) == 0) ? "AND" : "OR";
        std::vector<HACondition> conds;
        
        String ent1 = String(lv_textarea_get_text(ta_cond1_entity)); ent1.trim();
        if (ent1.length() > 0) {
            HACondition c1; c1.entity = ent1;
            char buf[10]; lv_dropdown_get_selected_str(dd_cond1_op, buf, sizeof(buf)); c1.op = String(buf);
            c1.value = String(lv_textarea_get_text(ta_cond1_val)); c1.value.trim();
            conds.push_back(c1);
        }
        
        if (ta_cond2_entity && lv_obj_is_valid(ta_cond2_entity)) {
            String ent2 = String(lv_textarea_get_text(ta_cond2_entity)); ent2.trim();
            if (ent2.length() > 0) {
                HACondition c2; c2.entity = ent2;
                char buf[10]; lv_dropdown_get_selected_str(dd_cond2_op, buf, sizeof(buf)); c2.op = String(buf);
                c2.value = String(lv_textarea_get_text(ta_cond2_val)); c2.value.trim();
                conds.push_back(c2);
            }
        }
        w->setConditions(type, conds);
    }
}