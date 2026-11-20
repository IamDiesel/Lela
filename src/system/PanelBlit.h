#pragma once
// ==============================================================================
// PanelBlit - schneller Weg von LVGL in den Bildspeicher des Tab5-Panels
// ==============================================================================
// Ersetzt im LVGL-Flush den allgemeinen Weg ueber M5.Display.pushImage durch
// ein direktes, gedrehtes Kopieren (PanelBlitCopy.h - dort stehen Anlass und
// Messwerte). Geschrieben wird in denselben Speicher wie bisher, mit der CPU
// wie bisher: kein PPA, kein 2D-DMA, der Panel-Patch (DebugFlags.h) bleibt
// unberuehrt. Den Abgleich Cache -> Bildspeicher erledigt weiter M5GFX
// (M5.Display.display nach dem Kopieren).
//
// Sicherheit: Vor der ersten Benutzung prueft ein Selbsttest gegen M5GFX,
// wohin ein Punkt der Oberflaeche im Bildspeicher gehoert und ob Bytes zu
// tauschen sind: Er schreibt ueber M5GFX zwei kleine Muster und sucht sie im
// Bildspeicher. Passt keine der bekannten Lagen (andere Drehung, anderes
// Geraet, andere Bibliotheksfassung), bleibt es beim Weg ueber M5GFX.
//
// Nur aus dem loopTask unter der LVGL-Sperre (wie jeder Displayzugriff).
// ==============================================================================

#include <stdint.h>

// true, wenn der schnelle Weg benutzt werden kann. Der erste Aufruf richtet
// ihn ein (Bildspeicher holen, Selbsttest) - danach kostet er nichts mehr.
bool PanelBlit_Ready();

// Kopiert das Rechteck [x1..x2] x [y1..y2] der Oberflaeche in den Bildspeicher.
// `src` zeigt auf den Punkt (srcX, srcY) eines zeilenweisen RGB565-Puffers mit
// `srcW` Pixeln je Zeile. Nur nach PanelBlit_Ready() == true.
void PanelBlit_Push(const uint16_t* src, int srcW, int srcX, int srcY, int x1, int y1, int x2, int y2);
