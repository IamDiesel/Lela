// ==============================================================================
// VideoOverlay - Implementierung
// ==============================================================================

#include "VideoOverlay.h"
#include <M5Unified.hpp>
#include <stdio.h>
#include <string.h>

// Bewusst als Zeiger: ein Sprite-Objekt im Dateibereich wuerde seinen
// Konstruktor VOR M5.begin() ausfuehren und dabei aus einem noch nicht
// initialisierten Display lesen.
static LGFX_Sprite* s_sprite = nullptr;
static bool     s_ready       = false;
static bool     s_failed      = false;
static bool     s_hasContent  = false;   // Sprite schon einmal gezeichnet?
static uint32_t s_lastRedraw  = 0;
static int      s_lastFps     = -1;
static int      s_lastCamBat  = -999;
static int      s_lastTabBat  = -999;

// Die Schluesselfarbe SO, WIE SIE IM SPRITE-PUFFER STEHT.
//
// Nicht OVERLAY_KEY verwenden: das Sprite legt 16-Bit-Farben je nach
// Konfiguration byte-vertauscht ab. Der Vergleich gegen das Literal traf
// deshalb nie zu, kein einziger Bildpunkt galt als "leer", und die Leiste
// bekam einen rosa Kasten. Der Wert wird einmalig aus dem frisch gefuellten
// Sprite gelesen - damit stimmt er unabhaengig von der Byte-Reihenfolge.
static uint16_t s_keyRaw = OVERLAY_KEY;

// ------------------------------------------------------------------------------
// Text mit Schattenkante
// ------------------------------------------------------------------------------
// Ohne Hintergrundkasten steht die Schrift direkt auf dem Videobild. Auf einem
// hellen Motiv waere Weiss auf Weiss unlesbar. Ein um einen Bildpunkt
// versetzter schwarzer Zweitdruck kostet nichts und macht jede Farbe auf
// jedem Untergrund lesbar.
static void drawShadowed(int x, int y, uint16_t col, const char* text) {
    s_sprite->setTextColor(TFT_BLACK);
    s_sprite->setCursor(x + 1, y + 1);
    s_sprite->print(text);

    s_sprite->setTextColor(col);
    s_sprite->setCursor(x, y);
    s_sprite->print(text);
}

// ------------------------------------------------------------------------------
// Akkusymbol mit Fuellstand
// ------------------------------------------------------------------------------
// x ist der linke Rand des GESAMTEN Blocks; die Aufteilung steht im Header.
static void drawBattery(int x, int percent, const char* label) {
    s_sprite->setFont(&fonts::Font2);
    drawShadowed(x, OVERLAY_TEXT_Y, TFT_WHITE, label);

    // Gehaeuse und Pluspol. Der schwarze Rahmen einen Bildpunkt ausserhalb
    // uebernimmt hier die Rolle der Schattenkante.
    const int bx = x + OVERLAY_LABEL_W;
    s_sprite->drawRect(bx - 1, OVERLAY_ICON_Y - 1,
                       OVERLAY_BODY_W + 2, OVERLAY_ICON_H + 2, TFT_BLACK);
    s_sprite->drawRect(bx, OVERLAY_ICON_Y, OVERLAY_BODY_W, OVERLAY_ICON_H, TFT_WHITE);
    s_sprite->fillRect(bx + OVERLAY_BODY_W, OVERLAY_ICON_Y + 4, OVERLAY_TIP_W, 6, TFT_WHITE);

    const int tx = bx + OVERLAY_BODY_W + OVERLAY_TIP_W + OVERLAY_GAP_W;

    if (percent >= 0) {
        const int p = (percent > 100) ? 100 : percent;
        const uint16_t col = (p < 20) ? TFT_RED : ((p < 50) ? TFT_YELLOW : TFT_GREEN);

        // Fuellbalken innerhalb des Gehaeuses, ein Bildpunkt Luft ringsum.
        const int fillW = ((OVERLAY_BODY_W - 4) * p) / 100;
        if (fillW > 0) {
            s_sprite->fillRect(bx + 2, OVERLAY_ICON_Y + 2, fillW, OVERLAY_ICON_H - 4, col);
        }

        // Prozentwert in derselben Warnfarbe wie der Balken.
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", p);
        drawShadowed(tx, OVERLAY_TEXT_Y, col, buf);
    } else {
        drawShadowed(tx, OVERLAY_TEXT_Y, TFT_LIGHTGREY, "--");
    }
}

