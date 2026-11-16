#pragma once
// ==============================================================================
// HangGuard - erkennt ein eingefrorenes Geraet und startet es neu
// ==============================================================================
// Anlass (Projekt-Notiz video_freeze_2026-11-15): Beim Babycam-Video blieb das
// Bild stehen und die Oberflaeche reagierte nicht mehr - der Ton lief weiter,
// neu gestartet hat sich das Geraet nicht. Kein Watchdog greift in so einem
// Fall: Der Task-Watchdog des Systems ueberwacht nur den Leerlauf von Kern 0;
// Oberflaeche und Video-Decoder laufen auf Kern 1. Ein Task, der dort ewig
// wartet (z.B. der PPA-Treiber ohne Zeitlimit, mit gehaltener LVGL-Sperre)
// oder ewig rechnet, friert das Geraet dauerhaft ein.
//
// Warum nicht einfach die haengende Funktion abbrechen? Der PPA-Treiber der
// IDF kennt keinen Abbruch: Ein Transfer, der nie endet, haelt den Semaphor
// der PPA-Einheit fuer den Rest des Laufs (er wird nur im "fertig"-Interrupt
// freigegeben). Vollbild und Overlay kaemen ohne Neustart nicht zurueck. Die
// kleinste Einheit, die sich sauber neu starten laesst, ist deshalb das
// Geraet - mit SessionResume laeuft der Stream danach von selbst weiter.
// Was sich VOR dem Treiber abfangen laesst, wird dort abgefangen
// (JpegHeaderCheck: beschaedigte Bilder verwerfen).
//
// HangGuard ist ein kleiner Waechter-Task auf Kern 0. Er prueft jede Sekunde:
//   1. Taetigkeit: Meldet ein Teilnehmer (z.B. der Video-Decoder) laenger als
//      kActivityLimitMs dieselbe Taetigkeit, haengt er darin.
//   2. Oberflaeche: Laeuft die Hauptschleife, bekommt aber seit kUiLimitMs die
//      LVGL-Sperre nicht, haelt ein anderer Task sie fest.
//   3. Hauptschleife: Seit kLoopLimitMs kein Lebenszeichen - sie wartet auf
//      etwas oder kommt nicht mehr an die Reihe.
//
// Dann schreibt er einen Kurzbericht ins NVS (Was, wie lange, Zustand des
// Tasks, Halter der Sperre, Zusatzangaben des Teilnehmers) und bricht gezielt
// ab (esp_system_abort). Das ergibt einen Coredump mit den Stapeln aller
// Tasks und - ueber ColdRestart - einen Neustart mit kurzer Stromlos-Phase.
// Der Bericht steht danach unter Einstellungen > System > Diagnose.
//
// Abschaltbar ueber HANG_GUARD in DebugFlags.h (dann sind alle Aufrufe leer).
// ==============================================================================

#include <stddef.h>
#include <stdint.h>
#include "DebugFlags.h"

// Teilnehmer, die ihre Taetigkeit melden.
enum class HangSlot : uint8_t { VideoDecoder, Count };

// Liefert Zusatzangaben fuer den Bericht (z.B. Bildrate, Fehlerzaehler). Laeuft
// im Waechter-Task, waehrend der Teilnehmer haengt: nur Variablen lesen, nichts
// sperren, nichts Blockierendes.
typedef void (*HangContextFn)(char* buf, size_t len);

#if HANG_GUARD

// Am Ende von setup(): laedt den letzten Bericht und startet den Waechter.
void HangGuard_Init();

// Aus loop(), in jedem Durchlauf.
void HangGuard_LoopAlive();

// Aus loop(), wenn die LVGL-Sperre gewonnen wurde.
void HangGuard_UiAlive();

// Taetigkeit eines Teilnehmers melden. 'what' muss ein Zeichenketten-Literal
// sein (der Zeiger wird aufbewahrt); nullptr = wartet auf Arbeit, darf dauern.
// Aus dem Task aufrufen, der die Taetigkeit ausfuehrt.
void HangGuard_Activity(HangSlot slot, const char* what);

void HangGuard_SetContext(HangSlot slot, HangContextFn fn);

// Letzter aufgezeichneter Haenger (leer, wenn keiner) und Anzahl insgesamt.
const char* HangGuard_LastReport();
uint32_t    HangGuard_Count();

// true, wenn der Waechter den vorherigen Lauf beendet hat (gilt fuer genau
// einen Start, nach HangGuard_Init()).
bool HangGuard_TrippedLastRun();

#else

inline void HangGuard_Init() {}
inline void HangGuard_LoopAlive() {}
inline void HangGuard_UiAlive() {}
inline void HangGuard_Activity(HangSlot, const char*) {}
inline void HangGuard_SetContext(HangSlot, HangContextFn) {}
inline const char* HangGuard_LastReport() { return ""; }
inline uint32_t    HangGuard_Count() { return 0; }
inline bool        HangGuard_TrippedLastRun() { return false; }

#endif
