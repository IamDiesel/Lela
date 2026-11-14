#include "HAMapWidget.h"
#include "HaWebsocketLogic.h"
#include "HaTrackHistory.h"
#include "HaDialogMap.h"
#include <algorithm>
#include <math.h>
#include <time.h>

// Ein ruhender Tracker erzeugt keine Aenderung - sein Weg muss trotzdem aus
// dem Zeitfenster herauslaufen, und "veraltet" haengt an der Uhrzeit. So oft
// werden Punkte und Wege deshalb neu geholt.
static const uint32_t TRACK_REBUILD_MS = 30000;

// Unterschiede darunter gelten beim festen Ausschnitt als gleich (~1 cm) -
// die Umrechnung Weltpixel <-> Grad ist nicht exakt umkehrbar.
static const double SAME_VIEW_DEG = 1e-7;

// Kleinerer Eckenradius als bei den anderen Kacheln: die Karte wird an den
// Ecken beschnitten (clip_corner), und ein grosser Radius frisst Karte.
static const lv_coord_t MAP_RADIUS = 12;

HAMapWidget::HAMapWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h,
                         const char* name, const char* mdi_icon, const char* c_on, const char* c_off,
                         const std::vector<String>& entities, int maxZoom, float hoursToShow)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off),
      entities(entities), max_zoom(maxZoom), hours_to_show(hoursToShow) {

    lv_obj_set_style_radius(container, MAP_RADIUS, 0);
    lv_obj_set_style_clip_corner(container, true, 0);

    // Kein Symbol; der Name dient als Titel oben links auf der Karte.
    lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(name_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(name_label, LV_OPA_50, 0);
    lv_obj_set_style_pad_hor(name_label, 8, 0);
    lv_obj_set_style_pad_ver(name_label, 3, 0);
    lv_obj_set_style_radius(name_label, 6, 0);

    lv_coord_t cw, ch;
    contentSize(w, h, cw, ch);
    view.setMaxZoom(max_zoom);
    view.create(container, cw, ch);
    view.setTrackStyle(historySeconds(), false);

    // Die Kartenobjekte liegen nach dem Anlegen ueber dem Titel.
    lv_obj_move_foreground(name_label);
    placeTitle();

    requestHistory();
}

HAMapWidget::~HAMapWidget() {
    HaTrackHistory::Unregister(this);
}

void HAMapWidget::contentSize(int w, int h, lv_coord_t& cw, lv_coord_t& ch) const {
    const lv_coord_t border = lv_obj_get_style_border_width(container, LV_PART_MAIN);
    cw = (lv_coord_t)(w - 2 * border);
    ch = (lv_coord_t)(h - 2 * border);
}

void HAMapWidget::placeTitle() {
    lv_obj_set_size(name_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(name_label, lv_obj_get_width(container) - 40, 0);
    lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 10, 8);
    if (widget_name.length() > 0) lv_obj_clear_flag(name_label, LV_OBJ_FLAG_HIDDEN);
    else                          lv_obj_add_flag(name_label, LV_OBJ_FLAG_HIDDEN);
}

void HAMapWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    // Werte merken (werden gespeichert), die Lage des Titels bestimmt aber die Karte.
    HAWidget::setAlignments(i_align, t_align, s_align, i_margin, t_margin, s_margin);
    placeTitle();
}

void HAMapWidget::setSize(int w, int h) {
    HAWidget::setSize(w, h);   // ruft setAlignments -> placeTitle
    lv_coord_t cw, ch;
    contentSize(w, h, cw, ch);
    view.resize(cw, ch);
}

// ------------------------------------------------------------------------------
// Einstellungen
// ------------------------------------------------------------------------------
void HAMapWidget::setMapMaxZoom(int zoom) {
    max_zoom = zoom;
    view.setMaxZoom(zoom);
    have_last = false;   // neu einpassen
    refreshFromCache();
}

