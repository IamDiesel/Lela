// ==============================================================================
// WifiNetworks - Implementierung (siehe Header)
// ==============================================================================

#include "WifiNetworks.h"
#include <Preferences.h>

static const char* const NVS_NAMESPACE = "wifinets";

// Ueberbleibsel eines frueheren Versuchs (Kanal + BSSID gemerkt) - wird beim
// ersten Zugriff einmal geleert.
static const char* const OLD_HINT_NAMESPACE = "wifihint";

// Bereich der allgemeinen Einstellungen: dort lag frueher die eine feste IP
// fuer alle Netze (siehe adoptLegacyIpOnce).
static const char* const MAIN_NAMESPACE = "catmat";

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
namespace {

// Alles, was im NVS-Bereich steht: die Liste und der Vormerk-Platz.
struct Store {
    std::vector<WifiNetwork> list;
    String       pendingSsid;   // leer = nichts vorgemerkt
    WifiIpConfig pendingIp;
};

// NVS-Schluessel je Eintrag: Buchstabe + Index (kurz, NVS erlaubt 15 Zeichen).
//   s = SSID, p = Passwort, c = Kanal,
//   f = feste IP an, a = Adresse, g = Gateway, m = Maske, d = DNS
// Vormerk-Platz: derselbe Buchstabe mit 'x' statt des Index ("sx", "fx", ...).
String key(char kind, size_t i) {
    return String(kind) + String((unsigned)i);
}

String pendingKey(char kind) {
    return String(kind) + "x";
}

// Lesen ohne Fehlermeldung im Log, wenn der Schluessel fehlt (aeltere Eintraege
// haben keine IP-Felder).
String readText(Preferences& p, const String& k) {
    return p.isKey(k.c_str()) ? p.getString(k.c_str(), "") : String();
}

// Leere Texte nicht speichern - save() leert den Bereich vorher ohnehin.
void writeText(Preferences& p, const String& k, const String& value) {
    if (value.length() > 0) p.putString(k.c_str(), value);
}

WifiIpConfig readIp(Preferences& p, const String& kf, const String& ka, const String& kg,
                    const String& km, const String& kd) {
    WifiIpConfig c;
    c.fixed   = p.isKey(kf.c_str()) ? p.getBool(kf.c_str(), false) : false;
    c.ip      = readText(p, ka);
    c.gateway = readText(p, kg);
    c.subnet  = readText(p, km);
    c.dns     = readText(p, kd);
    return c;
}

void writeIp(Preferences& p, const WifiIpConfig& c, const String& kf, const String& ka,
             const String& kg, const String& km, const String& kd) {
    if (c.fixed) p.putBool(kf.c_str(), true);
    writeText(p, ka, c.ip);
    writeText(p, kg, c.gateway);
    writeText(p, km, c.subnet);
    writeText(p, kd, c.dns);
}

bool sameIp(const WifiIpConfig& a, const WifiIpConfig& b) {
    return a.fixed == b.fixed && a.ip == b.ip && a.gateway == b.gateway &&
           a.subnet == b.subnet && a.dns == b.dns;
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

Store load() {
    removeOldHintOnce();
    Store st;
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return st;   // noch nichts gemerkt
    const size_t n = p.getUChar("n", 0);
    for (size_t i = 0; i < n && i < WIFI_NETWORKS_MAX; i++) {
        WifiNetwork w;
        w.ssid    = readText(p, key('s', i));
        w.pass    = readText(p, key('p', i));
        w.channel = p.getUChar(key('c', i).c_str(), 0);
        w.ip      = readIp(p, key('f', i), key('a', i), key('g', i), key('m', i), key('d', i));
        if (w.ssid.length() > 0) st.list.push_back(w);
    }
    st.pendingSsid = readText(p, pendingKey('s'));
    if (st.pendingSsid.length() > 0) {
        st.pendingIp = readIp(p, pendingKey('f'), pendingKey('a'), pendingKey('g'),
                              pendingKey('m'), pendingKey('d'));
    }
    p.end();
    return st;
}

void save(const Store& st) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.clear();   // entfernte Eintraege und geleerte Felder nicht stehen lassen
    const size_t n = st.list.size() < WIFI_NETWORKS_MAX ? st.list.size() : WIFI_NETWORKS_MAX;
    for (size_t i = 0; i < n; i++) {
        const WifiNetwork& w = st.list[i];
        p.putString(key('s', i).c_str(), w.ssid);
        writeText(p, key('p', i), w.pass);
        p.putUChar(key('c', i).c_str(), w.channel);
        writeIp(p, w.ip, key('f', i), key('a', i), key('g', i), key('m', i), key('d', i));
    }
    p.putUChar("n", (uint8_t)n);
    if (st.pendingSsid.length() > 0) {
        p.putString(pendingKey('s').c_str(), st.pendingSsid);
        writeIp(p, st.pendingIp, pendingKey('f'), pendingKey('a'), pendingKey('g'),
                pendingKey('m'), pendingKey('d'));
    }
    p.end();
}

int indexOf(const Store& st, const String& ssid) {
    for (size_t i = 0; i < st.list.size(); i++) {
        if (st.list[i].ssid == ssid) return (int)i;
    }
    return -1;
}

// IP-Einstellung in den Eintrag oder auf den Vormerk-Platz legen.
// Rueckgabe: true, wenn sich dadurch etwas geaendert hat.
bool putIp(Store& st, const String& ssid, const WifiIpConfig& cfg) {
    const int i = indexOf(st, ssid);
    if (i >= 0) {
        if (sameIp(st.list[i].ip, cfg)) return false;
        st.list[i].ip = cfg;
        return true;
    }
    if (st.pendingSsid == ssid && sameIp(st.pendingIp, cfg)) return false;
    st.pendingSsid = ssid;
    st.pendingIp = cfg;
    return true;
}

// Die fruehere feste IP fuer alle Netze (Bereich "catmat") geht einmalig auf
// das Netz ueber, das damals eingestellt war. Danach werden die alten
// Schluessel entfernt - es gibt nur noch die Einstellung je Netz.
void adoptLegacyIpOnce() {
    static bool done = false;
    if (done) return;
    done = true;

    Preferences p;
    if (!p.begin(MAIN_NAMESPACE, true)) return;
    const bool present = p.isKey("ipFix") || p.isKey("ipAddr");
    WifiIpConfig cfg;
    String ssid;
    if (present) {
        cfg  = readIp(p, "ipFix", "ipAddr", "ipGw", "ipMask", "ipDns");
        ssid = readText(p, "wifiSsid");
    }
    p.end();
    if (!present) return;

    if (ssid.length() > 0 && (cfg.fixed || cfg.ip.length() > 0)) {
        Store st = load();
        if (putIp(st, ssid, cfg)) save(st);
        Serial.printf("[WLAN] Bisherige feste IP (%s) dem Netz '%s' zugeordnet.\n",
                      cfg.fixed ? cfg.ip.c_str() : "aus", ssid.c_str());
    }
    if (p.begin(MAIN_NAMESPACE, false)) {
        static const char* const OLD_KEYS[] = { "ipFix", "ipAddr", "ipGw", "ipMask", "ipDns" };
        for (const char* k : OLD_KEYS) {
            if (p.isKey(k)) p.remove(k);
        }
        p.end();
    }
}

Store loadCurrent() {
    adoptLegacyIpOnce();
    return load();
}

} // namespace

