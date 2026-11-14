// ==============================================================================
// HaTheme - Farben der HA-App im Farbschema (siehe HaTheme.h)
// ==============================================================================
#include "HaTheme.h"
#include "UiTheme.h"
#include <Preferences.h>
#include <math.h>

namespace {

// ------------------------------------------------------------------------------
// Feste Paare {dunkel, hell}
// ------------------------------------------------------------------------------
// Dunkel = gespeicherter/Standardwert (unveraendert wie bisher). Hell = kraeftigerer
// Ton derselben Farbe (Material 700-900 bzw. abgedunkelt), Kontrast gerechnet:
// >= 3:1 auf Weiss und auf der getoenten An-Kachel (12 %). Ausnahmen bewusst:
// Aus-Grau 0x555555 (soll zuruecktreten) und 0x222222 ("fast unsichtbar").
struct Pair { uint32_t dark; uint32_t light; };

constexpr Pair PAIRS[] = {
    // --- Palette der Farbwahl (Reihenfolge = Anzeige) ---
    { 0xF44336, 0xC62828 }, { 0xE91E63, 0xAD1457 }, { 0x9C27B0, 0x7B1FA2 }, { 0x673AB7, 0x512DA8 },
    { 0x3F51B5, 0x303F9F }, { 0x2196F3, 0x1565C0 }, { 0x03A9F4, 0x0277BD }, { 0x00BCD4, 0x00838F },
    { 0x009688, 0x00796B }, { 0x4CAF50, 0x2E7D32 }, { 0x8BC34A, 0x558B2F }, { 0xCDDC39, 0x827717 },
    { 0xFFEB3B, 0x8D7A00 }, { 0xFFC107, 0x9A6B00 }, { 0xFF9800, 0xB35C00 }, { 0xFF5722, 0xD84315 },
    { 0x795548, 0x5D4037 }, { 0x9E9E9E, 0x616161 }, { 0x607D8B, 0x455A64 }, { 0xFFFFFF, 0x15171A },
    { 0xAAAAAA, 0x5B616A }, { 0x555555, 0x8A9099 }, { 0x222222, 0xD5D8DD },
    // --- Standardfarben der Widget-Typen (nicht in der Palette) ---
    { 0xFFD700, 0x8F7400 },   // Licht an (gold)
    { 0x9B59B6, 0x7D3C98 },   // Aktion (violett)
    { 0x00A0FF, 0x0A74C2 },   // Medien / Akzent
    { 0x3498DB, 0x1F6FB2 },   // Sensor, Klima kuehlen, Rollladen
    { 0x1ABC9C, 0x117A65 },   // Sauger
    { 0x888888, 0x6B7280 },   // Sensor aus, Klima aus
    { 0xE74C3C, 0xC0392B },   // Klima heizen, Fehler
    { 0x2ECC71, 0x1E8449 },   // Klima auto, Akku ok
};
constexpr size_t PALETTE_COUNT = 23;   // die ersten 23 Paare

bool s_autoAdapt = true;

// ------------------------------------------------------------------------------
// Rechenhilfen
// ------------------------------------------------------------------------------
float channelLin(uint8_t c8) {
    const float c = c8 / 255.0f;
    return (c <= 0.03928f) ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

float luminance(uint32_t rgb) {
    return 0.2126f * channelLin((rgb >> 16) & 0xFF)
         + 0.7152f * channelLin((rgb >> 8) & 0xFF)
         + 0.0722f * channelLin(rgb & 0xFF);
}

float contrast(uint32_t a, uint32_t b) {
    float la = luminance(a), lb = luminance(b);
    if (la < lb) { const float t = la; la = lb; lb = t; }
    return (la + 0.05f) / (lb + 0.05f);
}

void toHsl(uint32_t rgb, float& h, float& s, float& l) {
    const float r = ((rgb >> 16) & 0xFF) / 255.0f;
    const float g = ((rgb >> 8) & 0xFF) / 255.0f;
    const float b = (rgb & 0xFF) / 255.0f;
    const float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
    l = (mx + mn) / 2.0f;
    if (mx == mn) { h = s = 0.0f; return; }
    const float d = mx - mn;
    s = (l > 0.5f) ? d / (2.0f - mx - mn) : d / (mx + mn);
    if (mx == r)      h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else              h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

float hueToRgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f)     return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

uint32_t fromHsl(float h, float s, float l) {
    float r, g, b;
    if (s == 0.0f) {
        r = g = b = l;
    } else {
        const float q = (l < 0.5f) ? l * (1 + s) : l + s - l * s;
        const float p = 2 * l - q;
        r = hueToRgb(p, q, h + 1.0f / 3);
        g = hueToRgb(p, q, h);
        b = hueToRgb(p, q, h - 1.0f / 3);
    }
    auto to8 = [](float v) { return (uint32_t)lroundf(fminf(fmaxf(v, 0.0f), 1.0f) * 255.0f); };
    return (to8(r) << 16) | (to8(g) << 8) | to8(b);
}

// Berechnete Gegenfarbe fuer Farben ohne festes Paar.
uint32_t computeLight(uint32_t dark) {
    float h, s, l;
    toHsl(dark, h, s, l);
    if (s < 0.15f) {
        // Grauton: spiegeln (hell <-> dunkel), Extreme vermeiden.
        return fromHsl(h, s, fminf(0.92f, fmaxf(0.08f, 1.0f - l)));
    }
    // Bunt: gleicher Farbton, abdunkeln bis der Kontrast auf Weiss reicht
    // (3,2 statt 3,0: Reserve fuer die getoente Kachel).
    uint32_t c = dark;
    while (l > 0.05f) {
        c = fromHsl(h, s, l);
        if (contrast(c, 0xFFFFFF) >= 3.2f) break;
        l -= 0.01f;
    }
    return c;
}

} // namespace

namespace HaTheme {

void init() {
    Preferences p;
    p.begin("catmat", true);
    s_autoAdapt = p.getBool("haAutoCol", true);
    p.end();
}

bool autoAdapt() { return s_autoAdapt; }

void setAutoAdapt(bool on) {
    if (on == s_autoAdapt) return;
    s_autoAdapt = on;
    Preferences p;
    p.begin("catmat", false);
    p.putBool("haAutoCol", on);
    p.end();
}

uint32_t lightOf(uint32_t darkHex) {
    darkHex &= 0xFFFFFF;
    for (const Pair& pr : PAIRS) {
        if (pr.dark == darkHex) return pr.light;
    }
    return computeLight(darkHex);
}

uint32_t themed(uint32_t darkHex) {
    return UiTheme::isDark() ? (darkHex & 0xFFFFFF) : lightOf(darkHex);
}

uint32_t user(uint32_t darkHex, bool widgetAdapt) {
    return (s_autoAdapt && widgetAdapt) ? themed(darkHex) : (darkHex & 0xFFFFFF);
}

lv_color_t card()       { return UiTheme::c().surface; }
lv_color_t cardOff()    { return UiTheme::isDark() ? lv_color_hex(0x141518) : UiTheme::c().surface2; }
lv_color_t cardBorder() { return UiTheme::isDark() ? lv_color_hex(0x33363C) : lv_color_hex(0xD5D8DD); }

lv_color_t cardOn(uint32_t accent) {
    if (UiTheme::isDark()) return UiTheme::c().surface2;
    // Hell: Weiss mit 12 % Widget-Farbe (lv_color_mix: Anteil der ERSTEN Farbe).
    return lv_color_mix(lv_color_hex(accent), UiTheme::c().surface, 31);
}

lv_color_t control() { return UiTheme::c().surface2; }

bool isNeutral(uint32_t rgb) {
    float h, s, l;
    toHsl(rgb & 0xFFFFFF, h, s, l);
    return s < 0.15f;
}

lv_color_t onColor(uint32_t bgRgb) {
    // Die Schrift mit dem hoeheren Kontrast gewinnt.
    return contrast(bgRgb & 0xFFFFFF, 0xFFFFFF) >= contrast(bgRgb & 0xFFFFFF, 0x15171A)
         ? lv_color_white() : lv_color_hex(0x15171A);
}

lv_color_t track() {
    return UiTheme::isDark() ? lv_color_hex(0x3A3D44) : lv_color_hex(0xD5D8DD);
}

const uint32_t* palette(size_t* count) {
    // Die Dunkel-Werte der ersten PALETTE_COUNT Paare, einmalig herausgezogen.
    static uint32_t s_pal[PALETTE_COUNT];
    static bool s_init = false;
    if (!s_init) {
        for (size_t i = 0; i < PALETTE_COUNT; i++) s_pal[i] = PAIRS[i].dark;
        s_init = true;
    }
    if (count) *count = PALETTE_COUNT;
    return s_pal;
}

} // namespace HaTheme
