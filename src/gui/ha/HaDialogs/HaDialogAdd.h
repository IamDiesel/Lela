#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"

class HAFolderWidget; // Forward Declaration

class HaDialogAdd {
public:
    static HAFolderWidget* target_folder; // NEU: Merkt sich das Ziel
    static void showAddWidgetDialog(HAFolderWidget* folder = nullptr);
    static void resetState();
};