#pragma once
// ==============================================================================
// DisplayBrightnessCurve - Kennlinie Regler -> Hintergrundlicht (reine Rechnung)
// ==============================================================================
// Befund 17.11.2026: Die Helligkeit laesst sich nicht dunkel stellen - das
// Display ist entweder aus oder mindestens "halbwegs hell".
//
// Ursache ist die Hardware des Tab5, nicht die Software:
// GPIO 22 schaltet den Freigabe-Eingang (EN) des Aufwaertswandlers ME2212
// (U8 im Schaltplan), der die LEDs des Hintergrundlichts speist. Gedimmt wird
// per PWM auf diesem Eingang. Unterhalb von rund 35-40 % Einschaltdauer
// aendert sich die Helligkeit aber nicht mehr - nur 0 % schaltet ganz aus.
//   - Eigener Test: Regler mit quadratischer Kennlinie 1..60 % (Stufe 1..91)
//     gleich hell; Schaltfrequenz 5 kHz statt 44,1 kHz ohne Wirkung.
//   - Gleicher Befund eines anderen Tab5-Nutzers mit ESPHome, gemessen von
//     25 Hz bis 44,1 kHz: github.com/m5stack/esphome-yaml, Issue 117 (dort
//     auch der Schaltplan-Bezug; von M5Stack bis 17.11.2026 unbeantwortet).
//
// Folge fuer die Kennlinie: Stufen unter der Untergrenze sind wirkungslos und
// wuerden nur einen toten Bereich am Regler erzeugen. Der Regler (1..100 %)
// wird deshalb linear auf den Bereich abgebildet, in dem sich etwas aendert:
//
//     Regler   1 %  ->  Stufe  90/255  (35 % Einschaltdauer, Untergrenze)
//     Regler  50 %  ->  Stufe 172/255  (67 %)
//     Regler 100 %  ->  Stufe 255/255  (100 %)
//
// Linear genuegt: Ueber die verbleibende Spanne (1 : 2,8) empfindet das Auge
// gleiche Schritte der Lichtmenge als nahezu gleich grosse Schritte.
//
// Auch langsames Schalten hilft nicht (Versuche 17.11.2026): Pulse im Abstand
// von 2,5 bis 25 ms (40-400 Hz) blieben mindestens so hell wie der Sockel -
// der Wandler laeuft in den Pausen weiter. Erst bei noch laengeren Pausen wird
// es dunkler, dann flimmert es sichtbar.
//
// Dunkler als die Untergrenze geht es nur ausserhalb des Hintergrundlichts
// (Bildinhalt abdunkeln, Folie, Bauteiltausch) - siehe Projekt-Doku.
//
// M5GFX dimmt per PWM (GPIO 22, 44,1 kHz, 9 Bit), linear in der Stufe 0..255.
// Stufe 0 heisst "aus" und bleibt dem Abschalten des Bildschirms vorbehalten.
//
// Ohne Arduino/M5-Abhaengigkeit, damit die Kennlinie am PC pruefbar ist:
// tools/tests/display_brightness.
// ==============================================================================

#include <stdint.h>

// Bereich des Reglers in Prozent.
static const int DISPLAY_BRIGHTNESS_MIN_PERCENT = 1;
static const int DISPLAY_BRIGHTNESS_MAX_PERCENT = 100;

// Bereich der Stufen des Hintergrundlichts.
//
// MIN_LEVEL ist die Untergrenze der Hardware (siehe oben): die Stufe, unter
// der sich die Helligkeit nicht mehr aendert. 90/255 = 35 %. Bleibt am
// unteren Ende des Reglers ein Stueck ohne sichtbare Wirkung, den Wert
// anheben; springt die Helligkeit am unteren Anschlag sichtbar, absenken.
// Die Logzeile "[Display] Helligkeit ..." nennt die Stufe zum Reglerwert.
static const uint8_t DISPLAY_BRIGHTNESS_MIN_LEVEL = 90;
static const uint8_t DISPLAY_BRIGHTNESS_MAX_LEVEL = 255;

// Reglerwert auf den gueltigen Bereich begrenzen.
inline int DisplayBrightness_ClampPercent(int percent) {
    if (percent < DISPLAY_BRIGHTNESS_MIN_PERCENT) return DISPLAY_BRIGHTNESS_MIN_PERCENT;
    if (percent > DISPLAY_BRIGHTNESS_MAX_PERCENT) return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    return percent;
}

// Regler (1..100 %) -> Stufe des Hintergrundlichts (MIN_LEVEL..255).
// Linear, kaufmaennisch gerundet, an beiden Enden verankert.
inline uint8_t DisplayBrightness_LevelFor(int percent) {
    const int32_t x     = DisplayBrightness_ClampPercent(percent) - DISPLAY_BRIGHTNESS_MIN_PERCENT;
    const int32_t span  = DISPLAY_BRIGHTNESS_MAX_PERCENT - DISPLAY_BRIGHTNESS_MIN_PERCENT;
    const int32_t range = DISPLAY_BRIGHTNESS_MAX_LEVEL - DISPLAY_BRIGHTNESS_MIN_LEVEL;
    return (uint8_t)(DISPLAY_BRIGHTNESS_MIN_LEVEL + (x * range + span / 2) / span);
}

// Umkehrung: der Reglerwert, dessen Stufe 'level' am naechsten kommt.
inline int DisplayBrightness_PercentForLevel(uint8_t level) {
    int best     = DISPLAY_BRIGHTNESS_MIN_PERCENT;
    int bestDiff = 256;
    for (int p = DISPLAY_BRIGHTNESS_MIN_PERCENT; p <= DISPLAY_BRIGHTNESS_MAX_PERCENT; p++) {
        int diff = (int)DisplayBrightness_LevelFor(p) - (int)level;
        if (diff < 0) diff = -diff;
        if (diff < bestDiff) { bestDiff = diff; best = p; }
    }
    return best;
}

// Uebernahme einer Einstellung aus der Zeit vor der Kennlinie: damals galt
// Stufe = Prozent * 255 / 100 (Regler 5..100 %). Liefert den Reglerwert, der
// heute dieselbe Helligkeit ergibt (alt 80 % -> neu 69 %). Alte Werte unter
// der Untergrenze sahen ohnehin alle gleich aus und landen am unteren Anschlag.
inline int DisplayBrightness_PercentFromLinear(int oldPercent) {
    if (oldPercent < 0)   oldPercent = 0;
    if (oldPercent > 100) oldPercent = 100;
    int level = (oldPercent * 255) / 100;
    if (level < DISPLAY_BRIGHTNESS_MIN_LEVEL) level = DISPLAY_BRIGHTNESS_MIN_LEVEL;
    return DisplayBrightness_PercentForLevel((uint8_t)level);
}

// Einschaltdauer einer Stufe in Zehntelprozent (fuer die Logzeile).
inline int DisplayBrightness_DutyPermille(uint8_t level) {
    return ((int)level * 1000 + 127) / 255;
}
