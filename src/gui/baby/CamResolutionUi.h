#pragma once
// ==============================================================================
// CamResolutionUi - Auswahl der Kamera-Aufloesung (gemeinsame Bedienstelle)
// ==============================================================================
// Die Aufloesung der Baby-Ansicht laesst sich an zwei Stellen einstellen: in
// den Einstellungen und im Einstellungs-Dialog der Baby-Ansicht. Beide sollen
// sich exakt gleich verhalten. Deshalb fuellen beide ihr Dropdown hier und
// melden Aenderungen ueber denselben Callback - die Einstellung selbst gibt es
// nur einmal (BabyService).
//
// Inhalt des Dropdowns:
//   - die Aufloesungen der Kamera (live oder zuletzt bekannt),
//   - steht die Einstellung nicht darin, zusaetzlich die Einstellung selbst
//     (markiert) - sonst saehe man nicht, was eingestellt ist,
//   - Fremde Kamera (Expertenmodus): gesperrt, die Aufloesung steht dort in
//     der URL und laesst sich nicht per API setzen.
// ==============================================================================

#include <Arduino.h>
#include <lvgl.h>

// Fuellt das Dropdown und waehlt die Einstellung aus. Mehrfach aufrufbar
// (z.B. bei jedem Oeffnen eines Dialogs).
void CamResolutionUi_Fill(lv_obj_t* dropdown);

// Kurzer Hinweis zum Zustand, leer wenn alles passt. Beispiele:
// "nicht verfuegbar, verwendet: 800x450", "Kamera liefert gerade 800x450".
String CamResolutionUi_Hint();

// LV_EVENT_VALUE_CHANGED-Callback fuer das Dropdown. Speichert die Auswahl.
void CamResolutionUi_OnChanged(lv_event_t* e);