bool HAMapWidget::setFixedView(double lat, double lon, int zoom) {
    if (fixed_view && zoom == fixed_zoom &&
        fabs(lat - fixed_lat) < SAME_VIEW_DEG && fabs(lon - fixed_lon) < SAME_VIEW_DEG) {
        return false;
    }
    fixed_view = true;
    fixed_lat = lat;
    fixed_lon = lon;
    fixed_zoom = zoom;
    view.setView(lat, lon, zoom);
    return true;
}

bool HAMapWidget::clearFixedView() {
    if (!fixed_view) return false;
    fixed_view = false;
    view.showAll();
    have_last = false;   // Punkte neu einpassen
    refreshFromCache();
    return true;
}

void HAMapWidget::setMapEntities(const std::vector<String>& list) {
    entities = list;
    reported_without_location.clear();
    requestHistory();
    have_last = false;
    refreshFromCache();
}

void HAMapWidget::setMapHours(float hours) {
    hours_to_show = hours < 0 ? 0 : hours;
    view.setTrackStyle(historySeconds(), false);
    requestHistory();
    have_last = false;   // Wege neu holen
    refreshFromCache();
}

void HAMapWidget::setMapStaleMinutes(int minutes) {
    stale_minutes = minutes < 0 ? 0 : minutes;
    have_last = false;   // veraltet/Unterbrechungen neu bewerten
    refreshFromCache();
}

uint32_t HAMapWidget::hoursToSeconds(float hours) {
    if (hours <= 0) return 0;
    const float s = hours * 3600.0f;
    return s > (float)HaTrackHistory::MAX_WINDOW_S ? HaTrackHistory::MAX_WINDOW_S : (uint32_t)s;
}

uint32_t HAMapWidget::sourceVersion() {
    // Summe der Aenderungszaehler: steigt, sobald sich eine Entitaet aendert.
    uint32_t sum = 0;
    for (const String& id : entities) sum += HaWebsocketLogic_GetEntityVersion(id);
    return sum;
}

void HAMapWidget::requestHistory() {
    // Anmeldung ersetzt die vorige; 0 s meldet ab (kein Weg = nichts laden).
    HaTrackHistory::Register(this, entities, historySeconds());
}

// ------------------------------------------------------------------------------
// Daten
// ------------------------------------------------------------------------------
// Gleiche Punkte (ohne Wege): Positionen, Radius, Name.
static bool samePositions(const std::vector<MapViewPoint>& a, const std::vector<MapViewPoint>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].id != b[i].id || a[i].lat != b[i].lat || a[i].lon != b[i].lon
            || a[i].radiusM != b[i].radiusM || a[i].label != b[i].label || a[i].stale != b[i].stale) {
            return false;
        }
    }
    return true;
}

