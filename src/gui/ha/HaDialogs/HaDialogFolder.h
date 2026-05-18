#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"
#include "HAFolderWidget.h"

class HaDialogFolder {
private:
    static lv_obj_t* overlay;
    static HAFolderWidget* current_folder;

public:
    static void show(HAFolderWidget* folder);
    static void resetState();
};