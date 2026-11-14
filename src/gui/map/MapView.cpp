// ==============================================================================
// MapView - Implementierung (siehe Header)
// ==============================================================================

#include "MapView.h"
#include "MapTileService.h"
#include "MapTileConfig.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <time.h>

static_assert(sizeof(lv_color_t) == sizeof(uint16_t),
              "MapView erwartet RGB565 (LV_COLOR_DEPTH 16) - wie MapTileService liefert");

// ------------------------------------------------------------------------------
// Parameter
// ------------------------------------------------------------------------------
static const uint8_t    MIN_ZOOM          = 3;
static const lv_coord_t FIT_PADDING       = 28;    // Rand um die Punkte (Pixel)
static const lv_coord_t ZOOM_IN_EXTRA_PAD = 40;    // Hysterese beim Hineinzoomen
static const float      INNER_AREA        = 0.8f;  // Verlaesst ein Punkt diesen Anteil: neuer Ausschnitt
static const uint32_t   TICK_INTERVAL_MS  = 250;
static const uint32_t   RETRY_INTERVAL_MS = 5000;  // fehlende Kacheln erneut anfordern
// Kommen Kacheln einzeln an, wird hoechstens so oft neu zusammengesetzt. Ein
// Neuzeichnen kostet im Vollbild ~0,7 s (LVGL-Flush) - je Kachel einmal neu
// zu zeichnen, wuerde das Bild minutenlang flackern lassen.
static const uint32_t   MIN_RECOMPOSE_MS  = 800;
static const lv_coord_t MARKER_SIZE       = 34;
static const lv_coord_t MAX_CIRCLE_PX     = 2000;  // lv_coord_t-Grenze mit Abstand
static const size_t     MAX_TRACK_DRAW    = 400;   // gezeichnete Wegpunkte je Marker (ausgeduennt)
static const lv_coord_t TRACK_CLAMP_PX    = 4000;  // Wegpunkte weit ausserhalb begrenzen (int16)
// Weg nach Alter: neuester Teil kraeftig und breit, aeltester blass und schmal.
static const lv_coord_t TRACK_WIDTH_NEW   = 4;
static const lv_coord_t TRACK_WIDTH_OLD   = 2;
static const lv_opa_t   TRACK_OPA_NEW     = LV_OPA_90;
static const lv_opa_t   TRACK_OPA_OLD     = LV_OPA_20;
static const lv_coord_t TRACK_DOT_RADIUS  = 3;
// Deckkraft haengt an der Uhrzeit: Wege so oft neu umrechnen, auch ohne Aenderung.
static const uint32_t   TRACK_RESTYLE_MS  = 60000;
// Veralteter Tracker: halbdurchsichtig in der eigenen Farbe (die Farbe bleibt
// die Kennung). Bewusst je Bestandteil statt style_opa fuer das ganze Objekt -
// das wuerde LVGL 8.3 ueber eine eigene Zwischenebene (Puffer) zeichnen.
static const lv_opa_t   STALE_FILL_OPA    = LV_OPA_40;
static const lv_opa_t   STALE_BORDER_OPA  = LV_OPA_50;
static const lv_opa_t   STALE_TEXT_OPA    = LV_OPA_70;

// Hintergrund fuer noch fehlende Kacheln (wie MapTileService).
static const uint32_t BACKGROUND_HEX = 0xDDDDDD;

// Markerfarben, je Entitaet fest aus der ID abgeleitet.
static const uint32_t MARKER_COLORS[] = { 0xE74C3C, 0x3498DB, 0x2ECC71, 0x9B59B6, 0xE67E22, 0x16A085 };

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
static uint32_t colorFor(const String& id) {
    uint32_t h = 0;
    for (size_t i = 0; i < id.length(); i++) h = h * 31 + (uint8_t)id[i];
    return MARKER_COLORS[h % (sizeof(MARKER_COLORS) / sizeof(MARKER_COLORS[0]))];
}

// Laenge des UTF-8-Zeichens, das bei s[i] beginnt.
static size_t utf8Len(uint8_t lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    return 4;
}

