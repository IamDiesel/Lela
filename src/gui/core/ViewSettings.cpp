// ==============================================================================
// ViewSettings - Einstellungen: Rahmen und Navigation
// ==============================================================================
// Aufbau (Entwurf "Lela Settings Redesign"):
//
//   +-------------------------- Topbar (ViewTopbar, Farben aus UiTheme) --------+
//   | <- Einstellungen      |  Titel                                            |
//   | [#] Katzenmatte       |  Untertitel                                       |
//   |     Matte verbunden   |  ABSCHNITT                                        |
//   | [#] Babyphone         |  +--------------------------------------------+  |
//   | [#] Verbindungen      |  | Zeile                         [Bedienung]  |  |
//   | [#] Anzeige           |  +--------------------------------------------+  |
//   | [#] System            |                                                   |
//   +-----------------------+---------------------------------------------------+
//
// Die Seitenleiste waehlt einen Bereich; Bearbeiten geschieht auf Unterseiten
// in derselben Inhaltsflaeche (statt Popups). Nur die sichtbare Seite existiert
// als LVGL-Objekte - das haelt jeden Aufbau kurz (ein voller Neuaufbau kostet
// auf dem Tab5 0,3-0,5 s, waehrenddessen gehen Beruehrungen verloren).
//
// Die Seiten selbst stehen in SettingsPage*.cpp (siehe SettingsPages.h), die
// Bausteine in SettingsUi, die Farben in UiTheme.
// ==============================================================================

#include "ViewSettings.h"
#include "SettingsPages.h"
#include "SettingsUi.h"
#include "UiTheme.h"
#include "GuiManager.h"
#include "ViewTopbar.h"
#include "SharedData.h"
#include "MdiMapper.h"
#include "BabyService.h"
#include <WiFi.h>

// ------------------------------------------------------------------------------
// Bereiche der Seitenleiste
// ------------------------------------------------------------------------------
struct AreaDef {
    SettingsPage page;
    const char*  label;
    const char*  icon;      // MDI-Glyphe aus lela_icons
    uint32_t     tile;      // Kachelfarbe (UiTile)
};

static const AreaDef AREAS[SETTINGS_AREA_COUNT] = {
    { SettingsPage::Cat,     "Katzenmatte",  MY_ICON_CAT,            UiTile::kCat     },
    { SettingsPage::Baby,    "Babyphone",    MY_ICON_BABY_CARRIAGE,  UiTile::kBaby    },
    { SettingsPage::Net,     "Verbindungen", MY_ICON_WIFI,           UiTile::kNet     },
    { SettingsPage::Display, "Anzeige",      MY_ICON_WEATHER_NIGHT,  UiTile::kDisplay },
    { SettingsPage::System,  "System",       MY_ICON_COG,            UiTile::kSystem  },
};

struct NavItem {
    lv_obj_t* btn    = nullptr;
    lv_obj_t* dot    = nullptr;
    lv_obj_t* status = nullptr;
    uint8_t   level  = 0xFF;   // zuletzt gezeigter Statuspegel (nur bei Aenderung umfaerben)
};

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
static lv_obj_t*    s_screen  = nullptr;
static lv_obj_t*    s_content = nullptr;
static NavItem      s_nav[SETTINGS_AREA_COUNT];
// Bleibt ueber Besuche erhalten: man landet wieder im zuletzt offenen Bereich.
static SettingsPage s_current = SettingsPage::Cat;
static SettingsPage s_pending = SettingsPage::Cat;
static bool         s_navPending = false;

static SettingsPage areaOf(SettingsPage p) {
    switch (p) {
        case SettingsPage::Camera:
        case SettingsPage::Audio:        return SettingsPage::Baby;
        case SettingsPage::Wifi:
        case SettingsPage::WifiEdit:
        case SettingsPage::WifiNew:
        case SettingsPage::WebSetup:     return SettingsPage::Net;
        case SettingsPage::BleScanMat:
        case SettingsPage::BleScanKippy: return SettingsPage::Cat;
        case SettingsPage::Diag:
        case SettingsPage::Screenshot:   return SettingsPage::System;
        default:                         return p;
    }
}

