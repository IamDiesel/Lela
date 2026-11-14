#pragma once
// ==============================================================================
// AacStreamDecoder - AAC (ADTS) aus einem Bytestrom in Mono-PCM
// ==============================================================================
// Kapselt den Helix-Decoder (lib/libhelix-aac). Der Bytestrom darf beliebig
// zerstueckelt ankommen: Der Decoder sammelt, sucht das ADTS-Syncwort und
// dekodiert jeden Rahmen erst, wenn er vollstaendig im Puffer liegt.
// Abtastrate und Kanalzahl liefert der Stream selbst; Stereo wird zu Mono
// gemischt (der Lautsprecher des Tab5 ist mono).
//
// Speicher: Decoder (einige 10 KB, mit SBR) und Puffer liegen im PSRAM - internes RAM
// braucht der WLAN-Treiber.
// ==============================================================================

#include "AudioSourceTypes.h"

class AacStreamDecoder {
public:
    // Legt Decoder und Puffer an. false bei Speichermangel.
    bool begin();
    // Gibt alles frei. Mehrfach aufrufbar.
    void end();
    // Nimmt Bytes aus dem Netz an und gibt fertige Rahmen an die Senke.
    void feed(const uint8_t* data, size_t len, const AudioSink& sink);

    uint32_t frames() const { return m_frames; }
    uint32_t errors() const { return m_errors; }

private:
    void decodeBuffered(const AudioSink& sink);
    void dropFront(size_t bytes);

    void*    m_dec       = nullptr;   // HAACDecoder (Header bleibt in der .cpp)
    uint8_t* m_in        = nullptr;   // gesammelte, noch nicht dekodierte Bytes
    size_t   m_inLen     = 0;
    int16_t* m_out       = nullptr;   // Ausgabe eines Rahmens (bis 2048 x 2 Samples)
    uint32_t m_rate      = 0;         // zuletzt gemeldete Abtastrate
    uint32_t m_frames    = 0;
    uint32_t m_errors    = 0;
    bool     m_announced = false;     // Formatzeile schon geloggt?
};
