#pragma once
#include <Arduino.h>

// ------------------------------------------------------------------------------
// Zustand (nur lesen ausserhalb von AudioStreamLogic.cpp)
// ------------------------------------------------------------------------------
extern volatile bool isAudioStreaming;   // Babyphone-Wiedergabe laeuft
extern volatile bool isPTTActive;        // Gegensprechen laeuft (Mikrofon)

// ------------------------------------------------------------------------------
// Steuerung. Alle Aufrufe sind nicht-blockierend: sie reihen nur ein Kommando
// ein, ausgefuehrt wird es im Besitzer-Task.
// ------------------------------------------------------------------------------
void AudioStreamLogic_Init();
void AudioStreamLogic_StartBaby();
void AudioStreamLogic_StopBaby();
void AudioStreamLogic_StartPTT();
void AudioStreamLogic_StopPTT();
void AudioStreamLogic_Stop();

// Neue Einstellung der Tonquelle (SharedData.h, audioSource ff.) uebernehmen.
// Laeuft der Babyphone-Ton, wird er mit der neuen Quelle neu gestartet; sonst
// gilt sie beim naechsten Start. Aus der Hauptschleife rufen.
void AudioStreamLogic_ApplyConfig();

// true, wenn die eingestellte Tonquelle Gegensprechen kann (nur BabyCam/UDP).
bool AudioStreamLogic_SupportsPTT();

// Diagnose: Teil der Audio-Kette zuruecksetzen, ohne das Geraet stromlos zu
// machen (Theorien siehe AudioCodecTab5.h; kind = AudioResetKind). Laeuft im
// Besitzer-Task: Lautsprecher aus, Reset, Lautsprecher im vorherigen Zustand
// wieder an, danach ein Testton. Waehrend des Gegensprechens wirkungslos.
// Registerauszug vorher/nachher auf der seriellen Konsole.
void AudioStreamLogic_ResetAudio(uint8_t kind);

// ------------------------------------------------------------------------------
// Zugriffsschutz fuer die I2S-Hardware
// ------------------------------------------------------------------------------
// Mic und Lautsprecher teilen sich auf dem Tab5 den I2S-Port und den MCLK-Pin.
// M5Unified 0.2.13 serialisiert begin()/end() nicht: faellt ein zweiter Task
// waehrend eines Teardowns in tone() oder playRaw(), wird ihm der Kanal unter
// den Fuessen weggezogen (Core Panic).
//
// Deshalb gilt: JEDER Zugriff auf M5.Speaker oder M5.Mic laeuft unter diesem
// Schloss. Ausser dem Besitzer-Task nimmt es nur der UI-Ton-Task in
// SharedData.cpp. Siehe src/doc/video_refactoring.md, 2.18.
//
// Vor dem Init von AudioStreamLogic existiert das Schloss noch nicht; dann gibt
// es auch keinen zweiten Nutzer, und Lock() meldet folgerichtig Erfolg.
bool AudioHw_Lock(uint32_t timeoutMs);
void AudioHw_Unlock();

// Senkt den Babyphone-Stream fuer die angegebene Dauer ab, damit ein UI- oder
// Alarmton durchkommt. Die Absenkung selbst passiert in der Wiedergabe - wer
// die Kanallautstaerke von aussen setzt, wird beim naechsten Haeppchen (alle
// ~5 ms) wieder ueberschrieben.
void AudioHw_DuckStream(uint32_t durationMs);

// false, solange der Lautsprecher abgeschaltet ist (Halbduplex waehrend PTT)
// oder gerade umkonfiguriert wird. Toene sind dann zu unterdruecken.
bool AudioHw_SpeakerUsable();
