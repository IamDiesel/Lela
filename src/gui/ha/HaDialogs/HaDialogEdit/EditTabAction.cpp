#include "EditTabAction.h"
#include "UIHelper.h"
#include "HaEntityCache.h"
#include "HaDialogEdit.h"

void EditTabAction::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    UIHelper::createLabel(parent, "Benutzerdefinierte Klick-Aktion (Optional):", nullptr, LV_ALIGN_TOP_LEFT, 10, 10, 0x00A0FF);
    
    UIHelper::createLabel(parent, "Domain:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 60);
    ta_domain = UIHelper::createTextarea(parent, 300, 40, LV_ALIGN_TOP_LEFT, 120, 50, w->getTapDomain().c_str(), "z.B. light");
    HaDialogEdit::bindKeyboard(ta_domain, LV_KEYBOARD_MODE_TEXT_LOWER);

    UIHelper::createLabel(parent, "Service:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 120);
    ta_service = UIHelper::createTextarea(parent, 300, 40, LV_ALIGN_TOP_LEFT, 120, 110, w->getTapService().c_str(), "z.B. toggle");
    HaDialogEdit::bindKeyboard(ta_service, LV_KEYBOARD_MODE_TEXT_LOWER);

    UIHelper::createLabel(parent, "Ziel:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 180);
    ta_target = UIHelper::createTextarea(parent, 670, 40, LV_ALIGN_TOP_LEFT, 120, 170, w->getTapTarget().c_str(), "Entitaet (z.B. light.flur)");
    HaDialogEdit::bindKeyboard(ta_target, LV_KEYBOARD_MODE_TEXT_LOWER);

    roller_search = lv_roller_create(parent);
    lv_obj_set_size(roller_search, 670, 110);
    lv_obj_set_style_text_font(roller_search, &lv_font_montserrat_16, 0);
    lv_roller_set_visible_row_count(roller_search, 3);
    lv_obj_add_flag(roller_search, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(roller_search, [](lv_event_t* e) {
        EditTabAction* tab = (EditTabAction*)lv_event_get_user_data(e);
        if (tab->current_ta && lv_obj_is_valid(tab->current_ta)) {
            char buf[128];
            lv_roller_get_selected_str(tab->roller_search, buf, sizeof(buf));
            lv_textarea_set_text(tab->current_ta, buf);
            lv_obj_add_flag(tab->roller_search, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    auto entity_search_cb = [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* ta = lv_event_get_target(e);
        EditTabAction* tab = (EditTabAction*)lv_event_get_user_data(e);
        
        if (code == LV_EVENT_FOCUSED) {
            tab->current_ta = ta; 
        } else if (code == LV_EVENT_VALUE_CHANGED) {
            String txt = lv_textarea_get_text(ta);
            if (txt.length() >= 2 && tab->roller_search) {
                std::vector<String> hits = HaEntityCache::SearchEntities(txt, 5);
                if (hits.size() > 0) {
                    String opts = "";
                    for (size_t k = 0; k < hits.size(); k++) {
                        opts += hits[k];
                        if (k < hits.size() - 1) opts += "\n";
                    }
                    lv_roller_set_options(tab->roller_search, opts.c_str(), LV_ROLLER_MODE_NORMAL);
                    lv_obj_align_to(tab->roller_search, ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
                    lv_obj_clear_flag(tab->roller_search, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_move_foreground(tab->roller_search); 
                } else {
                    lv_obj_add_flag(tab->roller_search, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (tab->roller_search) {
                lv_obj_add_flag(tab->roller_search, LV_OBJ_FLAG_HIDDEN);
            }
        }
    };
    lv_obj_add_event_cb(ta_target, entity_search_cb, LV_EVENT_ALL, this);
}

void EditTabAction::saveConfig(HAWidget* w) {
    if (ta_domain && lv_obj_is_valid(ta_domain) &&
        ta_service && lv_obj_is_valid(ta_service) &&
        ta_target && lv_obj_is_valid(ta_target)) {
        
        w->setTapAction(
            String(lv_textarea_get_text(ta_domain)),
            String(lv_textarea_get_text(ta_service)),
            String(lv_textarea_get_text(ta_target))
        );
    }
}