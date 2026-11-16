// ==============================================================================
// CameraExposure - Implementierung (Hintergrund und Rechenweg: siehe Header)
// ==============================================================================

#include "CameraExposure.h"
#include <math.h>
#include <stdlib.h>

namespace {

const float kPercentile   = 0.95f;
const float kDamping      = 0.6f;    // Exponent: gut die Haelfte des Fehlers je Schritt
const float kStepMax      = 2.0f;    // hoechstens Faktor 2 je Schritt, in beide Richtungen
const int   kBurnedOut    = 245;     // Perzentil in der obersten Klasse: Bild brennt aus
const float kProductMin   = (float)AE_LINES_MIN;
const float kProductMax   = (float)AE_LINES_MAX * AE_GAIN_MAX;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

int CameraExposure_Percentile95(const uint32_t hist[AE_HIST_BINS]) {
    uint32_t total = 0;
    for (int i = 0; i < AE_HIST_BINS; i++) total += hist[i];
    if (total == 0) return -1;

    const uint32_t limit = (uint32_t)((float)total * kPercentile);
    uint32_t seen = 0;
    for (int i = 0; i < AE_HIST_BINS; i++) {
        seen += hist[i];
        if (seen > limit) return i * (256 / AE_HIST_BINS) + (256 / AE_HIST_BINS) / 2;
    }
    return 255 - (256 / AE_HIST_BINS) / 2;   // alles in der letzten Klasse
}

float CameraExposure_Next(float product, int percentile95) {
    product = clampf(product, kProductMin, kProductMax);
    if (percentile95 < 0) return product;
    if (abs(percentile95 - AE_TARGET) <= AE_DEADBAND) return product;

    // Ausgebrannt: Der wahre Wert liegt irgendwo ueber dem Messbereich - nicht
    // tasten, sondern gleich halbieren.
    if (percentile95 >= kBurnedOut) return clampf(product / kStepMax, kProductMin, kProductMax);

    const float measured = (float)(percentile95 < 4 ? 4 : percentile95);
    const float wanted   = clampf((float)AE_TARGET / measured, 1.0f / kStepMax, kStepMax);
    return clampf(product * powf(wanted, kDamping), kProductMin, kProductMax);
}

ExposureSetting CameraExposure_Split(float product) {
    product = clampf(product, kProductMin, kProductMax);
    ExposureSetting s;

    if (product < (float)AE_FLICKER_LINES) {
        // Sehr hell: kurze Zeit, keine Verstaerkung. (So viel Licht ist in aller
        // Regel Tageslicht - dort flimmert nichts.)
        s.lines = (uint32_t)(product + 0.5f);
        if (s.lines < AE_LINES_MIN) s.lines = AE_LINES_MIN;
        s.gain = 1.0f;
        return s;
    }

    // Laengste Zeit im 10-ms-Raster, die das Produkt nicht ueberschreitet;
    // den Rest uebernimmt die Verstaerkung.
    uint32_t steps = (uint32_t)(product / (float)AE_FLICKER_LINES);
    const uint32_t maxSteps = AE_LINES_MAX / AE_FLICKER_LINES;
    if (steps > maxSteps) steps = maxSteps;
    s.lines = steps * AE_FLICKER_LINES;
    s.gain  = clampf(product / (float)s.lines, 1.0f, AE_GAIN_MAX);
    return s;
}

ExposureGainRegs CameraExposure_GainRegs(float gain) {
    gain = clampf(gain, 1.0f, AE_GAIN_MAX);

    // Analog: groesste Zweierpotenz bis 16, die nicht ueber der Verstaerkung liegt.
    uint32_t analog = 1;
    while (analog < 16 && gain >= (float)(analog * 2)) analog *= 2;

    // Rest (1 bis knapp 2) digital fein: 32 Schritte, Registerwert = Schritt * 4.
    int fine = (int)(gain / (float)analog * 32.0f + 0.5f);
    if (fine < 32) fine = 32;
    if (fine > 63) fine = 63;

    ExposureGainRegs r;
    r.analog        = (uint8_t)(analog - 1);   // 1,2,4,8,16 -> 0x00,0x01,0x03,0x07,0x0f
    r.digitalCoarse = 0x00;
    r.digitalFine   = (uint8_t)(fine * 4);     // 0x80 .. 0xfc
    return r;
}

float CameraExposure_GainOf(const ExposureGainRegs& regs) {
    return (float)(regs.analog + 1) * (float)(regs.digitalCoarse + 1) * (float)regs.digitalFine / 128.0f;
}
