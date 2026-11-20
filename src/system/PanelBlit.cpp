// ==============================================================================
// PanelBlit - Implementierung (siehe Header)
// ==============================================================================

#include "PanelBlit.h"
#include "PanelBlitCopy.h"
#include <M5Unified.h>
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>

namespace {

enum class State : uint8_t { Unknown, Ready, Unavailable };

State           s_state = State::Unknown;
PanelBlitTarget s_target;

// Testmuster 2 x 2: vier verschiedene Werte, deren Bytes sich auch getauscht
// nicht gleichen - so sind Lage, Richtung beider Achsen und Bytefolge eindeutig.
const uint16_t kPattern[4] = { 0x1234, 0x5678, 0x9ABC, 0xDEF1 };

// Liegt das Muster (links oben bei x0, y0) dort, wo diese Lage es erwartet?
bool patternAt(const PanelBlitTarget& t, int x0, int y0) {
    for (int i = 0; i < 4; i++) {
        uint16_t want = kPattern[i];
        if (t.swapBytes) want = (uint16_t)((want << 8) | (want >> 8));
        if (*PanelBlit_PixelAt(t, x0 + (i & 1), y0 + (i >> 1)) != want) return false;
    }
    return true;
}

// Einrichten beim ersten Aufruf. (Nicht "setup": so heisst die Arduino-Funktion.)
void prepare() {
    s_state = State::Unavailable;

    // Der Bildspeicher wird wie in LGFX_PPA ueber das DSI-Panel geholt; das
    // gibt es so nur auf dem Tab5.
    auto* panel = M5.Display.getPanel();
    if (!panel || M5.getBoard() != m5::board_t::board_M5Tab5) return;
    const auto& cfg = panel->config();
    const int w = M5.Display.width();
    const int h = M5.Display.height();

    // Nur Querformat auf hochkant liegendem Bildspeicher.
    if (w != (int)cfg.panel_height || h != (int)cfg.panel_width || w < 32 || h < 32) return;

    PanelBlitTarget t;
    t.fb        = (uint8_t*)static_cast<lgfx::Panel_DSI*>(panel)->config_detail().buffer;
    t.physW     = cfg.panel_width;
    t.physH     = cfg.panel_height;
    t.lineBytes = (((uint32_t)cfg.panel_width * 2u) + 3u) & ~3u;   // wie Panel_DSI::init
    if (!t.fb) return;

    // --- Selbsttest gegen M5GFX -------------------------------------------------
    // Zwei Stellen (nahe gegenueberliegender Ecken): Inhalt merken, Muster
    // ueber M5GFX schreiben, im Bildspeicher suchen, Inhalt zuruecksetzen.
    const int probe[2][2] = { { 4, 6 }, { w - 9, h - 7 } };
    uint16_t saved[2][4];
    for (int p = 0; p < 2; p++) {
        M5.Display.readRect(probe[p][0], probe[p][1], 2, 2, saved[p]);
        M5.Display.pushImage(probe[p][0], probe[p][1], 2, 2, kPattern);
    }

    bool found = false;
    static const PanelTurn kTurns[] = { PanelTurn::R1, PanelTurn::R3 };
    for (PanelTurn turn : kTurns) {
        for (int swap = 0; swap < 2 && !found; swap++) {
            t.turn = turn;
            t.swapBytes = (swap != 0);
            found = patternAt(t, probe[0][0], probe[0][1]) && patternAt(t, probe[1][0], probe[1][1]);
        }
        if (found) break;
    }

    for (int p = 0; p < 2; p++) {
        M5.Display.pushImage(probe[p][0], probe[p][1], 2, 2, saved[p]);
    }
    M5.Display.display();

    if (!found) {
        Serial.println("[LVGL] Schneller Flush nicht moeglich (Lage des Bildspeichers unbekannt) - weiter ueber M5GFX.");
        return;
    }
    s_target = t;
    s_state = State::Ready;
    Serial.printf("[LVGL] Schneller Flush aktiv (Lage %s, %s).\n",
                  t.turn == PanelTurn::R1 ? "R1" : "R3", t.swapBytes ? "Bytes getauscht" : "ohne Bytetausch");
}

}  // namespace

bool PanelBlit_Ready() {
    if (s_state == State::Unknown) prepare();
    return s_state == State::Ready;
}

void PanelBlit_Push(const uint16_t* src, int srcW, int srcX, int srcY, int x1, int y1, int x2, int y2) {
    if (s_state != State::Ready) return;
    PanelBlit_Copy(s_target, src, srcW, srcX, srcY, x1, y1, x2, y2);
}
