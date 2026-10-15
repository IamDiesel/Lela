#pragma once
// ==============================================================================
// ViewVideoPip - Video-Overlay (Bild im Bild) auf Dashboard, HA und CatMat
// ==============================================================================
// Aufteilung der Arbeit:
//
//   LVGL (diese Datei)   Rahmen auf der obersten Ebene, Statuszeile unter dem
//                        Bild, Beruehrung, Sichtbarkeit, Lage und Groesse.
//   PPA (VideoLogic)     das Videobild selbst - per Hardware skaliert und
//                        gedreht, direkt in den Bildspeicher.
//
// Warum nicht alles ueber LVGL: Das Panel wird gedreht betrieben, jeder
// LVGL-Flush schreibt deshalb Pixel fuer Pixel (2-3 MB/s, siehe VIDEO_USE_PPA
// in DebugFlags.h). Schon ein kleines Overlay kostete so ~60 ms je Bild in der
// Hauptschleife.
//
// Damit LVGL das Videobild nicht uebermalt, wird die Videoflaeche beim Flush
// ausgespart (Lvgl_SetReservedArea), solange Bilder kommen. Liegt etwas ueber
// dem Overlay (HA-Dialog, aufgeklappte Liste, QuickSettings, Alarm-Popup,
// Bildschirm aus), blendet es sich aus - VORHER wird die Videoausgabe
// gestoppt, damit kein Bild mehr ueber das Darueberliegende gemalt wird.
//
// Bedienung:
//   kurz tippen                    -> Baby-App
//   halten oder ziehen             -> verschieben
//   Griff unten rechts ziehen      -> Groesse aendern (Seitenverhaeltnis bleibt)
// Lage und Groesse werden gespeichert. Waehrend des Ziehens zeigt ein duenner
// Rahmen die neue Lage; das Bild springt erst beim Loslassen dorthin. Ein
// mitbewegtes Bild wuerde jede Bewegung zu einer grossflaechigen
// LVGL-Neuzeichnung machen - mit dem langsamen Flush also ruckeln.
//
// Nur aus der Hauptschleife verwenden.
// ==============================================================================

#include <lvgl.h>
#include <stdint.h>

class ViewVideoPip {
public:
    // Legt den (verborgenen) Rahmen auf der obersten Ebene an. Vor
    // ViewPopups::init() aufrufen, damit Alarm-Popups darueber liegen.
    static void init();

    // In jedem Durchlauf der Hauptschleife, NACH ViewPopups::update().
    static void update();

    // Lage und Groesse auf den Ausgangszustand (klein, unten rechts).
    static void resetLayout();
};
