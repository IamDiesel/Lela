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
static bool     s_staticIp = false;       // laufende Verbindung nutzt eine feste IP
static String   s_ssid;
static String   s_pass;
static WifiBootSequence s_boot;           // Startfolge (siehe Header)
// Auch aus dem Arduino-Ereignis-Task (32-Bit-Werte, ein Schreiber je Feld):
static volatile uint32_t s_beginMs = 0;
static volatile uint8_t  s_disconnectLogs = 0;
// Fehlschlag-Meldungen des WLAN-Chips seit WifiConnect_Begin. Geschrieben vom
// Ereignis-Task (zaehlen) und vom loopTask (nullen bei Begin); gelesen im
// loopTask. Ein verlorener Zaehlschritt waere nur ein spaeterer Netzwechsel.
static volatile uint8_t  s_failEvents = 0;

// Meldungen in den ersten Millisekunden nach Begin gehoeren noch zum
// vorherigen Netz (WiFi.disconnect beim Wechsel) und zaehlen nicht.
static const uint32_t FAIL_EVENT_GUARD_MS = 500;

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

// Zurueck zu DHCP. Nur noetig, wenn die vorherige Anmeldung eine feste IP
// gesetzt hatte - sonst bliebe sie fuer das naechste Netz stehen.
void useDhcp() {
    if (!s_staticIp) return;
    const IPAddress none((uint32_t)0);
    WiFi.config(none, none, none);
    s_staticIp = false;
}

// IP-Einstellung des Netzes anwenden (WifiNetworks). false = DHCP (nichts
// eingestellt, ungueltig oder nicht setzbar).
bool applyIpConfig(const String& ssid) {
    const WifiIpConfig cfg = WifiNetworks_IpConfigFor(ssid);
    if (!cfg.fixed) { useDhcp(); return false; }
    if (!WifiIpConfig_Valid(cfg)) {
        Serial.printf("[WLAN] Feste IP fuer '%s' unvollstaendig oder ungueltig - nutze DHCP.\n", ssid.c_str());
        useDhcp();
        return false;
    }
    IPAddress ip, gw, mask, dns;
    ip.fromString(cfg.ip);
    gw.fromString(cfg.gateway);
    mask.fromString(cfg.subnet);
    if (!dns.fromString(cfg.dns)) dns = gw;   // ueblich: Router ist DNS
    if (!WiFi.config(ip, gw, mask, dns)) {
        Serial.println("[WLAN] Feste IP konnte nicht gesetzt werden - nutze DHCP.");
        useDhcp();
        return false;
    }
    s_staticIp = true;
    Serial.printf("[WLAN] Feste IP %s (Gateway %s, DNS %s).\n",
                  ip.toString().c_str(), gw.toString().c_str(), dns.toString().c_str());
    return true;
}

// Netz als "aktuelles" speichern: damit beginnt der naechste Start.
void storeCurrent(const String& ssid, const String& pass) {
    wifiSsid = ssid;
    wifiPass = pass;
    preferences.begin("catmat", false);
    preferences.putString("wifiSsid", wifiSsid);
    preferences.putString("wifiPass", wifiPass);
    preferences.end();
}

// Funkadresse (BSSID) des Zugangspunkts als Text. Im Ereignis-Log steht sie
// bei An- und Abmeldung: Bedienen mehrere Geraete dasselbe Netz (Router und
// Mesh-Verstaerker), zeigt erst sie, WELCHES davon einen Versuch abgewiesen
// hat und welches ihn danach annahm.
struct BssidText { char text[18]; };
BssidText bssidText(const uint8_t* b) {
    BssidText out;
    snprintf(out.text, sizeof(out.text), "%02X:%02X:%02X:%02X:%02X:%02X",
             b[0], b[1], b[2], b[3], b[4], b[5]);
    return out;
}

