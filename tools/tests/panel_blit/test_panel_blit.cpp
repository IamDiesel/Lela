// ==============================================================================
// PC-Test fuer system/PanelBlitCopy
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/system
//       tools/tests/panel_blit/test_panel_blit.cpp -o /tmp/blittest
//   /tmp/blittest
//
// Vergleicht PanelBlit_Copy mit der Abbildung, die M5GFX fuer einzelne Punkte
// benutzt (Panel_FrameBufferBase::drawPixelPreclipped, Drehung 1 und 3):
// zufaellige Rechtecke und Teilrechtecke, mit und ohne Bytetausch; alles
// ausserhalb des Rechtecks muss unberuehrt bleiben (auch die Fuellbytes am
// Zeilenende), und es darf nicht ueber den Bildspeicher hinaus geschrieben werden.
// ==============================================================================
#include "PanelBlitCopy.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static const int W = 1280, H = 720;          // Oberflaeche
static const int PW = 720, PH = 1280;        // Bildspeicher
static const uint32_t LINE = 1444;           // absichtlich mit Fuellbytes

// M5GFX: drawPixelPreclipped mit _width = W, _height = H.
static void reference(int r, int x, int y, int* px, int* py) {
    if ((1u << r) & 0b10010110) y = H - (y + 1);
    if (r & 2)                  x = W - (x + 1);
    *px = y; *py = x;                          // r & 1: tauschen
}

int main() {
    std::mt19937 rng(20261116);
    std::vector<uint8_t> fb(LINE * PH), want(LINE * PH);
    std::vector<uint16_t> src;
    int failures = 0;

    for (int round = 0; round < 400 && failures == 0; round++) {
        const int r = (round & 1) ? 1 : 3;
        const bool swap = (round & 2) != 0;
        // Quellrechteck (wie ein LVGL-Flush) und ein Teilrechteck darin.
        const int sw = 1 + (int)(rng() % 1280), sh = 1 + (int)(rng() % (round % 5 == 0 ? 300 : 16));
        const int sx = (int)(rng() % (W - sw + 1)), sy = (int)(rng() % (H - sh + 1));
        const int x1 = sx + (int)(rng() % sw), x2 = x1 + (int)(rng() % (sx + sw - x1));
        const int y1 = sy + (int)(rng() % sh), y2 = y1 + (int)(rng() % (sy + sh - y1));

        src.resize((size_t)sw * sh);
        for (auto& v : src) v = (uint16_t)rng();
        for (auto& b : fb) b = (uint8_t)rng();
        want = fb;
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                int px, py;
                reference(r, x, y, &px, &py);
                uint16_t v = src[(size_t)(y - sy) * sw + (x - sx)];
                if (swap) v = (uint16_t)((v << 8) | (v >> 8));
                memcpy(&want[(size_t)py * LINE + (size_t)px * 2], &v, 2);
            }
        }

        PanelBlitTarget t;
        t.fb = fb.data(); t.lineBytes = LINE; t.physW = PW; t.physH = PH;
        t.turn = (r == 1) ? PanelTurn::R1 : PanelTurn::R3; t.swapBytes = swap;
        PanelBlit_Copy(t, src.data(), sw, sx, sy, x1, y1, x2, y2);

        if (fb != want) { failures++; printf("FEHLER Runde %d: r=%d swap=%d Rechteck %d,%d-%d,%d\n", round, r, swap, x1, y1, x2, y2); }

        int px, py;
        reference(r, x1, y1, &px, &py);
        if ((uint8_t*)PanelBlit_PixelAt(t, x1, y1) != fb.data() + (size_t)py * LINE + (size_t)px * 2) {
            failures++; printf("FEHLER Runde %d: PixelAt\n", round);
        }
    }
    printf("%s (%d Fehler)\n", failures == 0 ? "ALLES OK" : "FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
