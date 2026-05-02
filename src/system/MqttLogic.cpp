#include "MqttLogic.h"
#include "SharedData.h"
#include "SystemLogic.h"
#include "GuiManager.h"
#include <WiFi.h>
#include "secrets.h"

static uint32_t lastMqttReconnectAttempt = 0;
static uint32_t lastProxyKeepAlive = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    
    String t = String(topic);

    // ==========================================
    // 1. LELA OS: BABY MONITOR ALARME
    // ==========================================
    if (t == mqttBabyTopic) {
        if (msg == "on" || msg == "true" || msg == "1") {
            if (isBabyArmed && !babyAlarmActive) {
                babyAlarmActive = true;
                babyMuted = false;
                wakeDisplay();
                gui.switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
            }
        } else if (msg == "off" || msg == "false" || msg == "0") {
            babyAlarmActive = false;
        }
    }
    else if (t == mqttCameraTriggerTopic) {
        if (msg == "on" || msg == "true" || msg == "1") {
            isStreamActive = true;
            requestBabyStream = true;
            wakeDisplay();
            gui.switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
        } else if (msg == "off" || msg == "false" || msg == "0") {
            isStreamActive = false;
            requestBabyStream = false;
        }
    }
    
    // ==========================================
    // 2. CATMAT PROXY EMPFANG
    // ==========================================
    else if (t == SECRET_MQTT_CATMAT_PRESSURE) {
        proxyLastUpdate = millis(); 
        if (useMqttForMat) {
            if (msg != "unknown") {
                rawPressure = msg.toInt();
                lastConnectTime = millis();
            }
        }
    }
    else if (t == SECRET_MQTT_CATMAT_DISCONNECT) {
        proxyLastUpdate = millis(); 
        if (useMqttForMat) {
            if (msg == "ON") {
                disconnectAlarmActive = true;
                connected = false;
            } else if (msg == "OFF") {
                connected = true;
                disconnectAlarmActive = false;
            }
        }
    }
    else if (t == SECRET_MQTT_CATMAT_MAT_RSSI) {
        proxyLastUpdate = millis(); 
        if (useMqttForMat) {
            if (msg != "-130") matRssi = msg.toInt();
        }
    }
    else if (t == SECRET_MQTT_CATMAT_KIPPY_RSSI) {
        proxyLastUpdate = millis(); 
        if (msg != "-130") {
            catRssi = msg.toInt();
            lastCatSeenTime = millis();
        }
    }
    else if (t == "lolacatmat/sensor/wifi_rssi") {
        proxyWifiRssi = msg.toInt();
        proxyLastUpdate = millis(); 
    }
    else if (t == "lolacatmat/sensor/debug") {
        proxyStatusMsg = msg;
        proxyLastUpdate = millis(); 
    }
}

void MqttLogic_Init() {}

void MqttLogic_Update() {
    if (!mqttEnabled || !wifiEnabled || WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!mqttClient.connected()) {
        uint32_t now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            
            // Server-Daten setzen
            mqttClient.setServer(mqttBroker.c_str(), mqttPort);
            mqttClient.setCallback(mqttCallback);
            
            String clientId = "LolaTab5-" + String(esp_random() & 0xffff, HEX);
            
            Serial.print("[MQTT] Verbinde -> IP: "); Serial.print(mqttBroker);
            Serial.print(" | Erzwinge Login aus secrets.h mit User: '"); 
            Serial.print(SECRET_MQTT_USER); Serial.println("'");
            
            // WICHTIG: Hier wird das Gedächtnis ignoriert und stur die secrets.h genutzt!
            bool connectedBroker = mqttClient.connect(clientId.c_str(), SECRET_MQTT_USER, SECRET_MQTT_PASS);
            
            if (connectedBroker) {
                Serial.println("[MQTT] ERFOLGREICH VERBUNDEN!");
                if (mqttBabyTopic.length() > 0) mqttClient.subscribe(mqttBabyTopic.c_str());
                if (mqttCameraTriggerTopic.length() > 0) mqttClient.subscribe(mqttCameraTriggerTopic.c_str());
                
                mqttClient.subscribe(SECRET_MQTT_CATMAT_PRESSURE);
                mqttClient.subscribe(SECRET_MQTT_CATMAT_DISCONNECT);
                mqttClient.subscribe(SECRET_MQTT_CATMAT_MAT_RSSI);
                mqttClient.subscribe(SECRET_MQTT_CATMAT_KIPPY_RSSI);
                mqttClient.subscribe("lolacatmat/sensor/wifi_rssi");
                mqttClient.subscribe("lolacatmat/sensor/debug");
                
                mqttClient.publish(SECRET_MQTT_CATMAT_CMD_SET, "ON");
                lastProxyKeepAlive = millis();
            } else {
                Serial.println("[MQTT] FEHLER: Abgelehnt / Nicht autorisiert!");
            }
        }
    } else {
        mqttClient.loop();

        uint32_t now = millis();
        if (now - lastProxyKeepAlive > 180000) {
            lastProxyKeepAlive = now;
            mqttClient.publish(SECRET_MQTT_CATMAT_CMD_SET, "ON");
        }
    }
}