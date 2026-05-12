#pragma once
#include <Arduino.h>
#include <Stream.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define MAX_LIST_DEVICES 50

// Das Datenpaket, das durch die FreeRTOS Queue geschickt wird
struct BleDevice {
    char mac[18];
    char name[32];
    int rssi;
};

struct BleUpdateEvent {
    bool isClear; // True, wenn die Liste geleert werden soll
    bool isNew;   // True, wenn ein neues Geraet hinzugefuegt wird
    int index;    // Zeilennummer in der UI
    BleDevice device;
};

// Globaler Briefkasten fuer UI-Updates
extern QueueHandle_t bleUpdateQueue;

void BleLogic_Init();
void BleLogic_SetDongleStream(Stream* stream);
bool BleLogic_Update();

// UI Steuer-Befehle
void BleLogic_StartSetupScan(int mode, bool clearList); 
void BleLogic_StopSetupScan();
void BleLogic_CloseSetupWindow();
void BleLogic_SaveDevice(int index);

void BleLogic_GetScanStatus(char* infoBuf, size_t maxLen, bool& isScanning, bool& showSaveBtn);