// ==============================================================================
// VoiceProcessor - Implementierung (siehe VoiceProcessor.h)
// ==============================================================================
#include "VoiceProcessor.h"

#include <math.h>

namespace {

const float kPi = 3.14159265358979f;

// --- Pegelstufe -----------------------------------------------------------------
// Zielpegel der Automatik: Sprachspitzen bei -6 dB unter Vollaussteuerung.
const float kTargetPeak   = 16400.0f;
// Darunter gilt das Signal als Hintergrund: die Verstaerkung bleibt stehen.
// 250 entspricht etwa -42 dB - ueber dem Grundrauschen des Mikrofons, unter
// leiser Sprache.
const float kGateLevel    = 250.0f;
// Die Automatik darf lautes Sprechen auch absenken, hoechstens um 6 dB.
const float kMinAutoGain  = 0.5f;
// Begrenzer: knapp unter Vollaussteuerung.
const float kLimit        = 30000.0f;
// Zeitkonstanten: Die traege Huellkurve (Pegel der Stimme) klingt in 0,3 s
// ab; abgesenkt wird in 10 ms, angehoben in rund 1 s. Die schnelle Huellkurve
// (30 ms) sagt, ob gerade gesprochen wird: nur dann darf angehoben werden.
// Ohne sie stiege die Verstaerkung in jeder Pause mit der abklingenden
// Huellkurve an - das naechste Wort kaeme zu laut, dazwischen rauschte es.
const float kReleaseS     = 0.30f;
const float kActivityS    = 0.030f;
const float kAttackS      = 0.010f;
const float kRiseS        = 1.0f;

uint16_t clampHz(uint16_t hz, uint16_t lo, uint16_t hi) {
    if (hz == 0) return 0;          // 0 = aus
    if (hz < lo) return lo;
    if (hz > hi) return hi;
    return hz;
}

// Anteil je Sample fuer eine Zeitkonstante (einpoliges Nachfuehren).
float coefFor(float seconds, uint32_t rateHz) {
    return 1.0f - expf(-1.0f / (seconds * (float)rateHz));
}

// Filter 2. Ordnung nach den bekannten Formeln (RBJ Audio EQ Cookbook).
void design(VoiceBiquad* f, bool highpass, float cutoffHz, float q, uint32_t rateHz) {
    const float w0    = 2.0f * kPi * cutoffHz / (float)rateHz;
    const float cosW  = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float a0    = 1.0f + alpha;

    if (highpass) {
        f->b0 =  (1.0f + cosW) / 2.0f / a0;
        f->b1 = -(1.0f + cosW) / a0;
    } else {
        f->b0 = (1.0f - cosW) / 2.0f / a0;
        f->b1 = (1.0f - cosW) / a0;
    }
    f->b2 = f->b0;
    f->a1 = -2.0f * cosW / a0;
    f->a2 = (1.0f - alpha) / a0;
    f->z1 = 0.0f;
    f->z2 = 0.0f;
}

inline float run(VoiceBiquad* f, float x) {
    const float y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}

}  // namespace

VoiceConfig VoiceConfig_Sanitized(VoiceConfig cfg) {
    cfg.highpassHz = clampHz(cfg.highpassHz, VOICE_HIGHPASS_MIN_HZ, VOICE_HIGHPASS_MAX_HZ);
    cfg.lowpassHz  = clampHz(cfg.lowpassHz, VOICE_LOWPASS_MIN_HZ, VOICE_LOWPASS_MAX_HZ);
    if (cfg.gainDb > VOICE_GAIN_MAX_DB) cfg.gainDb = VOICE_GAIN_MAX_DB;
    return cfg;
}

