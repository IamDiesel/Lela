#pragma once
// ==============================================================================
// WifiQr - WLAN-Zugangsdaten aus einem QR-Code lesen
// ==============================================================================
// Format (ZXing "WIFI", von Android und iOS unter "WLAN teilen" erzeugt):
//
//   WIFI:T:<WPA|SAE|WEP|nopass>;S:<Name>;P:<Passwort>;H:<true|false>;;
//
// Reihenfolge der Felder beliebig. In Werten sind \ ; , : und " mit einem
// vorangestellten \ geschuetzt. Manche Erzeuger setzen Name/Passwort in
// Anfuehrungszeichen - die werden entfernt. Unbekannte Felder (z.B. fuer
// WPA2-Enterprise) werden ignoriert.
// ==============================================================================

#include <Arduino.h>

struct WifiQrData {
    String ssid;
    String password;   // leer bei "nopass"
    String security;   // "WPA", "SAE", "WEP", "nopass" oder leer
    bool   hidden = false;
};

// true, wenn text ein WLAN-Code mit Namen ist.
bool WifiQr_Parse(const String& text, WifiQrData& out);
