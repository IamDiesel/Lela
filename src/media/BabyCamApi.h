#pragma once
#include <Arduino.h>

// ==============================================================================
// BabyCamApi - Steuerung der Kamera-App (Endpunkte nach api_RE.md)
// ==============================================================================
// Alle Setter und Toggles kehren sofort zurueck; ausgefuehrt wird im
// Hintergrundtask. Nur die beiden Audio-Umschaltbefehle sind synchron.
// ==============================================================================

extern volatile bool isBabyApiActive;

void BabyCamApi_Init();

// Uebernimmt die zuletzt abgefragten Kameradaten (Status, Aufloesungen) in die
// von der Oberflaeche gelesenen Strings. MUSS aus der Hauptschleife gerufen
// werden - der Hintergrundtask darf diese String-Objekte nicht anfassen
// (V-05, siehe src/doc/video_refactoring.md 2.18).
void BabyCamApi_Update();

// --- Video- und Performance-Einstellungen ---
void BabyCamApi_SetQuality(int qual);        // 0..100
void BabyCamApi_SetSpeed(int speed);         // 0 = max fps, 100 = Eco
void BabyCamApi_SetZoom(int zoom);           // 0..100
void BabyCamApi_SetExposure(int percent);    // 0..100
void BabyCamApi_SetResolution(int w, int h);
void BabyCamApi_ApplyBestResolution(int reqW, int reqH);

// --- Kamera- und Licht-Toggles ---
void BabyCamApi_ToggleFlash();
void BabyCamApi_ToggleScrFlash();
void BabyCamApi_ToggleLense();
void BabyCamApi_ToggleScreen();
void BabyCamApi_ToggleCamera();
void BabyCamApi_RecordVideo();
void BabyCamApi_PowerOff();

// --- Audio (synchron, nur aus dem Audio-Task aufrufen) ---
void BabyCamApi_SetBabyAudio(bool play);
void BabyCamApi_SetParentAudio(bool play);
