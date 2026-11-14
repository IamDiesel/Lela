#include "HaDialogMap.h"
#include "HAMapWidget.h"
#include "ViewHomeAssistant.h"
#include "UIHelper.h"
#include "SharedData.h"
#include "HaEntityCache.h"
#include "HaTrackHistory.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "MapView.h"
#include "MapTileConfig.h"
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// Ab dieser Strecke (Pixel) ist ein Druck ein Verschieben, kein Tippen.
static const lv_coord_t DRAG_THRESHOLD_PX = 12;
// Neuaufbau der Punkte hoechstens so oft (der Cache aendert sich mit jedem
// Sensor im Haus).
static const uint32_t REFRESH_MIN_MS = 500;

static const lv_coord_t BUTTON_SIZE = 72;

// ------------------------------------------------------------------------------
// Zustand (nur loopTask)
// ------------------------------------------------------------------------------
static lv_obj_t* s_overlay  = nullptr;
static lv_obj_t* s_area     = nullptr;   // Kartenflaeche, nimmt Ziehen entgegen
static lv_obj_t* s_hint     = nullptr;   // "verschieben ..." waehrend des Ziehens
static lv_obj_t* s_zoomLbl  = nullptr;
static lv_obj_t* s_info     = nullptr;   // Angaben zum angetippten Marker
static MapView*  s_view     = nullptr;   // gehoert dem Dialog, geloescht mit dem Overlay
static HAMapWidget* s_widget = nullptr;  // geoeffnetes Widget; vor Gebrauch pruefen (validWidget)

static std::vector<String> s_entities;
static std::vector<String> s_jumpIds;   // Eintraege der Auswahl "Springe zu" (gleiche Reihenfolge)
static int      s_jumpZoom = 0;         // Mindest-Zoom beim Anfliegen (Zoom des Widgets)
static uint32_t s_seconds = 0;
static uint32_t s_staleSeconds = 0;     // Funkstille des Widgets (veraltet, Unterbrechungen)
static uint32_t s_cacheVersion = 0;
static uint32_t s_historyVersion = 0;
static uint32_t s_lastRefresh = 0;
static uint8_t  s_shownZoom = 0;

static lv_coord_t s_dragX = 0;
static lv_coord_t s_dragY = 0;
static bool       s_dragging = false;
static bool       s_wasDrag = false;    // letzter Druck war ein Verschieben (kein Tippen)

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
static void refreshPoints() {
    if (!s_view) return;
    std::vector<MapViewPoint> points;
    // Den Verlauf fuettert nur das Widget (laeuft weiter, solange das Vollbild offen ist).
    HAMapWidget::collectPositions(s_entities, points, nullptr, s_staleSeconds, false);
    HAMapWidget::attachTracks(points, s_seconds, s_staleSeconds);
    s_view->setPoints(points);

    s_cacheVersion = HaEntityCache::GetCacheVersion();
    s_historyVersion = HaTrackHistory::GetVersion();
    s_lastRefresh = millis();
}

static void updateZoomLabel() {
    if (!s_view || !s_zoomLbl || s_view->zoom() == s_shownZoom) return;
    s_shownZoom = s_view->zoom();
    if (s_shownZoom > MAP_MAX_ZOOM) {
        lv_label_set_text_fmt(s_zoomLbl, "Zoom %u (x%u)", (unsigned)s_shownZoom, 1u << (s_shownZoom - MAP_MAX_ZOOM));
    } else {
        lv_label_set_text_fmt(s_zoomLbl, "Zoom %u", (unsigned)s_shownZoom);
    }
}

// "vor 3 min", "vor 2 h", "vor 40 s"; leer, wenn unbekannt.
static String formatAge(uint32_t ts) {
    const time_t now = time(nullptr);
    if (ts == 0 || now < (time_t)ts) return "";
    const uint32_t s = (uint32_t)(now - ts);
    char buf[24];
    if (s < 60)         snprintf(buf, sizeof(buf), "vor %u s", (unsigned)s);
    else if (s < 3600)  snprintf(buf, sizeof(buf), "vor %u min", (unsigned)(s / 60));
    else if (s < 86400) snprintf(buf, sizeof(buf), "vor %u h", (unsigned)(s / 3600));
    else                snprintf(buf, sizeof(buf), "vor %u Tagen", (unsigned)(s / 86400));
    return String(buf);
}

// Zustand eines Trackers lesbar: home/not_home wie in HA.
static String describeState(const String& state) {
    if (state == "home")     return "Zuhause";
    if (state == "not_home") return "Unterwegs";
    return state;
}

