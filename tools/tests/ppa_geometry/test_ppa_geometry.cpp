// ==============================================================================
// PC-Test fuer media/PpaGeometry
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL):
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/media
//       tools/tests/ppa_geometry/test_ppa_geometry.cpp
//       src/media/PpaGeometry.cpp -o /tmp/ppatest
//   /tmp/ppatest
// (Befehl in einer Zeile eingeben.)
//
// Was geprueft wird:
//  1. Feste Faelle: die Rechnung aus der Projekt-Notiz (800x450 -> 1632 Bit,
//     1280x720 -> 16384 Bit) und die erwartete Anpassung (450 -> 448 Zeilen).
//  2. Alle Bildgroessen 16..1920 x 16..1080 (Schritt 2) in Vollbild und in
//     einige Overlay-Felder: das Ergebnis passt ins Feld, der Ausschnitt liegt
//     im Bild, die Kuerzung bleibt unter 16 Pixeln, der Faktor sinkt hoechstens
//     um 2/16 und um ein Achtel, und die Rest-Kachel ist nie kleiner als ohne
//     Anpassung.
//  3. Zur Einordnung: wie viele dieser Groessen ohne bzw. mit Anpassung unter
//     Espressifs Grenze (1536 Bit) und unter der Schwelle (6144 Bit) liegen.
// ==============================================================================
#include "PpaGeometry.h"
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

#define CHECK(cond, ...)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            g_failures++;                                           \
            std::printf("FEHLER %s:%d: ", __FILE__, __LINE__);      \
            std::printf(__VA_ARGS__);                               \
            std::printf("\n");                                      \
        }                                                           \
    } while (0)

static void fixedCases() {
    // Rechnung aus der Projekt-Notiz.
    CHECK(PpaGeometry_TailBits(1280, 720, 16) == 16384, "1280x720 @1");
    CHECK(PpaGeometry_TailBits(800, 450, 25) == 1632, "800x450 @25/16");
    CHECK(PpaGeometry_TailBits(800, 448, 25) == 13600, "800x448 @25/16");
    CHECK(PpaGeometry_TailBits(64, 16, 16) == PPA_NO_TAIL, "eine Einheit");

    // 1280x720 im Vollbild: unveraendert, 1:1.
    PpaFit f = PpaGeometry_Fit(1280, 720, 1280, 720);
    CHECK(f.valid && !f.adjusted && f.scale16 == 16 && f.outW == 1280 && f.outH == 720, "1280x720");

    // 800x450 im Vollbild: zwei Zeilen weniger, Faktor bleibt 25/16.
    f = PpaGeometry_Fit(800, 450, 1280, 720);
    CHECK(f.valid && f.adjusted, "800x450 angepasst");
    CHECK(f.srcX == 0 && f.srcY == 0 && f.srcW == 800 && f.srcH == 448, "800x450 Ausschnitt %u,%u %ux%u",
          f.srcX, f.srcY, f.srcW, f.srcH);
    CHECK(f.scale16 == 25 && f.outW == 1250 && f.outH == 700, "800x450 Ziel %ux%u @%u", f.outW, f.outH, f.scale16);
    CHECK(f.tailBits == 13600, "800x450 Rest %lu", (unsigned long)f.tailBits);

    // Dasselbe ohne Absicherung: das alte Verhalten.
    f = PpaGeometry_Fit(800, 450, 1280, 720, false);
    CHECK(f.valid && !f.adjusted && f.srcH == 450 && f.scale16 == 25 && f.outW == 1250 && f.outH == 703 &&
          f.tailBits == 1632, "800x450 ohne Absicherung");

    // 640x360: Faktor 2, Rest-Kachel gross genug - nichts kuerzen.
    f = PpaGeometry_Fit(640, 360, 1280, 720);
    CHECK(f.valid && !f.adjusted && f.scale16 == 32 && f.outW == 1280 && f.outH == 720, "640x360");

    // Unsinnige Masse und ein Feld, das kleiner ist als 1/16 des Bildes.
    CHECK(!PpaGeometry_Fit(0, 450, 1280, 720).valid, "Breite 0");
    CHECK(!PpaGeometry_Fit(800, 450, 0, 720).valid, "Feld 0");
    CHECK(!PpaGeometry_Fit(1920, 1080, 100, 50).valid, "Feld zu klein");
}

struct Tally {
    unsigned long total = 0;
    unsigned long belowFifoBefore = 0, belowFifoAfter = 0;
    unsigned long belowSafeBefore = 0, belowSafeAfter = 0;
    unsigned long adjusted = 0;
};

