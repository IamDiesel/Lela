// ==============================================================================
// PttCapture - Implementierung (siehe PttCapture.h)
// ==============================================================================
#include "PttCapture.h"

#include <string.h>

#ifdef ARDUINO
#include <esp_heap_caps.h>
#else
#include <stdlib.h>          // PC-Pruefung des WAV-Kopfs
#endif

namespace {

int16_t*      s_buf       = nullptr;   // Mitschnitt (PSRAM), einmal angelegt
size_t        s_capacity  = 0;         // Samples, die hineinpassen
size_t        s_count     = 0;         // Samples der laufenden/letzten Sitzung
uint32_t      s_rateHz    = 0;
volatile bool s_recording = false;     // Audio-Task schreibt gerade

void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void put32(uint8_t* p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }

}  // namespace

void PttCapture_Begin(uint32_t rateHz) {
    s_recording = true;      // zuerst: ab jetzt gibt Get() nichts mehr heraus
    s_count     = 0;
    s_rateHz    = rateHz;

    if (!s_buf) {
        const size_t samples = (size_t)rateHz * PTT_CAPTURE_SECONDS;
#ifdef ARDUINO
        s_buf = (int16_t*)heap_caps_malloc(samples * sizeof(int16_t), MALLOC_CAP_SPIRAM);
#else
        s_buf = (int16_t*)malloc(samples * sizeof(int16_t));
#endif
        s_capacity = s_buf ? samples : 0;   // ohne Speicher bleibt der Mitschnitt leer
    }
}

void PttCapture_Add(const int16_t* samples, size_t count) {
    if (!s_recording || !s_buf || !samples) return;

    const size_t room = s_capacity - s_count;
    if (count > room) count = room;
    memcpy(s_buf + s_count, samples, count * sizeof(int16_t));
    s_count += count;
}

void PttCapture_End() {
    s_recording = false;
}

bool PttCapture_Get(const int16_t** samples, size_t* count, uint32_t* rateHz) {
    if (s_recording || !s_buf || s_count == 0) return false;
    *samples = s_buf;
    *count   = s_count;
    *rateHz  = s_rateHz;
    return true;
}

void PttCapture_WavHeader(uint8_t out[PTT_WAV_HEADER_BYTES], uint32_t dataBytes, uint32_t rateHz) {
    const uint16_t channels      = 1;
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign    = (uint16_t)(channels * bitsPerSample / 8);

    memcpy(out, "RIFF", 4);
    put32(out + 4, 36 + dataBytes);              // Dateigroesse - 8
    memcpy(out + 8, "WAVEfmt ", 8);
    put32(out + 16, 16);                         // Laenge des fmt-Blocks
    put16(out + 20, 1);                          // PCM
    put16(out + 22, channels);
    put32(out + 24, rateHz);
    put32(out + 28, rateHz * blockAlign);        // Bytes je Sekunde
    put16(out + 32, blockAlign);
    put16(out + 34, bitsPerSample);
    memcpy(out + 36, "data", 4);
    put32(out + 40, dataBytes);
}
