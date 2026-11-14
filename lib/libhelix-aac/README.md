# libhelix-aac (eingebettet)

Festkomma-HE-AAC-Decoder von RealNetworks (Helix DNA), fuer den HTTP-AAC-Pfad
des Babyphone-Tons (`src/media/AacStreamDecoder`).

## Herkunft
- Quelle: <https://github.com/earlephilhower/ESP8266Audio>, Verzeichnis `src/libhelix-aac`
- Stand: Commit `e1f2a1a34aa460f5cca7a60b69d9a350fd099938` (07.10.2026)
- Dateien **unveraendert** uebernommen. Aenderungen gegenueber dem Helix-Original
  stammen aus ESP8266Audio (u. a. `PROGMEM`-Tabellen, `#include <Arduino.h>`,
  SBR auf allen Plattformen ausser ESP8266 aktiv).

## Warum nicht die ganze Bibliothek ESP8266Audio?
ESP8266Audio wurde mit v2.0.0 aus den Abhaengigkeiten entfernt. Der fruehere
AAC-Weg (`AudioGeneratorAAC` + eigene `AudioOutput`-Klasse) schrieb direkt auf
`M5.Speaker` - neben dem Audio-Task, der die I2S-Hardware allein besitzen muss
(Core Panic, siehe `src/doc/video_refactoring.md` 2.18). Gebraucht wird nur der
Decoder; HTTP-Empfang und Ausgabe macht Lela selbst.

## Lizenz
Die Dateien stehen unter der RealNetworks Public Source License (RPSL) bzw.
RealNetworks Community Source License (RCSL), siehe Kopf jeder Datei.

## Aktualisieren
Nur bewusst und mit Test auf dem Geraet: Dateien aus dem genannten Verzeichnis
eines neuen ESP8266Audio-Stands kopieren, Commit oben und in `library.json`
nachtragen.
