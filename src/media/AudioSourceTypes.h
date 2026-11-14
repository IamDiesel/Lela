#pragma once
// ==============================================================================
// AudioSourceTypes - gemeinsame Typen der Babyphone-Tonquellen
// ==============================================================================
// Jede Quelle liefert Mono-PCM (16 Bit, Little Endian) an eine Senke. Die Senke
// ist der Jitterpuffer in AudioStreamLogic; die I2S-Hardware fasst weiterhin
// nur dessen Besitzer-Task an.
//
//   [BabyCam UDP]  --------------------------\
//   [HTTP PCM / WAV] --PcmStreamParser-------+--Mono-PCM--> Ringpuffer --> Audio-Task --> I2S
//   [HTTP AAC]     --AacStreamDecoder--------/
//
// Welche Quelle gilt, steht in den Einstellungen (SharedData.h, audioSource).
// ==============================================================================

#include <Arduino.h>

// Momentaufnahme der Einstellungen fuer EINE Wiedergabe. Sie wird beim Start
// genommen und gilt bis zum Ende dieser Wiedergabe. So liest kein Hintergrund-
// task einen Arduino-String, den die Oberflaeche gerade aendert.
struct AudioSourceConfig {
    uint8_t  source;        // AudioSourceType (SharedData.h)
    uint8_t  format;        // AudioHttpFormat (SharedData.h)
    uint8_t  pcmChannels;   // 1/2 - HTTP-PCM ohne WAV-Kopf
    uint32_t pcmRate;       // Hz  - HTTP-PCM ohne WAV-Kopf
    uint16_t udpPortIn;     // BabyCam -> Tablet (Einstellung)
    uint16_t udpPortOut;    // Tablet -> BabyCam (Einstellung)
    char     url[256];      // HTTP-Quelle
};

// Senke. Alle Funktionen werden aus dem Empfangs-Task gerufen.
struct AudioSink {
    // Mono-Samples abgeben. Darf kurz blockieren, wenn der Puffer voll ist
    // (Gegendruck fuer TCP-Quellen).
    void (*write)(const int16_t* mono, size_t samples);
    // Abtastrate der folgenden Samples (Hz).
    void (*setRate)(uint32_t hz);
    // false: die Quelle soll aufhoeren (Ton aus, Gegensprechen, neue Einstellung).
    bool (*running)();
};

// Mischt verschachteltes Stereo (L R L R ...) an Ort und Stelle zu Mono.
// Gibt die Anzahl Mono-Samples zurueck (= Anzahl Stereo-Paare).
inline size_t AudioDownmixStereo(int16_t* samples, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        samples[i] = (int16_t)(((int32_t)samples[2 * i] + (int32_t)samples[2 * i + 1]) / 2);
    }
    return frames;
}
