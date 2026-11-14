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

// Reset-Grund dieses Starts als Klartext mit rohem Chip-Code, z.B.
// "PANIC / Exception [rst 0xC]". Nach einem automatischen Kaltstart der Grund
// des Warmstarts davor: "... [rst 0x..], danach stromlos". Fuer Diagnose-Seite,
// Bericht und MQTT.
const char* CrashDiag_ResetReasonText();

// Loest absichtlich einen Absturz aus (Schreibzugriff auf Adresse 0 ->
// "Store access fault", Reset-Grund PANIC). Nur fuer Tests der Absturz-
// Behandlung (Diagnose, DIAG_TEST_TOOLS). Kehrt nicht zurueck.
//
// Bewusst keine Division durch null: Der ESP32-P4 ist ein RISC-V, und dort
// loest eine Ganzzahl-Division durch null KEINE Ausnahme aus (Ergebnis -1).
[[noreturn]] void CrashDiag_TriggerCrash();

// Neustart ueber 1 s Tiefschlaf: Anders als esp_restart() wird dabei der
// HP-Bereich des ESP32-P4 (CPU, Peripherie, Pads) abgeschaltet; Always-on-
// Bereich, IO-Expander und Audio-Chips bleiben. Test, ob der stumme
// Lautsprecher am HP-Bereich haengt (DIAG_TEST_TOOLS). Kehrt nicht zurueck.
[[noreturn]] void CrashDiag_DeepSleepRestart();

// Vor einem Kaltstart ueber ColdRestart aufrufen: sichert Grund, Laufkontext
// und Zaehler im NVS, weil der RTC-Speicher den Stromausfall nicht ueberlebt.
// CrashDiag_Init() uebernimmt sie beim naechsten Start; Bericht, MQTT und
// "Letzter Start" zeigen dann:
//   planned = false  Absturz beim Start erkannt: Grund und Kontext des
//                    abgestuerzten Laufs ("PANIC / Exception [rst 0x..],
//                    danach stromlos")
//   planned = true   gewollter Neustart aus dem laufenden Betrieb: Kontext
//                    dieses Laufs ("Neustart (gewollt), danach stromlos")
void CrashDiag_SaveForColdRestart(bool planned);

// true, wenn dieser Start ein automatischer Kaltstart nach einem Warmstart war.
bool CrashDiag_WasAutoColdRestart();

// true, wenn seit dem letzten Kaltstart mindestens ein Absturz war.
//
// Befund 14.10.2026: Nach einem Absturz kann die Audio-Ausgabe stumm bleiben,
// bis das Geraet einmal stromlos war - die Audio-Bausteine (Codec, Verstaerker)
// behalten ueber den Warmstart hinweg ihren Zustand, und die normale
// Initialisierung holt sie nicht zuverlaessig zurueck. Das gilt auch fuer
// spaetere Warmstarts (z.B. Upload), bis zum naechsten Kaltstart. Die
// Oberflaeche zeigt dann einen Hinweis (ViewTopbar).
//
// Befund 14.11.2026: Auch ein Software-Neustart (System > Neustart) waehrend
// laufendem Babyphone-Ton laesst den Ton stumm - ohne Hinweis, weil kein
// Absturz. Der Reset-Knopf des Tab5 holt ihn zurueck. Untersuchung siehe
// AudioCodecTab5.h (Codec-Reset in der Diagnose).
//
// Seit 14.11.2026 schaltet ColdRestart nach solchen Warmstarts automatisch kurz
// stromlos. Der Hinweis bleibt fuer die Faelle ohne Kaltstart (Upload am USB,
// Schleifenschutz, Uhr nicht lesbar).
bool CrashDiag_AudioMayBeMuted();
