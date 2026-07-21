#pragma once
#include <Arduino.h>
#include <WiFi.h>

enum NetworkMode {
    NET_MODE_HOME = 0,
    NET_MODE_TRANSITION_TO_DIRECT,
    NET_MODE_DIRECT,
    NET_MODE_TRANSITION_TO_HOME
};

class WifiStateLogic {
public:
    static void init();
    static void update();
    
    // API für das UI
    static void triggerDirectMode(String ssid, String pass);
    static void triggerHomeMode();
    
    static NetworkMode getMode();
    static bool isConnected();

private:
    static NetworkMode currentMode;
    static uint32_t stateTimer;
    static void disconnectAndClean();
};