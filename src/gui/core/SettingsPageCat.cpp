// ==============================================================================
// SettingsPageCat - Bereich "Katzenmatte" und Bluetooth-Suche
// ==============================================================================
// Nur Bedienoberflaeche: die Matten-Logik (BleLogic, SystemLogic, Dongle)
// bleibt unberuehrt; hier werden dieselben Einstellungen gesetzt und dieselben
// BleLogic-Funktionen aufgerufen wie bisher.
// ==============================================================================

#include "SettingsPages.h"
#include "SettingsUi.h"
#include "UiTheme.h"
#include "SharedData.h"
#include "BleLogic.h"
#include "LVGL_Driver.h"
#include <Preferences.h>

using namespace SettingsUi;

static void refreshCat();   // Live-Werte, siehe unten

static void savePrefInt(const char* key, int v)   { Preferences p; p.begin("catmat", false); p.putInt(key, v);  p.end(); }
static void savePrefBool(const char* key, bool v) { Preferences p; p.begin("catmat", false); p.putBool(key, v); p.end(); }

// ==============================================================================
// Bereich "Katzenmatte"
// ==============================================================================
static lv_obj_t* s_thrVal  = nullptr;
static lv_obj_t* s_timeVal = nullptr;
static lv_obj_t* s_matChip = nullptr;
static lv_obj_t* s_kipChip = nullptr;
static Row       s_taraRow;

static void setThresholdText(int v) { lv_label_set_text_fmt(s_thrVal, "%d", v); }

static void setDurationText(int s) {
    if (s < 60) lv_label_set_text_fmt(s_timeVal, "%d s", s);
    else        lv_label_set_text_fmt(s_timeVal, "%d:%02d min", s / 60, s % 60);
}

static void thresholdChanged(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target(e);
    const int v = (lv_slider_get_value(sl) / 10) * 10;
    thresholdVal = v;
    setThresholdText(v);
    // Speichern erst beim Loslassen - jede Zwischenstellung waere ein Flash-Schreibzugriff.
}

static void durationChanged(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target(e);
    const int v = (lv_slider_get_value(sl) / 5) * 5;
    graphTimeSeconds = v;
    setDurationText(v);
    requestChartUpdate = true;
}

static void sliderReleased(lv_event_t* e) {
    playToneI2S(1000, 40, true);
    const char* key = (const char*)lv_event_get_user_data(e);
    savePrefInt(key, strcmp(key, "thr") == 0 ? thresholdVal : graphTimeSeconds);
}

static void graphModeChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    currentGraphMode = lv_dropdown_get_selected(lv_event_get_target(e));
    // Neue Quelle, neuer Verlauf: die alten Werte haetten eine andere Einheit.
    historyIdx = 0;
    historyCount = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) pressureHistory[i] = -32000;
    requestChartUpdate = true;
    savePrefInt("gMode", currentGraphMode);
}

static void timeAxisChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    showTimeOnX = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    savePrefBool("timeX", showTimeOnX);
    requestChartUpdate = true;
}

static void taraClicked(lv_event_t*) {
    playToneI2S(800, 100, true);
    taraOffset = rawPressure;
    Preferences p; p.begin("catmat", false); p.putUInt("off", taraOffset); p.end();
    char buf[48];
    snprintf(buf, sizeof(buf), "Tariert auf %ld", (long)taraOffset);
    setDesc(s_taraRow, buf);
}

static void switchChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    const bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    const char* key = (const char*)lv_event_get_user_data(e);
    if      (strcmp(key, "matEn") == 0)   matEnabled = on;
    else if (strcmp(key, "kipEn") == 0)   kippyEnabled = on;
    else if (strcmp(key, "useMqtt") == 0) useMqttForMat = on;
    savePrefBool(key, on);
    refreshCat();
}

static void openScan(lv_event_t* e) {
    playToneI2S(800, 60, true);
    Settings_Navigate(lv_event_get_user_data(e) ? SettingsPage::BleScanKippy : SettingsPage::BleScanMat);
}

static const char* macOrNone(const String& mac) {
    return (mac.length() == 0 || mac == "00:00:00:00:00:00") ? "Kein Geraet gekoppelt" : mac.c_str();
}

