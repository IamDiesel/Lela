#include "EditTabMap.h"
#include "UiTheme.h"
#include "UIHelper.h"
#include "HaDialogEdit.h"
#include "HAMapWidget.h"
#include "HaWebsocketLogic.h"
#include "MapTileConfig.h"
#include <math.h>
#include <algorithm>

// Weglaengen zur Auswahl (Minuten). 0 = kein Weg. Obergrenze wie
// HaTrackHistory::MAX_WINDOW_S (24 h).
static const int   HOUR_CHOICES_MIN[] = { 0, 5, 15, 30, 60, 180, 360, 720, 1440 };
static const char* const HOUR_LABELS[] = { "Aus", "5 min", "15 min", "30 min", "1 h", "3 h", "6 h", "12 h", "24 h" };

// Funkstille (Minuten), ab der ein Tracker als veraltet gilt. 0 = aus.
static const int   STALE_CHOICES_MIN[] = { 0, 5, 10, 15, 30, 60 };
static const char* const STALE_LABELS[] = { "Aus", "5 min", "10 min", "15 min", "30 min", "60 min" };

// Zoomstufen: 12 (Stadtteil) bis 19 (Hausnummer, OSM-Maximum), 20 vergroessert
// (zweifach, wie die Grenze der HA-Karte).
static const int ZOOM_MIN = 12;
static const int ZOOM_MAX = MAP_MAX_VIEW_ZOOM;

// "1.5 min" / "2 h" fuer Werte, die nicht in der Liste stehen (z. B. aus HA).
static String formatHours(float hours) {
    const float minutes = hours * 60.0f;
    char buf[32];
    if (minutes < 60.0f) snprintf(buf, sizeof(buf), "%.1f min (aktuell)", minutes);
    else                 snprintf(buf, sizeof(buf), "%.1f h (aktuell)", hours);
    return String(buf);
}

// Anzeigename: aus HA (Live-Cache oder Katalog), sonst die ID.
static String displayName(const String& id) {
    const String name = HaWebsocketLogic_GetEntityName(id);
    return name.length() > 0 ? name : id;
}

// Der gerade offene Reiter. Verzoegerte Aufrufe (lv_async_call) pruefen
// damit, ob er noch lebt - der Dialog kann dazwischen geschlossen worden sein.
static EditTabMap* s_active = nullptr;

EditTabMap::~EditTabMap() {
    if (s_active == this) s_active = nullptr;
}

// ------------------------------------------------------------------------------
// Tracker-Liste
// ------------------------------------------------------------------------------
void EditTabMap::rebuildChips() {
    // lv_obj_is_valid sucht den Zeiger in den Bildschirmen - sicher auch, wenn
    // der Dialog samt Bildschirm schon geloescht ist.
    if (!chip_box || !lv_obj_is_valid(chip_box)) return;
    lv_obj_clean(chip_box);

    if (entities.empty()) {
        UIHelper::createLabel(chip_box, "(noch keine - unten suchen)", &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 0, 0, 0x888888);
        return;
    }

    for (size_t i = 0; i < entities.size(); i++) {
        lv_obj_t* chip = lv_btn_create(chip_box);
        lv_obj_set_height(chip, 44);
        lv_obj_set_width(chip, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(chip, 22, 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(entities[i].startsWith("zone.") ? 0x2980B9 : 0x34495E), 0);
        lv_obj_set_style_pad_hor(chip, 14, 0);
        lv_obj_set_user_data(chip, this);
        lv_obj_add_event_cb(chip, chip_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(chip);
        const String text = displayName(entities[i]) + "  " LV_SYMBOL_CLOSE;
        lv_label_set_text(lbl, text.c_str());
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl);
    }
}

void EditTabMap::addEntity(const String& id) {
    if (id.indexOf('.') <= 0) return;
    if (std::find(entities.begin(), entities.end(), id) != entities.end()) return;
    entities.push_back(id);
    rebuildChips();
}

// Chip antippen: Tracker entfernen.
void EditTabMap::chip_event_cb(lv_event_t* e) {
    EditTabMap* tab = (EditTabMap*)lv_obj_get_user_data(lv_event_get_current_target(e));
    const size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
    if (!tab || idx >= tab->entities.size()) return;
    tab->entities.erase(tab->entities.begin() + idx);
    // Nicht im eigenen Ereignis loeschen: lv_obj_clean() wuerde den Chip
    // entfernen, der dieses Ereignis gerade ausloest.
    lv_async_call([](void* p) {
        if (p == s_active) static_cast<EditTabMap*>(p)->rebuildChips();
    }, tab);
}

// Vorschlag gewaehlt: hinzufuegen, Suchfeld leeren.
void EditTabMap::roller_event_cb(lv_event_t* e) {
    EditTabMap* tab = (EditTabMap*)lv_event_get_user_data(e);
    if (!tab || !tab->roller) return;
    char buf[128];
    lv_roller_get_selected_str(tab->roller, buf, sizeof(buf));
    tab->addEntity(String(buf));
    if (tab->ta_search) lv_textarea_set_text(tab->ta_search, "");
    lv_obj_add_flag(tab->roller, LV_OBJ_FLAG_HIDDEN);
}

void EditTabMap::search_event_cb(lv_event_t* e) {
    EditTabMap* tab = (EditTabMap*)lv_event_get_user_data(e);
    if (!tab) return;
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        HaWebsocketLogic_RequestEntityCatalog();   // Suche ueber alle Entitaeten
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        UIHelper::showEntitySuggestions(tab->roller, tab->ta_search,
                                        String(lv_textarea_get_text(tab->ta_search)), true);
    } else if (code == LV_EVENT_READY) {
        // Enter: eingegebene ID direkt uebernehmen (z. B. bekannte Entity-ID).
        String id = String(lv_textarea_get_text(tab->ta_search));
        id.trim();
        id.toLowerCase();
        tab->addEntity(id);
        lv_textarea_set_text(tab->ta_search, "");
        if (tab->roller) lv_obj_add_flag(tab->roller, LV_OBJ_FLAG_HIDDEN);
    }
}

