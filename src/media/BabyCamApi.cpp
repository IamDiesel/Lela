#include "BabyCamApi.h"
#include "SharedData.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static TaskHandle_t apiTaskHandle = NULL;
static uint32_t lastHeartbeat = 0;
static uint32_t lastStatusPoll = 0;

static void sendPost(String endpoint, String payload) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.setTimeout(1500);
    http.begin("http://" + streamIp + ":8080" + endpoint);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    http.POST(payload);
    http.end();
}

static void sendGet(String endpoint) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.setTimeout(1500);
    http.begin("http://" + streamIp + ":8080" + endpoint);
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    http.GET();
    http.end();
}

static void apiTask(void* pv) {
    while(true) {
        if (wifiStarted && WiFi.status() == WL_CONNECTED) {
            uint32_t now = millis();
            
            // Heartbeat alle 10s
            if (now - lastHeartbeat > 10000) {
                lastHeartbeat = now;
                sendPost("/api/v1/client/online", "{\"deviceName\":\"Lela OS\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"uuidClient\":\"" + camUuid + "\"}");
            }
            
            // Status-Polling alle 3s
            if (now - lastStatusPoll > 3000) {
                lastStatusPoll = now;
                HTTPClient http;
                http.setTimeout(2000);
                http.begin("http://" + streamIp + ":8080/api/v1/status?fast=true");
                http.addHeader("uuid", camUuid);
                http.addHeader("versionApp", "2.48");
                if (http.GET() == HTTP_CODE_OK) {
                    DynamicJsonDocument doc(2048);
                    if (!deserializeJson(doc, http.getString())) {
                        camBatteryPercent = doc["batteryLevel"] | camBatteryPercent;
                        camStatus = doc["cameraStatus"] | camStatus;
                        camScreenOn = doc["screenOn"] | camScreenOn;
                        camFlash = doc["flash"] | camFlash;
                        camZoom = doc["zoom"] | camZoom;
                        currentCamRes = doc["resolution"] | currentCamRes;
                        
                        if (doc.containsKey("resolutions")) {
                            JsonArray arr = doc["resolutions"];
                            camResCount = 0;
                            for (JsonVariant v : arr) {
                                if (camResCount < 10) camResolutions[camResCount++] = v.as<String>();
                            }
                        }
                    }
                }
                http.end();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void BabyCamApi_Init() {
    if (apiTaskHandle == NULL) {
        xTaskCreatePinnedToCore(apiTask, "ApiTask", 6144, NULL, 2, &apiTaskHandle, 1);
    }
}

void BabyCamApi_SetQuality(int qual) { camQuality = qual; sendPost("/api/v1/camera/quality", "{\"level\":" + String(qual) + ",\"uuid\":\"" + camUuid + "\"}"); }
void BabyCamApi_SetSpeed(int speed) { camSpeed = speed; sendPost("/api/v1/camera/speed", "{\"level\":" + String(speed) + ",\"uuid\":\"" + camUuid + "\"}"); }
void BabyCamApi_SetZoom(int zoom) { camZoom = zoom; sendPost("/api/v1/camera/zoom", "{\"percentage\":" + String(zoom) + "}"); }
void BabyCamApi_SetResolution(int w, int h) { sendPost("/api/v1/camera/change-resolution", "{\"forceChangeResolution\":false,\"width\":" + String(w) + ",\"height\":" + String(h) + "}"); }

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
void BabyCamApi_SetBabyAudio(bool play) { sendPost("/api/v1/audio/baby/start-stop", "{\"audioBabyCompressed\":false,\"ip\":\"" + WiFi.localIP().toString() + "\",\"status\":\"" + String(play ? "PLAY" : "STOP") + "\"}"); }
void BabyCamApi_SetParentAudio(bool play) { sendPost("/api/v1/audio/parents/start-stop", "{\"ip\":\"" + WiFi.localIP().toString() + "\",\"status\":\"" + String(play ? "PLAY" : "STOP") + "\"}"); }