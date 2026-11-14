// ==============================================================================
// SettingsPageNet - Bereich "Verbindungen": WLAN, WLAN-Netze, MQTT, Web-Setup
// ==============================================================================

#include "SettingsPages.h"
#include "SettingsUi.h"
#include "UiTheme.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "WifiNetworks.h"
#include "WifiConnect.h"
#include "WebSetupLogic.h"
#include <Preferences.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>

using namespace SettingsUi;

static void refreshNet();   // Live-Werte, siehe unten

static void openPage(lv_event_t* e) {
    playToneI2S(800, 60, true);
    Settings_Navigate((SettingsPage)(uintptr_t)lv_event_get_user_data(e));
}

// ==============================================================================
// Bereich "Verbindungen"
// ==============================================================================
static lv_obj_t* s_wifiChip = nullptr;
static lv_obj_t* s_mqttChip = nullptr;

static void netSwitchChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    const bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    const char* key = (const char*)lv_event_get_user_data(e);
    if (strcmp(key, "wifiEn") == 0) wifiEnabled = on;   // SystemLogic schaltet das WLAN
    else                            mqttEnabled = on;
    Preferences p; p.begin("catmat", false); p.putBool(key, on); p.end();
    refreshNet();
}

void SettingsNet_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Verbindungen", "Netzwerk und Einrichtung im Browser");

    lv_obj_t* card = section(pg, "NETZWERK");
    Row r = row(card, "WLAN");
    s_wifiChip = chip(r, "", Chip::Neutral);
    addSwitch(r, wifiEnabled, netSwitchChanged, (void*)"wifiEn");
    linkRow(card, "WLAN-Netz", wifiSsid.length() ? wifiSsid.c_str() : "Nicht eingerichtet", "Wechseln",
            openPage, (void*)(uintptr_t)SettingsPage::Wifi);
    r = row(card, "MQTT");
    s_mqttChip = chip(r, "", Chip::Neutral);
    addSwitch(r, mqttEnabled, netSwitchChanged, (void*)"mqttEn");

    card = section(pg, "EINRICHTUNG", "Im Browser lassen sich WLAN, Home Assistant, MQTT, Kamera und Ton einstellen.");
    linkRow(card, "Web-Setup", "QR-Code mit dem Handy scannen", "Oeffnen",
            openPage, (void*)(uintptr_t)SettingsPage::WebSetup);

    refreshNet();
}

// Live-Werte ohne Drossel (Aufbau) bzw. gedrosselt (Hauptschleife).
static void refreshNet() {
    if (!s_wifiChip || !s_mqttChip) return;

    char buf[64];
    if (!wifiEnabled) {
        setChip(s_wifiChip, "Aus", Chip::Neutral);
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), "Verbunden, %d dBm", (int)WiFi.RSSI());
        setChip(s_wifiChip, buf, Chip::Ok);
    } else {
        setChip(s_wifiChip, "Verbindet ...", Chip::Warn);
    }

    if (!mqttEnabled) {
        setChip(s_mqttChip, "Aus", Chip::Neutral);
    } else if (mqttClient.connected()) {
        snprintf(buf, sizeof(buf), "Verbunden mit %s", mqttBroker.c_str());
        setChip(s_mqttChip, buf, Chip::Ok);
    } else {
        setChip(s_mqttChip, "Getrennt", Chip::Warn);
    }
}

void SettingsNet_Update() {
    static uint32_t last = 0;
    if (millis() - last < 1000 && last != 0) return;
    last = millis();
    refreshNet();
}

// ==============================================================================
// Unterseite "WLAN-Netz": gemerkte Netze, Netze in der Naehe, neues Netz
// ==============================================================================
static lv_obj_t* s_taSsid     = nullptr;
static lv_obj_t* s_taPass     = nullptr;
static lv_obj_t* s_nearCard   = nullptr;
static lv_obj_t* s_btnScan    = nullptr;
static std::vector<String> s_known;    // SSIDs der gemerkten Netze (Index = user_data)
static std::vector<String> s_near;     // SSIDs in der Naehe (Index = user_data)
static bool s_scanRunning = false;

static void connectKnown(lv_event_t* e) {
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    const std::vector<WifiNetwork> nets = WifiNetworks_List();
    for (const WifiNetwork& n : nets) {
        if (i < s_known.size() && n.ssid == s_known[i]) {
            playToneI2S(800, 100, true);
            WifiConnect_SwitchTo(n.ssid, n.pass);
            Settings_Back();
            return;
        }
    }
}

static void forgetKnown(lv_event_t* e) {
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (i >= s_known.size()) return;
    playToneI2S(600, 100, true);
    WifiNetworks_Forget(s_known[i]);
    Settings_Navigate(SettingsPage::Wifi);   // Liste neu aufbauen
}

static void nearClicked(lv_event_t* e) {
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (i >= s_near.size()) return;
    playToneI2S(800, 60, true);
    lv_textarea_set_text(s_taSsid, s_near[i].c_str());
    lv_textarea_set_text(s_taPass, "");
}