// Wohin "Zurueck" fuehrt: zum Bereich - ausser bei einer Unterseite, die selbst
// von einer Unterseite aus geoeffnet wird.
static SettingsPage parentOf(SettingsPage p) {
    if (p == SettingsPage::WifiEdit || p == SettingsPage::WifiNew) return SettingsPage::Wifi;
    return areaOf(p);
}

// ------------------------------------------------------------------------------
// Seiten
// ------------------------------------------------------------------------------
static void leavePage(SettingsPage p) {
    if (p == SettingsPage::BleScanMat || p == SettingsPage::BleScanKippy) SettingsBleScan_Leave();
    if (p == SettingsPage::WebSetup) SettingsWebSetup_Leave();
}

static void buildPage(SettingsPage p) {
    switch (p) {
        case SettingsPage::Cat:          SettingsCat_Build(s_content);            break;
        case SettingsPage::Baby:         SettingsBaby_Build(s_content);           break;
        case SettingsPage::Net:          SettingsNet_Build(s_content);            break;
        case SettingsPage::Display:      SettingsDisplay_Build(s_content);        break;
        case SettingsPage::System:       SettingsSystem_Build(s_content);         break;
        case SettingsPage::Camera:       SettingsCamera_Build(s_content);         break;
        case SettingsPage::Audio:        SettingsAudio_Build(s_content);          break;
        case SettingsPage::Wifi:         SettingsWifi_Build(s_content);           break;
        case SettingsPage::WifiEdit:     SettingsWifiEdit_Build(s_content);       break;
        case SettingsPage::WifiNew:      SettingsWifiNew_Build(s_content);        break;
        case SettingsPage::WebSetup:     SettingsWebSetup_Build(s_content);       break;
        case SettingsPage::BleScanMat:   SettingsBleScan_Build(s_content, false); break;
        case SettingsPage::BleScanKippy: SettingsBleScan_Build(s_content, true);  break;
        case SettingsPage::Diag:         SettingsDiag_Build(s_content);           break;
        case SettingsPage::Screenshot:   SettingsScreenshot_Build(s_content);     break;
    }
}

static void updatePage(SettingsPage p) {
    switch (p) {
        case SettingsPage::Cat:          SettingsCat_Update();        break;
        case SettingsPage::Baby:         SettingsBaby_Update();       break;
        case SettingsPage::Net:          SettingsNet_Update();        break;
        case SettingsPage::System:       SettingsSystem_Update();     break;
        case SettingsPage::Camera:       SettingsCamera_Update();     break;
        case SettingsPage::WifiNew:      SettingsWifiNew_Update();    break;
        case SettingsPage::WebSetup:     SettingsWebSetup_Update();   break;
        case SettingsPage::BleScanMat:
        case SettingsPage::BleScanKippy: SettingsBleScan_Update();    break;
        case SettingsPage::Diag:         SettingsDiag_Update();       break;
        case SettingsPage::Screenshot:   SettingsScreenshot_Update(); break;
        default: break;
    }
}

