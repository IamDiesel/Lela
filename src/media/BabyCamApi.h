#pragma once
#include <Arduino.h>

extern volatile bool isBabyApiActive; 

void BabyCamApi_Init();
void BabyCamApi_SetQuality(int qual);
void BabyCamApi_SetSpeed(int speed);
void BabyCamApi_SetZoom(int zoom);
void BabyCamApi_SetResolution(int w, int h);
void BabyCamApi_ApplyBestResolution(int reqW, int reqH);
void BabyCamApi_ToggleFlash();
void BabyCamApi_ToggleScrFlash();
void BabyCamApi_ToggleLense();
void BabyCamApi_ToggleScreen();
void BabyCamApi_ToggleCamera();
void BabyCamApi_PowerOff();
void BabyCamApi_SetBabyAudio(bool play);
void BabyCamApi_SetParentAudio(bool play);