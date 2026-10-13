#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"
#include "HAFolderWidget.h"

class HaDialogFolder {
private:
    static lv_obj_t* overlay;
    static HAFolderWidget* current_folder;

    // LV_EVENT_DELETE des Overlays: setzt overlay und current_folder zurueck,
    // auch wenn das Overlay zusammen mit dem Screen geloescht wird.
    static void overlay_delete_cb(lv_event_t* e);

public:
    static void show(HAFolderWidget* folder);
    static void resetState();
};