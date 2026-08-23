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
                    M5.Speaker.setChannelVolume(0, (int)( (muteMaster ? 0 : volMaster/100.0f) * (muteBaby ? 0 : volBaby/100.0f) * 255.0f ));
                    M5.Speaker.playRaw((const int16_t*)buffer, bytesRead / 2, 44100, false, 1, 0);
                }
                // --- UDP WDT FIX ---
                vTaskDelay(pdMS_TO_TICKS(1));
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
    
    // Hardware sicher umschalten
    M5.Mic.end(); 
    delay(50);
    M5.Speaker.begin();
    M5.Speaker.setVolume(255);
    
    BabyCamApi_SetBabyAudio(true);
    isAudioStreaming = true;
}

void AudioStreamLogic_StopBaby() {
    if (!isAudioStreaming) return;
    
    // WICHTIG: ERST das Flag setzen, damit der Consumer-Task stoppt!
    isAudioStreaming = false;
    delay(50); // Warten, bis der Task sicher aus playRaw() raus ist
    
    M5.Speaker.end();
    BabyCamApi_SetBabyAudio(false);
}

void AudioStreamLogic_StartPTT() {
    if (isPTTActive) return;
    
    // Pausiere Baby-Audio sicher, falls es an ist
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
    
    // WICHTIG: ERST das Flag setzen, damit der Producer-Task stoppt!
    isPTTActive = false; 
    delay(50); // Warten, bis der Task sicher aus record() raus ist
    
    M5.Mic.end(); 
    BabyCamApi_SetParentAudio(false);
    
    // Reaktiviere Baby-Audio, falls es vor PTT an war
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