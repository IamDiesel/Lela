#pragma once
// ==============================================================================
// MapView - Kartenansicht (LVGL) mit Markern und Zonen
// ==============================================================================
// Wiederverwendbare Komponente: kennt LVGL und den Kachel-Dienst
// (MapTileService), aber kein Home Assistant. Was dargestellt wird, bekommt sie
// als Punktliste (setPoints) - das HA-Widget (HAMapWidget) liest dafuer den
// Entitaets-Cache.
//
// Aufbau im Eltern-Objekt (Reihenfolge = Zeichenreihenfolge):
//   Canvas (Kartenbild, Puffer im PSRAM)
//   Wegebene: alle Wege, selbst gezeichnet (LV_EVENT_DRAW_MAIN)
//   Markerebene: je Punkt Zonen- bzw. Genauigkeitskreis + Marker mit Initialen
//   Statuszeile (Laden, keine Position, Speicher knapp)
//   Quellenhinweis "(c) OpenStreetMap" (Pflicht laut Tile-Usage-Policy)
// Die Markerebene ist ein eigenes Objekt: neue Marker landen darin und
// schieben sich nicht ueber Objekte, die der Aufrufer spaeter darueberlegt.
// Keines dieser Objekte ist klickbar - Klicks und Ziehen (Edit-Modus) gehen an
// das Eltern-Objekt.
//
// Ausschnitt: alle Punkte samt ihrer Wege werden eingepasst, hoechstens bis
// maxZoom.
//
// Wege: Linie in der Farbe des Markers, aeltere Teile blasser und duenner
// (relativ zum Zeitfenster, setTrackStyle). Unterbrechungen (brk) werden
// nicht verbunden. Auf Wunsch (Vollbild) die Messpunkte als kleine Punkte.
// Gezeichnet wird in EINEM Objekt statt je Weg ein lv_line: Deckkraft je
// Teilstueck, Luecken und Punkte gehen mit lv_line nicht.
//
// Veraltete Tracker (stale: lange keine Meldung oder keine Position mehr)
// erscheinen halbdurchsichtig in ihrer Farbe an der letzten bekannten
// Position, ohne Genauigkeitskreis. Ueber
// MAP_MAX_ZOOM (19) hinaus werden die Kacheln vergroessert (bis
// MAP_MAX_VIEW_ZOOM). Der Ausschnitt kann von Hand gefuehrt werden (zoomBy,
// panBy, centerOn im Vollbild) oder fest vorgegeben sein (setView, aus dem
// Vollbild uebernommen), bis showAll() wieder einpasst.
// Sparsam (Konzept, Abschnitt 3.3):
//   * Ein neuer Ausschnitt nur, wenn ein Punkt den inneren Bereich verlaesst
//     oder eine andere Zoomstufe noetig ist - GPS-Zittern laedt keine Kacheln.
//   * Bewegt sich nur ein Marker, wird nur der Marker verschoben.
//   * Kacheln werden nur angefordert, solange die Ansicht sichtbar ist.
//
// Speicher: Canvas-Puffer im PSRAM (Breite x Hoehe x 2 Byte). Er gehoert dem
// Canvas-Objekt und wird mit ihm geloescht (LV_EVENT_DELETE) - so bleibt er
// gueltig, solange LVGL ihn zeichnen kann, auch wenn das Widget vorher geht.
// Ebenso gehoeren die Zeichendaten der Wege der Wegebene.
//
// Nur im LVGL-Kontext (loopTask) benutzen.
// ==============================================================================

#include <Arduino.h>
#include <lvgl.h>
#include <vector>
#include "MapProjection.h"

// Ein Punkt eines Weges (Positionsverlauf).
struct MapTrackPoint {
    float    lat;
    float    lon;
    uint32_t ts;     // Unix-Zeit (fuer die Deckkraft nach Alter)
    bool     brk;    // keine Linie vom vorigen Punkt hierher
};

// Ein darzustellender Punkt.
struct MapViewPoint {
    String id;          // eindeutig (Entity-ID), bestimmt auch die Markerfarbe
    String label;       // Anzeigename; der Marker zeigt die Initialen
    double lat = 0;
    double lon = 0;
    float  radiusM = 0; // Zone: Radius; sonst: GPS-Genauigkeit (0 = unbekannt)
    bool   isZone = false;
    bool   stale = false;    // veraltet: halbdurchsichtig an der letzten bekannten Position
    uint32_t lastSeen = 0;   // letzte Meldung (Unix-Zeit, 0 = unbekannt)
    std::vector<MapTrackPoint> track;   // zurueckgelegter Weg, aelteste zuerst (leer = keiner)
};

// Zeichendaten der Wegebene (nur MapView.cpp).
struct MapTrackLayerData;

class MapView {
public:
    MapView() = default;
    ~MapView();

    MapView(const MapView&) = delete;
    MapView& operator=(const MapView&) = delete;

