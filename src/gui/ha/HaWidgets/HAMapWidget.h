#pragma once
// ==============================================================================
// HAMapWidget - Karte mit Trackern (Widget-Typ "map")
// ==============================================================================
// Zeigt Entitaeten mit Koordinaten (device_tracker, person, Sensoren mit
// latitude/longitude) als Marker und zone.* als Zonenkreis auf einer
// OpenStreetMap-Karte, auf Wunsch mit dem Weg der letzten Zeit (wie HA
// hours_to_show). Positionen kommen wie alle Zustaende ueber das bestehende
// subscribe_entities (HaEntityCache) - kein eigener Abruf.
//
// Arbeitsteilung:
//   HAMapWidget    - HA-Seite: Entitaeten -> Punktliste mit Weg, Rahmen, Titel
//   HaTrackHistory - Positionsverlauf (eine History-Abfrage, dann live)
//   MapView        - Darstellung (Kacheln, Marker, Wege, Ausschnitt), kennt kein HA
//   MapTileService - Kartenkacheln (Flash, Netz, PSRAM)
//
// Bearbeiten: Verschieben, Groesse, Anzeige (Titel), Bedingungen wie bei allen
// Widgets; Reiter "Karte" fuer Weglaenge, Zoom und Ausschnitt (EditTabMap).
//
// Funkstille (map_stale_min, Standard 10 min, 0 = aus): Meldet ein Tracker
// so lange nichts mehr oder liefert er keine Position (ausser Reichweite),
// erscheint er halbdurchsichtig an der letzten bekannten Position, und sein Weg wird an
// solchen Stellen unterbrochen statt quer verbunden.
//
// Ausschnitt: automatisch (alle Tracker einpassen) oder fest. Fest wird er,
// wenn im Vollbild gezoomt/verschoben und dann geschlossen wird (HaDialogMap
// uebernimmt ihn und speichert); "Alle" im Vollbild oder der Reiter "Karte"
// schalten zurueck auf automatisch.
// ==============================================================================

#include "HAWidgetBase.h"
#include "MapView.h"
#include <vector>

class HAMapWidget : public HAWidget {
public:
    HAMapWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h,
                const char* name, const char* mdi_icon, const char* c_on, const char* c_off,
                const std::vector<String>& entities, int maxZoom, float hoursToShow);
    ~HAMapWidget() override;

    void updateState(String state) override {}   // kein eigener Zustand
    void refreshFromCache() override;
    void tick() override;
    void onClick() override;

    void setSize(int w, int h) override;
    void setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) override;

    // --- Einstellungen (Bearbeiten-Dialog, Speichern) ---
    const std::vector<String>& getMapEntities() const { return entities; }
    int   getMapMaxZoom() const { return max_zoom; }
    float getMapHours() const { return hours_to_show; }
    int   getMapStaleMinutes() const { return stale_minutes; }
    // Funkstille in Sekunden (0 = aus) - auch fuer das Vollbild.
    uint32_t staleSeconds() const { return stale_minutes > 0 ? (uint32_t)stale_minutes * 60UL : 0; }

    // Fester Ausschnitt (Mitte + Zoom); false = automatisch einpassen.
    bool hasFixedView() const { return fixed_view; }
    double getFixedLat() const { return fixed_lat; }
    double getFixedLon() const { return fixed_lon; }
    int    getFixedZoom() const { return fixed_zoom; }

    void setMapMaxZoom(int zoom);
    // Fester Ausschnitt. Rueckgabe: true, wenn er sich geaendert hat (dann
    // muss gespeichert werden).
    bool setFixedView(double lat, double lon, int zoom);
    // Zurueck zum automatischen Einpassen. true, wenn vorher fest.
    bool clearFixedView();
    // Neue Entitaetenliste (Bearbeiten-Dialog).
    void setMapEntities(const std::vector<String>& list);
    // Laenge des Weges in Stunden, 0 = kein Weg.
    void setMapHours(float hours);
    // Funkstille in Minuten, 0 = aus.
    void setMapStaleMinutes(int minutes);

    // --- Gemeinsam mit dem Vollbild (HaDialogMap) ---
    // Punkte der Entitaeten aus dem Entitaets-Cache (ohne Wege). Tracker ohne
    // aktuelle Position erscheinen mit der letzten bekannten aus dem Verlauf
    // (veraltet). 'staleSeconds' > 0: laenger keine Meldung = veraltet.
    // 'reported': Entitaeten ohne Koordinaten werden darin einmal vermerkt und
    // geloggt (nullptr = nicht melden). 'feedHistory': aktuelle Positionen und
    // Funkluecken an HaTrackHistory geben (nur das Widget, nicht das Vollbild).
    static void collectPositions(const std::vector<String>& entities, std::vector<MapViewPoint>& out,
                                 std::vector<String>* reported, uint32_t staleSeconds, bool feedHistory);
    // Haengt jedem Tracker seinen Weg der letzten 'seconds' an; laengere
    // Funkstille als 'gapSeconds' unterbricht ihn (0 = nie).
    static void attachTracks(std::vector<MapViewPoint>& points, uint32_t seconds, uint32_t gapSeconds);
    // Fenster in Sekunden zu einer Weglaenge in Stunden (begrenzt auf 24 h).
    static uint32_t hoursToSeconds(float hours);

private:
    std::vector<String> entities;
    int      max_zoom;
    float    hours_to_show;
    int      stale_minutes = 10;
    bool     fixed_view = false;
    double   fixed_lat = 0;
    double   fixed_lon = 0;
    int      fixed_zoom = 0;
    MapView  view;

    // Letzter an die Ansicht gegebener Stand. Der Entitaets-Cache aendert
    // sich mit JEDEM Sensor im Haus (oft mehrmals pro Sekunde) - ohne diesen
    // Vergleich wuerden bei jeder Aenderung alle Wege kopiert.
    std::vector<MapViewPoint> last_points;
    uint32_t last_history_version = 0;
    bool     have_last = false;
    uint32_t last_track_build_ms = 0;   // Wege altern auch ohne Bewegung (Zeitfenster)

    // Entitaeten ohne Koordinaten, die schon gemeldet wurden (einmal je Widget).
    std::vector<String> reported_without_location;

    uint32_t historySeconds() const { return hoursToSeconds(hours_to_show); }

protected:
    // Aenderung, sobald sich einer der Tracker/Zonen aendert.
    uint32_t sourceVersion() override;

private:
    void requestHistory();

    // Groesse des Inhaltsbereichs (Container abzueglich Rahmen).
    void contentSize(int w, int h, lv_coord_t& cw, lv_coord_t& ch) const;
    void placeTitle();
};
