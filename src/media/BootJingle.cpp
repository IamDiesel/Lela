// ==============================================================================
// BootJingle - Implementierung (siehe BootJingle.h)
// ==============================================================================
#include "BootJingle.h"
#include "AudioStreamLogic.h"   // AudioHw_Lock, AudioHw_SpeakerUsable
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <stdlib.h>

namespace {

// ------------------------------------------------------------------------------
// Rahmen
// ------------------------------------------------------------------------------
constexpr uint32_t kRate    = 24000;                          // Abtastrate (Hz)
constexpr float    kLengthS = 1.40f;                          // Gesamtlaenge
constexpr size_t   kSamples = (size_t)(kRate * kLengthS);
constexpr float    kMaster  = 0.80f;                          // Aussteuerung (Reserve)
constexpr float    kFadeS   = 0.05f;                          // Ausblenden am Ende
constexpr int      kChannel = 1;                              // UI-Kanal des Mixers

// Sinus-Tabelle: schneller als sinf(), genau genug fuer den kleinen Lautsprecher.
constexpr int kLutSize = 2048;                                // Zweierpotenz

// ------------------------------------------------------------------------------
// Teil 1: "Mrrp?" - Tonhoehe steigt schnell an und faellt fragend zurueck
// ------------------------------------------------------------------------------
constexpr float kMeowLenS    = 0.26f;
constexpr float kMeowStartHz = 560.0f;
constexpr float kMeowPeakHz  = 900.0f;
constexpr float kMeowEndHz   = 690.0f;
constexpr float kMeowPeakS   = 0.08f;    // Zeitpunkt der hoechsten Tonhoehe
constexpr float kMeowGain    = 0.55f;
constexpr float kMeowAttackS = 0.02f;
constexpr float kMeowReleaseS = 0.08f;
constexpr float kTrillHz     = 26.0f;    // Schnurr-Triller am Anfang
constexpr float kTrillS      = 0.09f;
constexpr float kTrillDepth  = 0.7f;

// Obertoene der "Stimme": {Vielfaches, Anteil}
struct Harmonic { float mult; float gain; };
constexpr Harmonic kMeowHarmonics[] = { {1.0f, 1.00f}, {2.0f, 0.35f}, {3.0f, 0.12f} };

// ------------------------------------------------------------------------------
// Teil 2: Spieluhr - gezupfte Toene mit Abklingen
// ------------------------------------------------------------------------------
struct Pluck { float startS; float freqHz; float gain; float decayS; };
constexpr Pluck kPlucks[] = {
    { 0.30f, 1318.5f, 0.50f, 0.15f },    // E6
    { 0.42f, 1568.0f, 0.50f, 0.15f },    // G6
    { 0.56f, 1046.5f, 0.60f, 0.36f },    // C6  - Schlussklang
    { 0.56f, 1568.0f, 0.30f, 0.36f },    //       mit Quinte G6
};
constexpr float kPluckAttackS = 0.004f;

// Obertoene einer Spieluhr-Zunge: hoehere klingen schneller ab.
struct PluckHarmonic { float mult; float gain; float decayMul; };
constexpr PluckHarmonic kPluckHarmonics[] = {
    {1.0f, 1.00f, 1.0f}, {2.0f, 0.28f, 2.0f}, {3.0f, 0.08f, 3.0f}
};

constexpr size_t kVoices = (sizeof(kPlucks) / sizeof(kPlucks[0]))
                         * (sizeof(kPluckHarmonics) / sizeof(kPluckHarmonics[0]));

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
enum class State : uint8_t { Idle, Pending, Playing };
State    s_state = State::Idle;
int16_t* s_pcm   = nullptr;
uint32_t s_endMs = 0;

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
void* allocPsram(size_t bytes) {
    void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(bytes);
}

float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
float easeOut(float x) { x = clamp01(x); return 1.0f - (1.0f - x) * (1.0f - x); }
float smooth(float x)  { x = clamp01(x); return x * x * (3.0f - 2.0f * x); }

// Phase in Umdrehungen [0, 1) -> Sinus aus der Tabelle.
inline float lutSin(const float* lut, float phase) {
    return lut[(int)(phase * kLutSize) & (kLutSize - 1)];
}

inline void advance(float& phase, float inc) {
    phase += inc;
    if (phase >= 1.0f) phase -= (float)(int)phase;
}

float meowFreq(float t) {
    if (t < kMeowPeakS) {
        return kMeowStartHz + (kMeowPeakHz - kMeowStartHz) * easeOut(t / kMeowPeakS);
    }
    return kMeowPeakHz + (kMeowEndHz - kMeowPeakHz) * smooth((t - kMeowPeakS) / (kMeowLenS - kMeowPeakS));
}

float meowEnvelope(float t) {
    float env = clamp01(t / kMeowAttackS);
    const float rel = kMeowLenS - t;
    if (rel < kMeowReleaseS) env *= 0.5f - 0.5f * cosf(3.14159265f * clamp01(rel / kMeowReleaseS));
    if (t < kTrillS) {
        // Schnurr-Triller, der zum Ende des Abschnitts ausklingt.
        const float trill = 0.5f - 0.5f * cosf(6.28318531f * kTrillHz * t);
        env *= 1.0f - kTrillDepth * trill * (1.0f - t / kTrillS);
    }
    return env;
}

// Ein Teilton einer gezupften Note.
struct Voice {
    uint32_t start;     // erste Abtastung
    float    inc;       // Phasenschritt je Abtastung
    float    phase;
    float    amp;       // aktuelle Amplitude
    float    decay;     // Faktor je Abtastung
    float    attackInc; // Anstieg je Abtastung
    float    attack;
};

// Berechnet den ganzen Jingle nach 'out' (kSamples Werte).
void render(int16_t* out, const float* lut) {
    Voice voices[kVoices];
    size_t v = 0;
    for (const Pluck& p : kPlucks) {
        for (const PluckHarmonic& h : kPluckHarmonics) {
            voices[v++] = Voice{
                (uint32_t)(p.startS * kRate),
                p.freqHz * h.mult / kRate,
                0.0f,
                p.gain * h.gain,
                expf(-h.decayMul / (p.decayS * kRate)),
                1.0f / (kPluckAttackS * kRate),
                0.0f,
            };
        }
    }

    float meowPhase[sizeof(kMeowHarmonics) / sizeof(kMeowHarmonics[0])] = {};
    const uint32_t meowEnd  = (uint32_t)(kMeowLenS * kRate);
    const uint32_t fadeFrom = (uint32_t)((kLengthS - kFadeS) * kRate);

    for (uint32_t i = 0; i < kSamples; i++) {
        const float t = (float)i / kRate;
        float s = 0.0f;

        // Teil 1: "Mrrp?"
        if (i < meowEnd) {
            const float f = meowFreq(t);
            float m = 0.0f;
            for (size_t k = 0; k < sizeof(meowPhase) / sizeof(meowPhase[0]); k++) {
                m += kMeowHarmonics[k].gain * lutSin(lut, meowPhase[k]);
                advance(meowPhase[k], f * kMeowHarmonics[k].mult / kRate);
            }
            s += kMeowGain * meowEnvelope(t) * m;
        }

        // Teil 2: Spieluhr
        for (Voice& vc : voices) {
            if (i < vc.start || vc.amp < 1e-4f) continue;
            if (vc.attack < 1.0f) vc.attack = fminf(1.0f, vc.attack + vc.attackInc);
            s += vc.amp * vc.attack * lutSin(lut, vc.phase);
            advance(vc.phase, vc.inc);
            vc.amp *= vc.decay;
        }

        // Sanft ausblenden, damit am Ende nichts knackt.
        if (i >= fadeFrom) s *= (float)(kSamples - i) / (float)(kSamples - fadeFrom);

        s *= kMaster;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out[i] = (int16_t)lrintf(s * 32767.0f);
    }
}

void release() {
    free(s_pcm);   // heap_caps_malloc-Speicher darf mit free() freigegeben werden
    s_pcm   = nullptr;
    s_state = State::Idle;
}

} // namespace

