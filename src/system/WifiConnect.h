#pragma once
// ==============================================================================
// WifiConnect - WLAN-Verbindung starten, beschleunigen und vermessen
// ==============================================================================
// Ablauf beim Start (Messung 14.11.2026, Router auf Kanal 8):
//   ~2,9 s  WLAN-Chip hochfahren (ESP32-C6 ueber ESP-Hosted, in WiFi.begin)
//   ~3,0 s  Suchen + Anmelden am Router
//   1,5-2,5 s IP per DHCP (inkl. ARP-Pruefung der Adresse, CONFIG_LWIP_DHCP_DOES_ARP_CHECK)
//
// Beschleunigung:
//   * Feste IP je Netz (WifiNetworks, einstellbar unter Einstellungen >
//     Verbindungen > WLAN-Netz > Bearbeiten und im Web-Setup): DHCP entfaellt
//     (gemessen: IP 1 ms nach dem Anmelden statt 1,5-2,5 s). Ungueltige
//     Angaben werden ignoriert (dann DHCP).
//   * Nicht wirksam, daher entfernt: Kanal-Hinweis (mit und ohne BSSID). Das
//     Anmelden dauerte mit Hinweis exakt so lange wie ohne (3,0 s ab "Station
//     gestartet") - der Kanal-Suchlauf ist nicht der Engpass. Die 3,0 s liegen
//     im Anmelden selbst (Router: WPA2/WPA3-Mischbetrieb) bzw. im WLAN-Chip.
//
// Nach jeder erfolgreichen Verbindung merkt WifiNetworks das Netz (SSID,
// Passwort, Kanal) fuer den naechsten Start und die Auswahl im Web-Setup.
//
// Startfolge (WifiConnect_Boot...): Beim Einschalten wird zuerst das zuletzt
// verbundene Netz probiert, danach die gemerkten Netze nach Prioritaet - siehe
// WifiBootSequence.h. Der Bootscreen zeigt den laufenden Versuch und bietet
// "Naechstes Netz" und "Ohne WLAN starten" an. Gelingt die Verbindung mit
// einem anderen als dem bisherigen Netz, wird es das aktuelle (naechster Start
// beginnt dort). Nach dem Start wird nicht mehr selbsttaetig gewechselt.
//
// Messung: Jedes WLAN-Ereignis (Start, angemeldet mit Verfahren, getrennt mit
// Grund, IP) wird mit Zeit seit dem Verbindungsstart geloggt ([WLAN] +x ms).
// Getrennt-Meldungen sind begrenzt, damit eine Reconnect-Schleife ohne Router
// das Log nicht flutet.
//
// Nur aus dem loopTask aufrufen (die Ereignis-Rueckmeldung laeuft im
// Arduino-Ereignis-Task und loggt nur).
// ==============================================================================

#include <Arduino.h>
#include "WifiBootSequence.h"

// Startet die Verbindung (feste IP, falls fuer dieses Netz eingestellt).
void WifiConnect_Begin(const String& ssid, const String& pass);

// Regelmaessig aufrufen (SystemLogic_Update): Netz merken, erste Verbindung
// protokollieren.
void WifiConnect_Update();

// WLAN wurde abgeschaltet: Ueberwachung beenden.
void WifiConnect_Reset();

// Wechselt das Netz (Einstellungen: gemerkte Netze / anderes Netz): speichert
// SSID und Passwort als aktuelles WLAN und meldet sich neu an. Gemerkt wird
// das Netz wie immer erst nach erfolgreicher Verbindung (WifiNetworks).
void WifiConnect_SwitchTo(const String& ssid, const String& pass);

// true, wenn die laufende Verbindung mit fester IP aufgebaut wurde.
bool WifiConnect_IsStaticIp();

// --- Startfolge -------------------------------------------------------------------
// Beginnt die Startfolge und meldet das erste Netz an (in setup(), anstelle von
// WifiConnect_Begin). false: kein Netz bekannt - es wurde nichts gestartet.
bool WifiConnect_BootStart();

// Knopf "Naechstes Netz": laufenden Versuch aufgeben.
void WifiConnect_BootSkip();
// Knopf "Ohne WLAN starten": Folge beenden. Das erste Netz der Folge wird im
// Hintergrund weiter versucht.
void WifiConnect_BootAbort();

// Stand der Startfolge fuer den Bootscreen.
struct WifiBootStatus {
    WifiBootState state   = WifiBootState::Idle;
    String        ssid;          // Netz des laufenden bzw. letzten Versuchs
    uint8_t       attempt = 0;   // ab 1
    uint8_t       count   = 0;   // Netze in der Folge
};
WifiBootStatus WifiConnect_BootStatus();
