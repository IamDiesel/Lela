#pragma once
// ==============================================================================
// ColdRestart - Kaltstart (kurz stromlos) statt Warmstart
// ==============================================================================
// Problem (Projekt-Notiz audio_warmstart_2026-11-14):
// Nach einem Warmstart (Absturz, Watchdog, Software-Neustart) kann der
// Lautsprecher des Tab5 stumm, bis das Geraet einmal stromlos war. Ein
// Warmstart setzt nur den ESP32-P4 zurueck; IO-Expander und Audio-Chips
// (AUDIO_VDD) behalten ihren Zustand. Der Reset-Knopf hilft, ist aber Handarbeit.
//
// Loesung: Direkt nach einem Absturz schaltet sich das Geraet selbst
// stromlos (PWROFF_PULSE ueber Expander U7, M5.Power.timerSleep) und laesst
// sich von der Echtzeituhr RX8130 nach ca. kLeadS Sekunden wieder einschalten.
//
// Warum die Uhr verstellt wird:
// Der Sekunden-Zaehler des RX8130 weckt das Tab5 nicht (Befund 14.11.2026),
// nur der Minuten-Wecker (vergleicht Stunde und Minute). Damit die naechste
// volle Minute nach kLeadS Sekunden kommt, wird die Uhr vor dem Abschalten
// auf hh:mm:(60 - kLeadS) gestellt. Die Verschiebung steht im NVS und wird
// beim naechsten Start wieder abgezogen (Genauigkeit etwa 1 s; die Systemzeit
// korrigiert ohnehin NTP).
//
// Ausgeloest bei: Absturz (Panic), Watchdogs, CPU-Lockup - und nur, wenn der
// RTC-Block von CrashDiag gueltig ist, das Programm also vorher lief
// (CrashDiag_WasColdStart() == false).
// Nicht bei:
//   - Einschalten, RTC-Wecker, Reset-Knopf: Der Chip meldet danach zwar
//     "Watchdog" (rst 0x7, CORE_MWDT, Befund 14.11.2026), der RTC-Block ist
//     aber verloren - es war also kein Absturz des laufenden Programms.
//   - Gewollter Neustart (ColdRestart_WarmRestart: Web-Setup, Werkseinstellungen).
//     System > Neustart nutzt dagegen ColdRestart_RestartCold (Ton sicher da).
//   - Upload (USB-Reset): am Kabel bleibt die Versorgung an, wirkungslos.
//   - Brownout: schwacher Akku, Abschalten waere kontraproduktiv.
//   - Aufwachen aus dem Tiefschlaf: Rueckfallweg, wenn die Versorgung nicht
//     abschaltet (USB) - so entsteht keine Schleife.
//
// Schleifenschutz: hoechstens kMaxInRow automatische Kaltstarts in Folge. Der
// Zaehler (NVS) wird nach kStableMs ungestoerter Laufzeit zurueckgesetzt.
//
// Der Absturzbericht (CrashDiag) wird vorher im NVS gesichert und nach dem
// Kaltstart uebernommen - "Letzter Start" und MQTT zeigen den echten Grund.
// ==============================================================================

#include <stdint.h>

// Direkt nach M5.begin() aufrufen (braucht I2C, Uhr und Expander) und nach
// CrashDiag_Init(). Stellt nach einem eigenen Kaltstart die Uhr zurueck.
// Nach einem Absturz (siehe oben) wird sofort stromlos geschaltet - dann
// kehrt die Funktion nicht zurueck.
void ColdRestart_Init();

// Aus loop() aufrufen: setzt nach stabiler Laufzeit den Schleifenschutz zurueck.
void ColdRestart_Update();

// Gewollter Neustart OHNE Stromlos-Phase (Web-Setup, Werkseinstellungen) -
// statt ESP.restart() verwenden. Der Ton kann danach stumm sein.
// Merkt sich die Absicht im RTC-Speicher. ColdRestart_Init() startet danach in
// keinem Fall kalt - auch dann nicht, wenn esp_restart() beim Herunterfahren
// (WLAN usw.) haengt und der Neustart als Watchdog-Reset ankommt.
[[noreturn]] void ColdRestart_WarmRestart(const char* why);

// Gewollter Neustart MIT kurzer Stromlos-Phase (System > Neustart): ca. 3 s
// laenger, dafuer ist der Ton danach sicher da (Entscheidung Daniel,
// 15.11.2026). "Letzter Start" zeigt "Neustart (gewollt), danach stromlos".
// Ist die Uhr nicht lesbar, wird stattdessen warm neu gestartet.
[[noreturn]] void ColdRestart_RestartCold(const char* why);

// Was CrashDiag fuer den naechsten Start sichern soll.
enum class ColdRestartReport : uint8_t {
    None,      // nichts (Diagnose-Knopf): "Letzter Start" zeigt den Kaltstart selbst
    Crash,     // Absturzbericht des Warmstarts davor (aus ColdRestart_Init)
    Planned,   // gewollter Neustart aus dem laufenden Betrieb
};

// Sofort stromlos, Wiedereinschalten nach ca. 3 s.
//   why      Text fuer das Log
//   report   was CrashDiag fuer den naechsten Start sichert
// Gibt false zurueck (ohne abzuschalten), wenn die Uhr nicht lesbar ist - sonst
// kaeme das Geraet nicht zurueck. Bei Erfolg kehrt die Funktion nicht zurueck.
bool ColdRestart_Now(const char* why, ColdRestartReport report);
