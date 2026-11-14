#pragma once
// ==============================================================================
// MapPngDecoder - PNG-Kachel -> RGB565
// ==============================================================================
// Einzige Stelle im Projekt, die den PNG-Decoder kennt.
//
// Verwendet wird lgfx_pngle aus M5GFX (Original kikuchan/pngle, MIT-Lizenz).
// Er ist Teil des gepinnten M5GFX und ohnehin mitkompiliert (Basis von
// drawPng) - keine neue Abhaengigkeit. Er dekodiert im Durchlauf (zeilenweise),
// beherrscht alle PNG-Farbtypen einschliesslich Palette (OSM-Kacheln) und
// Interlace.
//
// lgfx_pngle ist ein INTERNES Hilfsmodul von M5GFX, keine dokumentierte
// Schnittstelle. Darum ist es hier gekapselt: aendert ein spaeteres
// M5GFX-Update etwas, betrifft das nur diese Datei.
//
// Speicher: eine Decoder-Struktur von ~44 KB (Inflate-Zustand + 32-KB-Fenster).
// Sie wird per malloc angelegt und landet wegen SPIRAM_MALLOC_ALWAYSINTERNAL
// = 4096 im PSRAM. Dazu kurzlebig ~1-2 KB intern (Zeilen- und Palettenpuffer).
//
// Nicht threadsicher: ein Decoder-Objekt je Task.
// ==============================================================================

#include <Arduino.h>

// true, wenn die Daten mit der PNG-Signatur beginnen.
bool MapPngDecoder_IsPng(const uint8_t* data, size_t len);

// Decoder-Struktur aus lgfx_pngle.h (dort: typedef struct _pngle_t pngle_t).
// Nur vorwaerts deklariert, damit dieser Header M5GFX nicht einbindet.
struct _pngle_t;

class MapPngDecoder {
public:
    MapPngDecoder() = default;
    ~MapPngDecoder();

    MapPngDecoder(const MapPngDecoder&) = delete;
    MapPngDecoder& operator=(const MapPngDecoder&) = delete;

    // Dekodiert ein PNG mit genau dstW x dstH Pixeln nach 'dst'.
    // Ausgabe: RGB565 in nativer Byte-Reihenfolge - identisch mit lv_color_t
    // bei LV_COLOR_DEPTH 16 und LV_COLOR_16_SWAP 0 (lv_conf.h).
    // Transparente Pixel erhalten 'background'.
    // false bei defekten Daten oder abweichender Bildgroesse.
    bool decode(const uint8_t* png, size_t len, uint16_t* dst, uint16_t dstW, uint16_t dstH,
                uint16_t background = 0xDEFB);

private:
    // Wiederverwendete Decoder-Struktur. Wie M5GFX selbst (LGFXBase::draw_png)
    // nicht je Bild neu anlegen: das vermeidet Fragmentierung.
    _pngle_t* m_pngle = nullptr;
};
