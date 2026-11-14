#pragma once
#include <lvgl.h>

// themed: Farben aus dem Farbschema (UiTheme) statt der bisherigen festen
// Werte. Nur Ansichten, die selbst schon das Schema nutzen (ab D1: die
// Einstellungen); Inhalt und Anordnung der Leiste sind in beiden Faellen gleich.
void ViewTopbar_Create(lv_obj_t* parent, bool themed = false);
void ViewTopbar_Update();
void ViewTopbar_SetHidden(bool hidden);