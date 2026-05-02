#pragma GCC optimize ("O3")

#include "AudioStreamLogic.h"
#include "SharedData.h"
#include <M5Unified.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/stream_buffer.h>

#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorAAC.h"
#include "AudioOutput.h"

volatile bool isAudioStreaming = false;
static TaskHandle_t audioProducerTaskHandle = NULL;
static TaskHandle_t audioConsumerTaskHandle = NULL;

static StreamBufferHandle_t audioStreamBuffer = NULL;
const size_t STREAM_BUFFER_SIZE = 24576; 
const size_t WATERMARK_SIZE     = 12288; 
const uint32_t SAMPLE_RATE = 44100; 

class AudioOutputM5Speaker : public AudioOutput {
protected:
    uint32_t m_rate = 44100;
    int16_t pcm_buffer[512]; 
    uint16_t buf_idx = 0;

public:
    AudioOutputM5Speaker() { m_rate = 44100; buf_idx = 0; }
    virtual ~AudioOutputM5Speaker() { stop(); }
    virtual bool begin() override { return true; }
    virtual bool SetRate(int hz) override { m_rate = hz; return true; }
    
    virtual bool ConsumeSample(int16_t sample[2]) override {
        pcm_buffer[buf_idx++] = sample[0]; 
        
        if (buf_idx >= 512) {
            bool played = false;
            while (!played && isAudioStreaming) {
                played = M5.Speaker.playRaw(pcm_buffer, 512, m_rate, false, 1, 0);
                if (!played) vTaskDelay(pdMS_TO_TICKS(1)); 
            }
            buf_idx = 0;
        }
        return true;
    }
    
    virtual bool stop() override { 
        if (buf_idx > 0) {
            bool played = false;
            while (!played && isAudioStreaming) {
                played = M5.Speaker.playRaw(pcm_buffer, buf_idx, m_rate, false, 1, 0);
                if (!played) vTaskDelay(pdMS_TO_TICKS(1));
            }
            buf_idx = 0;
        }
        return true; 
    }
};

static void audioConsumerTask(void * pvParameters) {
    uint8_t play_chunk[1024]; 

    // Die Schleife läuft ab sofort für immer!
    while (true) {
        if (!isAudioStreaming) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Schlafe ohne CPU-Last
            continue;
        }

        bool prebuffering = true;
        xStreamBufferReset(audioStreamBuffer); // Puffer saeubern bevor es losgeht

        while (isAudioStreaming) {
            size_t avail = xStreamBufferBytesAvailable(audioStreamBuffer);
            
            if (prebuffering) {
                if (avail >= WATERMARK_SIZE) {
                    prebuffering = false; 
                } else {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            }

            if (avail == 0) {
                prebuffering = true; 
                continue;
            }

            size_t toRead = (avail > sizeof(play_chunk)) ? sizeof(play_chunk) : avail;
            toRead = toRead & ~3; 
            
            if (toRead > 0) {
                size_t bytesRead = xStreamBufferReceive(audioStreamBuffer, play_chunk, toRead, pdMS_TO_TICKS(10));
                if (bytesRead > 0) {
                    bool played = false;
                    while (!played && isAudioStreaming) {
                        played = M5.Speaker.playRaw((const int16_t*)play_chunk, bytesRead / 2, SAMPLE_RATE, false, 1, 0);
                        if (!played) vTaskDelay(pdMS_TO_TICKS(1)); 
                    }
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        
        M5.Speaker.stop(0); // Hardware stummschalten
    }
}

static void audioProducerTask(void * pvParameters) {
    while (true) {
        if (!isAudioStreaming) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (WiFi.status() != WL_CONNECTED) { 
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            continue; 
        }
        
        M5.Speaker.setChannelVolume(0, (streamVolumePercent * 255) / 100);

        if (audioFormat == 1) {
            AudioFileSourceHTTPStream *in = new AudioFileSourceHTTPStream(babyStreamUrl.c_str());
            AudioGeneratorAAC *aac = new AudioGeneratorAAC();
            AudioOutputM5Speaker *out = new AudioOutputM5Speaker();

            if (in && aac && out && aac->begin(in, out)) {
                while (isAudioStreaming && aac->isRunning()) {
                    M5.Speaker.setChannelVolume(0, (streamVolumePercent * 255) / 100);
                    if (!aac->loop()) aac->stop();
                    vTaskDelay(1); 
                }
            }
            if (aac && aac->isRunning()) aac->stop();
            if (aac) delete aac; if (out) delete out; if (in) delete in;
            
        } else {
            HTTPClient http;
            http.setReuse(false);
            http.setTimeout(3000);
            http.begin(babyStreamUrl);
            
            if (http.GET() == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    stream->setNoDelay(true);
                    stream->setTimeout(100); 
                    
                    uint8_t read_chunk[4096]; 
                    uint32_t lastDataMs = millis();
                    
                    while (isAudioStreaming && http.connected() && stream->connected()) {
                        M5.Speaker.setChannelVolume(0, (streamVolumePercent * 255) / 100);
                        
                        int available = stream->available();
                        if (available > 1) {
                            lastDataMs = millis(); 
                            int toRead = available > sizeof(read_chunk) ? sizeof(read_chunk) : (available & ~1);
                            int bytesRead = stream->read(read_chunk, toRead);
                            if (bytesRead > 0) {
                                xStreamBufferSend(audioStreamBuffer, read_chunk, bytesRead, pdMS_TO_TICKS(50));
                            }
                        } else {
                            if (millis() - lastDataMs > 3000) break;
                            vTaskDelay(pdMS_TO_TICKS(2)); 
                        }
                    }
                }
            }
            http.end();
        }
        
        if (isAudioStreaming) vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =========================================================================
// NEU: Diese Funktion sichert den Audio-Speicher direkt beim Booten!
// =========================================================================
void AudioStreamLogic_Init() {
    isAudioStreaming = false;
    
    if (audioStreamBuffer == NULL) {
        audioStreamBuffer = xStreamBufferCreate(STREAM_BUFFER_SIZE, 1);
    }
    
    if (audioStreamBuffer != NULL) {
        if (audioConsumerTaskHandle == NULL) {
            xTaskCreatePinnedToCore(audioConsumerTask, "Aud_Cons", 8192, NULL, 4, &audioConsumerTaskHandle, 1); 
        }
        if (audioProducerTaskHandle == NULL) {
            xTaskCreatePinnedToCore(audioProducerTask, "Aud_Prod", 16384, NULL, 5, &audioProducerTaskHandle, 0); 
        }
        Serial.println("[Audio] 48KB RAM erfolgreich vorab reserviert.");
    } else {
        Serial.println("[Audio] KRITISCHER FEHLER beim Reservieren des RAMs!");
    }
}

void AudioStreamLogic_Start() {
    if (isAudioStreaming) return; 
    if (audioStreamBuffer == NULL) return; // Schutz gegen Abstuerze, falls RAM beim Booten voll war
    
    isAudioStreaming = true; // Weckt die schlafenden Tasks sofort auf
}

void AudioStreamLogic_Stop() {
    isAudioStreaming = false; // Schickt die Tasks wieder sicher schlafen
}