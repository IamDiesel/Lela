#include "BleLogic.h"
#include "SharedData.h"
#include <ArduinoJson.h>

static Stream* dongleStream = nullptr; 
static String dongleBuffer = "";
static bool currentlyScanning = false;
static uint32_t lastConnectAttempt = 0;

// Hardware-Status des USB-Dongles
static bool dongleReady = false; 

QueueHandle_t bleUpdateQueue = NULL;

void BleLogic_Init() {
    if (bleMutex == NULL) bleMutex = xSemaphoreCreateMutex();
    dongleBuffer.reserve(2048); 
    
    // Erstelle den Briefkasten fuer 60 gleichzeitige UI-Updates
    if (bleUpdateQueue == NULL) {
        bleUpdateQueue = xQueueCreate(60, sizeof(BleUpdateEvent));
    }
}

void BleLogic_SetDongleStream(Stream* stream) {
    dongleStream = stream;
    if (dongleStream) Serial.println("[BLE] >>> USB Dongle Interface initialisiert.");
}

// Wird aus der main.cpp aufgerufen und schuetzt das System vor Abstuerzen
void BleLogic_SetDongleReady(bool ready) {
    if (dongleReady && !ready) {
        currentlyScanning = false;
        connected = false; 
        topbarStatusMsg = "USB-Dongle getrennt!"; // NEU: Warnung
        Serial.println("[BLE] !!! USB Dongle getrennt !!!");
    } else if (!dongleReady && ready) {
        topbarStatusMsg = ""; // Meldung loeschen
        Serial.println("[BLE] >>> USB Dongle verbunden und bereit.");
    }
    dongleReady = ready;
}

void ParseDongleJSON(String jsonStr) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) return;

    String event = doc["event"] | "";

    if (event == "beacon") {
        String mac = doc["mac"] | "";
        String name = doc["name"] | "Unbekannt";
        int rssi = doc["rssi"] | -100;

        if (isSetupScanning) {
            bool exists = false;
            
            for (int i = 0; i < scanResultCount; i++) {
                if (scanResultMacs[i].equalsIgnoreCase(mac)) {
                    exists = true;
                    
                    bool nameChanged = (scanResultNames[i] == "Unbekannt" && name != "Unbekannt" && name != "");
                    
                    if (abs(scanResultRssi[i] - rssi) >= 3 || nameChanged) {
                        scanResultRssi[i] = rssi;
                        if (nameChanged) scanResultNames[i] = name;
                        
                        BleUpdateEvent ev;
                        ev.isClear = false; ev.isNew = false; ev.index = i;
                        strncpy(ev.device.mac, scanResultMacs[i].c_str(), 17); ev.device.mac[17] = '\0';
                        strncpy(ev.device.name, scanResultNames[i].c_str(), 31); ev.device.name[31] = '\0';
                        ev.device.rssi = rssi;
                        
                        xQueueSend(bleUpdateQueue, &ev, 0); 
                    }
                    break;
                }
            }
            
            if (!exists && scanResultCount < MAX_SCAN_DEVICES) {
                BleUpdateEvent ev;
                ev.isClear = false; ev.isNew = true; ev.index = scanResultCount;
                strncpy(ev.device.mac, mac.c_str(), 17); ev.device.mac[17] = '\0';
                strncpy(ev.device.name, name.c_str(), 31); ev.device.name[31] = '\0';
                ev.device.rssi = rssi;
                
                if (xQueueSend(bleUpdateQueue, &ev, 0) == pdTRUE) {
                    scanResultMacs[scanResultCount] = mac;
                    scanResultNames[scanResultCount] = name;
                    scanResultRssi[scanResultCount] = rssi;
                    scanResultCount++;
                }
            }
        }

        if (kippyEnabled && mac.equalsIgnoreCase(savedKippyMac)) {
            catRssi = rssi; lastCatSeenTime = millis();
        }
    } 
    else if (event == "sensor") {
        String mac = doc["mac"] | "";
        if (matEnabled && !useMqttForMat && mac.equalsIgnoreCase(savedMatMac)) {
            rawPressure = doc["value"] | 0;
            connected = true;
            
            // NEU: Fehler & Alarme loeschen, wenn Daten fliessen
            disconnectAlarmActive = false; 
            topbarStatusMsg = ""; 
            
            requestChartUpdate = true;
        }
    }
    // ==============================================================
    // NEU: DONGLE ERROR-HANDLING & RECONNECT-LOGIK
    // ==============================================================
    else if (event == "error") {
        String type = doc["type"] | "";
        String mac = doc["mac"] | "";
        String msg = doc["msg"] | "";
        
        Serial.printf("[BLE] Dongle Error (%s): %s\n", type.c_str(), msg.c_str());

        // Nachricht sofort auf die Topbar schieben
        topbarStatusMsg = msg;

        // Pruefen ob der Fehler unsere Matte betrifft
        if (matEnabled && !useMqttForMat && mac.equalsIgnoreCase(savedMatMac)) {
            
            if (type == "disconnected") {
                Serial.println("[BLE] Sensormatte getrennt! Versuche sofortigen Reconnect...");
                connected = false;
                // TRICK: lastConnectAttempt auf 10s in die Vergangenheit setzen, 
                // damit BleLogic_Update() sofort beim naechsten Tick verbindet.
                lastConnectAttempt = millis() - 10000; 
            } 
            else if (type == "connect_failed" || type == "invalid_service") {
                Serial.println("[BLE] Reconnect fehlgeschlagen! Warte 10s und starte Alarm.");
                connected = false;
                disconnectAlarmActive = true; 
                // lastConnectAttempt wird hier NICHT resetet! Die Logik wird 
                // dadurch exakt 10s warten, bis der naechste connect gefeuert wird.
            }
        }
    }
    // ==============================================================
}

