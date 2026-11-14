// ==============================================================================
// UiTheme - Implementierung (siehe Header)
// ==============================================================================

#include "UiTheme.h"
#include "SharedData.h"
#include "GuiManager.h"
#include <Preferences.h>
#include <time.h>

// Automatisch: hell von 07:00 bis 19:59, sonst dunkel.
static const int AUTO_LIGHT_FROM_H = 7;
static const int AUTO_LIGHT_TO_H   = 20;

static UiThemeMode s_mode = UiThemeMode::Dark;
static bool        s_dark = true;
static UiColors    s_c;

static lv_color_t hex(uint32_t v) { return lv_color_hex(v); }

static void fillColors(bool dark) {
    UiColors& c = s_c;
    if (dark) {
        c.bg = hex(0x0E0F11);  c.side = hex(0x141518);  c.surface = hex(0x1C1E22);
        c.surface2 = hex(0x2A2D33);  c.divider = hex(0x26292E);  c.navActive = hex(0x232E3A);
        c.text = hex(0xF2F3F5);  c.muted = hex(0x9EA3AB);  c.link = hex(0x4DB8FF);
        c.accent = hex(0x00A0FF);  c.primary = hex(0x0A74C2);  c.onPrimary = hex(0xFFFFFF);
        c.switchOff = hex(0x3F434A);  c.off = hex(0x4A4E56);
        c.ok = hex(0x3DD68C);  c.okBg = hex(0x16301F);
        c.warn = hex(0xF5A524);  c.warnBg = hex(0x3A2A10);
        c.danger = hex(0xE5484D);  c.dangerBg = hex(0x3A1D1F);
        c.overlay = hex(0x222222);  c.overlayBorder = hex(0x555555);  c.overlayOpa = 160;
        // Topbar exakt wie bisher (schwarz, halbtransparent, bisherige Statusfarben).
        c.bar = hex(0x000000);  c.barOpa = 150;
        c.barText = hex(0xFFFFFF);  c.barMuted = hex(0xAAAAAA);  c.barIdle = hex(0x555555);
        c.barOk = hex(0x00FF00);  c.barWarn = hex(0xFF8800);  c.barErr = hex(0xFF0000);
    } else {
        c.bg = hex(0xF2F3F5);  c.side = hex(0xFAFAFB);  c.surface = hex(0xFFFFFF);
        c.surface2 = hex(0xECEEF1);  c.divider = hex(0xE1E4E8);  c.navActive = hex(0xE3EDF7);
        c.text = hex(0x15171A);  c.muted = hex(0x5B616A);  c.link = hex(0x0A6AB0);
        c.accent = hex(0x0A74C2);  c.primary = hex(0x0A74C2);  c.onPrimary = hex(0xFFFFFF);
        c.switchOff = hex(0xC3C8CF);  c.off = hex(0x9AA0A8);
        c.ok = hex(0x1E7B46);  c.okBg = hex(0xDDF3E5);
        c.warn = hex(0xA35A00);  c.warnBg = hex(0xFDEBD3);
        c.danger = hex(0xC62828);  c.dangerBg = hex(0xFCE4E4);
        c.overlay = hex(0xFFFFFF);  c.overlayBorder = hex(0xD5D8DD);  c.overlayOpa = 215;
        // Auf hellem Grund waere die schwarze Halbtransparenz ein graues Band.
        c.bar = hex(0xFFFFFF);  c.barOpa = LV_OPA_COVER;
        c.barText = hex(0x15171A);  c.barMuted = hex(0x5B616A);  c.barIdle = hex(0x9AA0A8);
        c.barOk = hex(0x1E7B46);  c.barWarn = hex(0xA35A00);  c.barErr = hex(0xC62828);
    }
}

// Wirksames Schema des Modus. Automatisch ohne gestellte Uhr: dunkel - die
// sichere Wahl fuer ein Geraet, das auch nachts im Kinderzimmer steht.
static bool effectiveDark(UiThemeMode mode) {
    if (mode == UiThemeMode::Dark)  return true;
    if (mode == UiThemeMode::Light) return false;
    if (!timeSynced) return true;
    struct tm t;
    if (!getLocalTime(&t, 0)) return true;
    return !(t.tm_hour >= AUTO_LIGHT_FROM_H && t.tm_hour < AUTO_LIGHT_TO_H);
}

static void apply(bool dark) {
    s_dark = dark;
    isDarkMode = dark;   // Uebergang: noch nicht umgestellte Ansichten lesen das
    fillColors(dark);
}

namespace UiTheme {

void init() {
    Preferences p;
    p.begin("catmat", false);
    if (p.isKey("theme")) {
        s_mode = (UiThemeMode)p.getUChar("theme", 0);
    } else {
        // Einmalige Uebernahme des alten Schalters "Dark Mode".
        const bool oldDark = p.isKey("dark") ? p.getBool("dark", true) : true;
        s_mode = oldDark ? UiThemeMode::Dark : UiThemeMode::Light;
        p.putUChar("theme", (uint8_t)s_mode);
        if (p.isKey("dark")) p.remove("dark");
    }
    p.end();
    if ((uint8_t)s_mode > (uint8_t)UiThemeMode::Auto) s_mode = UiThemeMode::Dark;
    apply(effectiveDark(s_mode));
}

UiThemeMode mode() { return s_mode; }
bool isDark()      { return s_dark; }
const UiColors& c() { return s_c; }

void setMode(UiThemeMode m) {
    if (m == s_mode) return;
    s_mode = m;
    Preferences p;
    p.begin("catmat", false);
    p.putUChar("theme", (uint8_t)m);
    p.end();

    const bool dark = effectiveDark(m);
    if (dark == s_dark) return;
    apply(dark);
    // Aufgeschoben: Wir stecken im Event-Callback eines Objekts der Ansicht,
    // die jetzt ersetzt wird.
    lv_async_call([](void*) { gui.reloadCurrentScreen(); }, nullptr);
}

void update() {
    if (s_mode != UiThemeMode::Auto) return;
    static uint32_t last = 0;
    if (last != 0 && millis() - last < 60000) return;
    last = millis();

    const bool dark = effectiveDark(s_mode);
    if (dark == s_dark) return;
    apply(dark);
    Serial.printf("[Theme] Automatisch: %s\n", dark ? "dunkel" : "hell");
    gui.reloadCurrentScreen();
}

}  // namespace UiTheme