void VoiceProcessor_Reset(VoiceProcessor* vp, const VoiceConfig& cfg, uint32_t rateHz) {
    vp->cfg = VoiceConfig_Sanitized(cfg);

    // Filter muessen deutlich unter der halben Abtastrate bleiben.
    const float nyquistGuard = 0.45f * (float)rateHz;

    vp->useHighpass = vp->cfg.highpassHz > 0 && (float)vp->cfg.highpassHz < nyquistGuard;
    if (vp->useHighpass) {
        design(&vp->highpass, true, (float)vp->cfg.highpassHz, 0.70710678f, rateHz);   // Butterworth
    }

    vp->useLowpass = vp->cfg.lowpassHz > 0 && (float)vp->cfg.lowpassHz < nyquistGuard;
    if (vp->useLowpass) {
        // Butterworth 4. Ordnung = zwei Stufen mit diesen Guetewerten.
        design(&vp->lowpass[0], false, (float)vp->cfg.lowpassHz, 0.54119610f, rateHz);
        design(&vp->lowpass[1], false, (float)vp->cfg.lowpassHz, 1.30656296f, rateHz);
    }

    // Pegelstufe: aus, wenn weder Automatik noch feste Verstaerkung verlangt ist.
    vp->fixedGain   = powf(10.0f, (float)vp->cfg.gainDb / 20.0f);
    vp->maxGain     = vp->fixedGain;
    vp->useGain     = vp->cfg.autoGain || vp->cfg.gainDb > 0;
    vp->envelope     = 0.0f;
    vp->activity     = 0.0f;
    vp->gain         = vp->cfg.autoGain ? 1.0f : vp->fixedGain;
    vp->releaseCoef  = coefFor(kReleaseS, rateHz);
    vp->activityCoef = coefFor(kActivityS, rateHz);
    vp->attackCoef   = coefFor(kAttackS, rateHz);
    vp->riseCoef     = coefFor(kRiseS, rateHz);

    vp->bypass = !vp->useHighpass && !vp->useLowpass && !vp->useGain;
}

bool VoiceProcessor_IsBypass(const VoiceProcessor* vp) {
    return vp->bypass;
}

void VoiceProcessor_Process(VoiceProcessor* vp, int16_t* samples, size_t count) {
    if (vp->bypass) return;

    for (size_t i = 0; i < count; i++) {
        float x = (float)samples[i];

        if (vp->useHighpass) x = run(&vp->highpass, x);
        if (vp->useLowpass) {
            x = run(&vp->lowpass[0], x);
            x = run(&vp->lowpass[1], x);
        }

        if (vp->useGain) {
            // Huellkurven: Spitzen sofort, Abklingen traege bzw. schnell.
            const float level = fabsf(x);
            if (level > vp->envelope) vp->envelope = level;
            else                      vp->envelope -= vp->envelope * vp->releaseCoef;
            if (level > vp->activity) vp->activity = level;
            else                      vp->activity -= vp->activity * vp->activityCoef;

            // Gewuenschte Verstaerkung. Die Automatik rechnet nur, waehrend
            // gesprochen wird; sonst bleibt der Wert stehen.
            float want = vp->gain;
            if (!vp->cfg.autoGain) {
                want = vp->fixedGain;
            } else if (vp->activity > kGateLevel) {
                want = kTargetPeak / vp->envelope;
                if (want > vp->maxGain)  want = vp->maxGain;
                if (want < kMinAutoGain) want = kMinAutoGain;
            }

            // Absenken schnell, anheben langsam.
            if (want < vp->gain) vp->gain += (want - vp->gain) * vp->attackCoef;
            else                 vp->gain += (want - vp->gain) * vp->riseCoef;

            // Begrenzer: reicht das nicht, die Verstaerkung fuer dieses Sample
            // sofort zuruecknehmen - sie erholt sich danach von selbst.
            x *= vp->gain;
            if (level > 0.0f && fabsf(x) > kLimit) {
                vp->gain = kLimit / level;
                x = x < 0.0f ? -kLimit : kLimit;
            }
        }

        // Runden und auf 16 Bit begrenzen (Filter koennen leicht ueberschwingen).
        if (x >  32767.0f) x =  32767.0f;
        if (x < -32768.0f) x = -32768.0f;
        samples[i] = (int16_t)lrintf(x);
    }
}

int VoiceProcessor_GainTenthDb(const VoiceProcessor* vp) {
    if (!vp->useGain || vp->gain <= 0.0f) return 0;
    return (int)lrintf(200.0f * log10f(vp->gain));
}
