#pragma once
// ==============================================================================
// QrScanner - QR-Codes im Kamerabild finden (quirc, Hintergrund-Task)
// ==============================================================================
// Ablauf:
//   1. QrScanner_Start(): quirc fuer CAMERA_WIDTH x CAMERA_HEIGHT anlegen
//      (2 x 0,9 MB PSRAM) und den Auswerte-Task starten.
//   2. Der Kamera-Verbraucher (ViewCameraPreview, onFrame) bietet jedes Bild
//      mit QrScanner_Offer() an. Ist der Task frei, wird das Bild sofort in
//      Graustufen in den quirc-Puffer umgerechnet (~10 ms) und ausgewertet;
//      sonst wird es verworfen. So bleibt die Vorschau fluessig, und der Task
//      arbeitet immer mit einem aktuellen Bild.
//   3. QrScanner_TakeResult() liefert den Inhalt eines erkannten Codes.
//   4. QrScanner_Stop(): Task beenden, Speicher freigeben.
//
// Ausrichtung: QR-Codes sind drehunabhaengig, das Bild wird also nicht
// gedreht. Gespiegelte Codes (z.B. durch ein Spiegelbild) erkennt quirc ueber
// quirc_flip() ebenfalls.
//
// Task: Kern 0, niedrige Prioritaet, Stapel 20 KB im internen RAM - quirc_decode
// legt rund 9 KB auf den Stapel. Der Stapel liegt nur waehrend des Scannens an.
// ==============================================================================

#include <Arduino.h>

bool QrScanner_Start();
void QrScanner_Stop();

// Kamerabild (RGB565, CAMERA_WIDTH x CAMERA_HEIGHT) anbieten. true = wird
// ausgewertet, false = Task beschaeftigt oder nicht gestartet (Bild verworfen).
// Nur waehrend das Bild geliehen ist aufrufen (CameraTab5_AcquireFrame).
bool QrScanner_Offer(const uint16_t* frame);

// Inhalt des zuletzt erkannten Codes abholen (einmalig). false = keiner neu.
bool QrScanner_TakeResult(String& text);
