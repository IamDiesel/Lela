#pragma once
// ==============================================================================
// VideoOverlay - Einblendung im Vollbildmodus
// ==============================================================================
// Zeigt bei gesetztem FPS-Schalter drei Werte in der linken oberen Ecke des
// Videobildes:
//   - aktuelle Bildrate
//   - Akkustand der Kamera (Baby) als Symbol mit Prozentwert
//   - Akkustand des Tablets (Tab)  als Symbol mit Prozentwert
//
// Gezeichnet wird in ein Sprite, das anschliessend in den RGB565-Bildpuffer
// einkopiert wird. Die Einblendung hat KEINEN Hintergrund: uebertragen werden
// nur die Bildpunkte, auf denen wirklich etwas steht. Alles andere bleibt
// unveraendert - das Video laeuft sichtbar hinter der Leiste weiter.
//
// Umgesetzt ueber eine Schluesselfarbe: Das Sprite wird mit OVERLAY_KEY
// gefuellt, und dieser Wert bedeutet beim Kopieren "ueberspringen".
//
// ACHTUNG, hier lag ein Fehler: Das Sprite legt 16-Bit-Farben NICHT
// zwangslaeufig in derselben Byte-Reihenfolge ab wie das Zahlenliteral im
// Quelltext. Der Vergleich gegen OVERLAY_KEY traf deshalb nie zu, und die
// Leiste bekam einen rosa Kasten. Die tatsaechliche Darstellung wird jetzt
// einmalig aus dem Sprite selbst ausgelesen (siehe s_keyRaw).
//
// Damit die Schrift auch auf hellen Bildinhalten lesbar bleibt, wird jeder
// Text zusaetzlich um einen Bildpunkt versetzt in Schwarz gezeichnet.
//
// Passt das Overlay nicht in das Bild, wird nichts gezeichnet.
// Siehe src/doc/video_refactoring.md, 2.18.
// ==============================================================================

#include <Arduino.h>

// Schluesselfarbe (Magenta). Sie darf in keinem gezeichneten Element
// vorkommen - in Weiss, Gruen, Gelb, Rot und Grau tut sie das nicht.
#define OVERLAY_KEY 0xF81F

// ------------------------------------------------------------------------------
// Aufteilung des Sprites
// ------------------------------------------------------------------------------
// Ein Akkublock besteht aus Beschriftung, Gehaeuse, Pluspol und Prozentwert.
// Die Breiten sind einzeln benannt, weil sich die Bloecke sonst ueberlappen:
// Font2 ist rund 9 bis 11 px je Zeichen breit, "100%" braucht also gut 40 px.
// Die Werte sind mit Reserve gewaehlt.
#define OVERLAY_LABEL_W    40      // "BABY" / "TAB" in Font2
#define OVERLAY_BODY_W     26      // Gehaeuse des Akkusymbols
#define OVERLAY_TIP_W       3      // Pluspol
#define OVERLAY_GAP_W       6      // Abstand vor dem Prozentwert
#define OVERLAY_VALUE_W    44      // "100%" in Font2
#define OVERLAY_BLOCK_W  (OVERLAY_LABEL_W + OVERLAY_BODY_W + OVERLAY_TIP_W + \
                          OVERLAY_GAP_W + OVERLAY_VALUE_W)

#define OVERLAY_FPS_X       6
#define OVERLAY_FPS_W      62      // "99 fps" in Font2
#define OVERLAY_BAT1_X   (OVERLAY_FPS_X + OVERLAY_FPS_W + 12)
#define OVERLAY_BAT2_X   (OVERLAY_BAT1_X + OVERLAY_BLOCK_W + 12)

#define OVERLAY_WIDTH    (OVERLAY_BAT2_X + OVERLAY_BLOCK_W + 6)
#define OVERLAY_HEIGHT    24
#define OVERLAY_MARGIN    12

// Senkrechte Lage innerhalb des Sprites.
#define OVERLAY_TEXT_Y     4       // Grundlinie der Font2-Zeilen
#define OVERLAY_ICON_Y     5       // Oberkante des Akkugehaeuses
#define OVERLAY_ICON_H    14       // Hoehe des Akkugehaeuses

// Zeichnet das Overlay in einen RGB565-Bildpuffer.
//   pixels  : Bildpuffer (Zeilenlaenge == picW, siehe VideoDecoder::compactRows)
//   picW/H  : Bildmasse in Pixeln
//   fps     : anzuzeigende Bildrate
//   camBat  : Akkustand der Kamera in Prozent (negativ = unbekannt)
//   tabBat  : Akkustand des Tablets in Prozent (negativ = unbekannt)
void VideoOverlay_Draw(uint16_t* pixels, int picW, int picH,
                       int fps, int camBat, int tabBat);
