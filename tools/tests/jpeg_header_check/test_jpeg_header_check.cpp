// ==============================================================================
// PC-Test fuer media/JpegHeaderCheck
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL):
//   python3 tools/tests/jpeg_header_check/make_samples.py /tmp/jpegs
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/media \
//       tools/tests/jpeg_header_check/test_jpeg_header_check.cpp \
//       src/media/JpegHeaderCheck.cpp -o /tmp/jpegtest
//   /tmp/jpegtest /tmp/jpegs/*.jpg
//
// Was geprueft wird:
//  1. Gute Bilder (Name ohne "bad_") werden angenommen, mit denselben Massen,
//     die der Nachbau des IDF-Parsers liest. "bad_"-Bilder werden abgelehnt.
//  2. Zufaellig beschaedigte Kopfdaten: Die Pruefung selbst bleibt in ihren
//     Grenzen (ASan/UBSan), und JEDES Bild, das sie annimmt, laesst den
//     Nachbau des IDF-Parsers innerhalb seiner Puffer und Tabellen.
//  3. Zum Vergleich: wie viele beschaedigte Bilder den IDF-Parser OHNE die
//     Pruefung aus seinen Grenzen laufen liessen.
//
// idfParse() ist ein Nachbau von esp_driver_jpeg (IDF 5.4.2: jpeg_decode.c
// jpeg_parse_marker(), jpeg_parse_marker.c) mit denselben Rechenschritten,
// aber mit Grenzwaechtern statt Speicherzugriffen ausserhalb.
// ==============================================================================
#include "JpegHeaderCheck.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

enum class Idf { Ok, Error, Violation };

struct IdfState {
    const uint8_t* buf; size_t len;
    size_t   offset = 0;
    uint32_t left   = 0;        // wie im Treiber: vorzeichenlos
    bool     violation = false;
    uint16_t width = 0, height = 0;
    uint8_t  nf = 0, qtid[4] = {}, hi[4] = {}, vi[4] = {};
};

uint8_t getChar(IdfState& s) {
    uint8_t c = 0;
    if (s.offset < s.len) c = s.buf[s.offset]; else s.violation = true;     // Lesen hinter dem Puffer
    s.offset++;
    if (s.left == 0) s.violation = true;                                    // Unterlauf der Restlaenge
    s.left--;
    return c;
}

uint32_t getBytes(IdfState& s, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) r |= (uint32_t)getChar(s) << ((n - i - 1) * 8);
    return r;
}

void skip(IdfState& s) {                       // jpeg_parse_appn_marker / _com_marker
    const uint32_t skipNum = getBytes(s, 2);
    const uint32_t n = skipNum - 2;            // wie im Treiber: darf unterlaufen
    if (skipNum < 2 || n > s.left) s.violation = true;
    s.offset += n; s.left -= n;
}

Idf idfParse(const uint8_t* buf, size_t len, IdfState& s) {
    s.buf = buf; s.len = len; s.left = (uint32_t)len;
    uint64_t guard = 0;
    const uint64_t kGuardMax = 4 * (uint64_t)len + 100000;   // weit ueber jedem regulaeren Lauf
    bool sawSos = false;

    while (s.left) {
        if (++guard > kGuardMax || s.violation) return Idf::Violation;
        const uint8_t last = getChar(s), cur = getChar(s);
        const uint16_t marker = (uint16_t)(last << 8 | cur);

        if (marker >= 0xFFE0 && marker <= 0xFFEF) skip(s);
        else if (marker == 0xFFFE) skip(s);
        else if (marker == 0xFFDB) {                                   // DQT
            uint32_t n = getBytes(s, 2); n -= 2;
            while (n) {
                if (++guard > kGuardMax || s.violation) return Idf::Violation;
                uint32_t id = getBytes(s, 1); const uint32_t prec = id >> 4; id &= 0x0F; n -= 1;
                if (id >= 4) s.violation = true;                       // qt_tbl[4][64]
                for (int i = 0; i < 64; i++) { getBytes(s, 1); n -= 1; if (prec) { getBytes(s, 1); n -= 1; } }
            }
        } else if (marker == 0xFFC0) {                                 // SOF0
            getBytes(s, 2);
            if (getBytes(s, 1) != 8) return Idf::Error;
            s.height = (uint16_t)getBytes(s, 2);
            s.width  = (uint16_t)getBytes(s, 2);
            if (((uint32_t)s.width * s.height % 8) != 0) return Idf::Error;
            s.nf = (uint8_t)getBytes(s, 1);
            if (s.nf >= 4 || s.nf == 0) return Idf::Error;
            for (int i = 0; i < s.nf; i++) {
                getBytes(s, 1);
                const uint8_t hv = (uint8_t)getBytes(s, 1);
                s.vi[i] = hv & 0x0F; s.hi[i] = hv >> 4;
                s.qtid[i] = (uint8_t)getBytes(s, 1);
            }
            if (s.hi[0] == 0 || s.vi[0] == 0) s.violation = true;      // Division durch null (mcux/mcuy)
        } else if (marker >= 0xFFC1 && marker <= 0xFFCF && marker != 0xFFC4 &&
                   marker != 0xFFC8 && marker != 0xFFCC) {
            return Idf::Error;                                         // "Only baseline-DCT"
        } else if (marker == 0xFFDD) {                                 // DRI
            if (getBytes(s, 2) != 4) return Idf::Error;
            getBytes(s, 2);
        } else if (marker == 0xFFC4) {                                 // DHT
            uint32_t n = getBytes(s, 2) - 2;
            while (n) {
                if (++guard > kGuardMax || s.violation) return Idf::Violation;
                const uint8_t info = (uint8_t)getBytes(s, 1);
                if ((info >> 4) >= 2 || (info & 0x0F) >= 2) s.violation = true;   // huffbits[2][2]
                uint32_t np = 0;
                for (int i = 0; i < 16; i++) np += getBytes(s, 1);
                if (np > 256) s.violation = true;                                  // huffcode[..][256]
                for (uint32_t i = 0; i < np; i++) getBytes(s, 1);
                n -= (1 + 16 + np);
            }
        } else if (marker == 0xFFDA) {                                 // SOS
            s.offset -= 2; s.left += 2; sawSos = true;
        } else if (marker == 0xFFFF) {                                 // "INV"
            s.offset -= 1; s.left += 1;
        }
        if (marker == 0xFFDA) break;
    }
    if (s.violation) return Idf::Violation;
    for (int i = 0; i < s.nf; i++) if (s.qtid[i] >= 4) return Idf::Violation;      // spaeterer Tabellenzugriff
    return sawSos ? Idf::Ok : Idf::Error;
}

