#pragma once
// ==============================================================================
// PpaGeometry - Ausschnitt und Faktor fuer den PPA so waehlen, dass am Bildrand
//               keine winzige Rest-Kachel entsteht
// ==============================================================================
// Anlass (Projekt-Notiz video_freeze_befund_heimwlan_2026-11-15): Das Geraet
// fror im Vollbild ein, weil ein Skalier-Auftrag an den PPA (SRM) nie fertig
// wurde. Der Treiber wartet darauf ohne Zeitlimit, der Decoder-Task haelt
// dabei die LVGL-Sperre - Bild steht, Touch tot, Ton laeuft weiter.
// Aufgetreten bei 800x450 (Faktor 1,5625), nie bei 1280x720 (Faktor 1).
//
// Hintergrund: Der PPA verarbeitet das Bild in Makrobloecken (16x16 Pixel der
// Quelle) und schreibt das Ergebnis in Einheiten von 64 Pixeln Breite (RGB565)
// ueber einen Zwischenpuffer von 12 x 128 Bit. Espressif beschreibt dazu einen
// Hardwarefehler (DIG-734, "SRM operations may get stuck on DMA", umgangen ab
// IDF 5.5.3 - dieses Projekt baut mit IDF 5.4.2): Ist die letzte Kachel
// unten rechts kleiner als dieser Zwischenpuffer, kann der Transfer haengen
// bleiben. Espressifs Bedingung, mit denselben Groessen wie hier:
//
//   Breite im Ziel   = Quellbreite * Faktor
//   Restbreite       = Breite im Ziel mod 64        (0 zaehlt als 64)
//   Restzeilen Quelle= Quellhoehe mod 16            (0 zaehlt als 16)
//   Resthoehe        = Restzeilen Quelle * Faktor
//   Rest-Kachel      = Restbreite * Resthoehe * Bit je Pixel   <  1536 Bit
//
//   1280x720, Faktor 1      -> 64 x 16 Pixel = 16384 Bit   (unauffaellig)
//   800x450,  Faktor 25/16  -> 34 x  3 Pixel =  1632 Bit   (hing am Geraet)
//
// 800x450 liegt knapp UEBER Espressifs Grenze. Dass es derselbe Fehler ist,
// ist damit nicht bewiesen - Ursache und Wirkung passen, die Zahl nicht ganz.
// Diese Datei haelt deshalb Abstand: Die Rest-Kachel soll mindestens das
// Vierfache der Grenze haben (PPA_SAFE_TAIL_BITS). Reicht sie nicht, wird in
// dieser Reihenfolge nachgeholfen - das Unauffaelligste zuerst:
//   1. Hoehe auf ein Vielfaches von 16 kuerzen (hoechstens 15 Zeilen am
//      unteren Rand; 450 -> 448)
//   2. Breite auf ein Vielfaches von 16 kuerzen (hoechstens 15 Pixel am linken
//      Rand - die Zeilenlaenge im Puffer muss die Bildbreite bleiben)
//   3. beides
//   4. dasselbe mit einem um 1/16 bzw. 2/16 kleineren Faktor (das Bild wird
//      dabei hoechstens um ein Achtel kleiner)
// Findet sich nichts ueber der Schwelle, gewinnt die groesste Rest-Kachel.
// Liegt selbst die noch unter Espressifs Grenze (PPA_FIFO_BITS), wird der
// Faktor weiter gesenkt - bis zur Haelfte -, bis die Grenze erreicht ist.
//
// Nebenbei rechnet die Datei mit dem Faktor, den der PPA wirklich benutzt
// (Schritte von 1/16, abgerundet). Die Zielgroesse stimmt damit - das Bild
// laesst sich mittig setzen statt nach einer Groesse, die es nie hat.
//
// Reines C++ ohne Abhaengigkeiten: auf dem PC testbar (tools/tests/ppa_geometry).
// Der HangGuard bleibt das Netz, falls der PPA trotzdem haengt.
// ==============================================================================

#include <stdint.h>

// Espressifs Grenze: Zwischenpuffer des 2D-DMA, 12 Worte zu 128 Bit.
static const uint32_t PPA_FIFO_BITS = 12u * 128u;

// Gewuenschter Abstand: das Vierfache. Annahme, kein Messwert - siehe oben.
static const uint32_t PPA_SAFE_TAIL_BITS = 4u * PPA_FIFO_BITS;

// Rest-Kachel "gibt es nicht": der Auftrag besteht aus einer einzigen Einheit.
static const uint32_t PPA_NO_TAIL = 0xFFFFFFFFu;

struct PpaFit {
    bool     valid    = false;   // false: nichts zeichnen (Masse unsinnig)
    uint16_t srcX     = 0;       // Ausschnitt in der Quelle
    uint16_t srcY     = 0;       // (derzeit immer 0: gekuerzt wird unten)
    uint16_t srcW     = 0;
    uint16_t srcH     = 0;
    uint16_t scale16  = 0;       // Faktor in Sechzehnteln (16 = 1:1)
    uint16_t outW     = 0;       // Groesse im Ziel, wie der PPA sie erzeugt
    uint16_t outH     = 0;
    uint32_t tailBits = 0;       // Rest-Kachel in Bit bzw. PPA_NO_TAIL
    bool     adjusted = false;   // Ausschnitt oder Faktor wurde angepasst
};

// Rest-Kachel eines Auftrags nach Espressifs Rechnung (RGB565: 16 Bit je Pixel).
uint32_t PpaGeometry_TailBits(uint16_t blockW, uint16_t blockH, uint16_t scale16,
                              uint8_t bitsPerPixel = 16);

// Passt ein Bild (srcW x srcH) unter Wahrung des Seitenverhaeltnisses in ein
// Feld (boxW x boxH) ein.
//   avoidSmallTail = true : Ausschnitt/Faktor wie oben beschrieben anpassen.
//   avoidSmallTail = false: nur einpassen (Verhalten vor dieser Absicherung,
//                           zum Nachstellen des Fehlers).
PpaFit PpaGeometry_Fit(uint16_t srcW, uint16_t srcH, uint16_t boxW, uint16_t boxH,
                       bool avoidSmallTail = true);
