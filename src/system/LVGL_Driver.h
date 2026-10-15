#ifndef LVGL_DRIVER_H
#define LVGL_DRIVER_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

void Lvgl_Init();
void Set_Backlight(uint8_t val);
bool lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

// ------------------------------------------------------------------------------
// Ausgesparte Flaeche
// ------------------------------------------------------------------------------
// Eine Rechteckflaeche, in die LVGL beim Flush NICHT schreibt. Dort zeichnet
// jemand anderes direkt in den Bildspeicher (Video-Overlay per PPA). Ohne die
// Aussparung wuerde jede Neuzeichnung darunterliegender Widgets das Videobild
// bis zum naechsten Videobild uebermalen - sichtbar als Flackern.
//
// Koordinaten wie LVGL (Querformat 1280x720). Nur unter der LVGL-Sperre bzw.
// aus der Hauptschleife setzen. Wer die Flaeche freigibt, muss sie danach mit
// lv_obj_invalidate()/lv_inv_area() neu zeichnen lassen.
void Lvgl_SetReservedArea(const lv_area_t* area);   // nullptr = keine Aussparung

#endif