// ==============================================================================
// PpaGeometry - Implementierung (Hintergrund und Rechenweg: siehe Header)
// ==============================================================================

#include "PpaGeometry.h"

namespace {

// Makroblock des PPA in der Quelle. IDF 5.4.2 kennt nur diese Groesse
// (PPA_LL_SRM_DEFAULT_BLOCK_SIZE 18 = 16 + 2 Pixel Ueberlappung).
const uint16_t kMacroBlock = 16;

// Der PPA nimmt Faktoren von 1/16 bis knapp 256 (8 Bit Ganzzahl, 4 Bit Bruch).
const uint32_t kScaleMin16 = 1;
const uint32_t kScaleMax16 = 256u * 16u - 1u;

// So weit darf der Faktor sinken, um eine groessere Rest-Kachel zu erreichen:
// hoechstens 2/16 und hoechstens um ein Achtel - mehr faellt als kleineres
// Bild auf (vor allem im Overlay, wo der Faktor selbst klein ist).
const uint32_t kMaxStepsDown   = 2;
const uint32_t kMaxShrinkParts = 8;

// Groesse im Ziel, wie die Hardware sie bildet: Ganzzahl- und Bruchanteil
// getrennt, der Bruchanteil abgerundet.
uint32_t scaled(uint32_t pixels, uint32_t scale16) {
    return pixels * (scale16 / 16u) + (pixels * (scale16 % 16u)) / 16u;
}

// Groesster Faktor (in 1/16), mit dem w x h noch in das Feld passt.
uint32_t fitScale16(uint32_t w, uint32_t h, uint32_t boxW, uint32_t boxH) {
    const uint32_t sx = (boxW * 16u) / w;
    const uint32_t sy = (boxH * 16u) / h;
    uint32_t s = (sx < sy) ? sx : sy;
    if (s > kScaleMax16) s = kScaleMax16;
    return s;
}

// Ein Kandidat: Ausschnitt (Vollbild oder aufs Raster gekuerzt) samt Faktor.
PpaFit makeFit(uint16_t srcW, uint16_t srcH, bool cropW, bool cropH,
               uint32_t boxW, uint32_t boxH, uint32_t stepsDown) {
    PpaFit fit;

    uint16_t w = srcW;
    uint16_t h = srcH;
    if (cropW && (srcW % kMacroBlock) != 0 && srcW > kMacroBlock) w = (uint16_t)(srcW - srcW % kMacroBlock);
    if (cropH && (srcH % kMacroBlock) != 0 && srcH > kMacroBlock) h = (uint16_t)(srcH - srcH % kMacroBlock);

    const uint32_t fullScale = fitScale16(w, h, boxW, boxH);
    if (fullScale < kScaleMin16 + stepsDown) return fit;      // Feld zu klein
    const uint32_t scale16 = fullScale - stepsDown;

    fit.valid    = true;
    fit.srcW     = w;
    fit.srcH     = h;
    // Breite: nur links kuerzen. Rechts ginge nur ueber eine kleinere
    // Zeilenlaenge - die gehoert aber zum Puffer, nicht zum Ausschnitt.
    fit.srcX     = (uint16_t)(srcW - w);
    // Hoehe: unten kuerzen. Der Ausschnitt beginnt damit wie bisher am
    // Pufferanfang - gegenueber dem alten Aufruf aendert sich nur die Hoehe.
    fit.srcY     = 0;
    fit.scale16  = (uint16_t)scale16;
    fit.outW     = (uint16_t)scaled(w, scale16);
    fit.outH     = (uint16_t)scaled(h, scale16);
    fit.tailBits = PpaGeometry_TailBits(w, h, (uint16_t)scale16);
    fit.adjusted = (w != srcW) || (h != srcH) || (stepsDown != 0);
    return fit;
}

}  // namespace

// ------------------------------------------------------------------------------
// Rest-Kachel nach Espressifs Rechnung (IDF 5.5.3, ppa_srm.c, DIG-734)
// ------------------------------------------------------------------------------
uint32_t PpaGeometry_TailBits(uint16_t blockW, uint16_t blockH, uint16_t scale16,
                              uint8_t bitsPerPixel) {
    // Schreibeinheit im Ziel: 64 Pixel bei 16 Bit, 32 Pixel bei 24/32 Bit.
    const uint32_t unitW = (bitsPerPixel > 16) ? 32u : 64u;

    const uint32_t outW = scaled(blockW, scale16);

    // Eine einzige Einheit: nichts wird aufgeteilt, es gibt keinen Rest.
    if (outW <= unitW && blockH <= kMacroBlock) return PPA_NO_TAIL;

    uint32_t tailW = outW % unitW;
    if (tailW == 0) tailW = unitW;

    uint32_t tailSrcH = blockH % kMacroBlock;
    if (tailSrcH == 0) tailSrcH = kMacroBlock;
    const uint32_t tailH = scaled(tailSrcH, scale16);

    return tailW * tailH * bitsPerPixel;
}

// ------------------------------------------------------------------------------
// Einpassen
// ------------------------------------------------------------------------------
PpaFit PpaGeometry_Fit(uint16_t srcW, uint16_t srcH, uint16_t boxW, uint16_t boxH,
                       bool avoidSmallTail) {
    if (srcW == 0 || srcH == 0 || boxW == 0 || boxH == 0) return PpaFit();

    // Ausgangspunkt: ganzes Bild, groesster passender Faktor.
    PpaFit best = makeFit(srcW, srcH, false, false, boxW, boxH, 0);
    if (!avoidSmallTail) return best;
    if (best.valid && best.tailBits >= PPA_SAFE_TAIL_BITS) return best;

    // Kandidaten in der Reihenfolge "faellt am wenigsten auf". Der erste ueber
    // der Schwelle gewinnt; sonst der mit der groessten Rest-Kachel.
    static const struct { bool cropW; bool cropH; } kCrops[] = {
        { false, false }, { false, true }, { true, false }, { true, true },
    };
    uint32_t maxSteps = best.valid ? (uint32_t)best.scale16 / kMaxShrinkParts : 0;
    if (maxSteps > kMaxStepsDown) maxSteps = kMaxStepsDown;
    for (uint32_t stepsDown = 0; stepsDown <= maxSteps; stepsDown++) {
        for (const auto& crop : kCrops) {
            const PpaFit fit = makeFit(srcW, srcH, crop.cropW, crop.cropH, boxW, boxH, stepsDown);
            if (!fit.valid) continue;
            if (fit.tailBits >= PPA_SAFE_TAIL_BITS) return fit;
            if (!best.valid || fit.tailBits > best.tailBits) best = fit;
        }
    }
    return best;
}