// ------------------------------------------------------------------------------
// Schnittstelle
// ------------------------------------------------------------------------------
bool WifiIpConfig_Valid(const WifiIpConfig& cfg) {
    IPAddress ip, gw, mask, dns;
    if (!ip.fromString(cfg.ip) || !gw.fromString(cfg.gateway)) return false;
    if (!mask.fromString(cfg.subnet) || mask[0] != 255) return false;
    return cfg.dns.length() == 0 || dns.fromString(cfg.dns);
}

std::vector<WifiNetwork> WifiNetworks_List() {
    return loadCurrent().list;
}

bool WifiNetworks_Find(const String& ssid, WifiNetwork* out) {
    const Store st = loadCurrent();
    const int i = indexOf(st, ssid);
    if (i < 0) return false;
    if (out) *out = st.list[i];
    return true;
}

void WifiNetworks_Remember(const String& ssid, const String& pass, uint8_t channel) {
    if (ssid.length() == 0) return;
    Store st = loadCurrent();

    const int i = indexOf(st, ssid);
    if (i >= 0) {
        // Bekannt: nur auffrischen, die Stelle in der Liste bleibt.
        WifiNetwork& w = st.list[i];
        if (w.pass == pass && w.channel == channel) return;   // NVS schonen
        w.pass = pass;
        w.channel = channel;
        save(st);
        return;
    }

    // Neu: ans Ende. Ist die Liste voll, weicht das Netz mit der niedrigsten
    // Prioritaet.
    WifiNetwork w;
    w.ssid = ssid;
    w.pass = pass;
    w.channel = channel;
    if (st.pendingSsid == ssid) {
        w.ip = st.pendingIp;
        st.pendingSsid = "";
        st.pendingIp = WifiIpConfig();
    }
    while (st.list.size() >= WIFI_NETWORKS_MAX) {
        Serial.printf("[WLAN] Liste voll - Netz '%s' faellt heraus.\n", st.list.back().ssid.c_str());
        st.list.pop_back();
    }
    st.list.push_back(w);
    save(st);
    Serial.printf("[WLAN] Netz '%s' gemerkt (Kanal %u, Prioritaet %u von %u).\n",
                  ssid.c_str(), (unsigned)channel, (unsigned)st.list.size(), (unsigned)st.list.size());
}

bool WifiNetworks_Move(const String& ssid, int delta) {
    Store st = loadCurrent();
    const int i = indexOf(st, ssid);
    const int j = i + delta;
    if (i < 0 || delta == 0 || j < 0 || j >= (int)st.list.size()) return false;
    const WifiNetwork moved = st.list[i];
    st.list.erase(st.list.begin() + i);
    st.list.insert(st.list.begin() + j, moved);
    save(st);
    Serial.printf("[WLAN] Netz '%s' hat jetzt Prioritaet %d.\n", ssid.c_str(), j + 1);
    return true;
}

bool WifiNetworks_Forget(const String& ssid) {
    Store st = loadCurrent();
    const int i = indexOf(st, ssid);
    if (i < 0) return false;
    st.list.erase(st.list.begin() + i);
    save(st);
    Serial.printf("[WLAN] Netz '%s' vergessen.\n", ssid.c_str());
    return true;
}

void WifiNetworks_Clear() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}

WifiIpConfig WifiNetworks_IpConfigFor(const String& ssid) {
    const Store st = loadCurrent();
    const int i = indexOf(st, ssid);
    if (i >= 0) return st.list[i].ip;
    if (ssid.length() > 0 && st.pendingSsid == ssid) return st.pendingIp;
    return WifiIpConfig();
}

void WifiNetworks_SetIpConfig(const String& ssid, const WifiIpConfig& cfg) {
    if (ssid.length() == 0) return;
    Store st = loadCurrent();
    if (!putIp(st, ssid, cfg)) return;
    save(st);
    Serial.printf("[WLAN] IP-Einstellung fuer '%s': %s%s.\n", ssid.c_str(),
                  cfg.fixed ? "fest " : "DHCP", cfg.fixed ? cfg.ip.c_str() : "");
}
