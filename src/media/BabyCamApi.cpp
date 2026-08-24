#include "BabyCamApi.h"
#include "SharedData.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

volatile bool isBabyApiActive = false; 

static TaskHandle_t apiTaskHandle = NULL;
static uint32_t lastHeartbeat = 0;
static uint32_t lastStatusPoll = 0;

// Optimierung: const char* statt String, um Heap-Fragmentierung zu stoppen
static void sendPost(const char* endpoint, const char* payload) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.setReuse(false); 
    http.setTimeout(1500);
    char url[128];
    snprintf(url, sizeof(url), "http://%s:8080%s", streamIp.c_str(), endpoint);
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    http.POST(payload);
    http.end();
}

static void sendGet(const char* endpoint) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.setReuse(false); 
    http.setTimeout(1500);
    char url[128];
    snprintf(url, sizeof(url), "http://%s:8080%s", streamIp.c_str(), endpoint);
    
    http.begin(url);
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    http.GET();
    http.end();
}

static void apiTask(void* pv) {
    // Optimierung: Persistenter HTTPClient für Polling (TCP Keep-Alive)
    HTTPClient httpPoll;
    httpPoll.setReuse(true);
    httpPoll.setTimeout(2000);
    
    // Optimierung: Statischer Puffer für JSON (Zero Heap Allocation in der Schleife)
    StaticJsonDocument<1024> doc; 

    while(true) {
        if (isBabyApiActive && wifiStarted && WiFi.status() == WL_CONNECTED) {
            uint32_t now = millis();
            
            if (now - lastHeartbeat > 10000) {
                lastHeartbeat = now;
                char payload[128];
                snprintf(payload, sizeof(payload), "{\"deviceName\":\"Lela OS\",\"ip\":\"%s\",\"uuidClient\":\"%s\"}", WiFi.localIP().toString().c_str(), camUuid.c_str());
                sendPost("/api/v1/client/online", payload);
            }
            
            if (now - lastStatusPoll > 3000) {
                lastStatusPoll = now;
                
                char url[128];
                snprintf(url, sizeof(url), "http://%s:8080/api/v1/status?fast=true", streamIp.c_str());
                httpPoll.begin(url);
                httpPoll.addHeader("uuid", camUuid);
                httpPoll.addHeader("versionApp", "2.48");
                
                int httpCode = httpPoll.GET();
                
                if (httpCode == HTTP_CODE_OK) {
                    doc.clear(); // Dokument zurücksetzen statt neu anlegen
                    DeserializationError error = deserializeJson(doc, httpPoll.getStream());
                    
                    if (!error) {
                        camBatteryPercent = doc["batteryLevel"] | camBatteryPercent;
                        camStatus = doc["cameraStatus"].as<String>();
                        camScreenOn = doc["screenOn"] | camScreenOn;
                        camFlash = doc["flash"] | camFlash;
                        camZoom = doc["zoom"] | camZoom;
                        
                        if (doc.containsKey("resolutions")) {
                            JsonArray arr = doc["resolutions"];
                            camResCount = 0;
                            for (JsonVariant v : arr) {
                                if (camResCount < 10) camResolutions[camResCount++] = v.as<String>();
                            }
                        }
                    } else {
                        Serial.printf("[BabyCamApi] JSON Parse Error: %s\n", error.c_str());
                    }
                }
                httpPoll.end();
            }
        } else {
            lastHeartbeat = 0;
            lastStatusPoll = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void BabyCamApi_Init() {
    if (apiTaskHandle == NULL) {
        xTaskCreatePinnedToCore(apiTask, "ApiTask", 6144, NULL, 2, &apiTaskHandle, 1);
    }
}

// Optimierte Wrapper-Funktionen ohne String-Verkettung
void BabyCamApi_SetQuality(int qual) { 
    camQuality = qual; 
    char payload[64]; snprintf(payload, sizeof(payload), "{\"level\":%d,\"uuid\":\"%s\"}", qual, camUuid.c_str());
    sendPost("/api/v1/camera/quality", payload); 
}
void BabyCamApi_SetSpeed(int speed) { 
    camSpeed = speed; 
    char payload[64]; snprintf(payload, sizeof(payload), "{\"level\":%d,\"uuid\":\"%s\"}", speed, camUuid.c_str());
    sendPost("/api/v1/camera/speed", payload); 
}
void BabyCamApi_SetZoom(int zoom) { 
    camZoom = zoom; 
    char payload[64]; snprintf(payload, sizeof(payload), "{\"percentage\":%d}", zoom);
    sendPost("/api/v1/camera/zoom", payload); 
}
void BabyCamApi_SetResolution(int w, int h) { 
    char payload[128]; snprintf(payload, sizeof(payload), "{\"forceChangeResolution\":false,\"width\":%d,\"height\":%d}", w, h);
    sendPost("/api/v1/camera/change-resolution", payload); 
}

void BabyCamApi_ApplyBestResolution(int reqW, int reqH) {
    if (camResCount == 0) return;
    int bestW = 0, bestH = 0;
    int reqArea = reqW * reqH;
    int bestArea = 0;
    for (int i = 0; i < camResCount; i++) {
        int xIdx = camResolutions[i].indexOf('x');
        if (xIdx != -1) {
            int w = camResolutions[i].substring(0, xIdx).toInt();
            int h = camResolutions[i].substring(xIdx + 1).toInt();
            if (w == reqW && h == reqH) { bestW = w; bestH = h; break; }
            int area = w * h;
            if (area <= reqArea && area > bestArea) { bestArea = area; bestW = w; bestH = h; }
        }
    }
    if (bestW == 0) {
        int xIdx = camResolutions[0].indexOf('x');
        bestW = camResolutions[0].substring(0, xIdx).toInt();
        bestH = camResolutions[0].substring(xIdx + 1).toInt();
    }
    if (bestW > 0) BabyCamApi_SetResolution(bestW, bestH);
}

void BabyCamApi_ToggleFlash() { sendGet("/api/v1/camera/flash"); }
void BabyCamApi_ToggleScrFlash() { sendGet("/api/v1/camera/flash-screen"); camScrFlash = !camScrFlash; }
void BabyCamApi_ToggleLense() { sendGet("/api/v1/camera/switch"); }
void BabyCamApi_ToggleScreen() { sendGet("/api/v1/screen/on-off"); }
void BabyCamApi_ToggleCamera() { if (camStatus == "PLAY") sendGet("/api/v1/camera/off"); else sendGet("/api/v1/camera/on"); }
void BabyCamApi_PowerOff() { sendGet("/api/v1/camera/power-off"); }

void BabyCamApi_SetBabyAudio(bool play) { 
    char payload[128]; snprintf(payload, sizeof(payload), "{\"audioBabyCompressed\":false,\"ip\":\"%s\",\"status\":\"%s\"}", WiFi.localIP().toString().c_str(), play ? "PLAY" : "STOP");
    sendPost("/api/v1/audio/baby/start-stop", payload); 
}
void BabyCamApi_SetParentAudio(bool play) { 
    char payload[128]; snprintf(payload, sizeof(payload), "{\"ip\":\"%s\",\"status\":\"%s\"}", WiFi.localIP().toString().c_str(), play ? "PLAY" : "STOP");
    sendPost("/api/v1/audio/parents/start-stop", payload); 
}