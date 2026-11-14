// ==============================================================================
// AudioSourceHttp - Implementierung (siehe Header)
// ==============================================================================

#include "AudioSourceHttp.h"
#include "AacStreamDecoder.h"
#include "SharedData.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <new>               // placement new (Parser im PSRAM)

// ------------------------------------------------------------------------------
// Abstimmung
// ------------------------------------------------------------------------------
static const uint32_t CONNECT_TIMEOUT_MS = 4000;
// Kommt so lange nichts, gilt die Verbindung als tot. Live-Quellen senden
// auch Stille als Samples bzw. Rahmen.
static const uint32_t DATA_TIMEOUT_MS    = 5000;
static const size_t   READ_CHUNK         = 2048;

// ------------------------------------------------------------------------------
// PCM-Strom: optionaler WAV-Kopf, Ausrichtung auf ganze Sample-Paare, Mono
// ------------------------------------------------------------------------------
class PcmStreamParser {
public:
    PcmStreamParser(uint32_t rate, uint8_t channels) : m_rate(rate), m_channels(channels) {}

    void feed(const uint8_t* data, size_t len, const AudioSink& sink) {
        if (m_unsupported) return;
        if (m_state == State::Data) { payload(data, len, sink); return; }

        // Kopfbereich sammeln, bis klar ist, ob ein WAV-Kopf vorliegt.
        const size_t take = (len < sizeof(m_hdr) - m_hdrLen) ? len : (sizeof(m_hdr) - m_hdrLen);
        memcpy(m_hdr + m_hdrLen, data, take);
        m_hdrLen += take;
        parseHeader(sink);
        // Was nach dem gesammelten Kopfbereich kam, gehoert zu den Nutzdaten.
        if (m_state == State::Data && take < len) payload(data + take, len - take, sink);
    }

private:
    enum class State : uint8_t { Header, Data };

    static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    static uint32_t le32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    void startData(const AudioSink& sink, const char* origin) {
        m_state = State::Data;
        sink.setRate(m_rate);
        Serial.printf("[Audio] PCM: %lu Hz, %s (%s)\n", (unsigned long)m_rate,
                      m_channels == 2 ? "Stereo -> Mono" : "Mono", origin);
    }

    void parseHeader(const AudioSink& sink) {
        if (m_hdrLen < 12) return;

        // Kein WAV: alles bisher Gesammelte sind schon Samples.
        if (memcmp(m_hdr, "RIFF", 4) != 0 || memcmp(m_hdr + 8, "WAVE", 4) != 0) {
            startData(sink, "Einstellung");
            payload(m_hdr, m_hdrLen, sink);
            return;
        }

        // WAV: Bloecke durchgehen bis "data". Groessenangaben eines Live-
        // Stroms sind oft Platzhalter (0xFFFFFFFF) - beim data-Block egal.
        size_t off = 12;
        while (off + 8 <= m_hdrLen) {
            const uint8_t* id = m_hdr + off;
            const uint32_t size = le32(m_hdr + off + 4);

            if (memcmp(id, "data", 4) == 0) {
                startData(sink, "WAV-Kopf");
                payload(m_hdr + off + 8, m_hdrLen - off - 8, sink);
                return;
            }
            if (memcmp(id, "fmt ", 4) == 0) {
                if (off + 8 + 16 > m_hdrLen) break;           // fmt noch unvollstaendig
                const uint16_t tag  = le16(m_hdr + off + 8);
                const uint16_t ch   = le16(m_hdr + off + 10);
                const uint32_t rate = le32(m_hdr + off + 12);
                const uint16_t bits = le16(m_hdr + off + 22);
                if (tag != 1 || bits != 16 || ch < 1 || ch > 2 || rate < 8000 || rate > 48000) {
                    Serial.printf("[Audio] WAV nicht unterstuetzt (Format %u, %u Bit, %u Kanaele, %lu Hz) - "
                                  "noetig: PCM 16 Bit, 1-2 Kanaele, 8-48 kHz.\n",
                                  tag, bits, ch, (unsigned long)rate);
                    m_unsupported = true;
                    return;
                }
                m_rate = rate;
                m_channels = (uint8_t)ch;
            }
            off += 8 + size + (size & 1);
        }

        if (m_hdrLen == sizeof(m_hdr)) {
            Serial.println("[Audio] WAV-Kopf zu lang oder unlesbar - Strom wird ignoriert.");
            m_unsupported = true;
        }
    }

    // Nutzdaten: nur ganze Sample-Paare weitergeben, den Rest aufheben.
    void payload(const uint8_t* data, size_t len, const AudioSink& sink) {
        const size_t frameBytes = 2u * m_channels;
        uint8_t* work = (uint8_t*)m_work;
        while (len > 0) {
            const size_t space = sizeof(m_work) - m_workLen;
            const size_t take = (len < space) ? len : space;
            memcpy(work + m_workLen, data, take);
            m_workLen += take;
            data += take;
            len -= take;

            const size_t frames = m_workLen / frameBytes;
            if (frames == 0) continue;
            const size_t mono = (m_channels == 2) ? AudioDownmixStereo(m_work, frames) : frames;
            sink.write(m_work, mono);

            const size_t used = frames * frameBytes;
            memmove(work, work + used, m_workLen - used);
            m_workLen -= used;
        }
    }

