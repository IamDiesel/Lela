#include "BleLogic.h"
#include "SharedData.h"

void BleLogic_Init() {
    bleMutex = xSemaphoreCreateMutex();
}

bool BleLogic_Update() {
    // --- SETUP SCAN SIMULATOR ---
    // Bleibt erhalten, damit die UI beim Suchen der MAC-Adresse funktioniert
    if (isSetupScanning) {
        static uint32_t scanStart = 0;
        if (scanStart == 0) scanStart = millis();

        // Simuliere einen Fund nach 2 Sekunden
        if (millis() - scanStart > 2000 && scanResultCount == 0) {
            if (bleMutex != NULL && xSemaphoreTake(bleMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                scanResultMacs[0] = "20:24:08:29:13:f0"; // Echte MAC als Dummy
                scanResultNames[0] = "CatMat-EXT";
                scanResultRssi[0] = -65;
                scanResultCount = 1;
                xSemaphoreGive(bleMutex);
            }
        }

        if (millis() - setupScanStartTime > 10000) {
            isSetupScanning = false;
            scanJustFinished = true;
            scanStart = 0;
        }

        static uint32_t lastUiStringUpdate = 0;
        if (millis() - lastUiStringUpdate > 1000) {
            lastUiStringUpdate = millis();
            if (bleMutex != NULL && xSemaphoreTake(bleMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                if (scanResultCount == 0) { strcpy(scanOptionsStr, "Suche laeuft..."); } 
                else { strcpy(scanOptionsStr, "20:24:08:29:13:f0 | CatMat-EXT | -65"); }
                xSemaphoreGive(bleMutex);
                requestRollerUpdate = true;
            }
        }
        return true;
    }

    // --- PLATZHALTER FÜR DEN HARDWARE BLUETOOTH DONGLE ---
    if (matEnabled && !useMqttForMat) {
        // Hier kommt später deine echte lokale BLE Verbindung rein!
        // Der Code hier wird dann `connected = true` setzen und 
        // die Variable `rawPressure` mit echten lokalen BLE-Werten füllen.
    }

    return false;
}