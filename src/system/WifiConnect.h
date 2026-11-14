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
//   * Feste IP (Web-Setup, SharedData staticIp...): DHCP entfaellt (gemessen:
//     IP 1 ms nach dem Anmelden statt 1,5-2,5 s). Ungueltige Angaben werden
//     ignoriert (dann DHCP).
//   * Nicht wirksam, daher entfernt: Kanal-Hinweis (mit und ohne BSSID). Das
//     Anmelden dauerte mit Hinweis exakt so lange wie ohne (3,0 s ab "Station
//     gestartet") - der Kanal-Suchlauf ist nicht der Engpass. Die 3,0 s liegen
//     im Anmelden selbst (Router: WPA2/WPA3-Mischbetrieb) bzw. im WLAN-Chip.
//
// Nach jeder erfolgreichen Verbindung merkt WifiNetworks das Netz (SSID,
// Passwort, Kanal) fuer den naechsten Start und die Auswahl im Web-Setup.
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

// Startet die Verbindung (feste IP, falls eingestellt).
void WifiConnect_Begin(const String& ssid, const String& pass);

// Regelmaessig aufrufen (SystemLogic_Update): Netz merken, erste Verbindung
// protokollieren.
void WifiConnect_Update();

// WLAN wurde abgeschaltet: Ueberwachung beenden.
void WifiConnect_Reset();
