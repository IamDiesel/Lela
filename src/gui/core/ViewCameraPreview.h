#pragma once
// ==============================================================================
// ViewCameraPreview - Kamerabild als Vollbild-Ebene
// ==============================================================================
// Ebene ueber allem (lv_layer_top): startet die Kamera (CameraTab5), zeigt das
// Bild auf halbe Groesse verkleinert (640x360) und darunter einen Hinweis bzw.
// Bildraten. "Abbrechen"/"Schliessen" haelt die Kamera an und gibt alles frei.
//
// Genutzt von:
//   - Diagnose > KAMERA > Vorschau (Test, mit Bildraten)
//   - Einstellungen > WLAN-Netz > Per QR-Code (Spiegelbild, Hinweis, onFrame
//     reicht die Bilder an den QR-Leser weiter)
//
// Verkleinert wird in Software (jeder zweite Bildpunkt jeder zweiten Zeile)
// in einem LVGL-Timer, dabei auch gedreht (CAMERA_UPSIDE_DOWN) und auf Wunsch
// gespiegelt. Bewusst ohne PPA: der haengt am 2D-DMA, der auf diesem Silizium
// Probleme macht (Dma2dGuard).
//
// Es gibt hoechstens eine Ansicht zur Zeit; Open() bei offener Ansicht tut nichts.
// ==============================================================================

#include <stdint.h>

struct CameraViewConfig {
    const char* title;        // Ueberschrift
    const char* hint;         // Text unter dem Bild (nullptr: keiner)
    const char* closeText;    // Beschriftung des Knopfs ("Abbrechen", "Schliessen")
    bool        mirror;       // wie ein Spiegel (Frontkamera) statt wie ein Foto
    bool        showStats;    // Bildraten und Weissabgleich anzeigen
    // Je abgeholtem Kamerabild (volle Groesse, Sensor-Ausrichtung, nur waehrend
    // des Aufrufs gueltig). Rueckgabe true = Ansicht schliessen. Darf nullptr sein.
    bool (*onFrame)(const uint16_t* frame);
    // Nach dem Schliessen (Kamera ist dann aus). byUser = Knopf getippt.
    // Darf nullptr sein.
    void (*onClosed)(bool byUser);
};

void ViewCameraPreview_Open(const CameraViewConfig& cfg);

// Hinweistext unter dem Bild aendern (z.B. "Kein WLAN-Code erkannt").
void ViewCameraPreview_SetHint(const char* text);

bool ViewCameraPreview_IsOpen();