    // Legt die Ansicht in 'parent' an (Inhaltsbereich ab 0,0, Groesse w x h).
    bool create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);

    // Neue Groesse (Bearbeiten-Dialog). Passt den Ausschnitt neu ein.
    void resize(lv_coord_t w, lv_coord_t h);

    // Hoechste Zoomstufe beim Einpassen (begrenzt auf MAP_MAX_ZOOM).
    void setMaxZoom(int zoom);

    // Darstellung der Wege: Zeitfenster fuer die Deckkraft nach Alter
    // (0 = alle gleich kraeftig), Messpunkte zeigen (Vollbild).
    void setTrackStyle(uint32_t windowSeconds, bool showDots);

    // Ersetzt die Punktliste. Ohne Aenderung billig (Vergleich).
    void setPoints(const std::vector<MapViewPoint>& points);

    // Regelmaessig aufrufen: zieht nachgeladene Kacheln nach.
    void tick();

    // --- Von Hand gefuehrter bzw. fester Ausschnitt ---
    // Ab dem ersten Aufruf passt die Ansicht nicht mehr selbst ein, bis
    // showAll() aufgerufen wird. Zoom bis MAP_MAX_VIEW_ZOOM (Ueberzoom).
    void zoomBy(int delta);                    // +1 naeher, -1 weiter weg
    void panBy(lv_coord_t dx, lv_coord_t dy);  // Ausschnitt um Bildpixel verschieben
    void centerOn(double lat, double lon);     // Mitte setzen, Zoom bleibt
    void setView(double lat, double lon, int zoom);   // fester Ausschnitt (Mitte + Zoom)
    void showAll();                            // zurueck zum automatischen Einpassen
    bool isManual() const { return m_manual; }
    uint8_t zoom() const { return m_zoom; }
    // Aktueller Ausschnitt (Mitte, Zoom). false, solange es keinen gibt.
    bool getView(double& lat, double& lon, uint8_t& zoom) const;

    // Punkt mit dieser ID aus der aktuellen Liste, sonst nullptr. Zeiger gilt
    // bis zum naechsten setPoints().
    const MapViewPoint* findPoint(const String& id) const;

    // Punkt unter (x, y) in Koordinaten der Ansicht - Tracker vor Zonen.
    // nullptr, wenn dort nichts ist. Zeiger gilt bis zum naechsten setPoints().
    const MapViewPoint* hitTest(lv_coord_t x, lv_coord_t y) const;

private:
    struct Marker {
        String    id;
        lv_obj_t* area  = nullptr;   // Zonen- bzw. Genauigkeitskreis
        lv_obj_t* dot   = nullptr;   // Marker (nur Tracker)
        lv_obj_t* label = nullptr;   // Initialen bzw. Haus-Symbol
        bool      stale = false;     // aktuell gesetzter Stil
    };

    lv_obj_t*   m_canvas = nullptr;
    lv_obj_t*   m_tracks = nullptr;  // Wegebene
    MapTrackLayerData* m_trackData = nullptr;   // gehoert der Wegebene (siehe oben)
    lv_obj_t*   m_layer  = nullptr;  // Markerebene
    lv_color_t* m_buf    = nullptr;  // gehoert dem Canvas (siehe oben)
    lv_obj_t*   m_status = nullptr;
    lv_obj_t*   m_attrib = nullptr;
    lv_coord_t  m_w = 0;
    lv_coord_t  m_h = 0;

    std::vector<MapViewPoint> m_points;
    std::vector<Marker>       m_markers;

    uint8_t  m_maxZoom = 16;
    bool     m_haveView = false;
    bool     m_manual = false;           // Ausschnitt von Hand (zoomBy/panBy)
    uint8_t  m_zoom = 0;
    MapPoint m_center = {0, 0};      // Mittelpunkt in Weltpixeln bei m_zoom

    uint16_t m_missingTiles = 0;
    uint16_t m_totalTiles = 0;
    bool     m_dirty = false;        // Karte neu zusammensetzen, sobald sichtbar
    bool     m_wasVisible = false;
    uint32_t m_seenVersion = 0;
    uint32_t m_lastTick = 0;
    uint32_t m_lastCompose = 0;
    uint32_t m_trackWindowS = 0;
    bool     m_showDots = false;
    uint32_t m_lastTrackLayout = 0;

    bool allocateBuffer(lv_coord_t w, lv_coord_t h);
    bool isVisible() const;

    // Ausschnitt fuer die aktuellen Punkte; 'extraPad' verlangt zusaetzlichen
    // Rand (Hysterese beim Hineinzoomen). false ohne Punkte.
    bool fitView(lv_coord_t extraPad, uint8_t& zoom, MapPoint& center) const;
    // Mitte der Punkte bei einer festen Zoomstufe.
    MapPoint pointsCenter(uint8_t zoom) const;
    // Rechteck aller Punkte samt Wegen in Weltpixeln einer Zoomstufe.
    void bounds(uint8_t zoom, double& minX, double& minY, double& maxX, double& maxY) const;
    // Uebernimmt einen neuen Ausschnitt nur, wenn noetig (siehe Kopf).
    bool updateView();

    void compose();          // Kacheln in den Canvas
    void applyManualView(uint8_t zoom, const MapPoint& center);
    void syncMarkers();      // Marker anlegen/loeschen passend zu m_points
    void placeMarkers();     // Marker auf den Ausschnitt setzen
    void applyMarkerStyle(Marker& m, const MapViewPoint& p);
    void layoutTracks();     // Wege auf den Ausschnitt umrechnen, Ebene neu zeichnen
    void updateStatus();
};
