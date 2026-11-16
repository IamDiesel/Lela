#pragma once
// ==============================================================================
// WifiNetworks - gemerkte WLAN-Netze: Zugangsdaten, Prioritaet, IP-Einstellung
// ==============================================================================
// Jedes Netz, mit dem sich das Geraet erfolgreich verbunden hat, wird mit
// Passwort und Kanal gemerkt - hoechstens WIFI_NETWORKS_MAX. Gemerkt wird erst
// nach einer erfolgreichen Verbindung, also nur Zugangsdaten, die nachweislich
// funktionieren.
//
// Reihenfolge = Prioritaet. Der Nutzer legt sie fest (WifiNetworks_Move); ein
// neues Netz kommt ans Ende. Beim Start probiert WifiConnect zuerst das zuletzt
// verbundene Netz und danach die Liste in dieser Reihenfolge.
//
// IP-Einstellung je Netz: DHCP oder feste Adresse (WifiIpConfig). Fuer ein
// Netz, das noch nicht gemerkt ist (neu im Web-Setup eingetragen), haelt ein
// einzelner Vormerk-Platz die Einstellung, bis die erste Verbindung gelingt -
// dann geht sie in den Eintrag ueber.
//
// Nutzer:
//   * WifiConnect: Startfolge, IP-Einstellung beim Anmelden, Merken nach jeder
//     Verbindung (Kanal nur zur Information - als Suchhinweis brachte er
//     keinen Gewinn, siehe WifiConnect.h).
//   * Einstellungen > Verbindungen > WLAN-Netz: Liste, Reihenfolge, "Netz
//     bearbeiten" (feste IP, vergessen).
//   * Web-Setup: Auswahlliste "Gespeicherte Netze", feste IP des gewaehlten
//     Netzes, einzelne Netze vergessen.
//
// Speicher: eigener NVS-Bereich "wifinets" (Passwoerter im Klartext wie das
// aktuelle WLAN-Passwort im Bereich "catmat"). Geschrieben wird nur bei
// Aenderung. factoryReset() loescht ihn mit (WifiNetworks_Clear).
// Uebernahme: Bis 16.11.2026 gab es EINE feste IP fuer alle Netze (Bereich
// "catmat", Schluessel ipFix/ipAddr/ipGw/ipMask/ipDns). Sie geht beim ersten
// Zugriff einmalig auf das damals aktuelle Netz ueber.
//
// Nur aus dem loopTask aufrufen (Einstellungen, Web-Setup und WifiConnect
// laufen dort).
// ==============================================================================

#include <Arduino.h>
#include <vector>

// IP-Einstellung eines Netzes.
struct WifiIpConfig {
    bool   fixed = false;   // false = DHCP; die Felder bleiben trotzdem gemerkt
    String ip;
    String gateway;
    String subnet;
    String dns;             // leer = Gateway (ueblich: der Router ist DNS)
};

// true, wenn Adresse, Gateway und Maske gueltig sind (Maske beginnt mit 255)
// und DNS leer oder gueltig ist. Unabhaengig von `fixed`.
bool WifiIpConfig_Valid(const WifiIpConfig& cfg);

struct WifiNetwork {
    String       ssid;
    String       pass;
    uint8_t      channel = 0;   // 0 = unbekannt
    WifiIpConfig ip;
};

// Hoechstzahl gemerkter Netze. Ist die Liste voll, verdraengt ein neues Netz
// das mit der niedrigsten Prioritaet.
static constexpr size_t WIFI_NETWORKS_MAX = 8;

// Alle gemerkten Netze in der Reihenfolge ihrer Prioritaet (hoechste zuerst).
std::vector<WifiNetwork> WifiNetworks_List();

// Eintrag zu einer SSID. false, wenn das Netz nicht gemerkt ist.
bool WifiNetworks_Find(const String& ssid, WifiNetwork* out);

// Nach erfolgreicher Verbindung: Passwort und Kanal auffrischen bzw. ein neues
// Netz ans Ende der Liste setzen (mit einer vorgemerkten IP-Einstellung).
// Schreibt nur, wenn sich etwas aendert; die Prioritaet bleibt unberuehrt.
void WifiNetworks_Remember(const String& ssid, const String& pass, uint8_t channel);

// Prioritaet aendern: delta -1 = eine Stelle hoeher, +1 = eine tiefer.
// false, wenn das Netz nicht gemerkt ist oder schon am Rand steht.
bool WifiNetworks_Move(const String& ssid, int delta);

// Netz vergessen. false, wenn es nicht gemerkt war.
bool WifiNetworks_Forget(const String& ssid);

// Alle gemerkten Netze loeschen (Werkseinstellung).
void WifiNetworks_Clear();

// IP-Einstellung fuer eine SSID: die des Eintrags, sonst die vorgemerkte,
// sonst DHCP.
WifiIpConfig WifiNetworks_IpConfigFor(const String& ssid);

// IP-Einstellung setzen: im Eintrag, wenn das Netz gemerkt ist - sonst auf dem
// Vormerk-Platz (ersetzt eine fruehere Vormerkung fuer ein anderes Netz).
void WifiNetworks_SetIpConfig(const String& ssid, const WifiIpConfig& cfg);
