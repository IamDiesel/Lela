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
#include "WifiQr.h"
#include "QrScanner.h"
#include "ViewCameraPreview.h"
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

// Netz, das die Unterseite "Netz bearbeiten" zeigt (SettingsWifiEdit_Build).
static String s_editSsid;

static void editKnown(lv_event_t* e) {
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (i >= s_known.size()) return;
    playToneI2S(800, 60, true);
    s_editSsid = s_known[i];
    Settings_Navigate(SettingsPage::WifiEdit);
}

// Prioritaet aendern. user_data: Index * 2, +1 fuer "nach unten".
static void moveKnown(lv_event_t* e) {
    const uintptr_t code = (uintptr_t)lv_event_get_user_data(e);
    const size_t i = (size_t)(code / 2);
    if (i >= s_known.size()) return;
    playToneI2S(800, 60, true);
    WifiNetworks_Move(s_known[i], (code & 1) ? +1 : -1);
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

// ------------------------------------------------------------------------------
// Neues Netz per QR-Code (Kamera). Der gefundene Code fuellt nur Name und
// Passwort aus - verbunden wird erst mit "Verbinden", das Passwort laesst sich
// vorher mit "Passwort anzeigen" pruefen.
// ------------------------------------------------------------------------------
static Row        s_qrScanRow;
static bool       s_qrScanFound = false;
static WifiQrData s_qrScanData;

// Je Kamerabild (ViewCameraPreview, onFrame): an den QR-Leser geben und
// dessen Fund pruefen. true = WLAN-Code gefunden, Kamera schliessen.
static bool qrScanFrame(const uint16_t* frame) {
    QrScanner_Offer(frame);
    String text;
    if (!QrScanner_TakeResult(text)) return false;
    if (WifiQr_Parse(text, s_qrScanData)) {
        s_qrScanFound = true;
        return true;
    }
    ViewCameraPreview_SetHint("Das ist kein WLAN-Code. Am Handy: WLAN > Netz teilen > QR-Code anzeigen.");
    return false;
}

static void qrScanClosed(bool) {
    QrScanner_Stop();
    if (!s_qrScanFound || !s_taSsid || !s_taPass) return;
    lv_textarea_set_text(s_taSsid, s_qrScanData.ssid.c_str());
    lv_textarea_set_text(s_taPass, s_qrScanData.password.c_str());
    s_qrScanData = WifiQrData();   // Passwort nicht laenger als noetig im Speicher
    playToneI2S(1000, 100, true);
    setDesc(s_qrScanRow, "Uebernommen - pruefen und mit \"Verbinden\" bestaetigen");
}

static void qrScanClicked(lv_event_t*) {
    s_qrScanFound = false;
    if (!QrScanner_Start()) {
        setDesc(s_qrScanRow, "QR-Leser konnte nicht starten (Speicher)");
        return;
    }
    CameraViewConfig cfg = {};
    cfg.title     = "WLAN per QR-Code";
    cfg.hint      = "Am Handy das WLAN teilen (QR-Code anzeigen) und vor die Kamera des Tablets halten.";
    cfg.closeText = "Abbrechen";
    cfg.mirror    = true;      // wie ein Spiegel: das Handy erscheint dort, wo man es haelt
    cfg.onFrame   = qrScanFrame;
    cfg.onClosed  = qrScanClosed;
    ViewCameraPreview_Open(cfg);
}

void SettingsWifi_Build(lv_obj_t* parent) {
    s_known.clear();
    s_near.clear();

    lv_obj_t* pg = page(parent);
    header(pg, "WLAN-Netz", "Gemerkte Netze, ihre Reihenfolge und neue Netze", "Verbindungen", Settings_BackCb);

    lv_obj_t* card = section(pg, "GEMERKTE NETZE",
                             "Die Reihenfolge ist die Prioritaet: Beim Einschalten wird zuerst das zuletzt "
                             "verbundene Netz probiert, danach diese Liste von oben nach unten. Netze werden "
                             "nach einer erfolgreichen Verbindung gemerkt (hoechstens 8).");
    const std::vector<WifiNetwork> nets = WifiNetworks_List();
    const bool online = WiFi.status() == WL_CONNECTED;
    if (nets.empty()) row(card, "Noch keine Netze gemerkt");
    for (size_t i = 0; i < nets.size(); i++) {
        s_known.push_back(nets[i].ssid);
        const bool current = nets[i].ssid == wifiSsid;
        const String desc = nets[i].ip.fixed ? String("Feste IP ") + nets[i].ip.ip : String("IP automatisch (DHCP)");
        Row r = row(card, nets[i].ssid.c_str(), desc.c_str());
        if (current) chip(r, online ? "Verbunden" : "Aktuell, verbindet ...", online ? Chip::Ok : Chip::Warn);

        // Reihenfolge der Knoepfe: was in jeder Zeile vorkommt, steht rechts -
        // so liegen "Bearbeiten" und die Pfeile aller Zeilen untereinander.
        // "Verbinden" fehlt beim aktuellen Netz und steht deshalb ganz links.
        if (!current) addButton(r.obj, "Verbinden", Btn::Tonal, connectKnown, (void*)(uintptr_t)i);
        addButton(r.obj, "Bearbeiten", Btn::Tonal, editKnown, (void*)(uintptr_t)i);

        // Prioritaet: eine Stelle hoeher / tiefer. Am Rand der Liste gesperrt.
        lv_obj_t* up = addButton(r.obj, LV_SYMBOL_UP, Btn::Tonal, moveKnown, (void*)(uintptr_t)(i * 2));
        lv_obj_t* down = addButton(r.obj, LV_SYMBOL_DOWN, Btn::Tonal, moveKnown, (void*)(uintptr_t)(i * 2 + 1));
        if (i == 0)               lv_obj_add_state(up, LV_STATE_DISABLED);
        if (i + 1 == nets.size()) lv_obj_add_state(down, LV_STATE_DISABLED);
    }

    // Ein weiteres Netz wird auf einer eigenen Seite eingetragen: Dort gibt es
    // etwas zu bestaetigen ("Verbinden"), hier wirkt jeder Knopf sofort.
    card = section(pg, "WEITERES NETZ");
    linkRow(card, "Neues Netz verbinden", "Per QR-Code vom Handy, aus der Suche oder von Hand",
            "", openPage, (void*)(uintptr_t)SettingsPage::WifiNew);

    // Felder der Seite "Neues Netz" gibt es hier nicht (qrScanClosed prueft sie).
    s_taSsid = nullptr;
    s_taPass = nullptr;
}

// ==============================================================================
// Unterseite "Neues Netz": per QR-Code, aus der Suche oder von Hand
// ==============================================================================
void SettingsWifiNew_Build(lv_obj_t* parent) {
    s_near.clear();

    lv_obj_t* pg = page(parent);
    header(pg, "Neues Netz", "Per QR-Code, aus der Suche oder von Hand eintragen", "WLAN-Netz", Settings_BackCb);

    lv_obj_t* card = section(pg, "ZUGANGSDATEN", "Nach erfolgreicher Verbindung wird das Netz gemerkt und steht am Ende "
                                                 "der Liste. Gelingt sie nicht, bleibt die Liste unveraendert.");
    s_qrScanRow = row(card, "Per QR-Code", "WLAN-Code vom Handy mit der Kamera einlesen");
    addButton(s_qrScanRow.obj, "Scannen", Btn::Tonal, qrScanClicked);
    Row r = row(card, "In der Naehe", "Netze suchen und antippen");
    s_btnScan = addButton(r.obj, "Suchen", Btn::Tonal, scanClicked);
    r = row(card, "Name (SSID)");
    s_taSsid = addTextarea(r, "", "WLAN-Name", 420);
    r = row(card, "Passwort");
    s_taPass = addTextarea(r, "", "Passwort", 420, false, true);
    r = row(card, "Passwort anzeigen");
    setTransient(addSwitch(r, false, showPassChanged));   // nur Anzeige, nichts zu speichern

    s_nearCard = section(pg, "IN DER NAEHE");
    lv_obj_add_flag(lv_obj_get_parent(s_nearCard), LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Abbrechen", Btn::Tonal, Settings_DiscardCb);
    addButton(bar, "Verbinden", Btn::Primary, connectNew);

    // Eine noch laufende Suche einer frueheren Seite gilt nicht mehr.
    if (s_scanRunning) { WiFi.scanDelete(); s_scanRunning = false; }
}

// ==============================================================================
// Unterseite "Netz bearbeiten": IP-Einstellung eines gemerkten Netzes, vergessen
// ==============================================================================
static lv_obj_t* s_swFixed  = nullptr;
static lv_obj_t* s_taIpAddr = nullptr;
static lv_obj_t* s_taIpGw   = nullptr;
static lv_obj_t* s_taIpMask = nullptr;
static lv_obj_t* s_taIpDns  = nullptr;
static Row       s_fixedRow;

static String fieldText(lv_obj_t* ta) {
    String s = lv_textarea_get_text(ta);
    s.trim();
    return s;
}

// Gespeicherter Wert - sonst, wenn dieses Netz gerade verbunden ist, der
// aktuelle: dann genuegt meist der Schalter.
static String ipFieldDefault(const String& saved, bool connectedHere, IPAddress current) {
    if (saved.length() > 0) return saved;
    if (connectedHere && current != IPAddress((uint32_t)0)) return current.toString();
    return "";
}

static void editSave(lv_event_t*) {
    WifiIpConfig cfg;
    cfg.fixed   = lv_obj_has_state(s_swFixed, LV_STATE_CHECKED);
    cfg.ip      = fieldText(s_taIpAddr);
    cfg.gateway = fieldText(s_taIpGw);
    cfg.subnet  = fieldText(s_taIpMask);
    cfg.dns     = fieldText(s_taIpDns);

    // Nur gueltige Angaben einschalten - sonst bliebe das Geraet in diesem
    // Netz ohne Verbindung.
    if (cfg.fixed && !WifiIpConfig_Valid(cfg)) {
        playToneI2S(400, 150, true);
        setDesc(s_fixedRow, "Unvollstaendig oder ungueltig: Adresse, Gateway und Maske pruefen");
        return;
    }
    playToneI2S(800, 100, true);

    const WifiIpConfig old = WifiNetworks_IpConfigFor(s_editSsid);
    const bool changed = old.fixed != cfg.fixed || old.ip != cfg.ip || old.gateway != cfg.gateway ||
                         old.subnet != cfg.subnet || old.dns != cfg.dns;
    WifiNetworks_SetIpConfig(s_editSsid, cfg);

    // Betrifft es das aktuelle Netz: neu anmelden, damit die Einstellung gilt.
    WifiNetwork net;
    if (changed && s_editSsid == wifiSsid && WifiNetworks_Find(s_editSsid, &net)) {
        WifiConnect_SwitchTo(net.ssid, net.pass);
    }
    Settings_Back();
}

static void forgetConfirmed(void*) {
    WifiNetworks_Forget(s_editSsid);
    Settings_Back();
}

static void editForget(lv_event_t*) {
    playToneI2S(600, 100, true);
    const String title = String("Netz '") + s_editSsid + "' vergessen?";
    confirm(title.c_str(), "Passwort und IP-Einstellung dieses Netzes werden geloescht.",
            "Abbrechen", "Vergessen", Btn::Danger, forgetConfirmed);
}

void SettingsWifiEdit_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Netz bearbeiten", s_editSsid.c_str(), "WLAN-Netz", Settings_BackCb);

    WifiNetwork net;
    if (!WifiNetworks_Find(s_editSsid, &net)) {
        row(section(pg, nullptr), "Dieses Netz ist nicht mehr gemerkt");
        return;
    }
    const bool connectedHere = (net.ssid == wifiSsid) && WiFi.status() == WL_CONNECTED;

    lv_obj_t* card = section(pg, "IP-ADRESSE",
                             "Mit fester Adresse entfaellt die Anfrage beim Router (DHCP) - das Netz steht rund "
                             "2 s schneller. Die Adresse vorher im Router fest fuer dieses Geraet reservieren.");
    s_fixedRow = row(card, "Feste IP-Adresse", "Aus = automatisch vom Router (DHCP)");
    s_swFixed = addSwitch(s_fixedRow, net.ip.fixed, nullptr);
    Row r = row(card, "IP-Adresse");
    s_taIpAddr = addTextarea(r, ipFieldDefault(net.ip.ip, connectedHere, WiFi.localIP()).c_str(), "z.B. 192.168.2.125", 300);
    r = row(card, "Gateway (Router)");
    s_taIpGw = addTextarea(r, ipFieldDefault(net.ip.gateway, connectedHere, WiFi.gatewayIP()).c_str(), "z.B. 192.168.2.1", 300);
    r = row(card, "Subnetzmaske");
    s_taIpMask = addTextarea(r, ipFieldDefault(net.ip.subnet, connectedHere, WiFi.subnetMask()).c_str(), "255.255.255.0", 300);
    r = row(card, "DNS", "Leer = Gateway");
    s_taIpDns = addTextarea(r, net.ip.dns.c_str(), "leer = Gateway", 300);

    card = section(pg, "NETZ");
    r = row(card, "Netz vergessen", "Loescht Passwort und IP-Einstellung");
    addButton(r.obj, "Vergessen", Btn::Danger, editForget);

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Verwerfen", Btn::Tonal, Settings_DiscardCb);
    addButton(bar, "Uebernehmen", Btn::Primary, editSave);
}

void SettingsWifiNew_Update() {
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
    lv_obj_t* pg = page(pageHost(s_webPage));
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
