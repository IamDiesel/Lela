// ==============================================================================
// WifiNetworks - Implementierung (siehe Header)
// ==============================================================================

#include "WifiNetworks.h"
#include <Preferences.h>

static const char* const NVS_NAMESPACE = "wifinets";

// Ueberbleibsel eines frueheren Versuchs (Kanal + BSSID gemerkt) - wird beim
// ersten Zugriff einmal geleert.
static const char* const OLD_HINT_NAMESPACE = "wifihint";

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
namespace {

// NVS-Schluessel je Eintrag: s0/p0/c0 ... (kurz, NVS erlaubt 15 Zeichen).
String key(char kind, size_t i) {
    return String(kind) + String((unsigned)i);
}

void removeOldHintOnce() {
    static bool done = false;
    if (done) return;
    done = true;
    Preferences p;
    if (!p.begin(OLD_HINT_NAMESPACE, true)) return;   // gibt es nicht
    const bool hasData = p.isKey("ssid");
    p.end();
    if (hasData && p.begin(OLD_HINT_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}

std::vector<WifiNetwork> load() {
    removeOldHintOnce();
    std::vector<WifiNetwork> list;
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return list;   // noch nichts gemerkt
    const size_t n = p.getUChar("n", 0);
    for (size_t i = 0; i < n && i < WIFI_NETWORKS_MAX; i++) {
        WifiNetwork w;
        w.ssid = p.getString(key('s', i).c_str(), "");
        w.pass = p.getString(key('p', i).c_str(), "");
        w.channel = p.getUChar(key('c', i).c_str(), 0);
        if (w.ssid.length() > 0) list.push_back(w);
    }
    p.end();
    return list;
}

void save(const std::vector<WifiNetwork>& list) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.clear();   // entfernte Eintraege nicht stehen lassen
    const size_t n = list.size() < WIFI_NETWORKS_MAX ? list.size() : WIFI_NETWORKS_MAX;
    for (size_t i = 0; i < n; i++) {
        p.putString(key('s', i).c_str(), list[i].ssid);
        p.putString(key('p', i).c_str(), list[i].pass);
        p.putUChar(key('c', i).c_str(), list[i].channel);
    }
    p.putUChar("n", (uint8_t)n);
    p.end();
}

} // namespace

// ------------------------------------------------------------------------------
// Schnittstelle
// ------------------------------------------------------------------------------
std::vector<WifiNetwork> WifiNetworks_List() {
    return load();
}

void WifiNetworks_Remember(const String& ssid, const String& pass, uint8_t channel) {
    if (ssid.length() == 0) return;
    std::vector<WifiNetwork> list = load();

    // Schon so gemerkt und vorn: nichts zu schreiben (NVS schonen).
    if (!list.empty() && list[0].ssid == ssid && list[0].pass == pass && list[0].channel == channel) return;

    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].ssid == ssid) {
            list.erase(list.begin() + i);
            break;
        }
    }
    WifiNetwork w;
    w.ssid = ssid;
    w.pass = pass;
    w.channel = channel;
    list.insert(list.begin(), w);
    if (list.size() > WIFI_NETWORKS_MAX) list.resize(WIFI_NETWORKS_MAX);
    save(list);
    Serial.printf("[WLAN] Netz '%s' gemerkt (Kanal %u, %u Netze gespeichert).\n",
                  ssid.c_str(), (unsigned)channel, (unsigned)list.size());
}

bool WifiNetworks_Forget(const String& ssid) {
    std::vector<WifiNetwork> list = load();
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].ssid == ssid) {
            list.erase(list.begin() + i);
            save(list);
            Serial.printf("[WLAN] Netz '%s' vergessen.\n", ssid.c_str());
            return true;
        }
    }
    return false;
}

void WifiNetworks_Clear() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}
