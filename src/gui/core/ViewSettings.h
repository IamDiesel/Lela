#pragma once
#include <lvgl.h>

class ViewSettings {
public:
    // Durch das "static" koennen die Funktionen ueberall im Code direkt 
    // mit ViewSettings::build() aufgerufen werden, ohne ein Objekt zu instanziieren.
    static lv_obj_t* build();
    static void update();
    // Zurueck zum Dashboard (Kopf der Seitenleiste, Wischgeste). Fragt vorher
    // nach, wenn die offene Seite ungespeicherte Aenderungen hat.
    static void requestLeave();
};