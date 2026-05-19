#pragma once
#include <lvgl.h>

class ViewBootScreen {
public:
    // Initialisiert und startet die Boot-Animationen
    static void show();
};

// Externer Pointer, damit andere Systemteile (z.B. main.cpp) den Ladestatus aktualisieren koennen
extern lv_obj_t * splash_status_label;