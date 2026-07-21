#include "WifiStateLogic.h"
#include "SharedData.h"

NetworkMode WifiStateLogic::currentMode = NET_MODE_HOME;
uint32_t WifiStateLogic::stateTimer = 0;

void WifiStateLogic::init() {
    currentMode = NET_MODE_HOME;
}

NetworkMode WifiStateLogic::getMode() {
    return currentMode;
}

bool WifiStateLogic::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiStateLogic::triggerDirectMode(String ssid, String pass) {
    if (currentMode == NET_MODE_DIRECT || currentMode == NET_MODE_TRANSITION_TO_DIRECT) return;
    
    directSsid = ssid;
    directPass = pass;
    preferences.begin("catmat", false);
    preferences.putString("directSsid", directSsid);
    preferences.putString("directPass", directPass);
    preferences.end();

    currentMode = NET_MODE_TRANSITION_TO_DIRECT;
    stateTimer = millis();
    disconnectAndClean();
}

void WifiStateLogic::triggerHomeMode() {
    if (currentMode == NET_MODE_HOME || currentMode == NET_MODE_TRANSITION_TO_HOME) return;
    currentMode = NET_MODE_TRANSITION_TO_HOME;
    stateTimer = millis();
    disconnectAndClean();
}

void WifiStateLogic::disconnectAndClean() {
    WiFi.disconnect(true, false);
    delay(50); 
}

void WifiStateLogic::update() {
    switch (currentMode) {
        case NET_MODE_HOME:
            if (wifiEnabled && WiFi.status() != WL_CONNECTED && (millis() - stateTimer > 5000)) {
                stateTimer = millis();
                WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
            }
            break;

        case NET_MODE_TRANSITION_TO_DIRECT:
            if (millis() - stateTimer > 500) {
                Serial.println("[WifiState] Verbinde mit WiFi Direct (BabyCam)...");
                WiFi.begin(directSsid.c_str(), directPass.c_str());
                currentMode = NET_MODE_DIRECT;
                stateTimer = millis();
            }
            break;

        case NET_MODE_DIRECT:
            if (WiFi.status() != WL_CONNECTED && (millis() - stateTimer > 5000)) {
                stateTimer = millis();
                WiFi.begin(directSsid.c_str(), directPass.c_str());
            }
            break;

        case NET_MODE_TRANSITION_TO_HOME:
            if (millis() - stateTimer > 500) {
                Serial.println("[WifiState] Kehre zum Heimnetz zurueck...");
                WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
                currentMode = NET_MODE_HOME;
                stateTimer = millis();
            }
            break;
    }
}