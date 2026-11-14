#pragma once
// ==============================================================================
// HaTheme - Farben der HA-App im Farbschema (dunkel / hell)
// ==============================================================================
// Baut auf UiTheme auf und ergaenzt, was nur die HA-App braucht:
//
//  1. Kacheln: Grundflaeche, "an" und "aus". Im hellen Schema wird eine
//     eingeschaltete Kachel leicht in der Widget-Farbe getoent (12 %), eine
//     ausgeschaltete ist neutral grau. Im dunklen Schema hebt sich "an" als
//     hellere Flaeche ab (wie bisher).
//
//  2. Zustandsfarben (Symbol an/aus, Diagrammlinie, Klima heizen/kuehlen ...)
//     werden immer als DUNKEL-WERT angegeben - so sind sie gespeichert
//     (`color_on`/`color_off` im Widget-JSON) und so stehen die Standardwerte
//     im Code. Fuer das helle Schema liefert themed()/user() die GEGENFARBE:
//       - Palette und Standardfarben: feste Paare (Kontrast >= 3:1 auf Weiss
//         und auf der getoenten Kachel, siehe Tabelle in HaTheme.cpp).
//       - jede andere Farbe: berechnet (gleicher Farbton, abgedunkelt bis der
//         Kontrast reicht; Grautoene werden gespiegelt).
//
//  3. Abschalten der Gegenfarbe fuer NUTZERFARBEN: global (Einstellungen ->
//     Anzeige, NVS "haAutoCol") und je Widget (Bearbeiten -> Anzeige). Dann
//     gilt die gewaehlte Farbe unveraendert in beiden Schemata. Standardfarben
//     (keine Nutzerwahl) passen sich immer an.
//
// Wie UiTheme gelten die Farben beim AUFBAU. Die HA-Ansicht uebernimmt ein
// neues Schema beim naechsten Oeffnen (siehe GuiManager::reloadCurrentScreen).
// Bewusst fest in beiden Schemata: Stopp-/Loeschen-Rot, Kartenkacheln (OSM).
// ==============================================================================

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

namespace HaTheme {
    // Laedt die globale Einstellung (NVS). Nach UiTheme::init().
    void init();

    // Globale Einstellung "Gegenfarben automatisch" (Standard: an).
    bool autoAdapt();
    void setAutoAdapt(bool on);

    // Standard-/Systemfarbe (Dunkel-Wert) -> Farbe im aktuellen Schema.
    uint32_t themed(uint32_t darkHex);

    // Nutzerfarbe (Dunkel-Wert). Angepasst nur, wenn global UND fuer dieses
    // Widget erlaubt (widgetAdapt), sonst unveraendert.
    uint32_t user(uint32_t darkHex, bool widgetAdapt);

    // Gegenfarbe fuer das helle Schema, unabhaengig vom aktuellen Modus
    // (fuer die Vorschau im Bearbeiten-Dialog).
    uint32_t lightOf(uint32_t darkHex);

    // Kacheln
    lv_color_t card();                    // Grundflaeche (ohne an/aus)
    lv_color_t cardOn(uint32_t accent);   // eingeschaltet; accent = wirksame Symbolfarbe
    lv_color_t cardOff();                 // ausgeschaltet
    lv_color_t cardBorder();

    // Bedienelemente auf Kacheln und in Dialogen
    lv_color_t control();                 // Knopfflaeche (+/-, Auf/Ab, Auswahl)
    lv_color_t track();                   // Regler-Spur, Diagrammraster

    // Grauton (geringe Saettigung)? Graue Knoepfe werden zur Bedienflaeche
    // control(), bunte behalten ihre (angepasste) Farbe.
    bool isNeutral(uint32_t rgb);
    // Lesbare Schriftfarbe auf einer beliebigen Flaeche: Weiss oder fast Schwarz.
    lv_color_t onColor(uint32_t bgRgb);

    // Farben der Farbwahl (Dunkel-Werte, Reihenfolge = Anzeige).
    const uint32_t* palette(size_t* count);
}