// "Lola Kippy" -> "LK", "Lola" -> "Lo". UTF-8-sicher (Umlaute).
static String initials(const String& name) {
    String out;
    int letters = 0;
    bool wordStart = true;
    for (size_t i = 0; i < name.length() && letters < 2; ) {
        const size_t n = utf8Len((uint8_t)name[i]);
        const char c = name[i];
        if (c == ' ' || c == '_' || c == '-') { wordStart = true; i += n; continue; }
        if (wordStart) {
            out += name.substring(i, i + n);
            letters++;
            wordStart = false;
        }
        i += n;
    }
    if (letters == 1 && name.length() > 1) {
        // Nur ein Wort: zweiten Buchstaben dazunehmen.
        const size_t first = utf8Len((uint8_t)name[0]);
        if (first < name.length()) out += name.substring(first, first + utf8Len((uint8_t)name[first]));
    }
    out.toUpperCase();
    return out;
}

static void freeCanvasBuffer(lv_event_t* e) {
    void* buf = lv_event_get_user_data(e);
    if (buf) heap_caps_free(buf);
}

// ------------------------------------------------------------------------------
// Wegebene
// ------------------------------------------------------------------------------
// Zeichendaten in Bildkoordinaten der Ansicht. Gehoeren dem Objekt der
// Wegebene (geloescht in dessen LV_EVENT_DELETE), damit LVGL sie auch dann
// zeichnen kann, wenn das MapView vorher geht (wie der Canvas-Puffer).
struct MapTrackLayerData {
    struct Seg { lv_point_t a; lv_point_t b; lv_opa_t opa; lv_coord_t width; };
    struct Dot { lv_point_t p; lv_opa_t opa; };
    struct Track {
        lv_color_t       color;
        std::vector<Seg> segs;
        std::vector<Dot> dots;
    };
    std::vector<Track> tracks;
};

static void trackLayerDraw(lv_event_t* e) {
    const auto* data = static_cast<const MapTrackLayerData*>(lv_event_get_user_data(e));
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    if (!data || !ctx) return;

    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(e), &area);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.round_end = 1;          // saubere Knicke zwischen Teilstuecken
    line.round_start = 1;

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    dot.border_width = 1;
    dot.border_color = lv_color_white();

    for (const auto& t : data->tracks) {
        line.color = t.color;
        for (const auto& s : t.segs) {
            const lv_point_t a = { (lv_coord_t)(s.a.x + area.x1), (lv_coord_t)(s.a.y + area.y1) };
            const lv_point_t b = { (lv_coord_t)(s.b.x + area.x1), (lv_coord_t)(s.b.y + area.y1) };
            line.opa = s.opa;
            line.width = s.width;
            lv_draw_line(ctx, &line, &a, &b);
        }
        dot.bg_color = t.color;
        for (const auto& d : t.dots) {
            const lv_area_t r = { (lv_coord_t)(d.p.x + area.x1 - TRACK_DOT_RADIUS), (lv_coord_t)(d.p.y + area.y1 - TRACK_DOT_RADIUS),
                                  (lv_coord_t)(d.p.x + area.x1 + TRACK_DOT_RADIUS), (lv_coord_t)(d.p.y + area.y1 + TRACK_DOT_RADIUS) };
            dot.bg_opa = d.opa;
            dot.border_opa = d.opa;
            lv_draw_rect(ctx, &dot, &r);
        }
    }
}

static void trackLayerFree(lv_event_t* e) {
    delete static_cast<MapTrackLayerData*>(lv_event_get_user_data(e));
}

