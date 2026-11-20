// ==============================================================================
// PC-Test fuer media/VoiceProcessor
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I src/media
//       src/media/VoiceProcessor.cpp tools/tests/voice_processor/test_voice_processor.cpp
//       -o /tmp/voicetest
//   /tmp/voicetest
//
// Prueft: abgeschaltet bleibt das Signal bitgleich; Hoch- und Tiefpass lassen
// die Stimme durch und daempfen ausserhalb wie zugesagt; die Pegelautomatik
// hebt leise Sprache bis zur Obergrenze bzw. bis zum Zielpegel an, senkt laute
// ab, laesst Stille in Ruhe und uebersteuert nie; die feste Verstaerkung
// stimmt; Werte ausserhalb der Grenzen werden eingefangen.
// ==============================================================================
#include "VoiceProcessor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int g_errors = 0;

#define CHECK(cond, ...) \
    do { if (!(cond)) { g_errors++; printf("FEHLER: " __VA_ARGS__); printf("\n"); } } while (0)

static const uint32_t kRate = 44100;
static const double   kPi   = 3.14159265358979323846;

static std::vector<int16_t> sine(double hz, double amplitude, double seconds, uint32_t rate = kRate) {
    std::vector<int16_t> v((size_t)(seconds * rate));
    for (size_t i = 0; i < v.size(); i++) v[i] = (int16_t)lround(amplitude * sin(2 * kPi * hz * (double)i / rate));
    return v;
}

// Verarbeitet in Bloecken wie im Geraet (512 Samples).
static void process(VoiceProcessor* vp, std::vector<int16_t>& v) {
    for (size_t off = 0; off < v.size(); off += 512) {
        const size_t n = v.size() - off < 512 ? v.size() - off : 512;
        VoiceProcessor_Process(vp, v.data() + off, n);
    }
}

// Spitze im letzten Viertel (eingeschwungener Zustand).
static double tailPeak(const std::vector<int16_t>& v) {
    double peak = 0;
    for (size_t i = v.size() * 3 / 4; i < v.size(); i++) peak = fmax(peak, fabs((double)v[i]));
    return peak;
}

static double allPeak(const std::vector<int16_t>& v) {
    double peak = 0;
    for (int16_t s : v) peak = fmax(peak, fabs((double)s));
    return peak;
}

static double db(double ratio) { return 20.0 * log10(ratio); }

// Daempfung eines Tons durch die Filter (ohne Pegelstufe), in dB.
static double filterGainDb(uint16_t hp, uint16_t lp, double toneHz) {
    VoiceProcessor vp;
    VoiceProcessor_Reset(&vp, VoiceConfig{ hp, lp, false, 0 }, kRate);
    std::vector<int16_t> v = sine(toneHz, 10000, 1.0);
    process(&vp, v);
    return db(tailPeak(v) / 10000.0);
}

