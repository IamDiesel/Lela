#pragma once
// ==============================================================================
// PttCapture - Mitschnitt des Gegensprechens (Diagnose, DIAG_TEST_TOOLS)
// ==============================================================================
// Haelt fest, was das Tab5 beim Gegensprechen an die Kamera schickt - genau die
// Samples der UDP-Pakete, in Sendereihenfolge. Der Mitschnitt laesst sich als
// WAV-Datei am PC abholen und anhoeren (WebSetupLogic: /ptt_capture, erreichbar
// im Bildschirmfoto-Modus).
//
// Wozu: Klingt die Stimme an der Kamera schlecht, trennt der Mitschnitt die
// beiden moeglichen Fehlerorte. Ist die WAV-Datei sauber, liegt der Fehler
// hinter dem Tab5 (Netz, Kamera). Klingt sie selbst schlecht, liegt er in der
// Aufnahme (Mikrofon, Codec, I2S).
//
// Aufteilung: Begin/Add/End ruft nur der Audio-Task (AudioStreamLogic), Get der
// Webserver in der Hauptschleife. Waehrend einer laufenden Aufnahme gibt Get
// nichts heraus.
// ==============================================================================

#include <stddef.h>
#include <stdint.h>

// Laenge des Mitschnitts: der Anfang jeder Sitzung, hoechstens so viele Sekunden.
static const uint32_t PTT_CAPTURE_SECONDS  = 10;
static const size_t   PTT_WAV_HEADER_BYTES = 44;

// Neue Sitzung: Mitschnitt von vorn. Legt beim ersten Aufruf den Speicher an
// (PSRAM). rateHz ist die Abtastrate, die in der WAV-Datei stehen soll.
void PttCapture_Begin(uint32_t rateHz);

// Ein gesendetes Paket anhaengen. Ist der Mitschnitt voll, passiert nichts.
void PttCapture_Add(const int16_t* samples, size_t count);

// Sitzung beendet: der Mitschnitt darf abgeholt werden.
void PttCapture_End();

// Fertiger Mitschnitt der letzten Sitzung. false, solange aufgenommen wird
// oder noch nichts vorliegt.
bool PttCapture_Get(const int16_t** samples, size_t* count, uint32_t* rateHz);

// Kopf einer WAV-Datei (PCM, 16 Bit, Mono) fuer dataBytes Nutzdaten.
void PttCapture_WavHeader(uint8_t out[PTT_WAV_HEADER_BYTES], uint32_t dataBytes, uint32_t rateHz);
