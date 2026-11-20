#pragma once
// ==============================================================================
// PcmResampler - Abtastrate eines Mono-PCM-Stroms umrechnen (reines C++)
// ==============================================================================
// Rechnet 16-Bit-Samples fortlaufend von einer Abtastrate auf eine andere um,
// Block fuer Block und ohne Sprung an den Blockgrenzen. Zwischenwerte entstehen
// durch lineare Interpolation zwischen zwei Nachbarn.
//
// Wozu: Das Mikrofon des Tab5 liefert 44100 Hz. Damit laesst sich pruefen (und
// spaeter einstellen), mit welcher Rate die Kamera die Sprachpakete erwartet -
// ohne den Takt des Mikrofons anzufassen.
//
// Grenzen: Lineare Interpolation ist ein Werkzeug fuer Sprache, kein
// Studiofilter. Beim Herunterrechnen wird nicht vorgefiltert - sehr hohe Toene
// koennen dabei als Stoerung im Hoerbereich landen.
//
// Ohne Abhaengigkeiten, damit es am PC pruefbar ist (tools/tests/pcm_resampler).
// ==============================================================================

#include <stddef.h>
#include <stdint.h>

struct PcmResampler {
    uint32_t inRate;    // Abtastrate der Eingabe (Hz)
    uint32_t outRate;   // Abtastrate der Ausgabe (Hz)
    uint32_t acc;       // Lage der naechsten Ausgabe zwischen prev und dem
                        // folgenden Eingangswert, in Schritten von 1/outRate
    int16_t  prev;      // letzter Eingangswert des vorigen Blocks
    bool     primed;    // prev ist gueltig
};

// Neuer Strom: Raten setzen, Zustand loeschen. Beide Raten muessen > 0 sein.
void PcmResampler_Reset(PcmResampler* rs, uint32_t inRate, uint32_t outRate);

// So viele Ausgangswerte koennen aus count Eingangswerten hoechstens entstehen.
size_t PcmResampler_MaxOutput(const PcmResampler* rs, size_t count);

// Rechnet count Eingangswerte um und schreibt die Ausgabe nach out. Gibt die
// Zahl der geschriebenen Werte zurueck. outMax muss mindestens
// PcmResampler_MaxOutput() gross sein; was nicht hineinpasst, wird verworfen.
size_t PcmResampler_Process(PcmResampler* rs, const int16_t* in, size_t count,
                            int16_t* out, size_t outMax);
