#pragma once
// ==============================================================================
// SettingsPages - Seiten der Einstellungs-Ansicht (intern)
// ==============================================================================
// ViewSettings stellt den Rahmen (Topbar, Seitenleiste, Inhaltsflaeche,
// Tastatur) und die Navigation. Jede Seite baut sich in die Inhaltsflaeche und
// aktualisiert ihre Live-Werte selbst. Aufgeteilt nach Bereichen:
//
//   SettingsPageCat.cpp     Katzenmatte, Bluetooth-Suche (Matte/Kippy)
//   SettingsPageBaby.cpp    Babyphone, Kamera-Verbindung, Tonquelle
//   SettingsPageNet.cpp     Verbindungen, WLAN-Netze, Web-Setup
//   SettingsPageSystem.cpp  Anzeige, System, Diagnose, Bildschirmfoto
//
// Nur der sichtbare Bereich existiert als LVGL-Objekte. Zeiger einer Seite
// gelten nur, solange sie die aktuelle ist - das stellt ViewSettings sicher
// (update/leave nur fuer die aktuelle Seite).
// ==============================================================================

#include <lvgl.h>

enum class SettingsPage : uint8_t {
    // Bereiche der Seitenleiste (Reihenfolge = Reihenfolge der Leiste)
    Cat = 0, Baby, Net, Display, System,
    // Unterseiten
    Camera, Audio, Wifi, WebSetup, BleScanMat, BleScanKippy, Diag, Screenshot,
};
static constexpr uint8_t SETTINGS_AREA_COUNT = 5;

// --- Navigation (ViewSettings.cpp) ---------------------------------------------
// Aufgeschoben (lv_async_call): darf aus dem Event-Callback eines Objekts der
// aktuellen Seite kommen, die dabei geloescht wird.
void Settings_Navigate(SettingsPage page);
// Zurueck zum Bereich der aktuellen Unterseite.
void Settings_Back();
// Event-Callback-Form von Settings_Back (Zurueck-Link, Verwerfen).
void Settings_BackCb(lv_event_t* e);

// --- Seiten -----------------------------------------------------------------------
// Je Seite: Aufbau, Live-Aktualisierung (jeder Durchlauf der Hauptschleife;
// die Seite drosselt selbst) und Aufraeumen beim Verlassen.
void SettingsCat_Build(lv_obj_t* parent);
void SettingsCat_Update();
void SettingsBleScan_Build(lv_obj_t* parent, bool kippy);
void SettingsBleScan_Update();
void SettingsBleScan_Leave();

void SettingsBaby_Build(lv_obj_t* parent);
void SettingsBaby_Update();
void SettingsCamera_Build(lv_obj_t* parent);
void SettingsCamera_Update();
void SettingsAudio_Build(lv_obj_t* parent);

void SettingsNet_Build(lv_obj_t* parent);
void SettingsNet_Update();
void SettingsWifi_Build(lv_obj_t* parent);
void SettingsWifi_Update();
void SettingsWebSetup_Build(lv_obj_t* parent);
void SettingsWebSetup_Update();   // Setup von selbst beendet (Zeitlimit) -> Auswahl
void SettingsWebSetup_Leave();    // Seite verlassen -> laufendes Setup beenden

void SettingsDisplay_Build(lv_obj_t* parent);
void SettingsSystem_Build(lv_obj_t* parent);
void SettingsSystem_Update();
void SettingsDiag_Build(lv_obj_t* parent);
void SettingsDiag_Update();
void SettingsScreenshot_Build(lv_obj_t* parent);
void SettingsScreenshot_Update();

// Kurzer Live-Status je Bereich fuer die Seitenleiste ("Matte verbunden").
// level: 0 = aus, 1 = ok, 2 = Hinweis.
const char* SettingsArea_Status(SettingsPage area, uint8_t* level);
