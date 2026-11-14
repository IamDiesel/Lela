#include "HaColorPicker.h"
#include "HaTheme.h"
#include "UiTheme.h"
#include "UIHelper.h"
#include "ViewHomeAssistant.h"
#include "SharedData.h"

lv_obj_t* HaColorPicker::overlay = nullptr;
std::function<void(String)> HaColorPicker::onColorSelected = nullptr;

bool HaColorPicker::isActive() {
    return overlay != nullptr;
}

void HaColorPicker::hide() {
    if (overlay) {
        lv_obj_del_async(overlay);
        overlay = nullptr;
    }
}

void HaColorPicker::close_event_cb(lv_event_t * e) {
    playToneI2S(600, 100, true);
    hide();
}

void HaColorPicker::tile_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    const char* hex = (const char*)lv_event_get_user_data(e);
    
    if (onColorSelected) {
        onColorSelected(String(hex));
    }
    
    hide();
}

void HaColorPicker::show(String currentColor, std::function<void(String)> callback) {
    if (overlay != nullptr) {
        return;
    }
    
    onColorSelected = callback;

    overlay = UIHelper::createModalOverlay(ViewHomeAssistant::screen, 200, &overlay);
    
    lv_obj_add_event_cb(overlay, close_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* panel = lv_obj_create(overlay);
    lv_obj_set_size(panel, 820, 520);
    lv_obj_center(panel);
    UIHelper::themePanel(panel);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    
    // Palette aus HaTheme (eine Quelle fuer Farben und Gegenfarben). Die
    // Kacheln liefern "#RRGGBB" (Dunkel-Wert) an den Aufrufer; die Texte
    // liegen statisch, weil der Zeiger als user_data am Objekt haengt.
    size_t palCount = 0;
    const uint32_t* pal = HaTheme::palette(&palCount);
    static char palText[32][8];
    static const char* const RESET = "";
    if (palCount > 31) palCount = 31;

    for (size_t i = 0; i <= palCount; i++) {
        lv_obj_t* tile = lv_btn_create(panel);
        lv_obj_set_size(tile, 90, 90);
        lv_obj_set_style_radius(tile, 10, 0);
        lv_obj_set_style_shadow_width(tile, 0, 0);
        lv_obj_set_style_clip_corner(tile, true, 0);
        lv_obj_set_style_border_color(tile, UiTheme::c().divider, 0);
        lv_obj_set_style_border_width(tile, 1, 0);

        if (i == palCount) {
            // Letzte Kachel: zuruecksetzen auf die Standardfarbe des Typs.
            lv_obj_set_style_bg_color(tile, HaTheme::control(), 0);
            lv_obj_t* l = lv_label_create(tile);
            lv_label_set_text(l, LV_SYMBOL_CLOSE "\nReset");
            lv_obj_set_style_text_color(l, UiTheme::c().text, 0);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(l);
            lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED, (void*)RESET);
            continue;
        }

        // Oben die Farbe (Dunkel-Wert), unten als Streifen ihre Gegenfarbe im
        // hellen Schema - so ist das Paar schon vor der Wahl sichtbar.
        snprintf(palText[i], sizeof(palText[i]), "#%06lX", (unsigned long)pal[i]);
        lv_obj_set_style_bg_color(tile, lv_color_hex(pal[i]), 0);
        lv_obj_t* strip = lv_obj_create(tile);
        lv_obj_remove_style_all(strip);
        lv_obj_clear_flag(strip, LV_OBJ_FLAG_CLICKABLE);   // Tipp geht an die Kachel
        lv_obj_set_size(strip, lv_pct(100), 24);
        lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(strip, lv_color_hex(HaTheme::lightOf(pal[i])), 0);
        lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);

        lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED, (void*)palText[i]);
    }
}