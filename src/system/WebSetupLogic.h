#pragma once

void WebSetupLogic_Init();
void WebSetupLogic_Update();

// Beendet ein laufendes oder angefordertes Web-Setup ohne Neustart: Webserver
// und DNS aus, im eigenen Setup-WLAN zurueck ins Heimnetz. Aus dem LVGL-
// Kontext aufrufbar; ausgefuehrt wird im naechsten WebSetupLogic_Update().
void WebSetupLogic_RequestStop();