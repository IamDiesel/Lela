#pragma once
#include <lvgl.h>

// Die Zustaende unserer State-Machine.
// Der Start wartet nur noch auf das WLAN: Home Assistant verbindet sich erst
// beim Oeffnen der HA-App (GuiManager::switchScreen) - ein frueherer Zustand
// "warte auf HA" lief deshalb immer in seine 2,5-s-Wartezeit.
//
// Waehrend BOOT_STATE_WIFI laeuft die Startfolge der WLAN-Netze (WifiConnect):
// Der Statustext nennt das Netz des laufenden Versuchs, und am unteren Rand
// stehen zwei Knoepfe - "Naechstes Netz" (nur bei mehreren Netzen) und "Ohne
// WLAN starten". Die drei Endzustaende lassen die Animation auslaufen und
// fuehren ins Dashboard.
enum BootState {
    BOOT_STATE_INIT = 0,
    BOOT_STATE_WIFI,      // Startfolge laeuft
    BOOT_STATE_DONE,      // verbunden (extraInfo: SSID)
    BOOT_STATE_FAILED,    // kein Netz erreichbar (extraInfo: Meldung, bleibt 2,5 s lesbar)
    BOOT_STATE_SKIPPED    // ohne WLAN: abgebrochen, WLAN aus, keines eingerichtet (extraInfo: Meldung)
};

class ViewBootScreen {
public:
    static void show();
    
    // Wird von der main.cpp aufgerufen, um die Animation voranzutreiben
    static void setBootState(BootState state, const char* extraInfo = "");

    // Waehrend BOOT_STATE_WIFI: laufender Versuch der Startfolge (attempt ab 1).
    // Setzt den Statustext und zeigt die Knoepfe; ab dem zweiten Versuch laeuft
    // die Katze ein Stueck weiter, statt stehen zu bleiben.
    static void setWifiAttempt(const char* ssid, uint8_t attempt, uint8_t count);
};

// Wird weiterhin exportiert (wird intern aber nur noch gelesen)
extern lv_obj_t * splash_status_label;