void SettingsCat_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Katzenmatte", "Alarm, Verlauf und Bluetooth-Geraete");

    lv_obj_t* card = section(pg, "ALARM & VERLAUF");
    Row r = row(card, "Alarmschwelle", "Abweichung vom Mittelwert, ab der Alarm ausloest");
    lv_obj_t* sl = addSlider(r, 50, 600, thresholdVal, thresholdChanged, &s_thrVal);
    lv_obj_add_event_cb(sl, sliderReleased, LV_EVENT_RELEASED, (void*)"thr");
    setThresholdText(thresholdVal);

    r = row(card, "Verlauf", "Sichtbarer Zeitraum im Diagramm");
    sl = addSlider(r, 10, 900, graphTimeSeconds, durationChanged, &s_timeVal);
    lv_obj_add_event_cb(sl, sliderReleased, LV_EVENT_RELEASED, (void*)"gtime");
    setDurationText(graphTimeSeconds);

    r = row(card, "Datenquelle");
    addDropdown(r, "Druckwert\nBLE RSSI\nWLAN RSSI\nBLE Intervall\nKippy RSSI", currentGraphMode, graphModeChanged);

    r = row(card, "Uhrzeit auf der Zeitachse");
    addSwitch(r, showTimeOnX, timeAxisChanged);

    s_taraRow = row(card, "Nullpunkt", "Matte unbelastet lassen, dann tarieren");
    addButton(s_taraRow.obj, "Tarieren", Btn::Tonal, taraClicked);

    card = section(pg, "GERAETE", "Mit \"Ueber MQTT\" kommen die Mattenwerte vom Proxy statt direkt per Bluetooth.");
    r = row(card, "Matte");
    s_matChip = chip(r, "", Chip::Neutral);
    addSwitch(r, matEnabled, switchChanged, (void*)"matEn");
    linkRow(card, "Matte koppeln", macOrNone(savedMatMac), "Suchen", openScan, nullptr);

    r = row(card, "Kippy-Tracker");
    s_kipChip = chip(r, "", Chip::Neutral);
    addSwitch(r, kippyEnabled, switchChanged, (void*)"kipEn");
    linkRow(card, "Kippy koppeln", macOrNone(savedKippyMac), "Suchen", openScan, (void*)1);

    r = row(card, "Ueber MQTT");
    addSwitch(r, useMqttForMat, switchChanged, (void*)"useMqtt");

    refreshCat();
}

// Live-Werte ohne Drossel (Aufbau) bzw. gedrosselt (Hauptschleife).
static void refreshCat() {
    if (!s_matChip || !s_kipChip) return;

    if (!matEnabled)        setChip(s_matChip, "Aus", Chip::Neutral);
    else if (useMqttForMat) setChip(s_matChip, (millis() - proxyLastUpdate < 15000) ? "Ueber MQTT" : "Proxy offline",
                                    (millis() - proxyLastUpdate < 15000) ? Chip::Ok : Chip::Warn);
    else                    setChip(s_matChip, connected ? "Verbunden" : "Sucht ...", connected ? Chip::Ok : Chip::Warn);

    const bool seen = lastCatSeenTime != 0 && millis() - lastCatSeenTime <= 30000;
    if (!kippyEnabled) {
        setChip(s_kipChip, "Aus", Chip::Neutral);
    } else if (seen) {
        char buf[40];
        snprintf(buf, sizeof(buf), "In Reichweite, %d dBm", catRssi);
        setChip(s_kipChip, buf, Chip::Ok);
    } else {
        setChip(s_kipChip, "Nicht in Reichweite", Chip::Neutral);
    }
}

void SettingsCat_Update() {
    static uint32_t last = 0;
    if (millis() - last < 1000 && last != 0) return;
    last = millis();
    refreshCat();
}

// ==============================================================================
// Unterseite "Matte/Kippy suchen"
// ==============================================================================
static lv_obj_t* s_scanCard   = nullptr;
static lv_obj_t* s_scanChip   = nullptr;
static Row       s_scanStatus;
static lv_obj_t* s_btnSave    = nullptr;
static lv_obj_t* s_btnMore    = nullptr;
static lv_obj_t* s_rows[MAX_SCAN_DEVICES];
static int       s_selected   = -1;
static bool      s_scanActive = false;