    State    m_state       = State::Header;
    uint32_t m_rate;
    uint8_t  m_channels;
    bool     m_unsupported = false;
    uint8_t  m_hdr[512];
    size_t   m_hdrLen      = 0;
    int16_t  m_work[1024];               // 2 KB, ausgerichtet fuer int16-Zugriff
    size_t   m_workLen     = 0;          // in Bytes
};

// ------------------------------------------------------------------------------
// Oeffentliche Funktionen
// ------------------------------------------------------------------------------
String AudioSourceHttp_LogUrl(const char* url) {
    String s(url ? url : "");
    const int scheme = s.indexOf("://");
    const int at = s.indexOf('@');
    const int slash = s.indexOf('/', scheme >= 0 ? scheme + 3 : 0);
    // Zugangsdaten stehen zwischen "://" und "@" - und nur vor dem Pfad.
    if (scheme >= 0 && at > scheme && (slash < 0 || at < slash)) {
        s = s.substring(0, scheme + 3) + "***@" + s.substring(at + 1);
    }
    return s;
}

bool AudioSourceHttp_Run(const AudioSourceConfig& cfg, const AudioSink& sink) {
    if (strncmp(cfg.url, "http://", 7) != 0) {
        Serial.printf("[Audio] HTTP-Quelle: URL muss mit http:// beginnen (%s)\n",
                      AudioSourceHttp_LogUrl(cfg.url).c_str());
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.useHTTP10(true);                    // kein Chunked-Transfer, siehe Header
    http.setReuse(false);
    http.setConnectTimeout(CONNECT_TIMEOUT_MS);
    http.setTimeout(DATA_TIMEOUT_MS);
    if (!http.begin(cfg.url)) {
        Serial.printf("[Audio] HTTP-Quelle: URL ungueltig (%s)\n", AudioSourceHttp_LogUrl(cfg.url).c_str());
        return false;
    }
    const char* headerKeys[] = { "Content-Type" };
    http.collectHeaders(headerKeys, 1);

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Audio] HTTP-Quelle: Antwort %d (%s)\n", code, AudioSourceHttp_LogUrl(cfg.url).c_str());
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    const bool aac = (cfg.format == AUDIO_FMT_AAC);
    Serial.printf("[Audio] HTTP-Quelle verbunden: %s, erwartet %s, Content-Type '%s'\n",
                  AudioSourceHttp_LogUrl(cfg.url).c_str(), aac ? "AAC (ADTS)" : "PCM 16 Bit",
                  http.header("Content-Type").c_str());

    // Arbeitsspeicher je Verbindung im PSRAM: Der Empfangspuffer ist reine
    // Durchreiche, der PCM-Parser haelt Kopf (512 B) und Arbeitspuffer (2 KB).
    uint8_t* buf = (uint8_t*)heap_caps_malloc(READ_CHUNK, MALLOC_CAP_SPIRAM);
    PcmStreamParser* pcm = nullptr;
    AacStreamDecoder aacDec;
    bool ready = (buf != nullptr);
    if (ready && aac)  ready = aacDec.begin();
    if (ready && !aac) {
        void* mem = heap_caps_malloc(sizeof(PcmStreamParser), MALLOC_CAP_SPIRAM);
        if (mem) pcm = new (mem) PcmStreamParser(cfg.pcmRate, cfg.pcmChannels);
        ready = (pcm != nullptr);
    }

    bool gotAudio = false;
    uint32_t lastData = millis();
    while (ready && sink.running()) {
        const int avail = stream ? stream->available() : 0;
        if (avail <= 0) {
            if (!stream || !stream->connected()) {
                Serial.println("[Audio] HTTP-Quelle: Verbindung vom Server beendet.");
                break;
            }
            if (millis() - lastData > DATA_TIMEOUT_MS) {
                Serial.println("[Audio] HTTP-Quelle: seit 5 s keine Daten - neuer Versuch.");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        const int n = stream->read(buf, (avail < (int)READ_CHUNK) ? avail : (int)READ_CHUNK);
        if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
        lastData = millis();

        if (aac) aacDec.feed(buf, (size_t)n, sink);
        else     pcm->feed(buf, (size_t)n, sink);
        gotAudio = true;

        // Bei dichtem Datenstrom den Scheduler erreichen (gleich priorisierte Tasks).
        taskYIELD();
    }

    if (aac) {
        Serial.printf("[Audio] HTTP-Quelle beendet: %lu AAC-Rahmen, %lu Fehler.\n",
                      (unsigned long)aacDec.frames(), (unsigned long)aacDec.errors());
    }
    aacDec.end();
    if (pcm) { pcm->~PcmStreamParser(); heap_caps_free(pcm); }
    if (buf) heap_caps_free(buf);
    http.end();
    return gotAudio;
}
