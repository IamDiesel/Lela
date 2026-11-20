#pragma once
// ==============================================================================
// PanelBlitCopy - gedrehtes Kopieren in den hochkant liegenden Bildspeicher
// ==============================================================================
// Der Bildspeicher des Tab5-Panels liegt hochkant (720 x 1280), die Oberflaeche
// ist quer (1280 x 720). Jedes Bild muss beim Uebertragen um 90 Grad gedreht
// werden. M5GFX tut das allgemein: je Zielzeile ein Aufruf ueber einen
// Funktionszeiger mit Festkomma-Rechnung je Pixel - fuer einen LVGL-Streifen
// (1280 x 16) sind das 1280 Aufrufe zu je 16 Pixeln. Gemessen am 16.11.2026:
// 75-110 ms fuer zwei Drittel des Bildschirms, mehr als die Haelfte der Zeit
// eines Bildes beim Scrollen.
//
// Diese Funktion kennt nur die beiden Querformat-Lagen und kopiert direkt:
// aussen ueber die Spalten der Quelle (= Zeilen des Bildspeichers, also
// fortlaufend im Ziel), innen ueber die Zeilen der Quelle.
//
// Lage ("Drehung" wie M5GFX intern, siehe Panel_FrameBufferBase):
//   R1: Punkt (x, y) der Oberflaeche liegt im Bildspeicher in Zeile x,
//       Spalte (Breite - 1 - y)
//   R3: Zeile (Hoehe - 1 - x), Spalte y
//
// Reines C++ ohne Abhaengigkeiten: auf dem PC testbar (tools/tests/panel_blit).
// Welche Lage gilt und ob Bytes zu tauschen sind, stellt PanelBlit am Geraet
// durch einen Selbsttest gegen M5GFX fest.
// ==============================================================================

#include <stdint.h>

enum class PanelTurn : uint8_t { R1, R3 };

struct PanelBlitTarget {
    uint8_t*  fb        = nullptr;   // Bildspeicher, zeilenweise
    uint32_t  lineBytes = 0;         // Bytes je Zeile des Bildspeichers
    uint16_t  physW     = 0;         // Breite des Bildspeichers in Pixeln (720)
    uint16_t  physH     = 0;         // Hoehe (1280)
    PanelTurn turn      = PanelTurn::R1;
    bool      swapBytes = false;     // Quelle und Ziel haben verschiedene Bytefolge
};

// Kopiert das Rechteck [x1..x2] x [y1..y2] (Koordinaten der Oberflaeche,
// einschliesslich) in den Bildspeicher. `src` zeigt auf den Punkt (srcX, srcY)
// eines zeilenweisen RGB565-Puffers mit `srcW` Pixeln je Zeile; das Rechteck
// muss ganz in diesem Puffer und im Bildschirm liegen.
inline void PanelBlit_Copy(const PanelBlitTarget& t, const uint16_t* src, int srcW, int srcX, int srcY,
                           int x1, int y1, int x2, int y2) {
    if (x1 > x2 || y1 > y2) return;
    const int count = y2 - y1 + 1;

    for (int x = x1; x <= x2; x++) {
        uint16_t*       dst;
        const uint16_t* s;
        int             step;
        if (t.turn == PanelTurn::R1) {
            // Zielzeile x; die Spalten laufen gegen y, also bei y2 beginnen.
            dst  = (uint16_t*)(t.fb + (uint32_t)x * t.lineBytes) + (t.physW - 1 - y2);
            s    = src + (y2 - srcY) * srcW + (x - srcX);
            step = -srcW;
        } else {
            dst  = (uint16_t*)(t.fb + (uint32_t)(t.physH - 1 - x) * t.lineBytes) + y1;
            s    = src + (y1 - srcY) * srcW + (x - srcX);
            step = srcW;
        }
        if (!t.swapBytes) {
            for (int k = 0; k < count; k++) { dst[k] = *s; s += step; }
        } else {
            for (int k = 0; k < count; k++) { const uint16_t v = *s; dst[k] = (uint16_t)((v << 8) | (v >> 8)); s += step; }
        }
    }
}

// Ort eines einzelnen Punkts im Bildspeicher (fuer Selbsttest und PC-Test).
inline uint16_t* PanelBlit_PixelAt(const PanelBlitTarget& t, int x, int y) {
    if (t.turn == PanelTurn::R1) return (uint16_t*)(t.fb + (uint32_t)x * t.lineBytes) + (t.physW - 1 - y);
    return (uint16_t*)(t.fb + (uint32_t)(t.physH - 1 - x) * t.lineBytes) + y;
}