static void selectRow(int index) {
    s_selected = index;
    // Eine Auswahl gilt erst mit "Uebernehmen" - bis dahin ungespeichert.
    if (index >= 0) markDirty(); else clearDirty();
    for (int i = 0; i < MAX_SCAN_DEVICES; i++) {
        if (!s_rows[i]) continue;
        lv_obj_set_style_bg_opa(s_rows[i], i == index ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void rowClicked(lv_event_t* e) {
    playToneI2S(800, 50, true);
    selectRow((int)(intptr_t)lv_event_get_user_data(e));
}

static void rescanClicked(lv_event_t*) {
    playToneI2S(800, 60, true);
    selectRow(-1);
    BleLogic_StartSetupScan(setupScanMode, true);
}

static void continueClicked(lv_event_t*) {
    playToneI2S(800, 60, true);
    if (!isSetupScanning) BleLogic_StartSetupScan(setupScanMode, false);
}

static void saveClicked(lv_event_t*) {
    if (s_selected < 0) { playToneI2S(400, 150, true); return; }
    playToneI2S(800, 100, true);
    BleLogic_SaveDevice(s_selected);
    Settings_Back();
}

void SettingsBleScan_Build(lv_obj_t* parent, bool kippy) {
    memset(s_rows, 0, sizeof(s_rows));
    s_selected = -1;

    lv_obj_t* pg = page(parent);
    header(pg, kippy ? "Kippy suchen" : "Matte suchen",
           kippy ? "Tracker in die Naehe des Tablets bringen" : "Matte einschalten und in die Naehe des Tablets legen",
           "Katzenmatte", Settings_BackCb);

    lv_obj_t* card = section(pg, "SUCHE");
    s_scanStatus = row(card, "Bluetooth-Suche");
    s_scanChip = chip(s_scanStatus, "", Chip::Neutral);

    s_scanCard = section(pg, "GEFUNDENE GERAETE", "Geraet antippen, dann uebernehmen. Die Liste waechst, solange gesucht wird.");

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Neu suchen", Btn::Tonal, rescanClicked);
    s_btnMore = addButton(bar, "Weiter suchen", Btn::Tonal, continueClicked);
    s_btnSave = addButton(bar, "Uebernehmen", Btn::Primary, saveClicked);

    BleLogic_StartSetupScan(kippy ? 2 : 1, true);
    s_scanActive = true;
    SettingsBleScan_Update();
}

void SettingsBleScan_Leave() {
    if (!s_scanActive) return;
    s_scanActive = false;
    BleLogic_CloseSetupWindow();
}

static void addDeviceRow(const BleUpdateEvent& ev) {
    if (ev.index < 0 || ev.index >= MAX_SCAN_DEVICES || !s_scanCard) return;
    char desc[48];
    snprintf(desc, sizeof(desc), "%s  -  %d dBm", ev.device.mac, ev.device.rssi);
    Row r = row(s_scanCard, ev.device.name[0] ? ev.device.name : "(ohne Namen)", desc);
    lv_obj_add_flag(r.obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r.obj, UiTheme::c().navActive, 0);
    lv_obj_set_style_bg_opa(r.obj, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(r.obj, rowClicked, LV_EVENT_CLICKED, (void*)(intptr_t)ev.index);
    s_rows[ev.index] = r.obj;
}

static void updateDeviceRow(const BleUpdateEvent& ev) {
    if (ev.index < 0 || ev.index >= MAX_SCAN_DEVICES || !s_rows[ev.index]) return;
    // Zeile: [0] = Textspalte, darin [1] = Beschreibung
    lv_obj_t* textCol = lv_obj_get_child(s_rows[ev.index], 0);
    lv_obj_t* desc = textCol ? lv_obj_get_child(textCol, 1) : nullptr;
    if (desc) lv_label_set_text_fmt(desc, "%s  -  %d dBm", ev.device.mac, ev.device.rssi);
}

void SettingsBleScan_Update() {
    if (!s_scanCard) return;

    // Ereignisse der Suche (BleLogic -> Queue). Jedes Mal leeren, nicht gedrosselt:
    // die Queue fasst nur 60 Eintraege.
    if (bleUpdateQueue != NULL && uxQueueMessagesWaiting(bleUpdateQueue) > 0 && lvgl_port_lock(50)) {
        BleUpdateEvent ev;
        while (xQueueReceive(bleUpdateQueue, &ev, 0) == pdTRUE) {
            if (ev.isClear) {
                lv_obj_clean(s_scanCard);
                memset(s_rows, 0, sizeof(s_rows));
                s_selected = -1;
            } else if (ev.isNew) {
                addDeviceRow(ev);
            } else {
                updateDeviceRow(ev);
            }
        }
        lvgl_port_unlock();
    }

    static uint32_t last = 0;
    if (millis() - last < 500 && last != 0) return;
    last = millis();

    bool scanning = false, canSave = false;
    char info[96];
    BleLogic_GetScanStatus(info, sizeof(info), scanning, canSave);
    // "Sucht... (12s)\nGefunden: 3" -> eine Zeile fuer den Chip
    for (char* p = info; *p; p++) if (*p == '\n') *p = ' ';
    setChip(s_scanChip, info, scanning ? Chip::Warn : Chip::Neutral);

    if (canSave && s_selected >= 0) lv_obj_clear_state(s_btnSave, LV_STATE_DISABLED);
    else                            lv_obj_add_state(s_btnSave, LV_STATE_DISABLED);
    if (scanning) lv_obj_add_state(s_btnMore, LV_STATE_DISABLED);
    else          lv_obj_clear_state(s_btnMore, LV_STATE_DISABLED);
}