std::vector<uint8_t> readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

} // namespace

int main(int argc, char** argv) {
    int failures = 0;
    std::vector<std::vector<uint8_t>> seeds;

    // --- 1. Beispielbilder ---------------------------------------------------
    for (int i = 1; i < argc; i++) {
        const std::string name = argv[i];
        const bool expectBad = name.find("bad_") != std::string::npos;
        const std::vector<uint8_t> d = readFile(argv[i]);
        JpegHeaderInfo info;
        const JpegCheck r = JpegHeader_Check(d.data(), d.size(), &info);
        IdfState s;
        const Idf idf = idfParse(d.data(), d.size(), s);
        const bool ok = expectBad ? (r != JpegCheck::Ok)
                                  : (r == JpegCheck::Ok && idf == Idf::Ok &&
                                     info.width == s.width && info.height == s.height);
        if (!ok) {
            failures++;
            printf("FEHLER %s: Pruefung '%s' %ux%u, IDF-Nachbau %d %ux%u\n", argv[i],
                   JpegHeader_Text(r), info.width, info.height, (int)idf, s.width, s.height);
        }
        if (!expectBad) seeds.push_back(d);
    }
    printf("Beispielbilder: %d geprueft, %d Fehler\n", argc - 1, failures);

    // --- 2. und 3. Beschaedigte Kopfdaten -------------------------------------
    std::mt19937 rng(20261115);
    const int kRounds = 400000;
    long accepted = 0, rejected = 0, unsafeAccepted = 0, idfViolations = 0;
    for (int round = 0; round < kRounds && !seeds.empty(); round++) {
        std::vector<uint8_t> d = seeds[rng() % seeds.size()];
        const size_t header = std::min<size_t>(d.size(), 700);      // Kopfdaten liegen vorn
        switch (rng() % 5) {
            case 0: for (int k = 0, n = 1 + rng() % 4; k < n; k++) d[rng() % header] = (uint8_t)rng(); break;
            case 1: d[rng() % header] ^= (uint8_t)(1u << (rng() % 8)); break;
            case 2: d.resize(2 + rng() % header); break;                       // abgeschnitten
            case 3: d.erase(d.begin() + rng() % header); break;                // Byte fehlt
            case 4: d.insert(d.begin() + rng() % header, (uint8_t)rng()); break; // Byte zu viel
        }
        // Eigene Kopie in genau passender Groesse: ASan meldet jeden Zugriff dahinter.
        std::vector<uint8_t> exact(d);
        const JpegCheck r = JpegHeader_Check(exact.data(), exact.size(), nullptr);
        IdfState s;
        const Idf idf = idfParse(exact.data(), exact.size(), s);
        if (idf == Idf::Violation) idfViolations++;
        if (r == JpegCheck::Ok) {
            accepted++;
            if (idf == Idf::Violation) {
                unsafeAccepted++;
                if (unsafeAccepted <= 5) printf("FEHLER: angenommen, aber IDF-Nachbau verlaesst seine Grenzen (Runde %d)\n", round);
            }
        } else {
            rejected++;
        }
    }
    printf("Beschaedigte Bilder: %d Runden, angenommen %ld, abgelehnt %ld\n", kRounds, accepted, rejected);
    printf("  IDF-Parser ohne Pruefung ausserhalb seiner Grenzen: %ld\n", idfViolations);
    printf("  davon trotz Pruefung durchgelassen:                 %ld\n", unsafeAccepted);
    failures += (int)unsafeAccepted;

    printf(failures ? "ERGEBNIS: %d FEHLER\n" : "ERGEBNIS: alles in Ordnung\n", failures);
    return failures ? 1 : 0;
}