static void scanClicked(lv_event_t*) {
    if (s_scanRunning) return;
    playToneI2S(800, 60, true);
    // Asynchron: die bestehende Verbindung bleibt, das Ergebnis holt Update ab.
    if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
        lv_label_set_text(lv_obj_get_child(s_btnScan, 0), "Fehlgeschlagen");
        return;
    }
    s_scanRunning = true;
    lv_label_set_text(lv_obj_get_child(s_btnScan, 0), "Sucht ...");
    lv_obj_add_state(s_btnScan, LV_STATE_DISABLED);
}

static void showPassChanged(lv_event_t* e) {
    playToneI2S(800, 50, true);
    lv_textarea_set_password_mode(s_taPass, !lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static void connectNew(lv_event_t*) {
    String ssid = lv_textarea_get_text(s_taSsid);
    ssid.trim();
    if (ssid.length() == 0) { playToneI2S(400, 150, true); return; }
    playToneI2S(800, 100, true);
    WifiConnect_SwitchTo(ssid, String(lv_textarea_get_text(s_taPass)));
    Settings_Back();
}

void SettingsWifi_Build(lv_obj_t* parent) {
    s_known.clear();
    s_near.clear();

    lv_obj_t* pg = page(parent);
    header(pg, "WLAN-Netz", "Gemerktes Netz waehlen oder ein neues verbinden", "Verbindungen", Settings_BackCb);

    lv_obj_t* card = section(pg, "GEMERKTE NETZE",
                             "Netze werden nach einer erfolgreichen Verbindung mit Passwort gemerkt (hoechstens 8).");
    const std::vector<WifiNetwork> nets = WifiNetworks_List();
    const bool online = WiFi.status() == WL_CONNECTED;
    if (nets.empty()) row(card, "Noch keine Netze gemerkt");
    for (size_t i = 0; i < nets.size(); i++) {
        s_known.push_back(nets[i].ssid);
        const bool current = nets[i].ssid == wifiSsid;
        Row r = row(card, nets[i].ssid.c_str());
        if (current) chip(r, online ? "Verbunden" : "Aktuell, verbindet ...", online ? Chip::Ok : Chip::Warn);
        if (!current) addButton(r.obj, "Verbinden", Btn::Tonal, connectKnown, (void*)(uintptr_t)i);
        addButton(r.obj, "Vergessen", Btn::Danger, forgetKnown, (void*)(uintptr_t)i);
    }

    card = section(pg, "NEUES NETZ", "Nach erfolgreicher Verbindung wird das Netz gemerkt. "
                                     "Gelingt sie nicht, bleibt das bisherige in der Liste.");
    Row r = row(card, "In der Naehe", "Netze suchen und antippen");
    s_btnScan = addButton(r.obj, "Suchen", Btn::Tonal, scanClicked);
    r = row(card, "Name (SSID)");
    s_taSsid = addTextarea(r, "", "WLAN-Name", 420);
    r = row(card, "Passwort");
    s_taPass = addTextarea(r, "", "Passwort", 420, false, true);
    r = row(card, "Passwort anzeigen");
    addSwitch(r, false, showPassChanged);

    s_nearCard = section(pg, "IN DER NAEHE");
    lv_obj_add_flag(lv_obj_get_parent(s_nearCard), LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Abbrechen", Btn::Tonal, Settings_BackCb);
    addButton(bar, "Verbinden", Btn::Primary, connectNew);

    // Eine noch laufende Suche einer frueheren Seite gilt nicht mehr.
    if (s_scanRunning) { WiFi.scanDelete(); s_scanRunning = false; }
}

void SettingsWifi_Update() {
    if (!s_scanRunning || !s_btnScan) return;
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    s_scanRunning = false;
    lv_obj_clear_state(s_btnScan, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(s_btnScan, 0), "Erneut suchen");
    if (n < 0) return;

    // Doppelte SSIDs (mehrere Zugangspunkte) zusammenfassen, staerkstes Signal zuerst.
    struct Net { String ssid; int32_t rssi; };
    std::vector<Net> found;
    for (int16_t i = 0; i < n; i++) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        auto it = std::find_if(found.begin(), found.end(), [&](const Net& x) { return x.ssid == ssid; });
        if (it == found.end()) found.push_back({ ssid, WiFi.RSSI(i) });
        else if (WiFi.RSSI(i) > it->rssi) it->rssi = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    std::sort(found.begin(), found.end(), [](const Net& a, const Net& b) { return a.rssi > b.rssi; });
    if (found.size() > 10) found.resize(10);

    lv_obj_clean(s_nearCard);
    lv_obj_clear_flag(lv_obj_get_parent(s_nearCard), LV_OBJ_FLAG_HIDDEN);
    s_near.clear();
    if (found.empty()) row(s_nearCard, "Keine Netze gefunden");
    for (size_t i = 0; i < found.size(); i++) {
        s_near.push_back(found[i].ssid);
        char desc[24];
        snprintf(desc, sizeof(desc), "%ld dBm", (long)found[i].rssi);
        linkRow(s_nearCard, found[i].ssid.c_str(), desc, "", nearClicked, (void*)(uintptr_t)i);
    }
}

// ==============================================================================
// Unterseite "Web-Setup"
// ==============================================================================
static lv_obj_t* s_webPage = nullptr;

// QR-Ansicht ersetzt die Auswahl. Das Setup lebt nur, solange diese Ansicht
// zu sehen ist: "Beenden", der Zurueck-Link, ein anderer Bereich oder das
// Verlassen der Einstellungen beenden es (SettingsWebSetup_Leave), ohne
// Neustart. Speichern im Browser startet das Tablet neu (Einstellungen wie
// WLAN und feste IP greifen so sauber); ohne Eingabe endet es nach 5 Minuten.
// Aufgeschoben (lv_async_call): Der Klick kommt von einem Knopf der Seite, die
// hier ersetzt wird.
static String      s_qrData, s_qrSteps;
static const char* s_qrTitle = "";
static bool        s_qrActive = false;   // QR-Ansicht offen, Setup gehoert zu ihr

static void endSetupClicked(lv_event_t*) {
    playToneI2S(600, 100, true);
    WebSetupLogic_RequestStop();
    s_qrActive = false;
    Settings_Navigate(SettingsPage::WebSetup);   // zurueck zur Auswahl
}

static void showQrNow(void*) {
    if (!s_webPage || gui.getCurrentScreen() != SCREEN_SETTINGS) return;
    lv_obj_t* pg = page(lv_obj_get_parent(s_webPage));
    s_webPage = pg;
    s_qrActive = true;
    header(pg, s_qrTitle, "Mit der Handykamera scannen", "Verbindungen", Settings_BackCb);
    qrCard(pg, s_qrData.c_str(), s_qrSteps.c_str(),
           "Nach dem Speichern im Browser startet das Tablet neu. Verlassen dieser Seite "
           "beendet das Setup, ohne Eingabe endet es nach 5 Minuten.");
    lv_obj_t* bar = actions(pg);
    addButton(bar, "Beenden", Btn::Tonal, endSetupClicked);
}

void SettingsWebSetup_Leave() {
    if (!s_qrActive) return;   // nur die Auswahl offen: nichts zu beenden
    s_qrActive = false;
    WebSetupLogic_RequestStop();
}

void SettingsWebSetup_Update() {
    // Setup von selbst beendet (Zeitlimit): QR-Code ist ungueltig -> Auswahl.
    if (s_qrActive && webSetupMode == 0 && pendingWebSetupMode == 0) {
        s_qrActive = false;
        Settings_Navigate(SettingsPage::WebSetup);
    }
}

static void showQr(const String& qrData, const char* title, const String& steps) {
    s_qrData = qrData;
    s_qrSteps = steps;
    s_qrTitle = title;
    lv_async_call(showQrNow, nullptr);
}

static void startHome(lv_event_t*) {
    if (isSetupScanning) return;
    if (WiFi.status() != WL_CONNECTED) { playToneI2S(400, 150, true); return; }
    playToneI2S(800, 100, true);
    const String ip = WiFi.localIP().toString();
    showQr("http://" + ip, "Web-Setup im Heimnetz",
           "1. Handy mit demselben WLAN verbinden\n2. QR-Code scannen\n3. Oder im Browser oeffnen:\n    http://" + ip);
    pendingWebSetupMode = 2;
}

static void startAp(lv_event_t*) {
    if (isSetupScanning) return;
    playToneI2S(800, 100, true);
    apPassword = String(esp_random() % 90000000 + 10000000);
    showQr("WIFI:S:Lela-Setup;T:WPA;P:" + apPassword + ";;", "Eigenes Setup-WLAN",
           "1. QR-Code scannen oder WLAN \"Lela-Setup\" waehlen\n   Passwort: " + apPassword +
           "\n2. Im Browser oeffnen: http://192.168.4.1");
    pendingWebSetupMode = 1;
}

void SettingsWebSetup_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    s_webPage = pg;
    header(pg, "Web-Setup", "Einstellungen bequem am Handy oder PC", "Verbindungen", Settings_BackCb);

    lv_obj_t* card = section(pg, "ZUGANG WAEHLEN",
                             "Im Browser: WLAN, feste IP, Home Assistant, MQTT, Kamera, Ton und Bluetooth-Geraete.");
    // Beide Wege sind gleichwertig - gleiche Knopfart wie die uebrigen Zeilen.
    Row r = row(card, "Im Heimnetz", "Handy ist im selben WLAN wie das Tablet");
    lv_obj_t* b = addButton(r.obj, "QR-Code zeigen", Btn::Tonal, startHome);
    if (WiFi.status() != WL_CONNECTED) lv_obj_add_state(b, LV_STATE_DISABLED);
    r = row(card, "Eigenes Setup-WLAN", "Fuer die Ersteinrichtung oder ein neues WLAN. Das Tablet trennt sich dafuer vom Heimnetz.");
    addButton(r.obj, "QR-Code zeigen", Btn::Tonal, startAp);
}
