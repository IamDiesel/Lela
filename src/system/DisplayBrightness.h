#pragma once
// ==============================================================================
// DisplayBrightness - Helligkeit des Displays (eine Stelle fuer alle)
// ==============================================================================
// Vorher stand die Umrechnung "Prozent -> Hintergrundlicht" an fuenf Stellen
// (Start, Bildschirm an, Einstellungen, Schnelleinstellungen, LVGL-Treiber).
// Jetzt gehen alle ueber dieses Modul:
//
//   - Einstellung: brightnessPercent (SharedData), Reglerwert 1..100 %.
//   - Kennlinie:   DisplayBrightnessCurve.h (linear ab der Untergrenze der
//                  Hardware, am PC getestet). Dort steht auch, warum sich das
//                  Tab5 nicht dunkler stellen laesst.
//   - Speicher:    NVS "catmat", Schluessel "bright2". Der alte Schluessel
//                  "bright" (lineare Prozente) wird beim Laden einmalig
//                  umgerechnet und bleibt unangetastet liegen.
//
// Der Dongle der Katzenmatte bekommt weiterhin brightnessPercent
// (SystemLogic) - also den Reglerwert, nicht die Lichtmenge.
// ==============================================================================

#include <Arduino.h>
#include <Preferences.h>
#include "DisplayBrightnessCurve.h"

// Liest den Reglerwert aus dem geoeffneten Einstellungsbereich ("catmat").
// Gibt es nur die alte, lineare Einstellung, wird sie auf gleiche Helligkeit
// umgerechnet. Schreibt nichts.
int DisplayBrightness_LoadPercent(Preferences& prefs);

// Wendet die aktuelle Einstellung an (Start, "Bildschirm an").
void DisplayBrightness_Apply();

// Regler wird bewegt: Einstellung uebernehmen und sofort anwenden. Gespeichert
// wird erst mit DisplayBrightness_Save() - nicht bei jedem Zwischenwert.
void DisplayBrightness_Set(int percent);

// Regler losgelassen: Einstellung dauerhaft speichern.
void DisplayBrightness_Save();

// Hintergrundlicht aus ("Bildschirm aus"). Die Einstellung bleibt erhalten.
void DisplayBrightness_Off();
