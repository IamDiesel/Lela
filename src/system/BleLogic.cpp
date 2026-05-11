#include "BleLogic.h"
#include "SharedData.h"
#include <ArduinoJson.h>

static Stream* dongleStream = nullptr; 
static String dongleBuffer = "";
static bool currentlyScanning = false;
static uint32_t lastConnectAttempt = 0;
static bool connectionSent = false;

void BleLogic_Init() {
    if (bleMutex == NULL) bleMutex = xSemaphoreCreateMutex();
}

void BleLogic_SetDongleStream(Stream* stream) {
    dongleStream = stream;
    if (dongleStream) {
        Serial.println("[BLE] >>> USB Dongle Interface initialisiert.");
    }
}

void ParseDongleJSON(String jsonStr) {
    // Echtzeit-Debug im Serial Monitor
    Serial.print("[DONGLE <<] ");
    Serial.println(jsonStr);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.println("[DONGLE !!] JSON Parse-Fehler");
        return;
    }

    String event = doc["event"] | "";

    // 1. Geräte im Scan gefunden (Beacon)
    if (event == "beacon") {
        String mac = doc["mac"] | "";
        String name = doc["name"] | "Unbekannt";
        int rssi = doc["rssi"] | -100;

        if (isSetupScanning) {
            if (bleMutex != NULL && xSemaphoreTake(bleMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                bool exists = false;
                for (int i = 0; i < scanResultCount; i++) {
                    if (scanResultMacs[i].equalsIgnoreCase(mac)) {
                        scanResultRssi[i] = rssi;
                        exists = true; break;
                    }
                }
                if (!exists && scanResultCount < 20) {
                    scanResultMacs[scanResultCount] = mac;
                    scanResultNames[scanResultCount] = name;
                    scanResultRssi[scanResultCount] = rssi;
                    scanResultCount++;
                }

                // Generiere die Liste für das LVGL-Widget (Format: Name\nName\n...)
                String rollerList = "";
                for(int i=0; i<scanResultCount; i++) {
                    rollerList += scanResultNames[i] + " [" + scanResultMacs[i] + "]";
                    if (i < scanResultCount - 1) rollerList += "\n";
                }
                strncpy(scanOptionsStr, rollerList.c_str(), sizeof(scanOptionsStr)-1);
                requestRollerUpdate = true; // Signal an ViewSettings.cpp
                xSemaphoreGive(bleMutex);
            }
        }

        // Hintergrund-Suche für Kippy
        if (kippyEnabled && mac.equalsIgnoreCase(savedKippyMac)) {
            catRssi = rssi;
            lastCatSeenTime = millis();
        }
    } 
    // 2. Sensordaten empfangen
    else if (event == "sensor") {
        String mac = doc["mac"] | "";
        if (matEnabled && !useMqttForMat && mac.equalsIgnoreCase(savedMatMac)) {
            rawPressure = doc["value"] | 0;
            connected = true;
            requestChartUpdate = true;
        }
    }
}

bool BleLogic_Update() {
    if (!dongleStream) return false;

    // 1. Daten vom USB-Stick lesen
    while (dongleStream->available()) {
        char c = dongleStream->read();
        if (c == '\n') {
            ParseDongleJSON(dongleBuffer);
            dongleBuffer = "";
        } else if (c >= 32) { // Steuerzeichen ignorieren
            dongleBuffer += c;
        }
    }

    // 2. Scan-Steuerung
    bool needScan = isSetupScanning || kippyEnabled;
    if (needScan && !currentlyScanning) {
        String cmd = "{\"cmd\":\"scan_start\"}\n";
        dongleStream->print(cmd);
        Serial.print("[DONGLE >>] "); Serial.print(cmd);
        currentlyScanning = true;
    } else if (!needScan && currentlyScanning) {
        dongleStream->println("{\"cmd\":\"scan_stop\"}");
        Serial.println("[DONGLE >>] scan_stop");
        currentlyScanning = false;
    }

    // 3. Verbindungs-Logik (Matte)
    if (matEnabled && !useMqttForMat && savedMatMac.length() > 0 && !connected) {
        if (millis() - lastConnectAttempt > 10000) { // Alle 10s versuchen
            lastConnectAttempt = millis();
            JsonDocument doc;
            doc["cmd"] = "connect";
            doc["mac"] = savedMatMac;
            doc["service"] = "FFF0"; doc["char"] = "FFF1";
            doc["offset"] = 5; doc["length"] = 2; doc["big_endian"] = true;
            
            String cmd; serializeJson(doc, cmd);
            dongleStream->println(cmd);
            Serial.print("[DONGLE >>] "); Serial.println(cmd);
        }
    }

    return isSetupScanning;
}