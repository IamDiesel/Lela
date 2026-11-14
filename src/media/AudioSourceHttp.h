#pragma once
// ==============================================================================
// AudioSourceHttp - Babyphone-Ton aus einem HTTP-Strom
// ==============================================================================
// Fuer Kameras ausser der BabyCam (oder die BabyCam ueber TCP). Formate:
//   - PCM 16 Bit, Little Endian. Beginnt der Strom mit einem WAV-Kopf
//     (RIFF/WAVE), gelten Abtastrate und Kanaele daraus, sonst die Einstellung.
//   - AAC im ADTS-Rahmenformat (z.B. go2rtc ".../stream.aac", ffmpeg -f adts).
// Nur http:// - fuer https fehlt die Zertifikatspruefung, und die Quellen im
// Heimnetz sprechen ohnehin unverschluesselt.
//
// Der Abruf erfolgt mit HTTP/1.0: Server duerfen dann keinen Chunked-Transfer
// verwenden, die Nutzdaten kommen ohne Zwischenkoepfe an. Ein Chunk-Kopf
// mitten im PCM verschoebe sonst alle folgenden Samples um ein Byte.
//
// Herkunft: Bis August 2026 gab es diesen Weg schon einmal (Format 0 per
// HTTPClient, Format 1 per ESP8266Audio). Neu ist, dass beide in den
// gemeinsamen Jitterpuffer schreiben statt selbst die Hardware anzufassen.
// ==============================================================================

#include "AudioSourceTypes.h"

// Verbindet sich, empfaengt und dekodiert, bis die Senke "aufhoeren" meldet
// oder die Verbindung endet. Blockiert so lange - nur aus dem Empfangs-Task.
// Rueckgabe: true, wenn Ton ankam (fuer die Wartezeit bis zum Neuversuch).
bool AudioSourceHttp_Run(const AudioSourceConfig& cfg, const AudioSink& sink);

// URL fuer Logzeilen: Zugangsdaten ("user:pass@") werden ausgeblendet.
String AudioSourceHttp_LogUrl(const char* url);
