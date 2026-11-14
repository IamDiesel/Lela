#pragma once
#include <Arduino.h>
#include "lvgl.h"
#include <vector>
#include "HAWidgets.h"

class ViewHomeAssistant {
public:
    static lv_obj_t* build();
    static void update();
    
    static lv_obj_t* screen;
    static lv_obj_t* tabview;
    static lv_obj_t* btn_edit;
    static lv_obj_t* label_edit;
    static lv_obj_t* trash_btn; 

    static volatile bool pendingHaReload;
    static uint16_t currentActiveTab;

    // Von GuiManager (Screenwechsel) und den Dialogen genutzt.
    static void clearWidgets();
    static String generateEntityId(String type, String input);
    
    // Uebernimmt das Layout des aktiven Tabs, speichert und baut neu auf.
    static void helper_saveWidgets(); 

    // Uebernimmt das Layout des aktiven Tabs (Positionen, Groessen, Loeschungen,
    // Aenderungen aus dem Bearbeiten-Dialog) nach HaConfigLogic::dashboards -
    // OHNE zu speichern oder neu aufzubauen.
    // Vor jeder Aktion im Edit-Modus aufrufen, die 'dashboards' selbst
    // veraendert und danach speichert (Tab verschieben, umbenennen, Widget
    // hinzufuegen, HA-Import). Sonst gehen die bisherigen Aenderungen verloren.
    static void captureActiveTabLayout();

private:
    static std::vector<HAWidget*> widgets;
    static std::vector<lv_obj_t*> tab_pages;

    // Schneller Start: build() legt nur die Widgets des aktiven Tabs an; die
    // uebrigen Tabs fuellt update() stueckweise im Hintergrund (Zeitbudget je
    // Durchlauf), nachdem das erste Bild gezeichnet ist. Ein Tab, der vorher
    // gebraucht wird (Wechsel, Bearbeiten, Layout uebernehmen), wird sofort
    // vollstaendig angelegt (ensureTabBuilt).
    // tab_progress[i]: Anzahl der schon angelegten Widgets von Tab i.
    static std::vector<size_t> tab_progress;

    static void helper_loadWidgets();
    // Legt Widgets von Tab 'tab' an, bis er fertig oder 'deadlineMs'
    // (millis(); 0 = ohne Grenze) erreicht ist. true = Tab fertig.
    static bool buildTabWidgets(size_t tab, uint32_t deadlineMs);
    static void ensureTabBuilt(size_t tab);
    static void ensureAllTabsBuilt();
    // Hintergrund-Aufbau (aus update()).
    static void buildPendingTabs();

    static void btn_back_event_cb(lv_event_t * e);
    static void btn_edit_event_cb(lv_event_t * e);
    static void tabview_event_cb(lv_event_t * e);
};