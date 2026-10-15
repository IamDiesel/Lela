#pragma once
#include <lvgl.h>

// ==============================================================================
// VideoLogic - Empfang und Anzeige des Kamerastreams
// ==============================================================================
// Gesteuert ausschliesslich vom BabyService. Empfang (Start/Stop) und Ausgabe
// (SetOutput) sind getrennt: der Stream kann weiterlaufen, waehrend gerade
// niemand ein Bild braucht - dann wird nicht dekodiert, und beim naechsten
// Anzeigen ist das Bild ohne den Verbindungsaufbau der Kamera (~3 s) sofort da.
// ==============================================================================

// Ziel der dekodierten Bilder.
enum class VideoOutput : uint8_t {
    None = 0,    // empfangen, aber nicht dekodieren
    BabyView,    // Baby-Ansicht (Fenster oder Vollbild, siehe vidFSMode)
    Pip,         // Video-Overlay auf anderen Bildschirmen (PPA, gedrosselt)
};

void VideoLogic_Init();   // Speicher und Warteschlangen, einmalig beim Boot

// Idempotent: legt die Tasks nur an, wenn keine laufen. Darf in jedem
// Durchlauf gerufen werden (siehe BabyService_Update).
void VideoLogic_Start();

// Fordert das Ende beider Tasks an und kehrt sofort zurueck.
void VideoLogic_Stop();

// true, solange noch ein Netz- oder Decoder-Task existiert.
bool VideoLogic_IsRunning();

void VideoLogic_SetOutput(VideoOutput out);
VideoOutput VideoLogic_GetOutput();

// --- Video-Overlay (VideoOutput::Pip) -----------------------------------------
// Zielrechteck in LVGL-Koordinaten (Querformat). Das Bild wird mit erhaltenem
// Seitenverhaeltnis eingepasst, Raender werden schwarz gefuellt. w/h <= 0 =
// kein Ziel (es wird nichts gezeichnet). Aus der Hauptschleife aufrufen.
void VideoLogic_SetPipRect(int x, int y, int w, int h);

// Hoechstens so viele Bilder je Sekunde werden fuer das Overlay dekodiert.
// Die Kamera sendet weiter in ihrem Takt; ueberzaehlige Bilder werden ohne
// Dekodierung verworfen.
void VideoLogic_SetPipFps(int fps);

// Millisekunden seit dem letzten ins Overlay gezeichneten Bild (UINT32_MAX,
// solange noch keines gezeichnet wurde). Dient der Anzeige "Verbindung ...".
uint32_t VideoLogic_MsSinceLastPipFrame();