// ==============================================================================
// Oeffentliche Funktionen
// ==============================================================================
void BootJingle_Start() {
    if (s_state != State::Idle) return;

    float* lut = (float*)allocPsram(kLutSize * sizeof(float));
    s_pcm      = (int16_t*)allocPsram(kSamples * sizeof(int16_t));
    if (!lut || !s_pcm) {
        free(lut);
        release();
        Serial.println("[Jingle] Kein Speicher - Start ohne Jingle.");
        return;
    }

    for (int i = 0; i < kLutSize; i++) lut[i] = sinf(6.28318531f * i / kLutSize);
    const uint32_t t0 = millis();
    render(s_pcm, lut);
    free(lut);

    Serial.printf("[Jingle] Berechnet in %lu ms.\n", (unsigned long)(millis() - t0));
    s_state = State::Pending;
}

void BootJingle_Update() {
    switch (s_state) {
        case State::Idle:
            return;

        case State::Pending:
            // Der Lautsprecher gehoert ab dem Start von AudioStreamLogic dessen
            // Schloss. Ist er (noch) nicht nutzbar, entfaellt der Jingle.
            if (!AudioHw_SpeakerUsable()) { release(); return; }
            if (!AudioHw_Lock(50)) return;   // naechster loop()-Durchlauf
            M5.Speaker.playRaw(s_pcm, kSamples, kRate, false, 1, kChannel, true);
            AudioHw_Unlock();
            s_endMs = millis() + (uint32_t)(kLengthS * 1000.0f);
            s_state = State::Playing;
            return;

        case State::Playing:
            // Puffer erst freigeben, wenn der Mixer ihn sicher nicht mehr liest.
            if ((int32_t)(millis() - s_endMs) < 100) return;
            if (M5.Speaker.isPlaying(kChannel)) return;
            release();
            return;
    }
}