int main() {
    // --- Alles aus: bitgleich ---------------------------------------------------
    {
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, false, 0 }, kRate);
        CHECK(VoiceProcessor_IsBypass(&vp), "alles aus gilt nicht als unveraendert");
        std::vector<int16_t> v = sine(1000, 20000, 0.2), ref = v;
        process(&vp, v);
        CHECK(v == ref, "alles aus: Signal veraendert");
        CHECK(VoiceProcessor_GainTenthDb(&vp) == 0, "alles aus: Verstaerkung gemeldet");
    }

    // --- Hochpass 100 Hz ----------------------------------------------------------
    CHECK(filterGainDb(100, 0, 1000) > -0.3, "Hochpass 100 Hz daempft 1 kHz um %.1f dB", filterGainDb(100, 0, 1000));
    CHECK(filterGainDb(100, 0, 300)  > -1.0, "Hochpass 100 Hz daempft 300 Hz um %.1f dB", filterGainDb(100, 0, 300));
    CHECK(fabs(filterGainDb(100, 0, 100) + 3.0) < 0.5, "Hochpass 100 Hz: bei der Grenzfrequenz %.1f dB statt -3", filterGainDb(100, 0, 100));
    CHECK(filterGainDb(100, 0, 50)  < -11.0, "Hochpass 100 Hz daempft 50 Hz nur um %.1f dB", filterGainDb(100, 0, 50));
    CHECK(filterGainDb(100, 0, 25)  < -22.0, "Hochpass 100 Hz daempft 25 Hz nur um %.1f dB", filterGainDb(100, 0, 25));

    // --- Tiefpass 7 kHz -----------------------------------------------------------
    CHECK(filterGainDb(0, 7000, 1000) > -0.3, "Tiefpass 7 kHz daempft 1 kHz um %.1f dB", filterGainDb(0, 7000, 1000));
    CHECK(filterGainDb(0, 7000, 4000) > -1.0, "Tiefpass 7 kHz daempft 4 kHz um %.1f dB", filterGainDb(0, 7000, 4000));
    CHECK(fabs(filterGainDb(0, 7000, 7000) + 3.0) < 0.6, "Tiefpass 7 kHz: bei der Grenzfrequenz %.1f dB statt -3", filterGainDb(0, 7000, 7000));
    CHECK(filterGainDb(0, 7000, 14000) < -24.0, "Tiefpass 7 kHz daempft 14 kHz nur um %.1f dB", filterGainDb(0, 7000, 14000));
    CHECK(filterGainDb(0, 7000, 18000) < -40.0, "Tiefpass 7 kHz daempft 18 kHz nur um %.1f dB", filterGainDb(0, 7000, 18000));

    // --- Beide Filter: die Stimme bleibt -----------------------------------------
    CHECK(filterGainDb(100, 7000, 500)  > -0.5, "beide Filter daempfen 500 Hz um %.1f dB", filterGainDb(100, 7000, 500));
    CHECK(filterGainDb(100, 7000, 2500) > -0.5, "beide Filter daempfen 2,5 kHz um %.1f dB", filterGainDb(100, 7000, 2500));

    // --- Feste Verstaerkung ---------------------------------------------------------
    {
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, false, 12 }, kRate);
        std::vector<int16_t> v = sine(1000, 2000, 0.5);
        process(&vp, v);
        CHECK(fabs(db(tailPeak(v) / 2000.0) - 12.0) < 0.2, "feste Verstaerkung 12 dB ergibt %.1f dB", db(tailPeak(v) / 2000.0));
        CHECK(abs(VoiceProcessor_GainTenthDb(&vp) - 120) <= 1, "feste Verstaerkung: gemeldet %d Zehntel-dB", VoiceProcessor_GainTenthDb(&vp));
    }
    {
        // Fest verstaerkt und zu laut: der Begrenzer greift, nichts uebersteuert.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, false, 20 }, kRate);
        std::vector<int16_t> v = sine(1000, 20000, 0.5);
        process(&vp, v);
        CHECK(allPeak(v) <= 30000.5, "Begrenzer: Spitze %.0f", allPeak(v));
        CHECK(tailPeak(v) > 25000, "Begrenzer nimmt zu viel weg: Spitze %.0f", tailPeak(v));
    }

    // --- Pegelautomatik --------------------------------------------------------------
    {
        // Leise Sprache, Obergrenze 12 dB: mehr als die Obergrenze gibt es nicht.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, true, 12 }, kRate);
        std::vector<int16_t> v = sine(800, 2000, 6.0);
        process(&vp, v);
        CHECK(fabs(db(tailPeak(v) / 2000.0) - 12.0) < 0.5, "Automatik, Obergrenze 12 dB: %.1f dB", db(tailPeak(v) / 2000.0));
    }
    {
        // Leise Sprache, Obergrenze 30 dB: es wird bis zum Zielpegel angehoben.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, true, 30 }, kRate);
        std::vector<int16_t> v = sine(800, 2000, 8.0);
        process(&vp, v);
        CHECK(fabs(db(tailPeak(v) / 16400.0)) < 1.5, "Automatik: Zielpegel verfehlt, Spitze %.0f", tailPeak(v));
    }
    {
        // Sehr laut: wird abgesenkt (hoechstens 6 dB), uebersteuert nie.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, true, 12 }, kRate);
        std::vector<int16_t> v = sine(800, 32000, 3.0);
        process(&vp, v);
        CHECK(allPeak(v) <= 32000.5, "Automatik, laut: Spitze %.0f", allPeak(v));
        CHECK(fabs(db(tailPeak(v) / 16400.0)) < 1.5, "Automatik, laut: nicht auf den Zielpegel gesenkt (%.0f)", tailPeak(v));
    }
    {
        // Hintergrund unter der Rauschschwelle: die Verstaerkung bleibt bei 0 dB.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, true, 30 }, kRate);
        std::vector<int16_t> v = sine(800, 100, 6.0);
        process(&vp, v);
        CHECK(tailPeak(v) < 110, "Automatik dreht Hintergrund auf: Spitze %.0f statt 100", tailPeak(v));
    }
    {
        // Erst leise (Verstaerkung steigt), dann schlagartig laut: kein Sample
        // ueber dem Begrenzer, auch nicht im ersten Augenblick.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 100, 7000, true, 30 }, kRate);
        std::vector<int16_t> quiet = sine(800, 1500, 6.0);
        process(&vp, quiet);
        std::vector<int16_t> loud = sine(800, 30000, 1.0);
        process(&vp, loud);
        CHECK(allPeak(loud) <= 30000.5, "Sprung leise -> laut: Spitze %.0f", allPeak(loud));
        // Nach dem lauten Stueck in die Pause: die Verstaerkung bleibt (fast)
        // stehen - sie darf mit der abklingenden Huellkurve nicht hochlaufen.
        const int before = VoiceProcessor_GainTenthDb(&vp);
        std::vector<int16_t> silence(kRate * 3, 0);
        process(&vp, silence);
        CHECK(abs(VoiceProcessor_GainTenthDb(&vp) - before) <= 20, "Pause: Verstaerkung wandert von %d auf %d Zehntel-dB",
              before, VoiceProcessor_GainTenthDb(&vp));
        CHECK(tailPeak(silence) == 0, "Pause: aus Stille wird Signal (%.0f)", tailPeak(silence));
    }
    {
        // Sprechen mit Pausen (0,3 s Ton, 0,6 s Ruhe, mit leisem Hintergrund):
        // Jedes Wort kommt gleich laut an, in den Pausen wird der Hintergrund
        // nicht hochgezogen ("Pumpen").
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 0, 0, true, 24 }, kRate);
        double firstPause = 0, lastPause = 0, lastWord = 0;
        for (int word = 0; word < 12; word++) {
            std::vector<int16_t> tone = sine(600, 3000, 0.3);
            process(&vp, tone);
            std::vector<int16_t> pause = sine(600, 60, 0.6);
            process(&vp, pause);
            if (word == 0)  firstPause = tailPeak(pause);
            if (word == 11) { lastPause = tailPeak(pause); lastWord = allPeak(tone); }
        }
        CHECK(lastWord > 9000 && lastWord <= 30000.5, "Worte mit Pausen: Wortspitze %.0f", lastWord);
        // Hintergrund 60: mit der fuer das Wort passenden Verstaerkung (~15 dB)
        // sind das rund 330, nie die volle Obergrenze von 24 dB (950).
        CHECK(lastPause < 500, "Worte mit Pausen: Hintergrund in der Pause auf %.0f hochgezogen", lastPause);
        (void)firstPause;
    }

    // --- Grenzen der Einstellungen ------------------------------------------------
    {
        const VoiceConfig c = VoiceConfig_Sanitized(VoiceConfig{ 5, 60000, true, 200 });
        CHECK(c.highpassHz == VOICE_HIGHPASS_MIN_HZ && c.lowpassHz == VOICE_LOWPASS_MAX_HZ && c.gainDb == VOICE_GAIN_MAX_DB,
              "Grenzen: %u Hz, %u Hz, %u dB", (unsigned)c.highpassHz, (unsigned)c.lowpassHz, (unsigned)c.gainDb);
        const VoiceConfig off = VoiceConfig_Sanitized(VoiceConfig{ 0, 0, false, 0 });
        CHECK(off.highpassHz == 0 && off.lowpassHz == 0, "0 (aus) wurde veraendert");

        // Niedrige Abtastrate: ein Tiefpass ueber der halben Rate wird weggelassen
        // statt instabil zu werden.
        VoiceProcessor vp;
        VoiceProcessor_Reset(&vp, VoiceConfig{ 100, 7000, false, 0 }, 8000);
        std::vector<int16_t> v = sine(500, 10000, 0.5, 8000);
        for (size_t off2 = 0; off2 < v.size(); off2 += 512) {
            VoiceProcessor_Process(&vp, v.data() + off2, v.size() - off2 < 512 ? v.size() - off2 : 512);
        }
        CHECK(allPeak(v) < 12000 && allPeak(v) > 8000, "8 kHz Abtastrate: Spitze %.0f", allPeak(v));
    }

    printf("%d Fehler\n", g_errors);
    return g_errors ? 1 : 0;
}
