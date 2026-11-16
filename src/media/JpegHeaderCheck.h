#pragma once
// ==============================================================================
// JpegHeaderCheck - strenge Pruefung der JPEG-Kopfdaten vor dem Hardware-Decoder
// ==============================================================================
// Anlass (Projekt-Notiz video_freeze_2026-11-15): Der Kopfdaten-Parser des
// IDF-JPEG-Treibers (esp_driver_jpeg, jpeg_parse_marker.c, IDF 5.4) vertraut
// den Laengenangaben im Bild:
//   - DQT/DHT zaehlen eine vorzeichenlose Restlaenge herunter. Passt sie nicht
//     zum Tabellenformat, laeuft sie unter null - die Schleife dreht dann rund
//     4 Milliarden Mal und schreibt ueber die Tabellenfelder hinaus.
//   - APPn/COM ziehen die Segmentlaenge ungeprueft von der Restlaenge ab.
//   - Unbekannte Marker werden paarweise ueberlesen; bei ungerader Restlaenge
//     laeuft auch die Hauptschleife unter null.
// Ein einziges beschaedigtes Bild kann den Decoder-Task damit endlos rechnen
// lassen: Bild steht, Oberflaeche verhungert, kein Watchdog.
//
// Diese Pruefung laeuft dieselben Segmente ab wie der Treiber, aber mit
// Grenzen. Durchgelassen wird nur, was der Treiber sicher verarbeitet:
//   SOI, dann ausschliesslich APPn, COM, DQT, SOF0, DHT, DRI - jedes Segment
//   vollstaendig im Puffer und in sich stimmig - bis SOS.
// Alles andere (progressive Bilder, fremde Marker, Fuellbytes, abgeschnittene
// oder unstimmige Segmente) wird abgelehnt; das Bild wird verworfen, der
// Stream laeuft mit dem naechsten weiter.
//
// Nebenbei liefert die Pruefung die Bildmasse aus dem echten SOF0-Segment.
// jpeg_decoder_get_info() des Treibers sucht dagegen byteweise nach FF C0 -
// auch in Vorschaubildern eines APP-Segments - und gibt bei unbekannter
// Abtastung seinen Speicher nicht frei.
//
// Reines C++ ohne Abhaengigkeiten: auf dem PC testbar (siehe
// tools/tests/jpeg_header_check - Beispielbilder und 400000 zufaellig
// beschaedigte Kopfdaten gegen einen Nachbau des IDF-Parsers mit
// Grenzwaechtern). Kosten: ein Durchlauf ueber die Kopfdaten (~600 B).
// ==============================================================================

#include <stddef.h>
#include <stdint.h>

enum class JpegCheck : uint8_t {
    Ok,
    NoSoi,          // beginnt nicht mit FF D8
    Truncated,      // Segment reicht ueber das Ende hinaus
    BadMarker,      // kein FF an der Segmentgrenze, Fuellbytes oder fremder Marker
    Unsupported,    // kein Baseline-JPEG (SOF1..15) bzw. nicht unterstuetzte Abtastung
    BadSegment,     // Laengenangabe eines Segments unzulaessig
    BadDqt,         // Quantisierungstabellen unstimmig
    BadDht,         // Huffman-Tabellen unstimmig
    BadSof,         // Bildkopf unstimmig
    NoFrame,        // SOS ohne vorheriges SOF0 oder ohne Quantisierungstabelle
    NoScan,         // Ende erreicht, ohne dass Bilddaten (SOS) beginnen
};

struct JpegHeaderInfo {
    uint16_t width      = 0;
    uint16_t height     = 0;
    uint8_t  components = 0;   // 1 = Graustufen, 3 = Farbe
};

// Prueft die Kopfdaten in data[0..len). info darf nullptr sein.
JpegCheck JpegHeader_Check(const uint8_t* data, size_t len, JpegHeaderInfo* info);

// Kurztext fuer Logzeilen.
const char* JpegHeader_Text(JpegCheck result);
