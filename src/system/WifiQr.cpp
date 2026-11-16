// ==============================================================================
// WifiQr - Implementierung (siehe WifiQr.h)
// ==============================================================================
#include "WifiQr.h"

namespace {

// Liest einen Wert ab pos bis zum naechsten ungeschuetzten ';' und entfernt
// die Schutzzeichen. pos steht danach hinter dem ';'.
String readValue(const String& s, size_t& pos) {
    String v;
    while (pos < s.length()) {
        const char c = s[pos++];
        if (c == '\\' && pos < s.length()) { v += s[pos++]; continue; }
        if (c == ';') break;
        v += c;
    }
    return v;
}

String unquote(const String& v) {
    if (v.length() >= 2 && v[0] == '"' && v[v.length() - 1] == '"') {
        return v.substring(1, v.length() - 1);
    }
    return v;
}

} // namespace

bool WifiQr_Parse(const String& text, WifiQrData& out) {
    out = WifiQrData();
    if (text.length() < 5 || !text.substring(0, 5).equalsIgnoreCase("WIFI:")) return false;

    size_t pos = 5;
    while (pos < text.length()) {
        const int colon = text.indexOf(':', pos);
        if (colon < 0) break;                       // Ende (";;") oder kaputt
        String key = text.substring(pos, colon);
        key.toUpperCase();
        pos = colon + 1;
        const String value = readValue(text, pos);

        if (key == "S")      out.ssid     = unquote(value);
        else if (key == "P") out.password = unquote(value);
        else if (key == "T") out.security = value;
        else if (key == "H") out.hidden   = value.equalsIgnoreCase("true");
    }

    if (out.security.equalsIgnoreCase("nopass")) out.password = "";
    return out.ssid.length() > 0;
}
