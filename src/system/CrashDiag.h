#pragma once
// ==============================================================================
// CrashDiag - Absturz-Diagnose ueber Neustarts hinweg
// ==============================================================================
// Zweck (siehe src/doc/video_refactoring.md, Experiment E-1):
// Die Abstuerze treten gehaeuft auf, sobald einer aufgetreten ist. Der Verdacht
// ist, dass der 2D-DMA-Block seinen verklemmten Zustand ueber den Panic-Reset
// hinweg behaelt und erst ein echter Stromausfall ihn loest.
//
// Um das zu messen statt zu vermuten, legt dieses Modul seinen Zustand in den
// RTC-Speicher (RTC_NOINIT_ATTR). Dieser ueberlebt einen Software-/Panic-Reset,
// geht bei einem Kaltstart (Stromlos) aber verloren. Die Magic-Konstante
// unterscheidet damit zuverlaessig Warmstart von Kaltstart - unabhaengig davon,
// was esp_reset_reason() meldet.
//
// Erfasst werden zusaetzlich Laufzeit und Videokontext des letzten Laufs, damit
// nach einem Absturz sofort ersichtlich ist, wie lange das Geraet lief und ob
// Video bzw. Vollbild dabei aktiv waren.
// ==============================================================================

#include <Arduino.h>

// MQTT-Topic, auf dem der Bericht einmalig nach jedem Start abgelegt wird.
// Retained: der letzte Stand liegt dauerhaft auf dem Broker und ist jederzeit
// abrufbar - ohne angeschlossenen Rechner und ohne serielle Konsole.
// Nutzlast (JSON):
//   reason   Reset-Grund im Klartext
//   cold     1 = Kaltstart (Geraet war stromlos), 0 = Warmstart
//   prev_ms  Laufzeit des vorherigen Laufs in Millisekunden
//   video    Video war beim letzten Lebenszeichen aktiv
//   fs       Vollbild war beim letzten Lebenszeichen aktiv
//   fps      zuletzt gemessene Bildrate
//   boots    Starts seit dem letzten Kaltstart
//   crashes  Abstuerze seit dem letzten Kaltstart
#define CRASHDIAG_MQTT_TOPIC "lela/diag/boot"

// Ganz frueh in setup() aufrufen. Liest den RTC-Zustand aus, sichert den
// Bericht des vorherigen Laufs und bereitet die Zaehler des aktuellen Laufs vor.
// Gibt bewusst NICHTS aus: durch ARDUINO_USB_CDC_ON_BOOT=1 ist der USB-CDC-Port
// zu diesem Zeitpunkt noch nicht enumeriert, fruehe Ausgaben gehen verloren.
void CrashDiag_Init();

// Spaeter in setup() aufrufen, sobald die serielle Verbindung traegt.
// Gibt den gesicherten Bericht des vorherigen Laufs aus.
void CrashDiag_Report();

// Aus loop() aufrufen. Schreibt hoechstens einmal pro Sekunde ein Lebenszeichen
// (Laufzeit + Videokontext) in den RTC-Speicher. Sehr guenstig.
void CrashDiag_Update();

// Anzahl der Abstuerze seit dem letzten Kaltstart (fuer Anzeige/Telemetrie).
uint32_t CrashDiag_GetPanicCount();

// true, wenn dieser Lauf auf einen Kaltstart folgt (Geraet war stromlos).
bool CrashDiag_WasColdStart();
