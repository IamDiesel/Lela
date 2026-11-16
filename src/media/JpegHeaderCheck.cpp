// ==============================================================================
// JpegHeaderCheck - Implementierung (siehe JpegHeaderCheck.h)
// ==============================================================================
#include "JpegHeaderCheck.h"

namespace {

// Marker (zweites Byte nach FF)
constexpr uint8_t kSoi  = 0xD8;
constexpr uint8_t kSof0 = 0xC0;
constexpr uint8_t kDht  = 0xC4;
constexpr uint8_t kDqt  = 0xDB;
constexpr uint8_t kDri  = 0xDD;
constexpr uint8_t kSos  = 0xDA;
constexpr uint8_t kCom  = 0xFE;

// Grenzen des IDF-Treibers (jpeg_private.h / hal/jpeg_types.h)
constexpr uint8_t  kMaxQuantTables = 4;     // qt_tbl[JPEG_COMPONENT_NUMBER_MAX][64]
constexpr uint8_t  kMaxHuffClass   = 1;     // huffbits[2][2][16]: Klasse 0/1 (DC/AC)
constexpr uint8_t  kMaxHuffId      = 1;     //                     Tabelle 0/1
constexpr uint16_t kMaxHuffCodes   = 256;   // huffcode[2][2][256]

inline uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

bool isApp(uint8_t m)        { return m >= 0xE0 && m <= 0xEF; }
bool isOtherSof(uint8_t m)   { return m >= 0xC1 && m <= 0xCF && m != kDht; }

// DQT: Folge von Tabellen. Je Tabelle 1 Byte (Genauigkeit << 4 | Nummer),
// dann 64 Werte zu 1 oder 2 Byte. Muss genau aufgehen.
bool checkDqt(const uint8_t* p, size_t n, bool& any) {
    while (n > 0) {
        const uint8_t precision = p[0] >> 4;
        const uint8_t id        = p[0] & 0x0F;
        if (precision > 1 || id >= kMaxQuantTables) return false;
        const size_t need = 1 + (precision ? 128 : 64);
        if (n < need) return false;
        p += need; n -= need;
        any = true;
    }
    return true;
}

// DHT: Folge von Tabellen. Je Tabelle 1 Byte (Klasse << 4 | Nummer), 16
// Laengenzaehler, dann so viele Werte, wie die Zaehler zusammen angeben.
bool checkDht(const uint8_t* p, size_t n) {
    while (n > 0) {
        if (n < 17) return false;
        if ((p[0] >> 4) > kMaxHuffClass || (p[0] & 0x0F) > kMaxHuffId) return false;
        uint16_t codes = 0;
        for (int i = 1; i <= 16; i++) codes += p[i];
        if (codes > kMaxHuffCodes) return false;
        const size_t need = 17 + codes;
        if (n < need) return false;
        p += need; n -= need;
    }
    return true;
}

// SOF0: Genauigkeit, Hoehe, Breite, Anzahl Komponenten, je Komponente 3 Byte.
// Zugelassen ist, was der Treiber kennt: 8 Bit, Graustufen oder 3 Komponenten,
// Helligkeit 1x1 / 2x1 / 2x2 (4:4:4, 4:2:2, 4:2:0), Farbe 1x1.
JpegCheck checkSof(const uint8_t* p, size_t n, JpegHeaderInfo& info) {
    if (n < 6) return JpegCheck::BadSof;
    const uint8_t  precision = p[0];
    const uint16_t height    = be16(p + 1);
    const uint16_t width     = be16(p + 3);
    const uint8_t  comps     = p[5];
    if (precision != 8) return JpegCheck::Unsupported;
    if (comps != 1 && comps != 3) return JpegCheck::Unsupported;
    if (n != (size_t)6 + 3 * comps) return JpegCheck::BadSof;
    if (width == 0 || height == 0) return JpegCheck::BadSof;

    for (uint8_t c = 0; c < comps; c++) {
        const uint8_t sampling = p[6 + 3 * c + 1];
        const uint8_t quantId  = p[6 + 3 * c + 2];
        if (quantId >= kMaxQuantTables) return JpegCheck::BadSof;
        const bool ok = (c == 0) ? (sampling == 0x11 || sampling == 0x21 || sampling == 0x22)
                                 : (sampling == 0x11);
        if (!ok) return JpegCheck::Unsupported;
    }
    info.width      = width;
    info.height     = height;
    info.components = comps;
    return JpegCheck::Ok;
}

} // namespace

JpegCheck JpegHeader_Check(const uint8_t* data, size_t len, JpegHeaderInfo* infoOut) {
    if (!data || len < 4 || data[0] != 0xFF || data[1] != kSoi) return JpegCheck::NoSoi;

    JpegHeaderInfo info;
    bool haveSof = false, haveDqt = false;
    size_t pos = 2;

    for (;;) {
        // Marker (2 Byte) und Segmentlaenge (2 Byte, zaehlt sich selbst mit).
        if (len - pos < 4) return JpegCheck::NoScan;
        if (data[pos] != 0xFF) return JpegCheck::BadMarker;
        const uint8_t marker = data[pos + 1];
        const size_t  segLen = be16(data + pos + 2);
        if (segLen < 2) return JpegCheck::BadSegment;
        if (segLen > len - pos - 2) return JpegCheck::Truncated;

        const uint8_t* payload = data + pos + 4;
        const size_t   n       = segLen - 2;

        if (marker == kSos) {
            if (!haveSof || !haveDqt) return JpegCheck::NoFrame;
            if (infoOut) *infoOut = info;
            return JpegCheck::Ok;           // dahinter folgen die Bilddaten
        } else if (isApp(marker) || marker == kCom) {
            // Inhalt beliebig; der Treiber ueberspringt das Segment.
        } else if (marker == kDqt) {
            if (!checkDqt(payload, n, haveDqt)) return JpegCheck::BadDqt;
        } else if (marker == kDht) {
            if (!checkDht(payload, n)) return JpegCheck::BadDht;
        } else if (marker == kSof0) {
            if (haveSof) return JpegCheck::BadSof;          // genau ein Bildkopf
            const JpegCheck r = checkSof(payload, n, info);
            if (r != JpegCheck::Ok) return r;
            haveSof = true;
        } else if (marker == kDri) {
            if (segLen != 4) return JpegCheck::BadSegment;  // der Treiber verlangt genau 4
        } else if (isOtherSof(marker)) {
            return JpegCheck::Unsupported;                  // progressiv, arithmetisch ...
        } else {
            return JpegCheck::BadMarker;                    // Fuellbyte FF FF, RSTn, EOI, fremd
        }
        pos += 2 + segLen;
    }
}

const char* JpegHeader_Text(JpegCheck result) {
    switch (result) {
        case JpegCheck::Ok:          return "in Ordnung";
        case JpegCheck::NoSoi:       return "kein JPEG-Anfang";
        case JpegCheck::Truncated:   return "Segment abgeschnitten";
        case JpegCheck::BadMarker:   return "unerwarteter Marker";
        case JpegCheck::Unsupported: return "nicht unterstuetztes Format";
        case JpegCheck::BadSegment:  return "Segmentlaenge unzulaessig";
        case JpegCheck::BadDqt:      return "Quantisierungstabelle unstimmig";
        case JpegCheck::BadDht:      return "Huffman-Tabelle unstimmig";
        case JpegCheck::BadSof:      return "Bildkopf unstimmig";
        case JpegCheck::NoFrame:     return "Bilddaten ohne Bildkopf";
        case JpegCheck::NoScan:      return "keine Bilddaten";
    }
    return "?";
}