bool BleLogic_Update() {
    // 1. Timeout (40s)
    if (isSetupScanning) {
        if ((millis() - setupScanStartTime) >= 40000) {
            BleLogic_StopSetupScan();
        }
    }

    // GUARD: Wenn der Dongle physisch nicht bereit ist, brechen wir hier sofort ab!
    if (!dongleStream || !dongleReady) return isSetupScanning;

    // DIE DROSSELKLAPPE (Throttling)
    int parsed_lines = 0; 
    while (dongleStream->available() && parsed_lines < 3) {
        char c = dongleStream->read();
        if (c == '\n') { 
            ParseDongleJSON(dongleBuffer); 
            dongleBuffer = ""; 
            parsed_lines++; 
        } 
        else if (c >= 32) { 
            dongleBuffer += c; 
        }
    }

    // Hardware Scanning Befehle
    bool needScan = isSetupScanning || kippyEnabled;
    if (needScan && !currentlyScanning) {
        dongleStream->print("{\"cmd\":\"scan_start\"}\n");
        currentlyScanning = true;
    } else if (!needScan && currentlyScanning) {
        dongleStream->println("{\"cmd\":\"scan_stop\"}");
        currentlyScanning = false;
    }

    // Sensor Reconnect (Nur wenn Fenster geschlossen)
    if (setupScanMode == 0 && matEnabled && !useMqttForMat && savedMatMac.length() > 0 && !connected) {
        if (millis() - lastConnectAttempt > 10000) { 
            lastConnectAttempt = millis();
            JsonDocument doc;
            doc["cmd"] = "connect"; doc["mac"] = savedMatMac;
            doc["service"] = "FFF0"; doc["char"] = "FFF1";
            doc["offset"] = 5; doc["length"] = 2; doc["big_endian"] = true;
            String cmd; serializeJson(doc, cmd);
            dongleStream->println(cmd);
        }
    }

    return isSetupScanning;
}

// ==============================================================
// GUI SCHNITTSTELLEN
// ==============================================================

void BleLogic_StartSetupScan(int mode, bool clearList) {
    topbarStatusMsg = ""; // Loescht Fehlermeldungen bei Scan-Start
    
    // GUARD: Nur senden wenn Dongle bereit ist
    if (dongleStream && dongleReady) {
        if (savedMatMac.length() > 0) {
            JsonDocument doc; doc["cmd"] = "disconnect"; doc["mac"] = savedMatMac;
            String cmdStr; serializeJson(doc, cmdStr);
            dongleStream->println(cmdStr);
        }
        connected = false;

        if (currentlyScanning) {
            dongleStream->println("{\"cmd\":\"scan_stop\"}");
            currentlyScanning = false;
        }
        delay(20); 
    }

    if (clearList) {
        scanResultCount = 0;
        BleUpdateEvent ev;
        ev.isClear = true; 
        xQueueSend(bleUpdateQueue, &ev, 0);
    }
    
    setupScanMode = mode;
    setupScanStartTime = millis();
    isSetupScanning = true;
    scanJustFinished = false;
}

void BleLogic_StopSetupScan() {
    isSetupScanning = false;
    scanJustFinished = true;
    
    // GUARD: Nur senden wenn Dongle bereit ist
    if (dongleStream && dongleReady && currentlyScanning) {
        dongleStream->println("{\"cmd\":\"scan_stop\"}");
        currentlyScanning = false;
    }
}

void BleLogic_CloseSetupWindow() {
    BleLogic_StopSetupScan();
    setupScanMode = 0; 
    scanJustFinished = false;
    lastConnectAttempt = 0; 
}

void BleLogic_SaveDevice(int index) {
    if (index >= 0 && index < scanResultCount) {
        if (setupScanMode == 1) {
            savedMatMac = scanResultMacs[index];
            preferences.begin("catmat", false); preferences.putString("macM", savedMatMac); preferences.end();
        } else if (setupScanMode == 2) {
            savedKippyMac = scanResultMacs[index];
            preferences.begin("catmat", false); preferences.putString("macK", savedKippyMac); preferences.end();
        }
    }
    BleLogic_CloseSetupWindow(); 
}

void BleLogic_GetScanStatus(char* infoBuf, size_t maxLen, bool& isScanning, bool& showSaveBtn) {
    isScanning = isSetupScanning;
    showSaveBtn = (scanResultCount > 0);
    
    if (isSetupScanning) {
        uint32_t elapsed = millis() - setupScanStartTime;
        uint32_t restzeit = (elapsed >= 40000) ? 0 : (40000 - elapsed) / 1000;
        snprintf(infoBuf, maxLen, "Sucht... (%ds)\nGefunden: %d", restzeit, scanResultCount);
    } else {
        snprintf(infoBuf, maxLen, "Suche beendet.\nGefunden: %d", scanResultCount);
    }
}

void BleLogic_SendAlarmOn() {
    if (dongleStream && dongleReady) {
        dongleStream->println("{\"cmd\":\"alarm_on\"}");
        Serial.println("[BLE] >>> Dongle-Alarm AKTIVIERT");
    }
}

void BleLogic_SendAlarmOff() {
    if (dongleStream && dongleReady) {
        dongleStream->println("{\"cmd\":\"alarm_off\"}");
        Serial.println("[BLE] >>> Dongle-Alarm DEAKTIVIERT");
    }
}

void BleLogic_SetBrightness(int value) {
    if (dongleStream && dongleReady) {
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        
        JsonDocument doc;
        doc["cmd"] = "brightness";
        doc["value"] = value;
        String cmdStr;
        serializeJson(doc, cmdStr);
        dongleStream->println(cmdStr);
        Serial.printf("[BLE] >>> Dongle-Helligkeit auf %d%% gesetzt\n", value);
    }
}