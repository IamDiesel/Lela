#pragma GCC optimize ("O3") 
#include "SystemLogic.h"
#include "SharedData.h"
#include "VideoLogic.h"      
#include "MqttLogic.h"       
#include "WebSetupLogic.h"   
#include "BleLogic.h"        
#include "AudioStreamLogic.h" 
#include <WiFi.h>
#include <esp_wifi.h> 
#include "secrets.h"         
#include "GuiManager.h"      
#include <M5Unified.h>
#include "HaWebsocketLogic.h" 
#include "HaConfigLogic.h"

#include "ViewDashboard.h"
#include "ViewBaby.h"
#include "ViewCatMat.h"
#include "ViewHomeAssistant.h" 
#include "ViewSettings.h"      

#include <time.h> 

int cameraRefreshMs = 300; 

void SystemLogic_Init() {
    HaConfigLogic::Init();
    
    // FIX: esp_wifi_set_ps(WIFI_PS_NONE) wurde hier gelöscht! 
    // Es darf erst nach WiFi.begin() aufgerufen werden!

    Audio_Init(); 
    VideoLogic_Init(); 
    AudioStreamLogic_Init(); 
    
    BleLogic_Init();     
    MqttLogic_Init(); 
    WebSetupLogic_Init(); 
    
    if (wifiSsid == "") { pendingWebSetupMode = 1; } 
    preferences.begin("catmat", true); 
    haIP = preferences.getString("haIP", SECRET_HA_IP); 
    haPort = preferences.getInt("haPort", SECRET_HA_PORT); 
    camEntity = preferences.getString("camEntity", SECRET_CAM_SNAPSHOT_PATH); 
    mqttBabyTopic = preferences.getString("mqttBaby", SECRET_MQTT_BABY_TOPIC); 
    mqttCameraTriggerTopic = preferences.getString("mqttCamTrig", SECRET_MQTT_CAM_TRIGGER);
    cameraRefreshMs = preferences.getInt("camRef", 300); 
    showFps = preferences.getBool("showFps", false);
    preferences.end();
    
    gui.init();
}