// Zeigt die Angaben zu einem angetippten Punkt (oder blendet sie aus).
static void showInfo(const MapViewPoint* p) {
    if (!s_info) return;
    if (!p) {
        lv_obj_add_flag(s_info, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    String text = p->label;
    if (p->isZone) {
        if (p->radiusM > 0) text += "\nZone, Radius " + String((int)lroundf(p->radiusM)) + " m";
    } else {
        const String state = HaWebsocketLogic_GetState(p->id);
        if (state.length()) text += "\n" + describeState(state);
        const String age = formatAge(p->lastSeen);
        if (p->stale) {
            // Genauigkeit gilt fuer eine alte Position nicht mehr.
            text += "\nKeine aktuelle Position";
            if (age.length()) text += "\nZuletzt gesehen " + age;
        } else {
            if (p->radiusM > 0) text += "\nGenauigkeit " + String((int)lroundf(p->radiusM)) + " m";
            if (age.length()) text += "\nAktualisiert " + age;
        }
    }
    lv_label_set_text(s_info, text.c_str());
    lv_obj_clear_flag(s_info, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* createRoundButton(lv_obj_t* parent, const char* symbol, uint32_t color,
                                   lv_align_t align, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BUTTON_SIZE, BUTTON_SIZE);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_90, 0);
    lv_obj_align(btn, align, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return btn;
}

// Das Widget, fuer das das Vollbild offen ist - nur, wenn es noch lebt (es
// kann waehrenddessen z. B. durch einen Neuaufbau des Bildschirms gegangen sein).
static HAMapWidget* validWidget() {
    if (!s_widget) return nullptr;
    const auto& all = HAWidget::all_active_widgets;
    if (std::find(all.begin(), all.end(), static_cast<HAWidget*>(s_widget)) == all.end()) return nullptr;
    if (s_widget->getKind() != HaWidgetKind::Map) return nullptr;
    return s_widget;
}

// Ausschnitt des Vollbilds ins Widget uebernehmen und bei Aenderung speichern.
// Das Vollbild oeffnet nur ausserhalb des Bearbeiten-Modus - es gibt also keine
// ungespeicherten Layout-Aenderungen, die beim Speichern mitgingen.
static void adoptViewIntoWidget() {
    HAMapWidget* w = validWidget();
    if (!w || !s_view) return;

    bool changed;
    double lat = 0, lon = 0;
    uint8_t zoom = 0;
    if (s_view->isManual() && s_view->getView(lat, lon, zoom)) {
        changed = w->setFixedView(lat, lon, zoom);
    } else {
        changed = w->clearFixedView();
    }
    if (!changed) return;

    ViewHomeAssistant::captureActiveTabLayout();
    HaConfigLogic::Save();
    if (w->hasFixedView()) {
        Serial.printf("[Map] Ausschnitt uebernommen: %.6f, %.6f, Zoom %u\n", lat, lon, (unsigned)zoom);
    } else {
        Serial.println("[Map] Ausschnitt wieder automatisch.");
    }
}

// Auswahl "Springe zu": Ziel zentrieren, mindestens mit dem Zoom des Widgets.
static void jumpTo(const String& id) {
    if (!s_view) return;
    // Position aus der Ansicht: dort steht auch die letzte bekannte eines
    // veralteten Trackers, der gerade keine liefert.
    const MapViewPoint* p = s_view->findPoint(id);
    if (!p) return;
    const double lat = p->lat;
    const double lon = p->lon;

    int zoom = s_view->zoom();
    if (zoom < s_jumpZoom) zoom = s_jumpZoom;
    s_view->setView(lat, lon, zoom);   // aendert die Punktliste nicht - p bleibt gueltig
    updateZoomLabel();
    showInfo(p);
}

// Auswahlliste oben links. Die Liste selbst legt LVGL auf dem Bildschirm an
// (nicht im Overlay) - deshalb auch dort die Gesten abfangen, sonst wechselt
// ein Wisch in der offenen Liste die App.
static void createJumpList(lv_obj_t* parent, lv_coord_t y) {
    std::vector<MapViewPoint> points;
    HAMapWidget::collectPositions(s_entities, points, nullptr, s_staleSeconds, false);

    s_jumpIds.clear();
    String options;
    for (int pass = 0; pass < 2; pass++) {          // Tracker zuerst, dann Zonen
        for (const MapViewPoint& p : points) {
            if (p.isZone != (pass == 1)) continue;
            if (options.length()) options += "\n";
            if (p.isZone) options += LV_SYMBOL_HOME " ";
            options += p.label;
            s_jumpIds.push_back(p.id);
        }
    }
    if (s_jumpIds.empty()) return;

    lv_obj_t* dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, options.c_str());
    lv_dropdown_set_text(dd, LV_SYMBOL_GPS "  Springe zu ...");   // feste Beschriftung
    lv_dropdown_set_selected_highlight(dd, false);
    lv_obj_set_size(dd, 340, 56);
    lv_obj_align(dd, LV_ALIGN_TOP_LEFT, 20, y);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_opa(dd, LV_OPA_90, 0);
    lv_obj_clear_flag(dd, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* list = lv_dropdown_get_list(dd);
    if (list) {
        lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
        lv_obj_set_style_max_height(list, 480, 0);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }

    lv_obj_add_event_cb(dd, [](lv_event_t* e) {
        const uint16_t i = lv_dropdown_get_selected(lv_event_get_target(e));
        if (i >= s_jumpIds.size()) return;
        playToneI2S(800, 50, true);
        jumpTo(s_jumpIds[i]);
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ------------------------------------------------------------------------------
// Ereignisse
// ------------------------------------------------------------------------------
// Ziehen auf der Karte: Weg sammeln, beim Loslassen einmal verschieben.
static void area_event_cb(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (!s_view) return;

    if (code == LV_EVENT_PRESSED) {
        s_dragX = 0;
        s_dragY = 0;
        s_dragging = false;
    } else if (code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t v;
        lv_indev_get_vect(indev, &v);
        s_dragX += v.x;
        s_dragY += v.y;
        if (!s_dragging && (abs(s_dragX) > DRAG_THRESHOLD_PX || abs(s_dragY) > DRAG_THRESHOLD_PX)) {
            s_dragging = true;
            if (s_hint) lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (s_hint) lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        s_wasDrag = s_dragging;
        if (s_dragging) {
            // Finger nach rechts = Karte nach rechts = Ausschnitt nach links.
            s_view->panBy(-s_dragX, -s_dragY);
            updateZoomLabel();
            showInfo(nullptr);   // Angaben gehoeren zum alten Ausschnitt
        }
        s_dragging = false;
    } else if (code == LV_EVENT_SHORT_CLICKED) {
        // Kommt nach RELEASED auch nach einem Verschieben - dann kein Tippen.
        if (s_wasDrag) return;
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_area_t area;
        lv_obj_get_coords(s_area, &area);
        showInfo(s_view->hitTest(p.x - area.x1, p.y - area.y1));
    }
    // LV_EVENT_GESTURE endet hier (GESTURE_BUBBLE ist geloescht) und wird
    // bewusst nicht ausgewertet: ein Wisch verschiebt nur die Karte.
}

static void overlay_deleted_cb(lv_event_t*) {
    // Das Overlay wird geloescht (Schliessen oder Bildschirmwechsel). Die
    // LVGL-Objekte gehen mit ihm, der Canvas-Puffer mit dem Canvas - hier nur
    // noch das MapView-Objekt selbst (es fasst im Destruktor kein LVGL an).
    delete s_view;
    s_view = nullptr;
    s_widget = nullptr;
    s_jumpIds.clear();
    s_area = nullptr;
    s_hint = nullptr;
    s_zoomLbl = nullptr;
    s_info = nullptr;
    s_entities.clear();
    s_shownZoom = 0;
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
bool HaDialogMap::isOpen() {
    return s_overlay != nullptr;
}

void HaDialogMap::show(HAMapWidget* w) {
    if (!w || s_overlay) return;
    playToneI2S(800, 100, true);

    s_widget = w;
    s_entities = w->getMapEntities();
    s_seconds = HAMapWidget::hoursToSeconds(w->getMapHours());
    s_staleSeconds = w->staleSeconds();
    s_jumpZoom = w->getMapMaxZoom();

    s_overlay = UIHelper::createModalOverlay(ViewHomeAssistant::screen, LV_OPA_COVER, &s_overlay);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_add_event_cb(s_overlay, overlay_deleted_cb, LV_EVENT_DELETE, nullptr);

    // Kartenflaeche: nimmt Druecken/Ziehen entgegen und gibt KEINE Gesten an
    // den Bildschirm weiter (zusaetzlich zum Overlay, das es auch nicht tut).
    s_area = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_area);
    lv_obj_set_size(s_area, UIHelper::SCREEN_W, UIHelper::SCREEN_H);
    lv_obj_set_pos(s_area, 0, 0);
    lv_obj_add_flag(s_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_area, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_area, area_event_cb, LV_EVENT_ALL, nullptr);

    s_view = new MapView();
    s_view->setMaxZoom(w->getMapMaxZoom());   // Obergrenze nur fuers Einpassen; +/- darf weiter
    if (!s_view->create(s_area, UIHelper::SCREEN_W, UIHelper::SCREEN_H)) {
        lv_obj_del_async(s_overlay);
        return;
    }

    // Ausschnitt des Widgets uebernehmen (fester Ausschnitt), sonst einpassen.
    if (w->hasFixedView()) s_view->setView(w->getFixedLat(), w->getFixedLon(), w->getFixedZoom());
    // Wege wie im Widget, zusaetzlich die Messpunkte.
    s_view->setTrackStyle(s_seconds, true);

    // Titel
    const String title = w->getName();
    if (title.length() > 0) {
        lv_obj_t* lbl = lv_label_create(s_area);
        lv_label_set_text(lbl, title.c_str());
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_bg_color(lbl, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_50, 0);
        lv_obj_set_style_pad_hor(lbl, 10, 0);
        lv_obj_set_style_pad_ver(lbl, 4, 0);
        lv_obj_set_style_radius(lbl, 8, 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 20, 20);
    }

    // Bedienung rechts
    createRoundButton(s_area, LV_SYMBOL_CLOSE, 0xC0392B, LV_ALIGN_TOP_RIGHT, -20, 20, [](lv_event_t*) {
        playToneI2S(600, 100, true);
        adoptViewIntoWidget();
        if (s_overlay) lv_obj_del_async(s_overlay);
    });
    createRoundButton(s_area, LV_SYMBOL_PLUS, 0x2C3E50, LV_ALIGN_RIGHT_MID, -20, -BUTTON_SIZE - 10, [](lv_event_t*) {
        playToneI2S(800, 50, true);
        if (s_view) { s_view->zoomBy(+1); updateZoomLabel(); }
    });
    createRoundButton(s_area, LV_SYMBOL_MINUS, 0x2C3E50, LV_ALIGN_RIGHT_MID, -20, 0, [](lv_event_t*) {
        playToneI2S(800, 50, true);
        if (s_view) { s_view->zoomBy(-1); updateZoomLabel(); }
    });
    // "Alle zeigen": zurueck zum automatischen Einpassen aller Tracker.
    createRoundButton(s_area, LV_SYMBOL_HOME, 0x16A085, LV_ALIGN_RIGHT_MID, -20, BUTTON_SIZE + 10, [](lv_event_t*) {
        playToneI2S(800, 50, true);
        if (s_view) { s_view->showAll(); updateZoomLabel(); }
    });

    s_zoomLbl = lv_label_create(s_area);
    lv_obj_set_style_text_font(s_zoomLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_zoomLbl, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_zoomLbl, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_zoomLbl, LV_OPA_50, 0);
    lv_obj_set_style_pad_hor(s_zoomLbl, 6, 0);
    lv_obj_set_style_radius(s_zoomLbl, 6, 0);
    lv_obj_align(s_zoomLbl, LV_ALIGN_RIGHT_MID, -14, 2 * BUTTON_SIZE + 40);
    lv_label_set_text(s_zoomLbl, "");

    s_hint = lv_label_create(s_area);
    lv_label_set_text(s_hint, LV_SYMBOL_LEFT LV_SYMBOL_UP LV_SYMBOL_DOWN LV_SYMBOL_RIGHT "  Loslassen zum Verschieben");
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_hint, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_hint, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(s_hint, 8, 0);
    lv_obj_set_style_radius(s_hint, 8, 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);

    s_info = lv_label_create(s_area);
    lv_obj_set_style_text_font(s_info, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_info, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_info, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(s_info, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(s_info, 12, 0);
    lv_obj_set_style_radius(s_info, 10, 0);
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_flag(s_info, LV_OBJ_FLAG_HIDDEN);

    refreshPoints();
    updateZoomLabel();

    // Nach den Punkten (Namen) und zuletzt: liegt ueber Karte und Hinweisen.
    createJumpList(s_area, title.length() > 0 ? 80 : 20);
}

void HaDialogMap::update() {
    if (!s_view) return;

    const bool changed = HaEntityCache::GetCacheVersion() != s_cacheVersion
                      || HaTrackHistory::GetVersion() != s_historyVersion;
    if (changed && millis() - s_lastRefresh > REFRESH_MIN_MS) refreshPoints();

    s_view->tick();
    updateZoomLabel();
}
