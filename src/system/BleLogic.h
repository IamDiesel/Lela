#pragma once
#include <Arduino.h>
#include <Stream.h>

// Initialisiert Mutexe
void BleLogic_Init();

// Setzt den USB-Stream (den T-Dongle)
void BleLogic_SetDongleStream(Stream* stream);

// Führt die Logik aus (Senden/Empfangen von JSON)
bool BleLogic_Update();