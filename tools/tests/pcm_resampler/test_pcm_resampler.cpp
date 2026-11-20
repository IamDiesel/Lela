// ==============================================================================
// PC-Test fuer media/PcmResampler
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/media
//       src/media/PcmResampler.cpp tools/tests/pcm_resampler/test_pcm_resampler.cpp
//       -o /tmp/resamplertest
//   /tmp/resamplertest
//
// Prueft fuer mehrere Zielraten: die Zahl der Ausgangswerte passt zur Rate
// (Tonhoehe und Tempo stimmen), ein Sinus bleibt ein Sinus derselben Frequenz
// (auch ueber Blockgrenzen hinweg, bei ungleichen Blockgroessen), gleiche Raten
// reichen die Werte unveraendert durch, und die Ausgabe sprengt den zugesagten
// Hoechstwert nie.
// ==============================================================================
#include "PcmResampler.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_errors = 0;

#define CHECK(cond, ...) \
    do { if (!(cond)) { g_errors++; printf("FEHLER: " __VA_ARGS__); printf("\n"); } } while (0)

static const double kPi = 3.14159265358979323846;

// Rechnet einen Sinus (toneHz, 2 s) in Bloecken wechselnder Groesse um.
static std::vector<int16_t> run(uint32_t inRate, uint32_t outRate, double toneHz, bool* overflow) {
    PcmResampler rs;
    PcmResampler_Reset(&rs, inRate, outRate);

    const size_t total = (size_t)inRate * 2;
    static const size_t kBlocks[] = { 512, 1, 7, 512, 300, 64 };   // auch krumme Groessen
    std::vector<int16_t> out;
    std::vector<int16_t> in, tmp;
    size_t done = 0, b = 0;
    *overflow = false;

    while (done < total) {
        size_t n = kBlocks[b++ % (sizeof(kBlocks) / sizeof(kBlocks[0]))];
        if (n > total - done) n = total - done;
        in.resize(n);
        for (size_t i = 0; i < n; i++) {
            in[i] = (int16_t)lround(12000.0 * sin(2 * kPi * toneHz * (double)(done + i) / inRate));
        }
        const size_t cap = PcmResampler_MaxOutput(&rs, n);
        tmp.assign(cap + 4, (int16_t)0x7ABC);                 // Waechter hinter dem Hoechstwert
        const size_t got = PcmResampler_Process(&rs, in.data(), n, tmp.data(), cap + 4);
        if (got > cap) *overflow = true;
        out.insert(out.end(), tmp.begin(), tmp.begin() + (long)got);
        done += n;
    }
    return out;
}

int main() {
    static const uint32_t kIn = 44100;
    static const uint32_t kOut[] = { 8000, 16000, 22050, 32000, 44100, 48000, 88200 };
    static const double   kTone  = 440.0;

    for (uint32_t outRate : kOut) {
        bool overflow = false;
        const std::vector<int16_t> y = run(kIn, outRate, kTone, &overflow);
        CHECK(!overflow, "%u Hz: mehr Ausgabe als PcmResampler_MaxOutput zusagt", (unsigned)outRate);

        // Laenge: 2 s Eingabe ergeben 2 s Ausgabe (der erste Wert ist Stuetzstelle).
        const long expect = (long)outRate * 2;
        CHECK(labs((long)y.size() - expect) <= 3, "%u Hz: %zu Werte statt %ld", (unsigned)outRate, y.size(), expect);

        // Inhalt: Vergleich mit dem idealen Sinus an den Ausgabezeitpunkten.
        // Lineare Interpolation eines 440-Hz-Tons bei 44100 Hz weicht um
        // hoechstens ~0,05 % der Amplitude ab; dazu Rundung.
        double worst = 0;
        for (size_t k = 0; k < y.size(); k++) {
            const double ideal = 12000.0 * sin(2 * kPi * kTone * (double)k / outRate);
            const double err = fabs((double)y[k] - ideal);
            if (err > worst) worst = err;
        }
        CHECK(worst < 12.0, "%u Hz: Abweichung vom Sinus %.1f (Sprung an einer Blockgrenze?)", (unsigned)outRate, worst);
    }

    // Gleiche Raten: Werte unveraendert (um die Stuetzstelle versetzt).
    {
        PcmResampler rs;
        PcmResampler_Reset(&rs, 44100, 44100);
        int16_t in[64], out[80];
        for (int i = 0; i < 64; i++) in[i] = (int16_t)(i * 37 - 1000);
        const size_t got = PcmResampler_Process(&rs, in, 64, out, 80);
        CHECK(got == 63, "gleiche Raten: %zu Werte statt 63", got);
        bool same = true;
        for (size_t i = 0; i < got; i++) same = same && (out[i] == in[i]);
        CHECK(same, "gleiche Raten: Werte veraendert");
    }

    // Vollaussteuerung: kein Ueberlauf beim Interpolieren zwischen -32768 und 32767.
    {
        PcmResampler rs;
        PcmResampler_Reset(&rs, 44100, 48000);
        int16_t in[32], out[64];
        for (int i = 0; i < 32; i++) in[i] = (i & 1) ? (int16_t)32767 : (int16_t)-32768;
        const size_t got = PcmResampler_Process(&rs, in, 32, out, 64);
        CHECK(got > 0 && got <= PcmResampler_MaxOutput(&rs, 32), "Vollaussteuerung: %zu Werte", got);
    }

    // Zu kleiner Ausgabepuffer: nichts wird dahinter geschrieben.
    {
        PcmResampler rs;
        PcmResampler_Reset(&rs, 44100, 48000);
        int16_t in[100] = { 0 }, out[12];
        for (int i = 0; i < 12; i++) out[i] = 0x1234;
        const size_t got = PcmResampler_Process(&rs, in, 100, out, 8);
        CHECK(got == 8, "kleiner Puffer: %zu Werte geschrieben", got);
        CHECK(out[8] == 0x1234 && out[11] == 0x1234, "kleiner Puffer: hinter dem Ende geschrieben");
    }

    printf("%d Fehler\n", g_errors);
    return g_errors ? 1 : 0;
}