static void sweep(uint16_t boxW, uint16_t boxH, Tally& t) {
    for (unsigned w = 16; w <= 1920; w += 2) {
        for (unsigned h = 16; h <= 1080; h += 2) {
            const PpaFit plain = PpaGeometry_Fit((uint16_t)w, (uint16_t)h, boxW, boxH, false);
            const PpaFit fit   = PpaGeometry_Fit((uint16_t)w, (uint16_t)h, boxW, boxH, true);
            CHECK(plain.valid == fit.valid || fit.valid, "%ux%u in %ux%u: Gueltigkeit", w, h, boxW, boxH);
            if (!fit.valid) continue;
            t.total++;

            CHECK(fit.outW <= boxW && fit.outH <= boxH, "%ux%u in %ux%u: Ziel %ux%u zu gross",
                  w, h, boxW, boxH, fit.outW, fit.outH);
            CHECK(fit.outW > 0 && fit.outH > 0, "%ux%u in %ux%u: Ziel leer", w, h, boxW, boxH);
            CHECK(fit.srcX + fit.srcW <= w && fit.srcY + fit.srcH <= h, "%ux%u: Ausschnitt ausserhalb", w, h);
            CHECK(w - fit.srcW < 16 && h - fit.srcH < 16, "%ux%u: zu viel gekuerzt", w, h);
            CHECK(fit.srcX + fit.srcW == w, "%ux%u: Breite nicht links gekuerzt", w, h);
            CHECK(fit.srcY == 0, "%ux%u: Hoehe nicht unten gekuerzt", w, h);
            CHECK(fit.scale16 >= 1, "%ux%u: Faktor 0", w, h);
            if (plain.valid) {
                // Hoechstens 2/16 und ein Achtel kleiner - ausser im Notfall (Rest-Kachel
                // sonst unter Espressifs Grenze): dann bis zur Haelfte.
                const bool mild = fit.scale16 + 2 >= plain.scale16 && fit.scale16 * 8u >= plain.scale16 * 7u;
                CHECK(mild || (fit.tailBits >= PPA_FIFO_BITS && fit.scale16 * 2u >= plain.scale16),
                      "%ux%u in %ux%u: Faktor %u statt %u", w, h, boxW, boxH, fit.scale16, plain.scale16);
                CHECK(fit.tailBits >= plain.tailBits, "%ux%u in %ux%u: Rest kleiner als vorher", w, h, boxW, boxH);
                if (plain.tailBits >= PPA_SAFE_TAIL_BITS) {
                    CHECK(!fit.adjusted, "%ux%u in %ux%u: ohne Not angepasst", w, h, boxW, boxH);
                }
                if (plain.tailBits < PPA_FIFO_BITS)      t.belowFifoBefore++;
                if (plain.tailBits < PPA_SAFE_TAIL_BITS) t.belowSafeBefore++;
            }
            if (fit.tailBits < PPA_FIFO_BITS)      t.belowFifoAfter++;
            if (fit.tailBits < PPA_SAFE_TAIL_BITS) t.belowSafeAfter++;
            if (fit.adjusted) t.adjusted++;
        }
    }
}

int main() {
    fixedCases();

    static const struct { uint16_t w, h; const char* name; } kBoxes[] = {
        { 1280, 720, "Vollbild" }, { 480, 270, "Overlay 480x270" },
        { 426, 240, "Overlay 426x240" }, { 320, 180, "Overlay 320x180" },
    };
    for (const auto& box : kBoxes) {
        Tally t;
        sweep(box.w, box.h, t);
        std::printf("%-16s %7lu Groessen | unter 1536 Bit: %6lu -> %5lu | unter 6144 Bit: %6lu -> %5lu | angepasst %lu\n",
                    box.name, t.total, t.belowFifoBefore, t.belowFifoAfter,
                    t.belowSafeBefore, t.belowSafeAfter, t.adjusted);
    }

    // Die Aufloesungen, die eine Kamera ueblicherweise anbietet.
    static const uint16_t kCommon[][2] = {
        {320, 180}, {320, 240}, {352, 288}, {640, 360}, {640, 480}, {720, 480}, {800, 450},
        {800, 600}, {960, 540}, {1024, 576}, {1024, 768}, {1280, 720}, {1280, 960}, {1920, 1080},
    };
    std::printf("\nVollbild 1280x720:\n");
    for (const auto& c : kCommon) {
        const PpaFit a = PpaGeometry_Fit(c[0], c[1], 1280, 720, false);
        const PpaFit b = PpaGeometry_Fit(c[0], c[1], 1280, 720, true);
        std::printf("  %4ux%-4u  vorher %2u/16 -> %4ux%-3u Rest %6lu Bit | jetzt Ausschnitt %4ux%-4u %2u/16 -> %4ux%-3u Rest %6lu Bit%s\n",
                    c[0], c[1], a.scale16, a.outW, a.outH, (unsigned long)a.tailBits,
                    b.srcW, b.srcH, b.scale16, b.outW, b.outH, (unsigned long)b.tailBits,
                    b.tailBits < PPA_SAFE_TAIL_BITS ? "  << unter Schwelle" : "");
    }

    std::printf("\n%s (%d Fehler)\n", g_failures == 0 ? "ALLES OK" : "FEHLGESCHLAGEN", g_failures);
    return g_failures == 0 ? 0 : 1;
}
