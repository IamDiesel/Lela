// ==============================================================================
// PcmResampler - Implementierung (siehe PcmResampler.h)
// ==============================================================================
#include "PcmResampler.h"

void PcmResampler_Reset(PcmResampler* rs, uint32_t inRate, uint32_t outRate) {
    rs->inRate  = inRate;
    rs->outRate = outRate;
    rs->acc     = 0;
    rs->prev    = 0;
    rs->primed  = false;
}

size_t PcmResampler_MaxOutput(const PcmResampler* rs, size_t count) {
    // count Eingangswerte ueberstreichen count Abstaende; je Abstand entstehen
    // outRate/inRate Ausgangswerte. +1 fuer den Rest aus dem vorigen Block.
    return (size_t)(((uint64_t)count * rs->outRate) / rs->inRate) + 1;
}

size_t PcmResampler_Process(PcmResampler* rs, const int16_t* in, size_t count,
                            int16_t* out, size_t outMax) {
    size_t written = 0;

    for (size_t i = 0; i < count; i++) {
        const int16_t cur = in[i];

        // Der allererste Wert ist nur Stuetzstelle: interpoliert wird immer
        // zwischen zwei Nachbarn.
        if (!rs->primed) {
            rs->prev   = cur;
            rs->primed = true;
            continue;
        }

        // Alle Ausgabezeitpunkte, die zwischen prev und cur liegen. acc zaehlt
        // in 1/outRate-Schritten eines Eingangsabstands: 0 = bei prev,
        // outRate = bei cur. Jede Ausgabe rueckt um inRate weiter.
        while (rs->acc < rs->outRate) {
            if (written < outMax) {
                const int32_t delta = (int32_t)cur - (int32_t)rs->prev;
                out[written++] = (int16_t)(rs->prev + (int32_t)(((int64_t)delta * rs->acc) / rs->outRate));
            }
            rs->acc += rs->inRate;
        }

        rs->acc -= rs->outRate;   // naechster Eingangsabstand
        rs->prev = cur;
    }

    return written;
}