void onWifiEvent(arduino_event_t* event) {
    const unsigned long t = (unsigned long)(millis() - s_beginMs);
    switch (event->event_id) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.printf("[WLAN] +%lu ms: Station gestartet (WLAN-Chip bereit).\n", t);
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.printf("[WLAN] +%lu ms: Am Router angemeldet (Kanal %u, %s, Zugangspunkt %s).\n", t,
                          (unsigned)event->event_info.wifi_sta_connected.channel,
                          authName((uint8_t)event->event_info.wifi_sta_connected.authmode),
                          bssidText(event->event_info.wifi_sta_connected.bssid).text);
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            const uint8_t reason = event->event_info.wifi_sta_disconnected.reason;
            // Fuer die Startfolge zaehlen: Netz nicht gefunden, Anmeldung
            // abgelehnt usw. Nicht das eigene Trennen (ASSOC_LEAVE) und nichts
            // aus den ersten Millisekunden nach einem Netzwechsel.
            if (reason != WIFI_REASON_ASSOC_LEAVE && t > FAIL_EVENT_GUARD_MS && s_failEvents < 255) {
                s_failEvents = s_failEvents + 1;   // volatile: kein ++
            }
            if (s_disconnectLogs >= MAX_DISCONNECT_LOGS) break;
            s_disconnectLogs = s_disconnectLogs + 1;   // volatile: kein ++
            Serial.printf("[WLAN] +%lu ms: Getrennt/abgelehnt, Grund %u (%s), Zugangspunkt %s, %d dBm.\n",
                          t, (unsigned)reason, WiFi.disconnectReasonName((wifi_err_reason_t)reason),
                          bssidText(event->event_info.wifi_sta_disconnected.bssid).text,
                          (int)event->event_info.wifi_sta_disconnected.rssi);
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

// ------------------------------------------------------------------------------
// Startfolge: Entscheidung von WifiBootSequence ausfuehren
// ------------------------------------------------------------------------------
// Anderes Netz anmelden, ohne den WLAN-Chip abzuschalten (das kostete ~3 s
// Hochfahren).
void switchNetwork(const WifiCandidate& c) {
    WiFi.disconnect(false, false);
    WifiConnect_Begin(c.ssid, c.pass);
}

void bootApply(WifiBootStep step) {
    switch (step) {
        case WifiBootStep::Stay:
            break;

        case WifiBootStep::Next:
            Serial.printf("[WLAN] Startfolge: '%s' nicht erreichbar - versuche '%s' (%u/%u).\n",
                          s_ssid.c_str(), s_boot.current().ssid.c_str(),
                          (unsigned)s_boot.attempt(), (unsigned)s_boot.count());
            switchNetwork(s_boot.current());
            break;

        case WifiBootStep::Connected:
            // Ein anderes als das bisherige Netz hat geklappt: Es ist ab jetzt
            // das aktuelle, der naechste Start beginnt dort.
            if (s_ssid != wifiSsid) {
                Serial.printf("[WLAN] Startfolge: verbunden mit '%s' - ab jetzt das aktuelle Netz.\n", s_ssid.c_str());
                storeCurrent(s_ssid, s_pass);
            }
            break;

        case WifiBootStep::GaveUp:
            Serial.printf("[WLAN] Startfolge beendet (%s) - '%s' wird im Hintergrund weiter versucht.\n",
                          s_boot.state() == WifiBootState::Aborted ? "abgebrochen" : "kein Netz erreichbar",
                          s_boot.home().ssid.c_str());
            if (s_ssid != s_boot.home().ssid) switchNetwork(s_boot.home());
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
    s_failEvents = 0;
    s_beginMs = millis();

    applyIpConfig(ssid);   // vor begin(): sonst startet DHCP
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("[Boot] WLAN-Start (WiFi.begin) dauerte %lu ms.\n", (unsigned long)(millis() - s_beginMs));
}

void WifiConnect_Update() {
    if (!s_active) return;
    const bool connected = WiFi.status() == WL_CONNECTED;

    // Startfolge: weiter warten, naechstes Netz oder Schluss. Ein Netzwechsel
    // ruft WifiConnect_Begin - danach gilt der Stand dieses Durchlaufs nicht mehr.
    if (s_boot.state() == WifiBootState::Trying) {
        const WifiBootStep step = s_boot.step(connected, s_failEvents, millis() - s_beginMs);
        bootApply(step);
        if (step == WifiBootStep::Next || step == WifiBootStep::GaveUp) return;
    }

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
    s_boot.abort();   // WLAN abgeschaltet: eine laufende Startfolge endet (ohne Netzwechsel)
    s_active = false;
    s_connected = false;
}

bool WifiConnect_IsStaticIp() {
    return s_staticIp;
}

bool WifiConnect_BootStart() {
    if (!s_boot.start(wifiSsid, wifiPass, WifiNetworks_List())) return false;
    Serial.printf("[WLAN] Startfolge: %u Netz(e), beginne mit '%s'.\n",
                  (unsigned)s_boot.count(), s_boot.current().ssid.c_str());
    WifiConnect_Begin(s_boot.current().ssid, s_boot.current().pass);
    return true;
}

void WifiConnect_BootSkip() {
    bootApply(s_boot.skip());
}

void WifiConnect_BootAbort() {
    bootApply(s_boot.abort());
}

WifiBootStatus WifiConnect_BootStatus() {
    WifiBootStatus st;
    st.state = s_boot.state();
    st.count = s_boot.count();
    if (st.count > 0) {
        st.ssid = s_boot.current().ssid;
        st.attempt = s_boot.attempt();
    }
    return st;
}

void WifiConnect_SwitchTo(const String& ssid, const String& pass) {
    storeCurrent(ssid, pass);
    Serial.printf("[WLAN] Wechsel zu '%s'.\n", ssid.c_str());

    // WLAN aus oder noch nie gestartet: SystemLogic startet spaeter mit den
    // neuen Daten. Sonst sofort neu anmelden - ohne den WLAN-Chip abzuschalten
    // (das kostete ~3 s Hochfahren, siehe oben).
    if (!wifiEnabled || !wifiStarted) return;
    WiFi.disconnect(false, false);
    WifiConnect_Begin(ssid, pass);
}
