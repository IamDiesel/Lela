#pragma GCC optimize ("O3")
#include "AudioStreamLogic.h"
#include "SharedData.h"
#include "BabyCamApi.h"
#include <M5Unified.hpp>
#include <WiFi.h>
#include <WiFiUdp.h>

volatile bool isAudioStreaming = false;
volatile bool isPTTActive = false;
static bool resumeBabyAudioAfterPTT = false;

static TaskHandle_t audioProducerTaskHandle = NULL;
static TaskHandle_t audioConsumerTaskHandle = NULL;

static void audioConsumerTask(void * pvParameters) {
    WiFiUDP udpIn;
    uint8_t buffer[1024];

    while (true) {
        if (!isAudioStreaming || isPTTActive) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        udpIn.begin(50001);
        while (isAudioStreaming && !isPTTActive) {
            int packetSize = udpIn.parsePacket();
            if (packetSize > 0) {
                int bytesRead = udpIn.read(buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    // FIX 1: Pegel berechnen und bei 255 strikt abriegeln (Behebt "zu leise" bei 150%)
                    int calcVol = (int)( (muteMaster ? 0 : volMaster/100.0f) * (muteBaby ? 0 : volBaby/100.0f) * 255.0f );
                    if (calcVol > 255) calcVol = 255; 
                    M5.Speaker.setChannelVolume(0, calcVol);
                    
                    // FIX 2 (Rollback): Kamera sendet 16-Bit! Wieder als int16_t abspielen (Behebt lautes Rauschen)
                    M5.Speaker.playRaw((const int16_t*)buffer, bytesRead / 2, 44100, false, 1, 0);
                }
                // FIX 3: Den Task flüssig durchlaufen lassen, ohne den Puffer verhungern zu lassen (Behebt Roboter-Stottern)
                taskYIELD(); 
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
        udpIn.stop();
        M5.Speaker.stop(0);
    }
}

static void audioProducerTask(void * pvParameters) {
    WiFiUDP udpOut;
    int16_t micData[256];

    while (true) {
        if (!isPTTActive) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        while (isPTTActive) {
            if (M5.Mic.record(micData, 256, 44100)) {
                udpOut.beginPacket(streamIp.c_str(), 50003);
                udpOut.write((const uint8_t*)micData, 256 * sizeof(int16_t));
                udpOut.endPacket();
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
    }
}

void AudioStreamLogic_Init() {
    isAudioStreaming = false;
    isPTTActive = false;
    xTaskCreatePinnedToCore(audioConsumerTask, "Aud_Cons", 8192, NULL, 4, &audioConsumerTaskHandle, 1); 
    xTaskCreatePinnedToCore(audioProducerTask, "Aud_Prod", 8192, NULL, 5, &audioProducerTaskHandle, 0); 
}

void AudioStreamLogic_StartBaby() {
    if (isAudioStreaming || isPTTActive) return; 
    
    M5.Mic.end(); 
    delay(50);
    M5.Speaker.begin();
    M5.Speaker.setVolume(255);
    
    BabyCamApi_SetBabyAudio(true);
    isAudioStreaming = true;
}

void AudioStreamLogic_StopBaby() {
    if (!isAudioStreaming) return;
    
    isAudioStreaming = false;
    delay(50); 
    
    M5.Speaker.end();
    BabyCamApi_SetBabyAudio(false);
}

void AudioStreamLogic_StartPTT() {
    if (isPTTActive) return;
    
    resumeBabyAudioAfterPTT = isAudioStreaming;
    if (resumeBabyAudioAfterPTT) {
        AudioStreamLogic_StopBaby(); 
    } else {
        M5.Speaker.end();
    }
    
    delay(50);
    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = 44100;
    mic_cfg.stereo = false;
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();

    BabyCamApi_SetParentAudio(true);
    isPTTActive = true;
}

void AudioStreamLogic_StopPTT() {
    if (!isPTTActive) return;
    
    isPTTActive = false; 
    delay(50); 
    
    M5.Mic.end(); 
    BabyCamApi_SetParentAudio(false);
    
    if (resumeBabyAudioAfterPTT) {
        AudioStreamLogic_StartBaby();
    } else {
        M5.Speaker.begin();
    }
}

void AudioStreamLogic_Stop() {
    AudioStreamLogic_StopBaby();
    if(isPTTActive) AudioStreamLogic_StopPTT();
}