#ifndef AUDIO_STREAM_LOGIC_H
#define AUDIO_STREAM_LOGIC_H

#include <Arduino.h>

extern volatile bool isAudioStreaming;

void AudioStreamLogic_Init();
void AudioStreamLogic_Start(); // Kein Parameter mehr noetig!
void AudioStreamLogic_Stop();

#endif