// ------------------------------------------------------------------------------
// Sprite neu zeichnen
// ------------------------------------------------------------------------------
static void redraw(int fps, int camBat, int tabBat) {
    // Schluesselfarbe statt Schwarz: alles, was hier stehen bleibt, wird beim
    // Kopieren uebersprungen.
    s_sprite->fillSprite(OVERLAY_KEY);

    // Bildrate
    s_sprite->setFont(&fonts::Font2);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d fps", (fps > 99) ? 99 : (fps < 0 ? 0 : fps));
    drawShadowed(OVERLAY_FPS_X, OVERLAY_TEXT_Y, TFT_GREEN, buf);

    // Akkustaende
    drawBattery(OVERLAY_BAT1_X, camBat, "BABY");
    drawBattery(OVERLAY_BAT2_X, tabBat, "TAB");

    s_hasContent = true;
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void VideoOverlay_Draw(uint16_t* pixels, int picW, int picH,
                       int fps, int camBat, int tabBat) {
    if (!pixels || s_failed) return;

    // Passt das Overlay ueberhaupt in das Bild? Bewusst mit Rand auf beiden
    // Seiten geprueft, damit die Kopie nie ueber das Zeilenende hinauslaeuft.
    if (picW < OVERLAY_WIDTH + 2 * OVERLAY_MARGIN ||
        picH < OVERLAY_HEIGHT + 2 * OVERLAY_MARGIN) {
        return;
    }

    if (!s_ready) {
        if (!s_sprite) s_sprite = new LGFX_Sprite(&M5.Display);
        if (!s_sprite) { s_failed = true; return; }

        // Internes SRAM statt PSRAM. Das Sprite ist klein (rund 16 KB), wird
        // aber JEDES Bild vollstaendig ausgelesen und Bildpunkt fuer
        // Bildpunkt geprueft. Diese Schleife liegt im heissen Pfad - die
        // Quelle gehoert deshalb in den schnellen Speicher, waehrend das Ziel
        // (Decoderausgabe) ohnehin im PSRAM liegen muss.
        s_sprite->setPsram(false);
        s_sprite->setColorDepth(16);

        if (!s_sprite->createSprite(OVERLAY_WIDTH, OVERLAY_HEIGHT)) {
            // Einmal melden und danach stillschweigend nichts mehr zeichnen -
            // ein fehlendes Overlay darf das Video nicht beeintraechtigen.
            Serial.println("[VideoOverlay] Sprite konnte nicht angelegt werden, Overlay bleibt aus.");
            s_failed = true;
            return;
        }

        // Die Darstellung der Schluesselfarbe aus dem Sprite selbst lernen,
        // statt sie anzunehmen. Siehe Kommentar bei s_keyRaw.
        s_sprite->fillSprite(OVERLAY_KEY);
        const uint16_t* probe = (const uint16_t*)s_sprite->getBuffer();
        if (probe) s_keyRaw = probe[0];

        s_ready      = true;
        s_hasContent = false;
        s_lastRedraw = 0;
    }

    // Erstes Bild IMMER zeichnen: ein frisch angelegtes Sprite enthaelt
    // uninitialisierten Speicher. Danach hoechstens zweimal pro Sekunde und
    // nur, wenn sich wirklich ein Wert geaendert hat.
    const uint32_t now = millis();
    const bool changed = (fps != s_lastFps || camBat != s_lastCamBat || tabBat != s_lastTabBat);

    if (!s_hasContent || (changed && (now - s_lastRedraw) >= 500)) {
        s_lastRedraw = now;
        s_lastFps    = fps;
        s_lastCamBat = camBat;
        s_lastTabBat = tabBat;
        redraw(fps, camBat, tabBat);
    }

    const uint16_t* src = (const uint16_t*)s_sprite->getBuffer();
    if (!src) return;

    // Zeilenweise uebertragen, aber nur die belegten Bildpunkte. Leere
    // Bildpunkte bleiben unberuehrt - dadurch hat die Leiste keinen
    // Hintergrund und das Video bleibt dahinter sichtbar.
    //
    // Kosten: OVERLAY_WIDTH * OVERLAY_HEIGHT Durchlaeufe je Bild, also rund
    // 8000 - gegenueber 10 ms Dekodierzeit nicht messbar.
    for (int y = 0; y < OVERLAY_HEIGHT; y++) {
        uint16_t*       dst = pixels + (size_t)(y + OVERLAY_MARGIN) * picW + OVERLAY_MARGIN;
        const uint16_t* row = src + (size_t)y * OVERLAY_WIDTH;

        for (int x = 0; x < OVERLAY_WIDTH; x++) {
            const uint16_t v = row[x];
            if (v != s_keyRaw) dst[x] = v;
        }
    }
}