void SystemLogic_Update() {
    static ScreenID lastScreen = SCREEN_DASHBOARD;
    ScreenID currentScreen = gui.getCurrentScreen();
    
    if (currentScreen != lastScreen) {
        if (lastScreen == SCREEN_BABY) { 
            VideoLogic_Stop(); 
            AudioStreamLogic_Stop(); 
        }
        lastScreen = currentScreen;
    }

    static bool lastStreamState = false;
    if (isStreamActive && !lastStreamState) {
        VideoLogic_Start();
        AudioStreamLogic_Start(); 
        lastStreamState = true;
    } else if (!isStreamActive && lastStreamState) {
        VideoLogic_Stop();
        AudioStreamLogic_Stop();  
        lastStreamState = false;
    }

    WebSetupLogic_Update(); 

    calcMultiplex(); 
    if (webSetupMode > 0 || pendingWebSetupMode > 0) return; 

    if (!wifiEnabled && wifiStarted) { WiFi.disconnect(true, false); wifiStarted = false; timeSynced = false; }
    
    if (wifiEnabled && !wifiStarted && !isTrackerMode && effPrioWifi > 0) { 
        // FIX: WiFi.begin() MUSS als allererstes aufgerufen werden, damit der RPC-Treiber hochfährt!
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str()); 
        
        // JETZT ist es sicher, die Konfigurationen an den Chip zu schicken:
        WiFi.setTxPower(WIFI_POWER_19_5dBm); 
        esp_wifi_set_ps(WIFI_PS_NONE);
        
        wifiStarted = true; 
    }

    static bool ntpConfigured = false;
    if (WiFi.status() == WL_CONNECTED && !ntpConfigured) {
        configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.google.com");
        ntpConfigured = true;
        timeSynced = true; 
    } else if (WiFi.status() != WL_CONNECTED) {
        ntpConfigured = false;
        timeSynced = false;
    }

    if (BleLogic_Update()) return;
    MqttLogic_Update(); 
    
    static uint32_t lastMathUpdate = 0;
    if (millis() - lastMathUpdate > 100) { 
        lastMathUpdate = millis();
        
        if (matEnabled && connected) {
            currentPressure = rawPressure - taraOffset;
            if (currentPressure > intervalMaxPressure) intervalMaxPressure = currentPressure;
            
            pressWindow[pWinIdx] = currentPressure; 
            pWinIdx = (pWinIdx + 1) % WINDOW_SIZE;
            if (pWinCount < WINDOW_SIZE) pWinCount++;
            
            if (pWinCount > 0) {
                int32_t sum = 0; 
                for (int i = 0; i < pWinCount; i++) sum += pressWindow[i];
                currentAvg = sum / pWinCount;
                
                if (isArmed && !alarmActive && !disconnectAlarmActive && millis() > cooldownUntil) {
                    if (pWinCount >= WINDOW_SIZE && abs(currentPressure - currentAvg) > thresholdVal) { 
                        alarmActive = true; 
                        muted = false; 
                        wakeDisplay(); 
                        gui.switchScreen(SCREEN_CATMAT, LV_SCR_LOAD_ANIM_NONE); 
                    }
                }
            }
        }
    }
    
    if (!muted && !isTrackerMode) { 
        static uint32_t lastBeep = 0; 
        if (alarmActive) { 
            if (millis() - lastBeep > 2500) { playCatAlarmI2S(); lastBeep = millis(); } 
        } else if (disconnectAlarmActive) { 
            if (millis() - lastBeep > 3000) { playToneI2S(440, 200, false); playToneI2S(349, 200, false); playToneI2S(261, 500, false); lastBeep = millis(); } 
        } else if (babyAlarmActive && !babyMuted) {
            if (millis() - lastBeep > 2500) { playBabyAlarmI2S(); lastBeep = millis(); }
        }
    }
    
    static uint32_t lastBatRead = 0; 
    if (millis() - lastBatRead > 5000) { 
        int batLevel = M5.Power.getBatteryLevel();
        if (batLevel >= 0 && batLevel <= 100) {
            batteryPercent = batLevel; 
        }
        
        static int highest_bat_while_charging = 0;
        static bool force_not_charging = false;
        bool raw_charging = M5.Power.isCharging();

        if (raw_charging) {
            if (highest_bat_while_charging == 0) {
                highest_bat_while_charging = batteryPercent;
            }

            if (batteryPercent > highest_bat_while_charging) {
                highest_bat_while_charging = batteryPercent;
                force_not_charging = false; 
            } 
            else if (batteryPercent < highest_bat_while_charging) {
                force_not_charging = true;
            }

            isBatteryCharging = force_not_charging ? false : true;
        } else {
            force_not_charging = false;
            highest_bat_while_charging = 0;
            isBatteryCharging = false;
        }

        lastBatRead = millis(); 
    }
    
    static uint32_t lastHistoryUpdate = 0; 
    if (millis() - lastHistoryUpdate >= 1000) { 
        lastHistoryUpdate = millis(); 
        int32_t valToPush = -32000; 
        if (isTrackerMode) { valToPush = (millis() - lastCatSeenTime < 5000) ? catRssi : -32000; } 
        else { 
            switch(currentGraphMode) { 
                case GRAPH_MODE_PRESSURE: if (connected) { valToPush = (intervalMaxPressure != -32000) ? intervalMaxPressure : currentPressure; intervalMaxPressure = -32000; } break; 
                case GRAPH_MODE_BLE_RSSI: valToPush = connected ? matRssi : -32000; break; 
                case GRAPH_MODE_WLAN_RSSI: valToPush = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -32000; break; 
                case GRAPH_MODE_BLE_INTERVAL: valToPush = connected ? (int32_t)avgInterval : -32000; break; 
                case GRAPH_MODE_KIPPY_RSSI: valToPush = (millis() - lastCatSeenTime < 5000) ? catRssi : -32000; break; 
            } 
        } 
        pressureHistory[historyIdx] = valToPush; historyIdx = (historyIdx + 1) % HISTORY_SIZE; if (historyCount < HISTORY_SIZE) historyCount++; requestChartUpdate = true; 
    }

    if (currentScreen == SCREEN_DASHBOARD) {
        ViewDashboard::update();
    } else if (currentScreen == SCREEN_BABY) {
        ViewBaby::update();
    } else if (currentScreen == SCREEN_CATMAT) {
        ViewCatMat::update();
    } else if (currentScreen == SCREEN_HA) {
        ViewHomeAssistant::update(); 
    } else if (currentScreen == SCREEN_SETTINGS) {
        ViewSettings::update();      
    }
}