#pragma once
// ==============================================================================
// CameraExposure - Belichtungsautomatik der Tab5-Kamera: die Rechnung
// ==============================================================================
// Der Sensor (SC202CS) lief bisher mit fester Belichtung: volle Bildzeit
// (1/30 s), Verstaerkung 1. Im hellen Raum war das Bild ueberstrahlt, im
// dunklen zu dunkel. Die Automatik fuehrt Belichtungszeit und Verstaerkung aus
// dem fertigen Bild nach (Regelkreis wie der Weissabgleich in CameraTab5).
//
// Gemessen wird nicht der Mittelwert, sondern das 95. Perzentil der Helligkeit:
// der Wert, unter dem 95 % der Bildpunkte liegen. Er soll auf AE_TARGET landen.
// Grund: Der wichtigste Anwendungsfall ist ein QR-Code auf einem Handy-
// Bildschirm. Der leuchtet selbst, der Rest des Bildes ist oft dunkel - eine
// Mittelwert-Regelung zoege die Belichtung hoch, bis der Bildschirm (und damit
// der Code) weiss ausbrennt. Das Perzentil richtet sich nach den hellen
// Flaechen, sobald sie mehr als 5 % des Bildes fuellen, und laesst einzelne
// Lichtpunkte (Lampe, Spiegelung) ausser Acht.
//
// Stellgroesse ist das "Belichtungsprodukt" = Zeilen x Verstaerkung:
//   * Belichtungszeit in Zeilen (eine Zeile = 26,7 us bei 30 Bildern/s und
//     1250 Zeilen je Bild), AE_LINES_MIN bis AE_LINES_MAX.
//   * Verstaerkung 1 bis AE_GAIN_MAX (mehr rauscht nur).
// Aufgeteilt wird "erst Zeit, dann Verstaerkung". Ab 10 ms Belichtungszeit
// rastet die Zeit auf Vielfache von 10 ms (AE_FLICKER_LINES): Kunstlicht am
// 50-Hz-Netz pulsiert mit 100 Hz, andere Zeiten gaeben wandernde Streifen im
// Bild. Den Rest gleicht die Verstaerkung aus.
//
// Reines C++ ohne Abhaengigkeiten: auf dem PC testbar (tools/tests/camera_exposure).
// Sensor-Register und Bildzugriff gehoeren CameraTab5.
// ==============================================================================

#include <stdint.h>

// Histogramm der Helligkeit: AE_HIST_BINS gleich breite Klassen ueber 0..255.
static const int AE_HIST_BINS = 32;

static const int      AE_TARGET        = 200;    // Ziel fuer das 95. Perzentil (0..255)
static const int      AE_DEADBAND      = 16;     // so weit daneben wird nicht nachgeregelt
static const uint32_t AE_LINES_MIN     = 4;
static const uint32_t AE_LINES_MAX     = 1244;   // Bildhoehe gesamt (VTS 1250) - 6, laut Sensortreiber
static const uint32_t AE_FLICKER_LINES = 375;    // 10 ms
static const float    AE_GAIN_MAX      = 16.0f;

// Aufteilung des Belichtungsprodukts.
struct ExposureSetting {
    uint32_t lines;   // Belichtungszeit in Zeilen
    float    gain;    // Gesamtverstaerkung (1 = keine)
};

// Register des SC202CS fuer eine Verstaerkung (Tabelle des Sensortreibers,
// "analog zuerst"): analog 1/2/4/8/16-fach (0x3e09 = 0x00/01/03/07/0f), der
// Rest digital fein in 1/32-Schritten (0x3e07 = 0x80..0xfc), grob bleibt 1 (0x3e06 = 0).
struct ExposureGainRegs {
    uint8_t analog;       // 0x3e09
    uint8_t digitalCoarse; // 0x3e06
    uint8_t digitalFine;   // 0x3e07
};

// 95. Perzentil aus dem Histogramm, als Helligkeit 0..255 (Klassenmitte).
// -1, wenn das Histogramm leer ist.
int CameraExposure_Percentile95(const uint32_t hist[AE_HIST_BINS]);

// Naechstes Belichtungsprodukt aus dem jetzigen und dem gemessenen Perzentil.
// Innerhalb des Totbands unveraendert; sonst gedaempft in Richtung Ziel, je
// Schritt hoechstens Faktor 2 (bei ausgebranntem Bild sofort die Haelfte).
float CameraExposure_Next(float product, int percentile95);

// Belichtungsprodukt in Zeit und Verstaerkung aufteilen (siehe oben).
ExposureSetting CameraExposure_Split(float product);

// Verstaerkung in die Sensor-Register uebersetzen (begrenzt auf 1..AE_GAIN_MAX).
ExposureGainRegs CameraExposure_GainRegs(float gain);

// Die Verstaerkung, die diese Register tatsaechlich ergeben.
float CameraExposure_GainOf(const ExposureGainRegs& regs);