// ------------------------------------------------------------------------------
// Aufbau
// ------------------------------------------------------------------------------
void EditTabMap::buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) {
    (void)keyboard;
    if (w->getKind() != HaWidgetKind::Map) return;
    HAMapWidget* map = static_cast<HAMapWidget*>(w);
    entities = map->getMapEntities();
    s_active = this;

    // --- Tracker ---
    UIHelper::createLabel(parent, "Tracker auf der Karte (antippen = entfernen):", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 5);

    chip_box = lv_obj_create(parent);
    lv_obj_set_size(chip_box, 800, 110);
    lv_obj_set_pos(chip_box, 10, 35);
    lv_obj_set_style_bg_opa(chip_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chip_box, 1, 0);
    lv_obj_set_style_border_color(chip_box, UiTheme::c().divider, 0);
    lv_obj_set_style_pad_all(chip_box, 6, 0);
    lv_obj_set_style_pad_gap(chip_box, 8, 0);
    lv_obj_set_flex_flow(chip_box, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(chip_box, LV_DIR_VER);
    rebuildChips();

    ta_search = UIHelper::createTextarea(parent, 500, 44, LV_ALIGN_TOP_LEFT, 10, 155, "", "Tracker suchen (z.B. lola) ...");
    HaDialogEdit::bindKeyboard(ta_search, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(ta_search, search_event_cb, LV_EVENT_ALL, this);

    // --- Weg ---
    hour_values.clear();
    String options;
    int selected = -1;
    const float current = map->getMapHours();
    for (size_t i = 0; i < sizeof(HOUR_CHOICES_MIN) / sizeof(HOUR_CHOICES_MIN[0]); i++) {
        const float h = HOUR_CHOICES_MIN[i] / 60.0f;
        if (fabsf(h - current) < 0.001f) selected = (int)hour_values.size();
        if (options.length()) options += "\n";
        options += HOUR_LABELS[i];
        hour_values.push_back(h);
    }
    if (selected < 0) {
        // Wert aus dem Import (z. B. hours_to_show: 0.025) erhalten statt still aendern.
        options = formatHours(current) + "\n" + options;
        hour_values.insert(hour_values.begin(), current);
        selected = 0;
    }
    UIHelper::createLabel(parent, "Weg anzeigen (Verlauf):", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 230);
    dd_hours = UIHelper::createDropdown(parent, 300, 50, LV_ALIGN_TOP_LEFT, 330, 220, options.c_str(), selected);

    // --- Zoom ---
    zoom_values.clear();
    options = "";
    selected = -1;
    const int zoom = map->getMapMaxZoom();
    if (zoom < ZOOM_MIN || zoom > ZOOM_MAX) {
        options = String(zoom) + " (aktuell)";
        zoom_values.push_back(zoom);
        selected = 0;
    }
    for (int z = ZOOM_MIN; z <= ZOOM_MAX; z++) {
        if (z == zoom) selected = (int)zoom_values.size();
        if (options.length()) options += "\n";
        options += String(z);
        if (z > MAP_MAX_ZOOM) options += " (2x vergroessert)";
        zoom_values.push_back(z);
    }
    UIHelper::createLabel(parent, "Max. Zoomstufe:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 300);
    dd_zoom = UIHelper::createDropdown(parent, 300, 50, LV_ALIGN_TOP_LEFT, 330, 290, options.c_str(), selected);

    // --- Funkstille ---
    stale_values.clear();
    options = "";
    selected = -1;
    const int stale = map->getMapStaleMinutes();
    for (size_t i = 0; i < sizeof(STALE_CHOICES_MIN) / sizeof(STALE_CHOICES_MIN[0]); i++) {
        if (STALE_CHOICES_MIN[i] == stale) selected = (int)stale_values.size();
        if (options.length()) options += "\n";
        options += STALE_LABELS[i];
        stale_values.push_back(STALE_CHOICES_MIN[i]);
    }
    if (selected < 0) {
        options = String(stale) + " min (aktuell)\n" + options;
        stale_values.insert(stale_values.begin(), stale);
        selected = 0;
    }
    UIHelper::createLabel(parent, "Veraltet nach Funkstille:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 370);
    dd_stale = UIHelper::createDropdown(parent, 300, 50, LV_ALIGN_TOP_LEFT, 330, 360, options.c_str(), selected);

    // --- Ausschnitt ---
    // Ein fester Ausschnitt entsteht im Vollbild (zoomen/verschieben, dann X).
    // Hier laesst er sich nur zuruecksetzen.
    lv_coord_t helpY = 425;
    if (map->hasFixedView()) {
        const String fixedText = "Fest: Zoom " + String(map->getFixedZoom()) + " (aus Vollbild)";
        const String viewOptions = fixedText + "\nAutomatisch (alle Tracker)";
        UIHelper::createLabel(parent, "Ausschnitt:", &lv_font_montserrat_20, LV_ALIGN_TOP_LEFT, 10, 440);
        dd_view = UIHelper::createDropdown(parent, 400, 50, LV_ALIGN_TOP_LEFT, 330, 430, viewOptions.c_str(), 0);
        helpY = 495;
    }

    UIHelper::createLabel(parent, "Die Karte passt alle Tracker ein, zoomt aber nie naeher heran\n"
                                  "(16 = Strassen, 19 = Haeuser). Im Vollbild (Karte antippen)\n"
                                  "geht es mit +/- weiter; beim Schliessen bleibt der Ausschnitt.\n"
                                  "Meldet ein Tracker laenger nichts (Funkstille), erscheint er halb-\n"
                                  "durchsichtig an der letzten Position, und sein Weg wird dort unterbrochen.",
                          &lv_font_montserrat_16, LV_ALIGN_TOP_LEFT, 10, helpY, 0xAAAAAA);

    // Vorschlagsliste zuletzt: liegt ueber allem darunter.
    roller = lv_roller_create(parent);
    lv_obj_set_size(roller, 500, 110);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_16, 0);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(roller, roller_event_cb, LV_EVENT_VALUE_CHANGED, this);
}

void EditTabMap::saveConfig(HAWidget* w) {
    if (!w || w->getKind() != HaWidgetKind::Map) return;
    HAMapWidget* map = static_cast<HAMapWidget*>(w);

    if (entities != map->getMapEntities()) {
        map->setMapEntities(entities);
    }
    if (dd_hours) {
        const uint16_t i = lv_dropdown_get_selected(dd_hours);
        if (i < hour_values.size() && fabsf(hour_values[i] - map->getMapHours()) > 0.0001f) {
            map->setMapHours(hour_values[i]);
        }
    }
    if (dd_zoom) {
        const uint16_t i = lv_dropdown_get_selected(dd_zoom);
        if (i < zoom_values.size() && zoom_values[i] != map->getMapMaxZoom()) {
            map->setMapMaxZoom(zoom_values[i]);
        }
    }
    if (dd_stale) {
        const uint16_t i = lv_dropdown_get_selected(dd_stale);
        if (i < stale_values.size() && stale_values[i] != map->getMapStaleMinutes()) {
            map->setMapStaleMinutes(stale_values[i]);
        }
    }
    // Eintrag 1 = "Automatisch"
    if (dd_view && lv_dropdown_get_selected(dd_view) == 1) {
        map->clearFixedView();
    }
}
