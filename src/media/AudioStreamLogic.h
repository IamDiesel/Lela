#pragma once
#include <Arduino.h>

extern volatile bool isAudioStreaming;
extern volatile bool isPTTActive;

void AudioStreamLogic_Init();
void AudioStreamLogic_StartBaby();
void AudioStreamLogic_StopBaby();
void AudioStreamLogic_StartPTT();
void AudioStreamLogic_StopPTT();
void AudioStreamLogic_Stop();