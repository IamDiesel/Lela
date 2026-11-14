// ==============================================================================
// WifiConnect - Implementierung (siehe Header)
// ==============================================================================

#include "WifiConnect.h"
#include "WifiNetworks.h"
#include "SharedData.h"
#include <WiFi.h>

// ------------------------------------------------------------------------------
// Parameter
// ------------------------------------------------------------------------------
// Hoechstens so viele Getrennt-Meldungen je Start (siehe Header).
static const uint8_t MAX_DISCONNECT_LOGS = 10;

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
// Nur loopTask:
static bool     s_active = false;
static bool     s_connected = false;
static bool     s_firstConnectLogged = false;
static bool     s_eventsRegistered = false;
static String   s_ssid;
static String   s_pass;
// Auch aus dem Arduino-Ereignis-Task (32-Bit-Werte, ein Schreiber je Feld):
static volatile uint32_t s_beginMs = 0;
static volatile uint8_t  s_disconnectLogs = 0;

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
namespace {

const char* authName(uint8_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:          return "offen";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3 (SAE)";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default:                      return "anderes";
    }
}

// Feste IP aus dem Web-Setup anwenden. false = ungueltig/aus (dann DHCP).
bool applyStaticIp() {
    if (!staticIpEnabled) return false;
    IPAddress ip, gw, mask, dns;
    if (!ip.fromString(staticIp) || !gw.fromString(staticGateway) || !mask.fromString(staticSubnet) || mask[0] != 255) {
        Serial.println("[WLAN] Feste IP unvollstaendig oder ungueltig - nutze DHCP.");
        return false;
    }
    if (!dns.fromString(staticDns)) dns = gw;   // ueblich: Router ist DNS
    if (!WiFi.config(ip, gw, mask, dns)) {
        Serial.println("[WLAN] Feste IP konnte nicht gesetzt werden - nutze DHCP.");
        return false;
    }
    Serial.printf("[WLAN] Feste IP %s (Gateway %s, DNS %s).\n",
                  ip.toString().c_str(), gw.toString().c_str(), dns.toString().c_str());
    return true;
}

void onWifiEvent(arduino_event_t* event) {
    const unsigned long t = (unsigned long)(millis() - s_beginMs);
    switch (event->event_id) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.printf("[WLAN] +%lu ms: Station gestartet (WLAN-Chip bereit).\n", t);
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.printf("[WLAN] +%lu ms: Am Router angemeldet (Kanal %u, %s).\n", t,
                          (unsigned)event->event_info.wifi_sta_connected.channel,
                          authName((uint8_t)event->event_info.wifi_sta_connected.authmode));
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            const uint8_t reason = event->event_info.wifi_sta_disconnected.reason;
            if (s_disconnectLogs >= MAX_DISCONNECT_LOGS) break;
            s_disconnectLogs = s_disconnectLogs + 1;   // volatile: kein ++
            Serial.printf("[WLAN] +%lu ms: Getrennt/abgelehnt, Grund %u (%s).\n", t, (unsigned)reason,
                          WiFi.disconnectReasonName((wifi_err_reason_t)reason));
            break;
        }
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WLAN] +%lu ms: IP erhalten (%s).\n", t,
                          IPAddress(event->event_info.got_ip.ip_info.ip.addr).toString().c_str());
            break;
        default:
            break;
    }
}

} // namespace

// ------------------------------------------------------------------------------
// Schnittstelle
// ------------------------------------------------------------------------------
void WifiConnect_Begin(const String& ssid, const String& pass) {
    if (!s_eventsRegistered) {
        WiFi.onEvent(onWifiEvent);
        s_eventsRegistered = true;
    }
    s_ssid = ssid;
    s_pass = pass;
    s_active = true;
    s_connected = false;
    s_disconnectLogs = 0;
    s_beginMs = millis();

    applyStaticIp();   // vor begin(): sonst startet DHCP
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("[Boot] WLAN-Start (WiFi.begin) dauerte %lu ms.\n", (unsigned long)(millis() - s_beginMs));
}

void WifiConnect_Update() {
    if (!s_active) return;
    const bool connected = WiFi.status() == WL_CONNECTED;

    if (connected && !s_connected) {
        s_connected = true;
        if (!s_firstConnectLogged) {
            s_firstConnectLogged = true;
            const uint32_t now = millis();
            Serial.printf("[Boot] WLAN verbunden nach %lu ms seit Verbindungsstart (%lu ms seit Einschalten), "
                          "Kanal %d, RSSI %d dBm.\n",
                          (unsigned long)(now - s_beginMs), (unsigned long)now, (int)WiFi.channel(), (int)WiFi.RSSI());
        }
        // Funktionierende Zugangsdaten merken (schreibt nur bei Aenderung).
        WifiNetworks_Remember(s_ssid, s_pass, (uint8_t)WiFi.channel());
        return;
    }
    if (!connected) s_connected = false;
}

void WifiConnect_Reset() {
    s_active = false;
    s_connected = false;
}
