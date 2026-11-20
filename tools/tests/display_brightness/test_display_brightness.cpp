// ==============================================================================
// PC-Test fuer system/DisplayBrightnessCurve
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/system
//       tools/tests/display_brightness/test_display_brightness.cpp -o /tmp/brighttest
//   /tmp/brighttest
//
// Prueft die Kennlinie Regler -> Hintergrundlicht: Verankerung an beiden Enden
// (unten die Untergrenze der Hardware, nie "aus"), jeder Reglerschritt wird
// heller, die dokumentierten Stuetzwerte und die Uebernahme alter (linearer)
// Einstellungen auf gleiche Helligkeit.
// ==============================================================================
#include "DisplayBrightnessCurve.h"
#include <cstdio>
#include <cstdlib>

static int g_errors = 0;

#define CHECK(cond, ...) \
    do { if (!(cond)) { g_errors++; printf("FEHLER: " __VA_ARGS__); printf("\n"); } } while (0)

int main() {
    // --- Enden ----------------------------------------------------------------
    CHECK(DisplayBrightness_LevelFor(DISPLAY_BRIGHTNESS_MIN_PERCENT) == DISPLAY_BRIGHTNESS_MIN_LEVEL,
          "unterer Anschlag ergibt Stufe %u", (unsigned)DisplayBrightness_LevelFor(DISPLAY_BRIGHTNESS_MIN_PERCENT));
    CHECK(DisplayBrightness_LevelFor(DISPLAY_BRIGHTNESS_MAX_PERCENT) == DISPLAY_BRIGHTNESS_MAX_LEVEL,
          "oberer Anschlag ergibt Stufe %u", (unsigned)DisplayBrightness_LevelFor(DISPLAY_BRIGHTNESS_MAX_PERCENT));
    CHECK(DISPLAY_BRIGHTNESS_MIN_LEVEL >= 1, "Untergrenze 0 hiesse: Regler am Anschlag schaltet aus");

    // --- Werte ausserhalb des Reglers werden begrenzt -------------------------
    CHECK(DisplayBrightness_LevelFor(-50) == DISPLAY_BRIGHTNESS_MIN_LEVEL, "negativer Reglerwert");
    CHECK(DisplayBrightness_LevelFor(0)   == DISPLAY_BRIGHTNESS_MIN_LEVEL, "Reglerwert 0");
    CHECK(DisplayBrightness_LevelFor(500) == DISPLAY_BRIGHTNESS_MAX_LEVEL, "Reglerwert 500");

    // --- Jeder Schritt am Regler wird heller (kein toter Bereich) -------------
    int previous = 0;
    for (int p = DISPLAY_BRIGHTNESS_MIN_PERCENT; p <= DISPLAY_BRIGHTNESS_MAX_PERCENT; p++) {
        const int level = DisplayBrightness_LevelFor(p);
        CHECK(level > previous, "Regler %d %% ist nicht heller als %d %%", p, p - 1);
        previous = level;
    }

    // --- Stuetzwerte aus dem Kopf von DisplayBrightnessCurve.h ----------------
    CHECK(DisplayBrightness_LevelFor(1)  == 90,  "Regler 1 %% -> Stufe %u",  (unsigned)DisplayBrightness_LevelFor(1));
    CHECK(DisplayBrightness_LevelFor(50) == 172, "Regler 50 %% -> Stufe %u", (unsigned)DisplayBrightness_LevelFor(50));
    CHECK(DisplayBrightness_DutyPermille(90)  == 353,  "Einschaltdauer Stufe 90");
    CHECK(DisplayBrightness_DutyPermille(172) == 675,  "Einschaltdauer Stufe 172");
    CHECK(DisplayBrightness_DutyPermille(255) == 1000, "Einschaltdauer Stufe 255");

    // --- Uebernahme alter Einstellungen ---------------------------------------
    // Alt: Stufe = Prozent * 255 / 100, Regler 5..100 %.
    //   - Ueber der Untergrenze: gleiche Helligkeit (die Kennlinie hat rund
    //     1,7 Stufen je Prozent, mehr als 1 Stufe Abweichung darf es nicht sein).
    //   - Darunter: unterer Anschlag (dort sah ohnehin alles gleich aus).
    int worst = 0;
    for (int oldPercent = 5; oldPercent <= 100; oldPercent++) {
        const int oldLevel   = (oldPercent * 255) / 100;
        const int newPercent = DisplayBrightness_PercentFromLinear(oldPercent);
        const int newLevel   = DisplayBrightness_LevelFor(newPercent);
        if (oldLevel < DISPLAY_BRIGHTNESS_MIN_LEVEL) {
            CHECK(newPercent == DISPLAY_BRIGHTNESS_MIN_PERCENT, "alt %d %% -> neu %d %% statt Anschlag", oldPercent, newPercent);
            continue;
        }
        const int diff = abs(newLevel - oldLevel);
        if (diff > worst) worst = diff;
        CHECK(diff <= 1, "alt %d %% (Stufe %d) -> neu %d %% (Stufe %d)", oldPercent, oldLevel, newPercent, newLevel);
    }
    CHECK(DisplayBrightness_PercentFromLinear(80) == 69, "alt 80 %% -> neu %d %%", DisplayBrightness_PercentFromLinear(80));
    CHECK(DisplayBrightness_PercentFromLinear(100) == 100, "alt 100 %% -> neu %d %%", DisplayBrightness_PercentFromLinear(100));

    printf("Kennlinie: 1 %% -> %u, 25 %% -> %u, 50 %% -> %u, 100 %% -> %u; "
           "Uebernahme alt 5/50/80 %% -> neu %d/%d/%d %% (groesste Abweichung %d Stufen)\n",
           (unsigned)DisplayBrightness_LevelFor(1), (unsigned)DisplayBrightness_LevelFor(25),
           (unsigned)DisplayBrightness_LevelFor(50), (unsigned)DisplayBrightness_LevelFor(100),
           DisplayBrightness_PercentFromLinear(5), DisplayBrightness_PercentFromLinear(50),
           DisplayBrightness_PercentFromLinear(80), worst);
    printf("%d Fehler\n", g_errors);
    return g_errors ? 1 : 0;
}
