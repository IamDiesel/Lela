#pragma once
// ==============================================================================
// WifiNetworks - gemerkte WLAN-Netze (SSID, Passwort, Kanal)
// ==============================================================================
// Jedes Netz, mit dem sich das Geraet erfolgreich verbunden hat, wird mit
// Passwort und Kanal gemerkt - das zuletzt verbundene zuerst, hoechstens
// MAX_NETWORKS. Gemerkt wird erst nach einer erfolgreichen Verbindung, also
// nur Zugangsdaten, die nachweislich funktionieren.
//
// Nutzer:
//   * Web-Setup: Auswahlliste "Gespeicherte Netze" fuellt SSID und Passwort,
//     einzelne Netze lassen sich vergessen.
//   * WifiConnect: merkt nach jeder Verbindung (Kanal nur zur Information -
//     als Suchhinweis brachte er keinen Gewinn, siehe WifiConnect.h).
//
// Speicher: eigener NVS-Bereich "wifinets" (Passwoerter im Klartext wie das
// aktuelle WLAN-Passwort im Bereich "catmat"). Geschrieben wird nur bei
// Aenderung. factoryReset() loescht ihn mit (WifiNetworks_Clear).
//
// Nur aus dem loopTask aufrufen (Web-Setup und WifiConnect laufen dort).
// ==============================================================================

#include <Arduino.h>
#include <vector>

struct WifiNetwork {
    String  ssid;
    String  pass;
    uint8_t channel = 0;   // 0 = unbekannt
};

// Hoechstzahl gemerkter Netze; das am laengsten nicht genutzte faellt heraus.
static constexpr size_t WIFI_NETWORKS_MAX = 8;

// Alle gemerkten Netze, zuletzt verbundenes zuerst.
std::vector<WifiNetwork> WifiNetworks_List();

// Nach erfolgreicher Verbindung: Netz (neu) an den Anfang setzen.
// Schreibt nur, wenn sich etwas aendert.
void WifiNetworks_Remember(const String& ssid, const String& pass, uint8_t channel);

// Netz vergessen. false, wenn es nicht gemerkt war.
bool WifiNetworks_Forget(const String& ssid);

// Alle gemerkten Netze loeschen (Werkseinstellung).
void WifiNetworks_Clear();
