#pragma once
// ==============================================================================
// VideoDecoder - Hardware-JPEG-Dekodierung mit treiberkonformen Puffern
// ==============================================================================
// Dieses Modul ist der EINZIGE Nutzer des Hardware-JPEG-Decoders und damit der
// einzige DMA2D-Client der Medienschicht.
//
// Kernpunkt (siehe doc/video_refactoring.md, 2.18):
// Ein- und Ausgabepuffer werden ausschliesslich ueber jpeg_alloc_decoder_mem()
// angelegt. Diese Funktion kapselt drei Dinge, die sich von Hand leicht falsch
// machen lassen und die zwischen IDF-Versionen wechseln:
//   1. die zur Laufzeit ermittelte Cache-Line-Groesse (auf dem P4 typisch 64 B),
//   2. das Aufrunden der GROESSE, nicht nur der Adresse,
//   3. die Wahl der unverschlüsselten PSRAM-Region.
//
// Der frueher benutzte Eingabepuffer lag in internem RAM und war nur
// adressaligniert. Der Selbsttest, der aus einem PSRAM-Puffer dekodierte, lief
// stabil; der Normalbetrieb aus internem RAM stuerzte ab. Das ist die
// belastbarste Spur, die wir haben.
// ==============================================================================

#include <Arduino.h>
#include <driver/jpeg_decode.h>

// Groessenrahmen. Bilder ausserhalb dieser Grenzen werden verworfen, bevor sie
// die Hardware erreichen - ein zu grosses Bild wuerde den DMA-Transfer
// abbrechen und einen 2D-DMA-Kanal verklemmt zuruecklassen.
#define VIDEO_MAX_WIDTH   1280
#define VIDEO_MAX_HEIGHT   960
#define VIDEO_BYTES_PER_PX   2      // RGB565

// Groesster akzeptierter Block (Content-Length der Kamera).
//
// Die Kamera-App sendet jedes Bild in einem Block FESTER Groesse und fuellt
// hinter dem JPEG mit Nullen auf. Beobachtet wurden drei Groessen:
//
//     81920 B   (80 KiB)   Normalfall
//    163840 B  (160 KiB)
//    655360 B  (640 KiB)
//
// Welche Groesse die Kamera waehlt, haengt NICHT einfach an der Qualitaet:
// die grossen Bloecke kamen auch bei Qualitaet 0. Ein Versuch, sie durch
// Senken der Qualitaet zu vermeiden, hat deshalb nie ein Bild gezeigt.
//
// Richtig ist, jeden dieser Bloecke anzunehmen: er wird vollstaendig gelesen
// (das muss er ohnehin, sonst geraet der Bytestrom ausser Tritt), und
// jpegLength() schneidet das echte JPEG heraus. Grosse Bloecke kosten
// Leitungszeit und damit Bildrate - aber es wird immer ein Bild gezeigt.
//
// 768 KiB deckt die groesste beobachtete Stufe mit Reserve. Die Puffer liegen
// im PSRAM (2 x 768 KiB von rund 23 MB frei).
#define VIDEO_MAX_JPEG_BYTES (768u * 1024u)

// Anzahl der Ausgabepuffer. Drei entkoppeln Dekodierung und Anzeige: der
// Decoder schreibt nie in den Puffer, den die Anzeige gerade liest.
#define VIDEO_OUT_SLOTS 3

// Anzahl der Eingabepuffer. Bewusst NICHT an VIDEO_OUT_SLOTS gekoppelt:
// Ein- und Ausgabeslots rotieren unabhaengig voneinander (s_nextOut).
//
// Zwei statt drei. Ein dritter Slot puffert Schwankungen zwischen Empfang und
// Anzeige ab. Gemessen braucht der Empfang 145 ms je Bild, Dekodieren und
// Anzeigen zusammen 25 ms - der Decoder ist nie der Engpass, der dritte Slot
// stand leer. Die Puffer liegen im PSRAM (siehe VIDEO_JPEG_IN_SRAM).
#define VIDEO_IN_SLOTS 2

// Ein fertig dekodiertes Bild.
struct DecodedFrame {
    uint8_t* pixels;    // RGB565, PSRAM
    uint16_t width;
    uint16_t height;
    uint8_t  slot;      // Index des Ausgabepuffers
};

// Legt Engine und Puffer an. Einmalig beim Systemstart aufrufen.
// Gibt false zurueck, wenn der Speicher nicht reicht - dann bleibt Video aus.
bool VideoDecoder_Init();

// true, sobald Init erfolgreich war.
bool VideoDecoder_IsReady();

// Liefert einen Eingabepuffer, in den der Netzwerkpfad einen JPEG-Frame legen
// darf, samt seiner Kapazitaet. Die Puffer sind decoder-konform aligniert.
uint8_t* VideoDecoder_GetInputBuffer(uint8_t slot, size_t* capacity);
uint8_t  VideoDecoder_InputSlotCount();

// Prueft die Kopfdaten eines JPEG-Frames, ohne ihn zu dekodieren.
// Gibt false zurueck, wenn der Frame unbrauchbar ist oder nicht in den
// Ausgabepuffer passt. Fuellt width/height, wenn nicht null.
bool VideoDecoder_Probe(const uint8_t* jpeg, size_t len, uint16_t* w, uint16_t* h);

// Dekodiert einen Frame in den naechsten freien Ausgabepuffer.
// Der Aufrufer darf das Ergebnis lesen, bis er erneut dekodiert.
// Gibt false bei Decode-Fehlern zurueck; die Hardware bleibt dabei unberuehrt.
bool VideoDecoder_Decode(const uint8_t* jpeg, size_t len, DecodedFrame* out);

// Legt die Kanalreihenfolge der Ausgabe fest.
//   true  = BGR: fuer den Weg ueber M5GFX/LVGL (Fenstermodus)
//   false = RGB: fuer den PPA, der direkt in den Framebuffer schreibt
// Muss vor dem Dekodieren gesetzt werden und gilt ab dem naechsten Bild.
void VideoDecoder_SetBgrOrder(bool bgr);

// Zaehler fuer die Diagnose.
uint32_t VideoDecoder_GetErrorCount();
uint32_t VideoDecoder_GetRejectCount();
