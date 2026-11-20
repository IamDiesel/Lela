#include "DisplayBrightness.h"
#include "SharedData.h"
#include <M5Unified.h>

// NVS-Schluessel im Bereich "catmat".
static const char* const KEY_PERCENT        = "bright2";  // Reglerwert 1..100 (Kennlinie)
static const char* const KEY_LINEAR_PERCENT = "bright";   // alt: lineare Prozente 5..100
static const int         DEFAULT_LINEAR_PERCENT = 80;     // Auslieferungswert der alten Einstellung

int DisplayBrightness_LoadPercent(Preferences& prefs) {
    if (prefs.isKey(KEY_PERCENT)) {
        return DisplayBrightness_ClampPercent(prefs.getInt(KEY_PERCENT, DISPLAY_BRIGHTNESS_MAX_PERCENT));
    }
    // Noch nie mit der Kennlinie gespeichert: alte Einstellung (oder deren
    // Auslieferungswert) auf gleiche Helligkeit umrechnen.
    return DisplayBrightness_PercentFromLinear(prefs.getInt(KEY_LINEAR_PERCENT, DEFAULT_LINEAR_PERCENT));
}

void DisplayBrightness_Apply() {
    M5.Display.setBrightness(DisplayBrightness_LevelFor(brightnessPercent));
}

void DisplayBrightness_Set(int percent) {
    brightnessPercent = DisplayBrightness_ClampPercent(percent);
    DisplayBrightness_Apply();
}

void DisplayBrightness_Save() {
    Preferences prefs;
    prefs.begin("catmat", false);
    prefs.putInt(KEY_PERCENT, brightnessPercent);
    prefs.end();

    // Eine Zeile je Loslassen des Reglers: Reglerwert, Stufe und
    // Einschaltdauer. Damit laesst sich am Geraet ablesen, ob die Untergrenze
    // passt (siehe DISPLAY_BRIGHTNESS_MIN_LEVEL).
    const uint8_t level    = DisplayBrightness_LevelFor(brightnessPercent);
    const int     permille = DisplayBrightness_DutyPermille(level);
    Serial.printf("[Display] Helligkeit %d %% -> Stufe %u/255 (%d,%d %% Einschaltdauer).\n",
                  brightnessPercent, (unsigned)level, permille / 10, permille % 10);
}

void DisplayBrightness_Off() {
    M5.Display.setBrightness(0);
}