static lv_obj_t* createCircle(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// ------------------------------------------------------------------------------
// Aufbau
// ------------------------------------------------------------------------------
MapView::~MapView() {
    // LVGL-Objekte gehoeren dem Bildschirm und werden mit ihm geloescht, der
    // Canvas-Puffer mit dem Canvas. Hier nur noch offene Kachelauftraege
    // verwerfen - niemand wartet mehr darauf.
    MapTileService_CancelPending();
}

bool MapView::allocateBuffer(lv_coord_t w, lv_coord_t h) {
    if (w <= 0 || h <= 0) return false;
    lv_color_t* buf = (lv_color_t*)heap_caps_malloc((size_t)w * h * sizeof(lv_color_t),
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        Serial.printf("[Map] FEHLER: kein PSRAM fuer die Kartenansicht (%dx%d).\n", (int)w, (int)h);
        return false;
    }

    // Alten Puffer erst freigeben, wenn der Canvas schon auf den neuen zeigt.
    lv_color_t* old = m_buf;
    m_buf = buf;
    m_w = w;
    m_h = h;
    lv_canvas_set_buffer(m_canvas, m_buf, m_w, m_h, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(m_canvas, lv_color_hex(BACKGROUND_HEX), LV_OPA_COVER);
    if (old) {
        lv_obj_remove_event_cb_with_user_data(m_canvas, freeCanvasBuffer, old);
        heap_caps_free(old);
    }
    lv_obj_add_event_cb(m_canvas, freeCanvasBuffer, LV_EVENT_DELETE, m_buf);
    return true;
}

bool MapView::create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    m_canvas = lv_canvas_create(parent);
    lv_obj_set_pos(m_canvas, 0, 0);
    if (!allocateBuffer(w, h)) return false;

    m_tracks = lv_obj_create(parent);
    lv_obj_remove_style_all(m_tracks);
    lv_obj_set_pos(m_tracks, 0, 0);
    lv_obj_set_size(m_tracks, w, h);
    lv_obj_clear_flag(m_tracks, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    m_trackData = new MapTrackLayerData();
    lv_obj_add_event_cb(m_tracks, trackLayerDraw, LV_EVENT_DRAW_MAIN, m_trackData);
    lv_obj_add_event_cb(m_tracks, trackLayerFree, LV_EVENT_DELETE, m_trackData);

    m_layer = lv_obj_create(parent);
    lv_obj_remove_style_all(m_layer);
    lv_obj_set_pos(m_layer, 0, 0);
    lv_obj_set_size(m_layer, w, h);
    lv_obj_clear_flag(m_layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    m_status = lv_label_create(parent);
    lv_obj_set_style_bg_color(m_status, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_status, LV_OPA_60, 0);
    lv_obj_set_style_text_color(m_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(m_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_all(m_status, 6, 0);
    lv_obj_set_style_radius(m_status, 6, 0);
    lv_obj_align(m_status, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(m_status, LV_OBJ_FLAG_HIDDEN);

    m_attrib = lv_label_create(parent);
    lv_label_set_text(m_attrib, "\xC2\xA9 OpenStreetMap");
    lv_obj_set_style_bg_color(m_attrib, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_attrib, LV_OPA_70, 0);
    lv_obj_set_style_text_color(m_attrib, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(m_attrib, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_hor(m_attrib, 4, 0);
    lv_obj_align(m_attrib, LV_ALIGN_BOTTOM_RIGHT, -8, -4);

    updateStatus();
    return true;
}

void MapView::resize(lv_coord_t w, lv_coord_t h) {
    if (!m_canvas || (w == m_w && h == m_h)) return;
    if (!allocateBuffer(w, h)) return;
    lv_obj_set_size(m_tracks, w, h);
    lv_obj_set_size(m_layer, w, h);
    // Fester Ausschnitt: Mitte und Zoom bleiben, nur der Rand waechst/schrumpft.
    if (!m_manual) m_haveView = false;
    updateView();
    placeMarkers();
    m_dirty = true;
    if (isVisible()) compose();
}

void MapView::setMaxZoom(int zoom) {
    if (zoom < MIN_ZOOM) zoom = MIN_ZOOM;
    if (zoom > MAP_MAX_VIEW_ZOOM) zoom = MAP_MAX_VIEW_ZOOM;
    if ((uint8_t)zoom == m_maxZoom) return;
    m_maxZoom = (uint8_t)zoom;
    // Nur das Einpassen haengt daran - ein fester Ausschnitt bleibt.
    if (!m_manual) m_haveView = false;
}

bool MapView::isVisible() const {
    return m_canvas && lv_obj_is_valid(m_canvas) && lv_obj_is_visible(m_canvas);
}

// ------------------------------------------------------------------------------
// Ausschnitt
// ------------------------------------------------------------------------------
bool MapView::fitView(lv_coord_t extraPad, uint8_t& zoom, MapPoint& center) const {
    if (m_points.empty()) return false;

    // Zonen gehen mit ihrem Mittelpunkt ein, nicht mit dem Radius: so bleiben
    // Zuhause und Tracker gross im Bild (wie HA ohne fit_zones).
    const double availW = (double)m_w - 2.0 * (FIT_PADDING + extraPad);
    const double availH = (double)m_h - 2.0 * (FIT_PADDING + extraPad);

    for (int z = m_maxZoom; z >= MIN_ZOOM; z--) {
        double minX, minY, maxX, maxY;
        bounds((uint8_t)z, minX, minY, maxX, maxY);
        if ((maxX - minX) <= availW && (maxY - minY) <= availH) {
            zoom = (uint8_t)z;
            center = { (minX + maxX) / 2.0, (minY + maxY) / 2.0 };
            return true;
        }
    }

    const MapViewPoint& p = m_points.front();
    zoom = MIN_ZOOM;
    center = MapProjection_LatLonToPixel(p.lat, p.lon, MIN_ZOOM);
    return true;
}

void MapView::bounds(uint8_t zoom, double& minX, double& minY, double& maxX, double& maxY) const {
    minX = 1e18; minY = 1e18; maxX = -1e18; maxY = -1e18;
    auto add = [&](double lat, double lon) {
        const MapPoint q = MapProjection_LatLonToPixel(lat, lon, zoom);
        minX = fmin(minX, q.x); maxX = fmax(maxX, q.x);
        minY = fmin(minY, q.y); maxY = fmax(maxY, q.y);
    };
    for (const MapViewPoint& p : m_points) {
        add(p.lat, p.lon);
        for (const MapTrackPoint& t : p.track) add(t.lat, t.lon);
    }
}

MapPoint MapView::pointsCenter(uint8_t zoom) const {
    double minX, minY, maxX, maxY;
    bounds(zoom, minX, minY, maxX, maxY);
    return { (minX + maxX) / 2.0, (minY + maxY) / 2.0 };
}

bool MapView::updateView() {
    // Von Hand gewaehlter Ausschnitt (Vollbild): nicht automatisch aendern.
    if (m_manual && m_haveView) return false;

    uint8_t z;
    MapPoint c;
    if (!fitView(0, z, c)) return false;

    bool apply = !m_haveView || z < m_zoom;

    // Hineinzoomen nur mit Abstand, sonst pendelt die Ansicht an der Grenze.
    if (!apply && z > m_zoom) {
        uint8_t zStrict;
        MapPoint cStrict;
        if (fitView(ZOOM_IN_EXTRA_PAD, zStrict, cStrict) && zStrict > m_zoom) {
            z = zStrict;
            c = cStrict;
            apply = true;
        }
    }

    // Gleiche Zoomstufe: nur neu zentrieren, wenn ein Punkt den inneren
    // Bereich verlassen hat.
    if (!apply) {
        const double halfW = m_w * INNER_AREA / 2.0;
        const double halfH = m_h * INNER_AREA / 2.0;
        for (const MapViewPoint& p : m_points) {
            const MapPoint q = MapProjection_LatLonToPixel(p.lat, p.lon, m_zoom);
            if (fabs(q.x - m_center.x) > halfW || fabs(q.y - m_center.y) > halfH) {
                // Zoomstufe bleibt (Hineinzoomen war oben nicht erlaubt).
                z = m_zoom;
                c = pointsCenter(m_zoom);
                apply = true;
                break;
            }
        }
    }

    if (!apply) return false;
    m_haveView = true;
    m_zoom = z;
    m_center = c;
    m_dirty = true;
    return true;
}

// ------------------------------------------------------------------------------
// Kartenbild
// ------------------------------------------------------------------------------
void MapView::compose() {
    if (!m_buf || !m_haveView) return;

    m_dirty = false;
    m_lastCompose = millis();
    m_seenVersion = MapTileService_GetVersion();

    lv_canvas_fill_bg(m_canvas, lv_color_hex(BACKGROUND_HEX), LV_OPA_COVER);

    const int32_t left = (int32_t)floor(m_center.x - m_w / 2.0);
    const int32_t top  = (int32_t)floor(m_center.y - m_h / 2.0);

    // Ueberzoom: jenseits von MAP_MAX_ZOOM die Kacheln dieser Stufe nehmen und
    // um 2^shift vergroessern. Eine Kachel ist dann 'edge' Bildpixel breit.
    const uint8_t tileZoom = m_zoom > MAP_MAX_ZOOM ? MAP_MAX_ZOOM : m_zoom;
    const uint8_t shift    = m_zoom - tileZoom;
    const int32_t edge     = (int32_t)MAP_TILE_PX << shift;
    const int32_t tiles    = (int32_t)MapProjection_TilesPerAxis(tileZoom);

    const int32_t tx0 = (int32_t)floor(left / (double)edge);
    const int32_t ty0 = (int32_t)floor(top / (double)edge);
    const int32_t tx1 = (int32_t)floor((left + m_w - 1) / (double)edge);
    const int32_t ty1 = (int32_t)floor((top + m_h - 1) / (double)edge);

    m_missingTiles = 0;
    m_totalTiles = 0;
    uint16_t* dst = reinterpret_cast<uint16_t*>(m_buf);

    for (int32_t ty = ty0; ty <= ty1; ty++) {
        if (ty < 0 || ty >= tiles) continue;               // jenseits der Pole: Hintergrund
        for (int32_t tx = tx0; tx <= tx1; tx++) {
            const int32_t wrappedX = ((tx % tiles) + tiles) % tiles;   // Datumsgrenze
            const MapTileKey key = MapTileKey_Make(tileZoom, (uint32_t)wrappedX, (uint32_t)ty);

            // Schnitt Kachel / Ansicht (in Bildpixeln der vergroesserten Kachel)
            const int32_t px = tx * edge - left;
            const int32_t py = ty * edge - top;
            const int32_t dx = px < 0 ? 0 : px;
            const int32_t dy = py < 0 ? 0 : py;
            const int32_t sx = dx - px;
            const int32_t sy = dy - py;
            const int32_t cw = edge - sx < m_w - dx ? edge - sx : m_w - dx;
            const int32_t ch = edge - sy < m_h - dy ? edge - sy : m_h - dy;
            if (cw <= 0 || ch <= 0) continue;

            m_totalTiles++;
            if (!MapTileService_Request(key) ||
                !MapTileService_CopyTile(key, dst + (size_t)dy * m_w + dx, (uint16_t)m_w,
                                         (uint32_t)sx, (uint32_t)sy, (uint16_t)cw, (uint16_t)ch, shift)) {
                m_missingTiles++;
            }
        }
    }

    lv_obj_invalidate(m_canvas);
    updateStatus();
}

// ------------------------------------------------------------------------------
// Marker
// ------------------------------------------------------------------------------
void MapView::syncMarkers() {
    // Wegfallende Punkte: Objekte loeschen.
    for (size_t i = 0; i < m_markers.size(); ) {
        bool keep = false;
        for (const MapViewPoint& p : m_points) {
            if (p.id == m_markers[i].id) { keep = true; break; }
        }
        if (keep) { i++; continue; }
        if (m_markers[i].area)  lv_obj_del(m_markers[i].area);
        if (m_markers[i].dot)   lv_obj_del(m_markers[i].dot);
        if (m_markers[i].label) lv_obj_del(m_markers[i].label);
        m_markers.erase(m_markers.begin() + i);
    }

    // Neue Punkte: Objekte anlegen. Zonen zuerst (liegen unter den Trackern).
    for (int pass = 0; pass < 2; pass++) {
        for (const MapViewPoint& p : m_points) {
            if (p.isZone != (pass == 0)) continue;
            bool exists = false;
            for (const Marker& m : m_markers) {
                if (m.id == p.id) { exists = true; break; }
            }
            if (exists) continue;

            Marker m;
            m.id = p.id;
            const lv_color_t color = lv_color_hex(p.isZone ? 0x2980B9 : colorFor(p.id));

            m.area = createCircle(m_layer);
            lv_obj_set_style_bg_color(m.area, color, 0);
            lv_obj_set_style_bg_opa(m.area, p.isZone ? LV_OPA_20 : LV_OPA_10, 0);
            lv_obj_set_style_border_color(m.area, color, 0);
            lv_obj_set_style_border_width(m.area, p.isZone ? 2 : 1, 0);
            lv_obj_set_style_border_opa(m.area, LV_OPA_70, 0);

            if (!p.isZone) {
                m.dot = createCircle(m_layer);
                lv_obj_set_size(m.dot, MARKER_SIZE, MARKER_SIZE);
                lv_obj_set_style_bg_color(m.dot, color, 0);
                lv_obj_set_style_bg_opa(m.dot, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(m.dot, lv_color_white(), 0);
                lv_obj_set_style_border_width(m.dot, 2, 0);
            }

            m.label = lv_label_create(m_layer);
            lv_obj_set_style_text_color(m.label, p.isZone ? color : lv_color_white(), 0);
            lv_obj_set_style_text_font(m.label, p.isZone ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
            lv_label_set_text(m.label, p.isZone ? LV_SYMBOL_HOME : initials(p.label).c_str());

            m_markers.push_back(m);
        }
    }
}

void MapView::placeMarkers() {
    if (!m_haveView) {
        for (Marker& m : m_markers) {
            if (m.area)  lv_obj_add_flag(m.area, LV_OBJ_FLAG_HIDDEN);
            if (m.dot)   lv_obj_add_flag(m.dot, LV_OBJ_FLAG_HIDDEN);
            if (m.label) lv_obj_add_flag(m.label, LV_OBJ_FLAG_HIDDEN);
        }
        layoutTracks();
        return;
    }

    const double left = m_center.x - m_w / 2.0;
    const double top  = m_center.y - m_h / 2.0;

    for (Marker& m : m_markers) {
        const MapViewPoint* p = nullptr;
        for (const MapViewPoint& q : m_points) {
            if (q.id == m.id) { p = &q; break; }
        }
        if (!p) continue;

        const MapPoint w = MapProjection_LatLonToPixel(p->lat, p->lon, m_zoom);
        const lv_coord_t x = (lv_coord_t)lround(w.x - left);
        const lv_coord_t y = (lv_coord_t)lround(w.y - top);

        if (m.stale != p->stale) applyMarkerStyle(m, *p);

        // Kreis: Zonenradius bzw. Genauigkeit. Kleiner als der Marker oder
        // veraltet (Genauigkeit gilt nicht mehr): weglassen.
        const double mpp = MapProjection_MetersPerPixel(p->lat, m_zoom);
        double r = (p->radiusM > 0 && mpp > 0 && !p->stale) ? p->radiusM / mpp : 0;
        if (r > MAX_CIRCLE_PX) r = MAX_CIRCLE_PX;
        if (m.area) {
            if (r * 2 > MARKER_SIZE) {
                const lv_coord_t d = (lv_coord_t)lround(r * 2);
                lv_obj_set_size(m.area, d, d);
                lv_obj_set_pos(m.area, x - d / 2, y - d / 2);
                lv_obj_clear_flag(m.area, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(m.area, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (m.dot) {
            lv_obj_set_pos(m.dot, x - MARKER_SIZE / 2, y - MARKER_SIZE / 2);
            lv_obj_clear_flag(m.dot, LV_OBJ_FLAG_HIDDEN);
        }
        if (m.label) {
            lv_obj_update_layout(m.label);
            lv_obj_set_pos(m.label, x - lv_obj_get_width(m.label) / 2, y - lv_obj_get_height(m.label) / 2);
            lv_obj_clear_flag(m.label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    layoutTracks();
}

void MapView::applyMarkerStyle(Marker& m, const MapViewPoint& p) {
    m.stale = p.stale;
    if (!m.dot) return;   // Zonen werden nie veraltet
    const lv_opa_t full = LV_OPA_COVER;
    lv_obj_set_style_bg_opa(m.dot, p.stale ? STALE_FILL_OPA : full, 0);
    lv_obj_set_style_border_opa(m.dot, p.stale ? STALE_BORDER_OPA : full, 0);
    if (m.label) lv_obj_set_style_text_opa(m.label, p.stale ? STALE_TEXT_OPA : full, 0);
}

void MapView::setTrackStyle(uint32_t windowSeconds, bool showDots) {
    if (windowSeconds == m_trackWindowS && showDots == m_showDots) return;
    m_trackWindowS = windowSeconds;
    m_showDots = showDots;
    layoutTracks();
}

void MapView::layoutTracks() {
    if (!m_trackData || !m_tracks) return;
    m_trackData->tracks.clear();
    m_lastTrackLayout = millis();

    if (m_haveView) {
        const double left = m_center.x - m_w / 2.0;
        const double top  = m_center.y - m_h / 2.0;
        const time_t now = time(nullptr);

        auto clampPx = [](double v) -> lv_coord_t {
            if (v < -TRACK_CLAMP_PX) return -TRACK_CLAMP_PX;
            if (v >  TRACK_CLAMP_PX) return  TRACK_CLAMP_PX;
            return (lv_coord_t)lround(v);
        };
        // Alter 0..1 relativ zum Zeitfenster (0 = jetzt).
        auto ageOf = [&](uint32_t ts) -> float {
            if (m_trackWindowS == 0 || ts == 0 || now <= (time_t)ts) return 0.0f;
            const float f = (float)(now - (time_t)ts) / (float)m_trackWindowS;
            return f > 1.0f ? 1.0f : f;
        };

        for (const MapViewPoint& p : m_points) {
            const size_t n = p.track.size();
            if (p.isZone || n == 0 || (n < 2 && !m_showDots)) continue;

            MapTrackLayerData::Track t;
            t.color = lv_color_hex(colorFor(p.id));

            // Ausduennen: jeder n-te Punkt, der letzte (aktuelle Position)
            // immer. Eine Unterbrechung auf einem ausgelassenen Punkt gilt
            // fuer das ueberbrueckende Teilstueck.
            const size_t step = (n + MAX_TRACK_DRAW - 1) / MAX_TRACK_DRAW;
            bool carryBreak = false;
            bool havePrev = false;
            lv_point_t prev = { 0, 0 };
            for (size_t i = 0; i < n; i++) {
                carryBreak = carryBreak || p.track[i].brk;
                if (i % step != 0 && i != n - 1) continue;

                const MapPoint w = MapProjection_LatLonToPixel(p.track[i].lat, p.track[i].lon, m_zoom);
                const lv_point_t cur = { clampPx(w.x - left), clampPx(w.y - top) };
                const float age = ageOf(p.track[i].ts);
                const lv_opa_t opa = (lv_opa_t)lroundf(TRACK_OPA_NEW - age * (TRACK_OPA_NEW - TRACK_OPA_OLD));

                if (havePrev && !carryBreak) {
                    const lv_coord_t width = (lv_coord_t)lroundf(TRACK_WIDTH_NEW - age * (TRACK_WIDTH_NEW - TRACK_WIDTH_OLD));
                    t.segs.push_back({ prev, cur, opa, width });
                }
                if (m_showDots) t.dots.push_back({ cur, opa });

                prev = cur;
                havePrev = true;
                carryBreak = false;
            }
            m_trackData->tracks.push_back(std::move(t));
        }
    }
    lv_obj_invalidate(m_tracks);
}

void MapView::updateStatus() {
    if (!m_status) return;

    const char* text = nullptr;
    if (m_points.empty()) {
        text = "Keine Position verfuegbar";
    } else if (m_missingTiles > 0 && MapTileService_IsMemoryPaused()) {
        text = "Karte pausiert - Speicher knapp";
    } else if (m_totalTiles > 0 && m_missingTiles == m_totalTiles) {
        text = "Karte wird geladen ...";
    }

    if (text) {
        lv_label_set_text(m_status, text);
        lv_obj_clear_flag(m_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m_status, LV_OBJ_FLAG_HIDDEN);
    }
}

// ------------------------------------------------------------------------------
// Punkte und Takt
// ------------------------------------------------------------------------------
static bool samePoints(const std::vector<MapViewPoint>& a, const std::vector<MapViewPoint>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        // ~1 cm: kleinere Aenderungen sind GPS-Rauschen ohne sichtbare Wirkung.
        if (a[i].id != b[i].id || a[i].label != b[i].label || a[i].isZone != b[i].isZone
            || fabs(a[i].lat - b[i].lat) > 1e-7 || fabs(a[i].lon - b[i].lon) > 1e-7
            || fabsf(a[i].radiusM - b[i].radiusM) > 0.5f || a[i].stale != b[i].stale) {
            return false;
        }
        // Weg: Laenge und Endpunkte genuegen (Punkte kommen nur hinzu oder
        // fallen vorne weg).
        const auto& ta = a[i].track;
        const auto& tb = b[i].track;
        if (ta.size() != tb.size()) return false;
        if (!ta.empty() && (ta.front().lat != tb.front().lat || ta.front().lon != tb.front().lon
                            || ta.back().lat != tb.back().lat || ta.back().lon != tb.back().lon)) {
            return false;
        }
    }
    return true;
}

void MapView::setPoints(const std::vector<MapViewPoint>& points) {
    if (!m_canvas) return;
    if (m_haveView && samePoints(points, m_points)) return;

    m_points = points;
    syncMarkers();
    updateView();
    placeMarkers();
    updateStatus();
    if (m_dirty && isVisible()) compose();
}

void MapView::tick() {
    const uint32_t now = millis();
    if (now - m_lastTick < TICK_INTERVAL_MS) return;
    m_lastTick = now;

    if (!isVisible()) {
        m_wasVisible = false;
        return;
    }

    // Gerade sichtbar geworden (Tabwechsel) oder Ausschnitt geaendert, waehrend
    // die Karte nicht sichtbar war.
    if (!m_wasVisible || m_dirty) {
        m_wasVisible = true;
        if (m_haveView) compose();
        return;
    }

    // Deckkraft nach Alter nachfuehren (nur die Wegebene, kein Kartenbild).
    if (now - m_lastTrackLayout > TRACK_RESTYLE_MS) layoutTracks();

    if (m_missingTiles == 0) return;

    // Neue Kacheln da (gedrosselt), oder lange nichts gekommen (Auftraege am
    // Ventil oder an einer Netzpause verworfen): neu zusammensetzen und dabei
    // nachfordern.
    const bool newTiles = MapTileService_GetVersion() != m_seenVersion && now - m_lastCompose > MIN_RECOMPOSE_MS;
    if (newTiles || now - m_lastCompose > RETRY_INTERVAL_MS) {
        compose();
    } else {
        updateStatus();
    }
}

// ------------------------------------------------------------------------------
// Antippen
// ------------------------------------------------------------------------------
const MapViewPoint* MapView::hitTest(lv_coord_t x, lv_coord_t y) const {
    if (!m_haveView) return nullptr;
    const double left = m_center.x - m_w / 2.0;
    const double top  = m_center.y - m_h / 2.0;
    // Etwas groesser als der Marker: mit dem Finger trifft man nicht genau.
    const double hitRadius = MARKER_SIZE;

    const MapViewPoint* zoneHit = nullptr;
    for (const MapViewPoint& p : m_points) {
        const MapPoint w = MapProjection_LatLonToPixel(p.lat, p.lon, m_zoom);
        const double dx = (w.x - left) - x;
        const double dy = (w.y - top) - y;
        const double dist = sqrt(dx * dx + dy * dy);
        if (!p.isZone) {
            if (dist <= hitRadius) return &p;   // Tracker haben Vorrang
            continue;
        }
        const double mpp = MapProjection_MetersPerPixel(p.lat, m_zoom);
        const double r = (p.radiusM > 0 && mpp > 0) ? fmax(p.radiusM / mpp, hitRadius) : hitRadius;
        if (!zoneHit && dist <= r) zoneHit = &p;
    }
    return zoneHit;
}

// ------------------------------------------------------------------------------
// Von Hand gefuehrter Ausschnitt (Vollbild)
// ------------------------------------------------------------------------------
void MapView::applyManualView(uint8_t zoom, const MapPoint& center) {
    m_manual = true;
    m_haveView = true;
    m_zoom = zoom;
    m_center = center;
    placeMarkers();
    m_dirty = true;
    if (isVisible()) compose();
}

void MapView::zoomBy(int delta) {
    if (!m_haveView) return;
    int z = (int)m_zoom + delta;
    if (z < MIN_ZOOM) z = MIN_ZOOM;
    if (z > MAP_MAX_VIEW_ZOOM) z = MAP_MAX_VIEW_ZOOM;
    if (z == m_zoom) return;

    // Gleicher geografischer Mittelpunkt: Weltpixel skalieren mit 2^Stufe.
    const double factor = ldexp(1.0, z - (int)m_zoom);
    applyManualView((uint8_t)z, { m_center.x * factor, m_center.y * factor });
}

void MapView::panBy(lv_coord_t dx, lv_coord_t dy) {
    if (!m_haveView || (dx == 0 && dy == 0)) return;
    applyManualView(m_zoom, { m_center.x + dx, m_center.y + dy });
}

void MapView::centerOn(double lat, double lon) {
    if (!m_haveView) return;
    applyManualView(m_zoom, MapProjection_LatLonToPixel(lat, lon, m_zoom));
}

void MapView::setView(double lat, double lon, int zoom) {
    if (zoom < MIN_ZOOM) zoom = MIN_ZOOM;
    if (zoom > MAP_MAX_VIEW_ZOOM) zoom = MAP_MAX_VIEW_ZOOM;
    applyManualView((uint8_t)zoom, MapProjection_LatLonToPixel(lat, lon, (uint8_t)zoom));
}

bool MapView::getView(double& lat, double& lon, uint8_t& zoom) const {
    if (!m_haveView) return false;
    MapProjection_PixelToLatLon(m_center, m_zoom, lat, lon);
    zoom = m_zoom;
    return true;
}

const MapViewPoint* MapView::findPoint(const String& id) const {
    for (const MapViewPoint& p : m_points) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void MapView::showAll() {
    m_manual = false;
    m_haveView = false;
    updateView();
    placeMarkers();
    updateStatus();
    if (isVisible()) compose();
}