void HAMapWidget::collectPositions(const std::vector<String>& entities, std::vector<MapViewPoint>& out,
                                   std::vector<String>* reported, uint32_t staleSeconds, bool feedHistory) {
    out.clear();
    out.reserve(entities.size());
    const time_t now = time(nullptr);

    for (const String& id : entities) {
        MapViewPoint p;
        p.id = id;
        p.label = formatEntityName(id, HaWebsocketLogic_GetEntityName(id));
        p.isZone = id.startsWith("zone.");

        float accuracy = 0;
        if (HaWebsocketLogic_GetLocation(id, p.lat, p.lon, accuracy)) {
            p.radiusM = p.isZone ? HaWebsocketLogic_GetZoneRadius(id) : accuracy;
            if (!p.isZone) {
                p.lastSeen = HaWebsocketLogic_GetLastUpdated(id);
                const String state = HaWebsocketLogic_GetState(id);
                p.stale = state == "unavailable" || state == "unknown";
                if (staleSeconds > 0 && p.lastSeen > 0 && now > (time_t)p.lastSeen &&
                    (uint32_t)(now - (time_t)p.lastSeen) > staleSeconds) {
                    p.stale = true;   // lange keine Meldung (z. B. Position eingefroren)
                }
                // Aktuelle Position in den Verlauf (unveraenderte werden dort
                // uebergangen). Zeit = Meldung in HA, nicht Abrufzeitpunkt.
                if (feedHistory) HaTrackHistory::Append(id, p.lat, p.lon, accuracy, p.lastSeen);
            }
            out.push_back(p);
            continue;
        }

        // Keine Position. Kennt HA die Entitaet, liefert sie gerade keine
        // (z. B. ausser Reichweite): Weg unterbrechen und die letzte bekannte
        // Position aus dem Verlauf veraltet zeigen.
        const bool knownToHa = HaWebsocketLogic_GetState(id).length() > 0;
        if (!p.isZone && knownToHa) {
            if (feedHistory) HaTrackHistory::MarkGap(id);
            HaTrackPoint last;
            if (HaTrackHistory::GetLast(id, last)) {
                p.lat = last.lat;
                p.lon = last.lon;
                p.stale = true;
                p.lastSeen = last.tsLast;
                out.push_back(p);
                continue;
            }
        }

        // Einmal melden, sobald HA die Entitaet kennt, sie aber keine
        // latitude/longitude-Attribute hat (z. B. Sensor ohne Position).
        if (reported && knownToHa && std::find(reported->begin(), reported->end(), id) == reported->end()) {
            reported->push_back(id);
            Serial.printf("[Map] %s hat keine Koordinaten (latitude/longitude) - wird nicht angezeigt.\n", id.c_str());
        }
    }
}

void HAMapWidget::attachTracks(std::vector<MapViewPoint>& points, uint32_t seconds, uint32_t gapSeconds) {
    if (seconds == 0) return;
    for (MapViewPoint& p : points) {
        if (p.isZone) continue;
        const std::vector<HaTrackPoint> track = HaTrackHistory::Get(p.id, seconds, gapSeconds);
        p.track.clear();
        p.track.reserve(track.size());
        for (const HaTrackPoint& t : track) p.track.push_back({ t.lat, t.lon, t.ts, t.brk });
    }
}

void HAMapWidget::refreshFromCache() {
    const uint32_t seconds = historySeconds();

    // Nur das Widget fuettert den Verlauf - es laeuft auch, waehrend das
    // Vollbild offen ist.
    std::vector<MapViewPoint> points;
    collectPositions(entities, points, &reported_without_location, staleSeconds(), seconds > 0);

    // Nichts Neues: weder Positionen noch Verlauf geaendert.
    const uint32_t historyVersion = HaTrackHistory::GetVersion();
    if (have_last && historyVersion == last_history_version && samePositions(points, last_points)) return;

    // Wege nur jetzt holen - also nur, wenn sich wirklich etwas bewegt hat.
    attachTracks(points, seconds, staleSeconds());

    view.setPoints(points);
    last_track_build_ms = millis();

    // Merken ohne Wege (die stecken jetzt in der Ansicht).
    for (MapViewPoint& p : points) { p.track.clear(); p.track.shrink_to_fit(); }
    last_points = std::move(points);
    last_history_version = historyVersion;
    have_last = true;
}

void HAMapWidget::tick() {
    // Verlauf nachgeladen, oder Zeit vergangen (Wege laufen aus dem Fenster,
    // Tracker werden veraltet).
    const bool timed = hours_to_show > 0 || stale_minutes > 0;
    if (timed && millis() - last_track_build_ms > TRACK_REBUILD_MS) {
        have_last = false;
        refreshFromCache();
    } else if (hours_to_show > 0 && HaTrackHistory::GetVersion() != last_history_version) {
        refreshFromCache();
    }
    view.tick();
}

void HAMapWidget::onClick() {
    // Tippen oeffnet die Vollbildkarte (im Edit-Modus kommt der Klick nicht
    // hier an, sondern oeffnet den Bearbeiten-Dialog).
    HaDialogMap::show(this);
}
