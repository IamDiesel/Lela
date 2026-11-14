// ==============================================================================
// AacStreamDecoder - Implementierung (siehe Header)
// ==============================================================================

#include "AacStreamDecoder.h"
#include <aacdec.h>          // lib/libhelix-aac
#include <esp_heap_caps.h>
#include <string.h>

// Eingangspuffer. Ein ADTS-Rahmen ist fuer AAC-LC mit 2 Kanaelen hoechstens
// AAC_MAINBUF_SIZE (1536 B) gross; der Puffer fasst mehr als zwei davon, damit
// ein angefangener Rahmen nie an der Puffergrenze haengen bleibt.
static const size_t IN_CAP = 4096;

// Ausgabe eines Rahmens: 1024 Samples je Kanal, mit SBR (HE-AAC) doppelt so
// viele, bei AAC_MAX_NCHANS = 2 Kanaelen.
static const size_t OUT_SAMPLES = AAC_MAX_NSAMPS * 2 * AAC_MAX_NCHANS;

static const size_t ADTS_HEADER = 7;

bool AacStreamDecoder::begin() {
    end();
    m_in  = (uint8_t*)heap_caps_malloc(IN_CAP, MALLOC_CAP_SPIRAM);
    m_out = (int16_t*)heap_caps_malloc(OUT_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    // AACInitDecoder() nimmt malloc(); Bloecke ueber 4 KB landen ohnehin im
    // PSRAM (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL).
    m_dec = AACInitDecoder();

    if (!m_in || !m_out || !m_dec) {
        Serial.println("[Audio] AAC: Speicher fuer den Decoder fehlt.");
        end();
        return false;
    }
    m_inLen = 0;
    m_rate = 0;
    m_frames = 0;
    m_errors = 0;
    m_announced = false;
    return true;
}

void AacStreamDecoder::end() {
    if (m_dec) { AACFreeDecoder((HAACDecoder)m_dec); m_dec = nullptr; }
    if (m_in)  { heap_caps_free(m_in);  m_in = nullptr; }
    if (m_out) { heap_caps_free(m_out); m_out = nullptr; }
    m_inLen = 0;
}

void AacStreamDecoder::dropFront(size_t bytes) {
    if (bytes >= m_inLen) { m_inLen = 0; return; }
    memmove(m_in, m_in + bytes, m_inLen - bytes);
    m_inLen -= bytes;
}

void AacStreamDecoder::feed(const uint8_t* data, size_t len, const AudioSink& sink) {
    if (!m_dec) return;
    while (len > 0) {
        size_t space = IN_CAP - m_inLen;
        if (space == 0) {
            // Kann nur passieren, wenn ein angeblicher Rahmen nie vollstaendig
            // wird (kaputter Kopf). Puffer verwerfen und neu synchronisieren.
            m_errors++;
            m_inLen = 0;
            space = IN_CAP;
        }
        const size_t take = (len < space) ? len : space;
        memcpy(m_in + m_inLen, data, take);
        m_inLen += take;
        data += take;
        len -= take;
        decodeBuffered(sink);
    }
}

void AacStreamDecoder::decodeBuffered(const AudioSink& sink) {
    while (m_inLen >= ADTS_HEADER) {
        // 1. Auf das naechste Syncwort (0xFFF) ausrichten.
        const int sync = AACFindSyncWord(m_in, (int)m_inLen);
        if (sync < 0) {
            // Kein Syncwort: das letzte Byte behalten - es kann die erste
            // Haelfte des naechsten sein.
            m_in[0] = m_in[m_inLen - 1];
            m_inLen = 1;
            return;
        }
        if (sync > 0) dropFront((size_t)sync);
        if (m_inLen < ADTS_HEADER) return;

        // 2. Rahmenlaenge aus dem ADTS-Kopf (13 Bit, inklusive Kopf).
        const size_t frameLen = ((size_t)(m_in[3] & 0x03) << 11) |
                                ((size_t)m_in[4] << 3) |
                                ((size_t)m_in[5] >> 5);
        if (frameLen < ADTS_HEADER || frameLen > IN_CAP) {
            // Zufaellige 0xFFF-Folge in den Nutzdaten: ein Byte weiter suchen.
            m_errors++;
            dropFront(1);
            continue;
        }
        if (m_inLen < frameLen) return;   // Rahmen noch nicht vollstaendig

        // 3. Genau diesen Rahmen dekodieren.
        unsigned char* p = m_in;
        int left = (int)frameLen;
        const int rc = AACDecode((HAACDecoder)m_dec, &p, &left, m_out);
        dropFront(frameLen);

        if (rc != ERR_AAC_NONE) {
            // Einzelne Fehler sind bei einem Live-Strom normal (Einstieg
            // mitten im Rahmen). Nur selten melden.
            m_errors++;
            if (m_errors == 1 || (m_errors % 100) == 0) {
                Serial.printf("[Audio] AAC: Rahmen nicht dekodierbar (Fehler %d, bisher %lu)\n",
                              rc, (unsigned long)m_errors);
            }
            continue;
        }

        AACFrameInfo fi;
        AACGetLastFrameInfo((HAACDecoder)m_dec, &fi);
        if (fi.nChans < 1 || fi.outputSamps <= 0 || fi.sampRateOut <= 0) continue;

        if (!m_announced) {
            Serial.printf("[Audio] AAC: %d Hz, %d Kanal/Kanaele, %d kbit/s, Profil %d\n",
                          fi.sampRateOut, fi.nChans, fi.bitRate / 1000, fi.profile);
            m_announced = true;
        }
        if ((uint32_t)fi.sampRateOut != m_rate) {
            m_rate = (uint32_t)fi.sampRateOut;
            sink.setRate(m_rate);
        }

        // outputSamps zaehlt ueber alle Kanaele.
        size_t mono = (size_t)fi.outputSamps;
        if (fi.nChans == 2) mono = AudioDownmixStereo(m_out, (size_t)fi.outputSamps / 2);
        sink.write(m_out, mono);
        m_frames++;
    }
}
