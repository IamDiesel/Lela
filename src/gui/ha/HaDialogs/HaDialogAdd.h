#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"

class HAFolderWidget; // Forward Declaration

class HaDialogAdd {
public:
    static HAFolderWidget* target_folder; // Zielordner, nullptr = aktiver Tab
    static void showAddWidgetDialog(HAFolderWidget* folder = nullptr);
    static void resetState();

    // Die Entitaetsliste wurde (neu) geladen - offene Vorschau aktualisieren.
    static void onCatalogUpdated();
};