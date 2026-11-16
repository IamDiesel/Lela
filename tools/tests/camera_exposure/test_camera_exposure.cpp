// ==============================================================================
// PC-Test fuer media/CameraExposure
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/media
//       tools/tests/camera_exposure/test_camera_exposure.cpp
//       src/media/CameraExposure.cpp -o /tmp/aetest
//   /tmp/aetest
//
// Geprueft werden: Perzentil, Register der Verstaerkung (gegen die Tabelle des
// Sensortreibers: Stuetzstellen 1/2/4/8/16-fach), Aufteilung in Zeit und
// Verstaerkung (Raster 10 ms, Grenzen), und der Regelkreis an einem einfachen
// Sensormodell: Er muss aus jeder Lage ins Totband finden und dort bleiben.
// ==============================================================================
#include "CameraExposure.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { g_failures++; printf("FEHLER Zeile %d: %s\n", __LINE__, #cond); } } while (0)

// Sensormodell: Helligkeit = Szene x Zeilen x Verstaerkung, bei 255 abgeschnitten.
// Die Regelung bekommt, was die Register wirklich ergeben (Rasterung inklusive).
static int simulate(float scene, float startProduct, int* steps) {
    float product = startProduct;
    int p95 = 0, stable = 0;
    for (*steps = 0; *steps < 60; (*steps)++) {
        const ExposureSetting s = CameraExposure_Split(product);
        const float gain = CameraExposure_GainOf(CameraExposure_GainRegs(s.gain));
        float luma = scene * (float)s.lines * gain;
        if (luma > 255) luma = 255;
        p95 = (int)luma;
        const float next = CameraExposure_Next(product, p95);
        if (next == product) { if (++stable >= 3) break; } else stable = 0;
        product = next;
    }
    return p95;
}

int main() {
    // 1. Perzentil.
    uint32_t hist[AE_HIST_BINS] = {};
    CHECK(CameraExposure_Percentile95(hist) == -1);
    hist[3] = 100;                                   // alles in Klasse 3 (24..31)
    CHECK(CameraExposure_Percentile95(hist) == 28);
    hist[31] = 4;                                    // 4 % sehr hell: zaehlt nicht
    CHECK(CameraExposure_Percentile95(hist) == 28);
    hist[31] = 10;                                   // 9 % sehr hell: zaehlt
    CHECK(CameraExposure_Percentile95(hist) == 252);

    // 2. Register der Verstaerkung: Stuetzstellen der Treibertabelle.
    struct { float gain; uint8_t analog, fine; } ref[] = {
        {1.0f, 0x00, 0x80}, {1.031f, 0x00, 0x84}, {1.969f, 0x00, 0xfc}, {2.0f, 0x01, 0x80},
        {4.0f, 0x03, 0x80}, {8.0f, 0x07, 0x80}, {16.0f, 0x0f, 0x80}, {3.0f, 0x01, 0xc0},
    };
    for (const auto& r : ref) {
        const ExposureGainRegs g = CameraExposure_GainRegs(r.gain);
        CHECK(g.analog == r.analog && g.digitalFine == r.fine && g.digitalCoarse == 0);
        CHECK(fabsf(CameraExposure_GainOf(g) - r.gain) < 0.02f);
    }
    CHECK(CameraExposure_GainRegs(0.2f).analog == 0x00 && CameraExposure_GainRegs(0.2f).digitalFine == 0x80);
    CHECK(CameraExposure_GainRegs(99.0f).analog == 0x0f && CameraExposure_GainRegs(99.0f).digitalFine == 0x80);
    for (float g = 1.0f; g <= 16.0f; g += 0.013f) {   // nie mehr als 1/64 daneben, nie ausserhalb
        const ExposureGainRegs regs = CameraExposure_GainRegs(g);
        CHECK(regs.digitalFine >= 0x80 && regs.digitalFine <= 0xfc && (regs.digitalFine % 4) == 0);
        CHECK(fabsf(CameraExposure_GainOf(regs) / g - 1.0f) < 0.02f);
    }

    // 3. Aufteilung.
    ExposureSetting s = CameraExposure_Split(100.0f);
    CHECK(s.lines == 100 && s.gain == 1.0f);
    s = CameraExposure_Split(0.5f);
    CHECK(s.lines == AE_LINES_MIN && s.gain == 1.0f);
    s = CameraExposure_Split(375.0f);
    CHECK(s.lines == 375 && s.gain == 1.0f);
    s = CameraExposure_Split(600.0f);
    CHECK(s.lines == 375 && fabsf(s.gain - 1.6f) < 0.001f);
    s = CameraExposure_Split(1125.0f);
    CHECK(s.lines == 1125 && s.gain == 1.0f);
    s = CameraExposure_Split(4500.0f);
    CHECK(s.lines == 1125 && fabsf(s.gain - 4.0f) < 0.001f);
    s = CameraExposure_Split(1e9f);
    CHECK(s.lines == 1125 && s.gain == AE_GAIN_MAX);
    for (float p = 1.0f; p < 25000.0f; p *= 1.07f) {
        s = CameraExposure_Split(p);
        CHECK(s.lines >= AE_LINES_MIN && s.lines <= AE_LINES_MAX && s.gain >= 1.0f && s.gain <= AE_GAIN_MAX);
        CHECK(s.lines < AE_FLICKER_LINES || s.lines % AE_FLICKER_LINES == 0);
    }

    // 4. Regelkreis: Totband, Richtung, Begrenzung.
    CHECK(CameraExposure_Next(500.0f, AE_TARGET) == 500.0f);
    CHECK(CameraExposure_Next(500.0f, AE_TARGET + AE_DEADBAND) == 500.0f);
    CHECK(CameraExposure_Next(500.0f, -1) == 500.0f);
    CHECK(CameraExposure_Next(500.0f, 100) > 500.0f && CameraExposure_Next(500.0f, 100) <= 1000.0f);
    CHECK(CameraExposure_Next(500.0f, 240) < 500.0f);
    CHECK(CameraExposure_Next(500.0f, 252) == 250.0f);            // ausgebrannt: halbieren
    CHECK(CameraExposure_Next(500.0f, 0) <= 1000.0f);             // hoechstens Faktor 2

    // 5. Einschwingen: Szenen von grell bis dunkel, Start an beiden Enden.
    int worst = 0;
    for (float scene = 60.0f; scene > 0.012f; scene *= 0.8f) {
        for (float start : { (float)AE_LINES_MIN, 1244.0f, 1244.0f * AE_GAIN_MAX }) {
            int steps = 0;
            const int p95 = simulate(scene, start, &steps);
            if (steps > worst) worst = steps;
            // Erreichbar ist das Ziel nur, wenn die Szene im Stellbereich liegt.
            const float atMin = scene * (float)AE_LINES_MIN;
            const float atMax = scene * 1125.0f * AE_GAIN_MAX;
            if (atMin < AE_TARGET - AE_DEADBAND && atMax > AE_TARGET + AE_DEADBAND) {
                CHECK(abs(p95 - AE_TARGET) <= AE_DEADBAND + 8);
                CHECK(steps < 40);
            }
        }
    }
    printf("Einschwingen: hoechstens %d Schritte\n", worst);

    printf("%s (%d Fehler)\n", g_failures == 0 ? "ALLES OK" : "FEHLGESCHLAGEN", g_failures);
    return g_failures == 0 ? 0 : 1;
}
