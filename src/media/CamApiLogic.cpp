#include "CamApiLogic.h"
#include "SharedData.h"
#include "AudioStreamLogic.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5Unified.h>

static TaskHandle_t camStatusTaskHandle = NULL;
static TaskHandle_t pttTaskHandle = NULL;
volatile bool isPttActive = false;

// Hintergrund-Task: Holt lautlos alle 5 Sekunden das Batterie-Level
static void camStatusTask(void *pvParameters) {
    while(true) {
        if (isStreamActive && WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.setTimeout(2000);
            http.begin("http://" + streamIp + ":8080/api/v1/status?fast=true");
            http.addHeader("uuid", uuidClient);
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                JsonDocument doc;
                if (!deserializeJson(doc, http.getStream())) {
                    camBatteryLevel = doc["batteryLevel"] | -1;
                    camIsCharging = doc["batteryIsCharging"] | false;
                    camAudioRecorderPort = doc["audioBabyRecorderPort"] | 50001;
                }
            }
            http.end();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Fliegender Task: Wird nur aktiv, waehrend der Talk-Button gedrueckt ist
static void pttAudioTask(void *pvParameters) {
    // 1. Verhindere Rueckkopplungen: Mute den eingehenden Baby-Stream temporaer
    bool wasAudioStreaming = isAudioStreaming;
    if (wasAudioStreaming) isAudioStreaming = false;

    // 2. HTTP POST: Kamera das Kommando geben, dass wir jetzt sprechen
    HTTPClient http;
    http.begin("http://" + streamIp + ":8080/api/v1/audio/parents/start-stop");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", uuidClient);
    String body = "{\"ip\":\"" + WiFi.localIP().toString() + "\",\"status\":\"PLAY\"}";
    http.POST(body);
    http.end();

    // =========================================================
    // FIX: Die Kamera braucht kurz Zeit, um das HTTP-JSON zu 
    // parsen und den TCP-Socket auf Port 50001 hochzufahren.
    // =========================================================
    vTaskDelay(pdMS_TO_TICKS(300)); 

    // 3. TCP-Socket oeffnen (Mit Retry-Logik!)
    WiFiClient micClient;
    bool connected = false;
    for(int i = 0; i < 4; i++) {
        if (micClient.connect(streamIp.c_str(), camAudioRecorderPort)) {
            connected = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Bei Ablehnung (Errno 104) kurz warten und erneut versuchen
    }

    if (connected) {
        micClient.setNoDelay(true);

        // M5Stack Mikrofon konfigurieren (LPCM Mono, 44.1kHz)
        auto mic_cfg = M5.Mic.config();
        mic_cfg.sample_rate = 44100;
        mic_cfg.stereo = false;
        M5.Mic.config(mic_cfg);
        M5.Mic.begin();

        int16_t mic_buf[1024];
        
        while (isPttActive && micClient.connected()) {
            if (M5.Mic.record(mic_buf, 1024, 44100)) {
                micClient.write((uint8_t*)mic_buf, 1024 * sizeof(int16_t));
            } else {
                vTaskDelay(pdMS_TO_TICKS(2)); // Watchdog-Schutz
            }
        }
        M5.Mic.end();
        micClient.stop();
    } else {
        Serial.println("[CamApi] PTT Fehler: Kamera hat TCP Port 50001 verweigert!");
    }

    // 4. HTTP POST: Kamera mitteilen, dass wir aufhoeren zu sprechen
    http.begin("http://" + streamIp + ":8080/api/v1/audio/parents/start-stop");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", uuidClient);
    body = "{\"ip\":\"" + WiFi.localIP().toString() + "\",\"status\":\"STOP\"}";
    http.POST(body);
    http.end();

    // 5. Baby-Stream wieder aufheben
    if (wasAudioStreaming) isAudioStreaming = true;

    pttTaskHandle = NULL;
    vTaskDelete(NULL);
}

void CamApiLogic_Init() {
    if (camStatusTaskHandle == NULL) {
        xTaskCreatePinnedToCore(camStatusTask, "CamStatus", 8192, NULL, 1, &camStatusTaskHandle, 0); 
    }
}

void CamApi_SetPtt(bool active) {
    isPttActive = active;
    if (active && pttTaskHandle == NULL) {
        xTaskCreatePinnedToCore(pttAudioTask, "PttAudio", 16384, NULL, 3, &pttTaskHandle, 1);
    }
}