static void highlightNav(SettingsPage area) {
    const UiColors& c = UiTheme::c();
    for (uint8_t i = 0; i < SETTINGS_AREA_COUNT; i++) {
        if (!s_nav[i].btn) continue;
        const bool on = AREAS[i].page == area;
        lv_obj_set_style_bg_color(s_nav[i].btn, c.navActive, 0);
        lv_obj_set_style_bg_opa(s_nav[i].btn, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void showPage(SettingsPage p) {
    if (p != s_current) leavePage(s_current);
    s_current = p;
    highlightNav(areaOf(p));
    buildPage(p);
}

static void doNavigate(void*) {
    s_navPending = false;
    if (gui.getCurrentScreen() != SCREEN_SETTINGS || !s_content) return;
    showPage(s_pending);
}

static void navigateNow(SettingsPage page) {
    s_pending = page;
    if (s_navPending) return;   // ein ausstehender Wechsel nimmt das neue Ziel mit
    s_navPending = true;
    lv_async_call(doNavigate, nullptr);
}

// ------------------------------------------------------------------------------
// Ungespeicherte Aenderungen
// ------------------------------------------------------------------------------
// Seiten mit "Uebernehmen" verlieren ihre Eingaben beim Verlassen. Wurde dort
// etwas geaendert, fragt jeder Weg nach, der die Seite verlaesst, ohne zu
// speichern: Seitenleiste, Zurueck-Link, Verweis auf eine andere Seite, Kopf
// "Einstellungen" und die Wischgeste zum Dashboard. Nicht gefragt wird, wenn
// das Geraet selbst den Bildschirm wechselt (Alarm) - dort darf nichts warten.
static void askDiscard(void (*onDiscard)(void*), void* userData) {
    SettingsUi::confirm("Aenderungen verwerfen?",
                        "Die Eingaben auf dieser Seite sind noch nicht uebernommen. "
                        "Beim Verlassen gehen sie verloren.",
                        "Weiter bearbeiten", "Verwerfen", SettingsUi::Btn::Danger,
                        onDiscard, userData);
}

static void discardAndNavigate(void* target) {
    SettingsUi::clearDirty();
    navigateNow((SettingsPage)(uintptr_t)target);
}

static void discardAndLeave(void*) {
    SettingsUi::clearDirty();
    gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE);
}

void Settings_Navigate(SettingsPage page) {
    if (SettingsUi::hasUnsavedChanges()) {
        askDiscard(discardAndNavigate, (void*)(uintptr_t)page);
        return;
    }
    navigateNow(page);
}

void Settings_Back() {
    SettingsUi::clearDirty();
    navigateNow(parentOf(s_current));
}

void Settings_BackCb(lv_event_t*) {
    playToneI2S(600, 60, true);
    Settings_Navigate(parentOf(s_current));
}

void Settings_DiscardCb(lv_event_t*) {
    playToneI2S(600, 60, true);
    Settings_Back();
}

void ViewSettings::requestLeave() {
    if (SettingsUi::hasUnsavedChanges()) {
        askDiscard(discardAndLeave, nullptr);
        return;
    }
    gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE);
}

// ------------------------------------------------------------------------------
// Seitenleiste
// ------------------------------------------------------------------------------
static void navClicked(lv_event_t* e) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (i >= SETTINGS_AREA_COUNT) return;
    if (AREAS[i].page == s_current) return;
    playToneI2S(800, 60, true);
    Settings_Navigate(AREAS[i].page);
}

static void backToDashboard(lv_event_t*) {
    playToneI2S(800, 100, true);
    ViewSettings::requestLeave();
}

