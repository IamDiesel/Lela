#pragma once
#include "IEditTab.h"
#include "HAWidgets.h"
#include <vector>

// ==============================================================================
// EditTabMap - Reiter "Karte" im Bearbeiten-Dialog
// ==============================================================================
//   * Tracker: Liste als Chips (Antippen entfernt), Suche zum Hinzufuegen.
//     Vorgeschlagen werden nur Entitaeten mit Koordinaten (Katalog-Merkmal
//     "g"), also Tracker, Personen, Zonen und Sensoren mit Position.
//   * Weg: Laenge des angezeigten Verlaufs (wie HA hours_to_show).
//   * Max. Zoomstufe beim Einpassen.
//   * Funkstille: ab wann ein Tracker als veraltet gilt (halbdurchsichtig, Weg unterbrochen).
//   * Ausschnitt: nur bei festem Ausschnitt (aus dem Vollbild uebernommen)
//     sichtbar - zurueck auf automatisch.
// Alles wird wie bei den anderen Reitern erst mit "Uebernehmen" wirksam. Neue
// Tracker liefern Positionen, sobald der Bearbeiten-Modus verlassen wird
// (dann werden die Abos neu aufgebaut).
// ==============================================================================
class EditTabMap : public IEditTab {
public:
    ~EditTabMap() override;

    const char* getName() override { return "Karte"; }
    void buildUI(lv_obj_t* parent, HAWidget* w, lv_obj_t* keyboard) override;
    void saveConfig(HAWidget* w) override;

private:
    lv_obj_t* chip_box  = nullptr;   // Chips der Tracker
    lv_obj_t* ta_search = nullptr;
    lv_obj_t* roller    = nullptr;   // Vorschlaege zur Suche
    lv_obj_t* dd_hours  = nullptr;
    lv_obj_t* dd_zoom   = nullptr;
    lv_obj_t* dd_stale  = nullptr;
    lv_obj_t* dd_view   = nullptr;   // nur bei festem Ausschnitt

    // Bearbeitete Liste (wird erst beim Uebernehmen ans Widget gegeben).
    std::vector<String> entities;

    // Werte zu den Eintraegen der Auswahllisten (gleiche Reihenfolge).
    std::vector<float> hour_values;
    std::vector<int>   zoom_values;
    std::vector<int>   stale_values;

    void rebuildChips();
    void addEntity(const String& id);

    static void chip_event_cb(lv_event_t* e);
    static void roller_event_cb(lv_event_t* e);
    static void search_event_cb(lv_event_t* e);
};
