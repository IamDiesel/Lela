#pragma once
// ==============================================================================
// HaTrackHistory - Positionsverlauf fuer das Kartenwidget
// ==============================================================================
// Wie HA-Karten mit hours_to_show: zu jedem Tracker der Weg der letzten
// Minuten/Stunden.
//
// Wer was braucht: Jedes Kartenwidget meldet seine Entitaeten und sein
// Zeitfenster an (Register) und beim Loeschen ab (Unregister). Geladen wird
// nur die Vereinigung davon (zone.* nie) - entfernte Tracker fallen sofort
// heraus.
//
// Laden (history/history_during_period):
//   * Je Entitaet einzeln, von jetzt aus rueckwaerts in Zeitabschnitten.
//     HA liefert jeden gespeicherten Zustand mit ALLEN Attributen; ein
//     gespraechiger Sensor ergab fuer 3 h eine Antwort von 37 MB in einem
//     Websocket-Frame - die Bibliothek legt Frames am Stueck an und brach ab.
//     Die Abschnittslaenge richtet sich nach der gemessenen Datenrate (Ziel
//     ~256 KB je Antwort, hoechstens 5 min je Abschnitt).
//   * Je Entitaet hoechstens ~4 MB; darueber bleibt der Weg kuerzer (der
//     neuere Teil). Das Log nennt je Entitaet Zustaende, Datenmenge und Rate
//     und gibt einen Hinweis bei grossen Attributen (> 4 KB je Zustand).
//   * Nur Luecken: Je Entitaet ist bekannt, welcher Zeitraum vollstaendig
//     vorliegt (Abdeckung). Beim Wiederverbinden oder Zurueckkehren auf den
//     HA-Bildschirm wird nur die Zeit seit dem Verbindungsende nachgeladen,
//     bei laengerem Fenster zusaetzlich der aeltere Teil.
//   * Beim Parsen bleiben nur Position, Genauigkeit und Zeit.
//
// Live: neue Positionen kommen aus dem ohnehin laufenden Abo
// (subscribe_entities); das Kartenwidget haengt sie an (Append) - kein Abruf.
//
// Reduktion: Ein neuer Punkt erst ab einem Abstand, der von der
// GPS-Genauigkeit abhaengt (halbe Genauigkeit, 2..25 m) - Zittern im Stand
// verlaengert nicht den Weg. Jeder Punkt kennt die erste und letzte Meldung an
// dieser Stelle (ts, tsLast).
//
// Unterbrechungen: Meldet ein Tracker keine Position (ausser Reichweite,
// unavailable), wird der Weg dort unterbrochen statt quer verbunden. Zusaetzlich
// ergibt Get() eine Unterbrechung, wenn zwischen zwei Meldungen mehr als
// 'gapSeconds' liegen (Einstellung des Widgets).
//
// Speicher: Punkte im PSRAM, je Entitaet hoechstens MAX_POINTS. Die Wege
// bleiben ueber den HA-Bildschirm hinaus erhalten (bis zum Neustart), damit
// beim Zurueckkehren nur die Luecke geladen werden muss.
//
// Threading: Register/Unregister/Append/MarkGap/Get/GetLast/GetVersion aus der
// GUI (loopTask); OnAuthenticated/Tick/HandleMessage/OnDisconnected aus dem
// HA_WS_Task. Daten unter einem Mutex.
// ==============================================================================

#include <Arduino.h>
#include <vector>

// Ein Punkt des Verlaufs. float reicht: ~0,5 m Aufloesung bei Mitteleuropa.
struct HaTrackPoint {
    float    lat;
    float    lon;
    uint32_t ts;       // erste Meldung an dieser Stelle (Unix-Zeit)
    uint32_t tsLast;   // letzte Meldung an dieser Stelle
    bool     brk;      // Weg unterbrochen: keine Linie vom vorigen Punkt hierher
};

class HaTrackHistory {
public:
    // Laengstes unterstuetztes Zeitfenster (Ladezeit: je 5 min ein Abschnitt).
    static constexpr uint32_t MAX_WINDOW_S = 24UL * 3600UL;

    // Obergrenze je Entitaet; aeltere Punkte fallen heraus.
    static constexpr size_t MAX_POINTS = 2000;

    // Einmalig vor der ersten Nutzung (HaWebsocketLogic_Start).
    static void Init();

    // --- GUI-Seite ---

    // Anmeldung eines Widgets ('owner'): diese Entitaeten ueber 'seconds'.
    // Erneuter Aufruf ersetzt die Anmeldung; seconds = 0 meldet ab.
    // Laedt nur, wenn Entitaeten oder ein laengeres Fenster dazukommen.
    static void Register(const void* owner, const std::vector<String>& entityIds, uint32_t seconds);
    static void Unregister(const void* owner);

    // Haengt eine Position aus dem Entitaets-Cache an. 'ts' = last_updated
    // aus HA (0 = jetzt). Nur fuer angemeldete Entitaeten.
    static void Append(const String& entityId, double lat, double lon, float accuracyM, uint32_t ts);

    // Entitaet liefert gerade keine Position: der naechste Punkt beginnt
    // einen neuen Wegabschnitt.
    static void MarkGap(const String& entityId);

    // Punkte der letzten 'seconds', aelteste zuerst (plus der letzte Punkt
    // davor als Startpunkt). brk ist gesetzt bei gespeicherten
    // Unterbrechungen und, wenn gapSeconds > 0, bei laengerer Funkstille.
    static std::vector<HaTrackPoint> Get(const String& entityId, uint32_t seconds, uint32_t gapSeconds);

    // Letzte bekannte Position (fuer Tracker, die gerade keine liefern).
    static bool GetLast(const String& entityId, HaTrackPoint& out);

    // Steigt, sobald sich ein Verlauf sichtbar aendert (geladen, neuer Punkt).
    static uint32_t GetVersion();

    // Laden anhalten, solange die Oberflaeche aufbaut (HA-App geoeffnet,
    // Tabs entstehen). Nach der Freigabe wird noch HOLD_GRACE gewartet. Eine
    // vergessene Freigabe laeuft nach HOLD_MAX von selbst ab.
    static void SetHold(bool hold);

    // --- HA_WS_Task ---
    static void OnAuthenticated();
    static void Tick();
    static bool HandleMessage(const String& type, uint32_t id, bool success, const String& payload);
    // Verbindung bzw. HA_WS_Task endet: laufendes Laden verwerfen, Wege und
    // Abdeckung behalten (Abdeckung endet jetzt).
    static void OnDisconnected();
};