// Reiner Layout-Container, NICHT klickbar: sonst schluckt z. B. die
// Symbol-Kachel oder die Textspalte den Tipp, und der Bereichs-Knopf darunter
// bekommt ihn nie (nur der schmale Rand reagierte). Klickbare Flaechen (Kopf,
// Bereichs-Knoepfe) setzen LV_OBJ_FLAG_CLICKABLE ausdruecklich.
static lv_obj_t* plain(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void buildSidebar(lv_obj_t* scr) {
    const UiColors& c = UiTheme::c();

    lv_obj_t* side = plain(scr);
    lv_obj_set_pos(side, 0, 80);
    lv_obj_set_size(side, 340, 640);
    lv_obj_set_style_bg_color(side, c.side, 0);
    lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(side, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(side, c.divider, 0);
    lv_obj_set_style_border_width(side, 1, 0);
    lv_obj_set_style_pad_all(side, 16, 0);
    lv_obj_set_style_pad_top(side, 18, 0);
    lv_obj_set_style_pad_row(side, 6, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(side, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    // Kopf: zurueck zum Dashboard. Runde Flaeche + Titel, beides antippbar.
    lv_obj_t* head = plain(side);
    lv_obj_set_size(head, lv_pct(100), 64);
    lv_obj_add_flag(head, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(head, 6, 0);
    lv_obj_set_style_pad_column(head, 14, 0);
    lv_obj_set_style_pad_bottom(head, 8, 0);
    lv_obj_add_event_cb(head, backToDashboard, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* circle = plain(head);
    lv_obj_set_size(circle, 48, 48);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, c.surface2, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_t* arrow = lv_label_create(circle);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(arrow, c.text, 0);
    lv_obj_center(arrow);

    lv_obj_t* title = lv_label_create(head);
    lv_label_set_text(title, "Einstellungen");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, c.text, 0);

    // Bereiche: farbige Symbol-Kachel, Name, Live-Status mit Punkt.
    for (uint8_t i = 0; i < SETTINGS_AREA_COUNT; i++) {
        lv_obj_t* b = plain(side);
        lv_obj_set_size(b, lv_pct(100), 84);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(b, 18, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(b, c.navActive, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_pad_left(b, 12, 0);
        lv_obj_set_style_pad_column(b, 16, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(b, navClicked, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* tile = plain(b);
        lv_obj_set_size(tile, 60, 60);
        lv_obj_set_style_radius(tile, 16, 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(AREAS[i].tile), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_t* icon = lv_label_create(tile);
        lv_obj_set_style_text_font(icon, &lela_icons, 0);
        lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        lv_label_set_text(icon, AREAS[i].icon);
        lv_obj_center(icon);

        lv_obj_t* col = plain(b);
        lv_obj_set_height(col, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, 4, 0);

        lv_obj_t* name = lv_label_create(col);
        lv_label_set_text(name, AREAS[i].label);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(name, c.text, 0);

        lv_obj_t* line = plain(col);
        lv_obj_set_size(line, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(line, 6, 0);

        lv_obj_t* dot = plain(line);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, c.off, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

        lv_obj_t* st = lv_label_create(line);
        lv_label_set_text(st, "");
        lv_obj_set_width(st, 200);                       // lange WLAN-Namen kuerzen
        lv_label_set_long_mode(st, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(st, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(st, c.muted, 0);

        s_nav[i].btn = b;
        s_nav[i].dot = dot;
        s_nav[i].status = st;
        s_nav[i].level = 0xFF;
    }
}

// Status der Bereiche: Text nur bei Aenderung setzen (jede Aenderung zeichnet
// die Zeile neu).
static void updateNavStatus() {
    const UiColors& c = UiTheme::c();
    for (uint8_t i = 0; i < SETTINGS_AREA_COUNT; i++) {
        if (!s_nav[i].status) continue;
        uint8_t level = 0;
        const char* txt = SettingsArea_Status(AREAS[i].page, &level);
        if (strcmp(lv_label_get_text(s_nav[i].status), txt) != 0) lv_label_set_text(s_nav[i].status, txt);
        if (level != s_nav[i].level) {
            s_nav[i].level = level;
            lv_obj_set_style_bg_color(s_nav[i].dot, level == 1 ? c.ok : (level == 2 ? c.warn : c.off), 0);
        }
    }
}

// Wird der Bildschirm verlassen (nicht nur neu aufgebaut), laeuft eine
// Bluetooth-Suche sonst weiter.
static void screenDeleted(lv_event_t* e) {
    if (lv_event_get_target(e) != s_screen) return;   // bereits ersetzt (Neuaufbau)
    SettingsUi::closeConfirm();   // eine offene Rueckfrage gehoerte zu diesem Bildschirm
    leavePage(s_current);
    s_screen = nullptr;
    s_content = nullptr;
    for (auto& n : s_nav) n = NavItem();
}

// ------------------------------------------------------------------------------
// Live-Status der Bereiche (Seitenleiste)
// ------------------------------------------------------------------------------
const char* SettingsArea_Status(SettingsPage area, uint8_t* level) {
    static char buf[40];
    *level = 0;
    switch (area) {
        case SettingsPage::Cat:
            if (!matEnabled) return "Matte aus";
            if (useMqttForMat) {
                const bool online = millis() - proxyLastUpdate < 15000;
                *level = online ? 1 : 2;
                return online ? "Ueber MQTT" : "Proxy offline";
            }
            *level = connected ? 1 : 2;
            return connected ? "Matte verbunden" : "Matte sucht";

        case SettingsPage::Baby:
            switch (BabyService_GetMode()) {
                case BabyMode::AudioOnly:  *level = 1; return "Ton laeuft";
                case BabyMode::Video:      *level = 1; return "Video laeuft";
                case BabyMode::VideoAudio: *level = 1; return "Video und Ton";
                default:                   return "Aus";
            }

        case SettingsPage::Net:
            if (!wifiEnabled) return "WLAN aus";
            if (WiFi.status() != WL_CONNECTED) { *level = 2; return "WLAN verbindet"; }
            *level = (mqttEnabled && !mqttClient.connected()) ? 2 : 1;
            snprintf(buf, sizeof(buf), "%s%s", wifiSsid.c_str(),
                     mqttEnabled ? (mqttClient.connected() ? " - MQTT" : " - MQTT getrennt") : "");
            return buf;

        case SettingsPage::Display:
            switch (UiTheme::mode()) {
                case UiThemeMode::Light: return "Hell";
                case UiThemeMode::Auto:  return UiTheme::isDark() ? "Automatisch (dunkel)" : "Automatisch (hell)";
                default:                 return "Dunkel";
            }

        case SettingsPage::System:
            if (pendingScreenshotMode == 1) { *level = 2; return "Bildschirmfoto aktiv"; }
            if (WiFi.status() != WL_CONNECTED) return "Kein WLAN";
            snprintf(buf, sizeof(buf), "%s", WiFi.localIP().toString().c_str());
            return buf;

        default:
            return "";
    }
}

// ------------------------------------------------------------------------------
// Ansicht
// ------------------------------------------------------------------------------
lv_obj_t* ViewSettings::build() {
    SettingsUi::begin();
    const UiColors& c = UiTheme::c();

    // Unterseiten (Suche, QR-Code) werden nicht wiederhergestellt: Wiedereinstieg
    // im zugehoerigen Bereich. Eine noch laufende Bluetooth-Suche beenden.
    leavePage(s_current);
    s_current = areaOf(s_current);
    s_navPending = false;

    lv_obj_t* scr = lv_obj_create(NULL);
    if (!scr) return nullptr;
    lv_obj_set_style_bg_color(scr, c.bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);
    lv_obj_add_event_cb(scr, screenDeleted, LV_EVENT_DELETE, nullptr);
    s_screen = scr;

    buildSidebar(scr);

    s_content = plain(scr);
    lv_obj_set_pos(s_content, 340, 80);
    lv_obj_set_size(s_content, 940, 640);
    lv_obj_add_event_cb(s_content, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(scr, true);
    SettingsUi::setKeyboard(lv_keyboard_create(scr));

    highlightNav(s_current);
    buildPage(s_current);
    updateNavStatus();
    return scr;
}

void ViewSettings::update() {
    if (gui.getCurrentScreen() != SCREEN_SETTINGS || !s_content) return;

    ViewTopbar_Update();

    // Bildschirmfoto-Modus beendet (Knopf oder Browser, /exit_screenshot):
    // 0 stoppt den Webserver (WebSetupLogic_Update).
    if (pendingScreenshotMode == 2) {
        pendingScreenshotMode = 0;
        if (s_current == SettingsPage::Screenshot) Settings_Back();
    }

    // Waehrend das Web-Setup startet oder laeuft, bleibt die Seite stehen.
    if (webSetupMode > 0 || pendingWebSetupMode > 0) return;

    static uint32_t lastNav = 0;
    if (millis() - lastNav > 1000) {
        lastNav = millis();
        updateNavStatus();
    }
    updatePage(s_current);
}
