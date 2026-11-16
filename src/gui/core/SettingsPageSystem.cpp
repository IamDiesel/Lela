// ==============================================================================
// SettingsPageSystem - Bereiche "Anzeige" und "System", Diagnose, Bildschirmfoto
// ==============================================================================

#include "SettingsPages.h"
#include "SettingsUi.h"
#include "UiTheme.h"
#include "HaTheme.h"
#include "CrashDiag.h"
#include "ColdRestart.h"
#include "HangGuard.h"
#include "CameraTab5.h"
#include "ViewCameraPreview.h"
#include "AudioStreamLogic.h"
#include "DebugFlags.h"
#include "AudioCodecTab5.h"
#include "SharedData.h"
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "WifiConnect.h"

using namespace SettingsUi;

static void refreshSystem();
static void refreshDiag();   // Live-Werte, siehe unten

// Laufzeit als "3 h 12 min" bzw. "12 min 5 s".
static void formatUptime(char* buf, size_t len) {
    const uint32_t s = (millis() - startTime) / 1000;
    if (s >= 3600) snprintf(buf, len, "%lu h %lu min", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
    else           snprintf(buf, len, "%lu min %lu s", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static String ipText() {
    if (WiFi.status() != WL_CONNECTED) return "Kein WLAN";
    String s = WiFi.localIP().toString();
    if (WifiConnect_IsStaticIp()) s += " (fest)";
    return s;
}

// ==============================================================================
// Bereich "Anzeige"
// ==============================================================================
static const char* const SCHEMES[] = { "Dunkel", "Hell", "Automatisch" };
static lv_obj_t* s_brightVal = nullptr;

static void schemeChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    UiTheme::setMode((UiThemeMode)segmentedSelected(lv_event_get_target(e)));
}

static void brightnessChanged(lv_event_t* e) {
    brightnessPercent = lv_slider_get_value(lv_event_get_target(e));
    M5.Display.setBrightness((brightnessPercent * 255) / 100);
    lv_label_set_text_fmt(s_brightVal, "%d %%", brightnessPercent);
}

static void brightnessReleased(lv_event_t*) {
    playToneI2S(1000, 40, true);
    Preferences p; p.begin("catmat", false); p.putInt("bright", brightnessPercent); p.end();
}

// Globale Gegenfarben der HA-Widgets (HaTheme). Wirkt beim naechsten Oeffnen
// der HA-App.
static void haAdaptChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    HaTheme::setAutoAdapt(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

void SettingsDisplay_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Anzeige", "Erscheinungsbild der Oberflaeche");

    lv_obj_t* card = section(pg, "DARSTELLUNG", "Automatisch: tagsueber hell (07-20 Uhr), sonst dunkel.");
    Row r = row(card, "Farbschema");
    segmented(r.obj, SCHEMES, 3, (uint8_t)UiTheme::mode(), schemeChanged);

    r = row(card, "Helligkeit", "Gleicher Wert wie in den Schnelleinstellungen");
    lv_obj_t* sl = addSlider(r, 5, 100, brightnessPercent, brightnessChanged, &s_brightVal);
    lv_obj_add_event_cb(sl, brightnessReleased, LV_EVENT_RELEASED, nullptr);
    lv_label_set_text_fmt(s_brightVal, "%d %%", brightnessPercent);

    card = section(pg, "HOME ASSISTANT",
                   "Eigene Widget-Farben bekommen im hellen Schema eine passende Gegenfarbe. "
                   "Je Widget abschaltbar unter Bearbeiten > Anzeige.");
    r = row(card, "Gegenfarben im hellen Schema");
    addSwitch(r, HaTheme::autoAdapt(), haAdaptChanged);
}

// ==============================================================================
// Bereich "System"
// ==============================================================================
static lv_obj_t* s_ipVal     = nullptr;
static lv_obj_t* s_batVal    = nullptr;
static lv_obj_t* s_heapVal   = nullptr;
static lv_obj_t* s_upVal     = nullptr;
static lv_obj_t* s_btnReboot = nullptr;
static uint32_t  s_rebootArmedMs = 0;

static void openPage(lv_event_t* e) {
    playToneI2S(800, 60, true);
    Settings_Navigate((SettingsPage)(uintptr_t)lv_event_get_user_data(e));
}

static void stopScreenshot(lv_event_t*) {
    playToneI2S(600, 100, true);
    pendingScreenshotMode = 2;                 // ViewSettings::update setzt 0 (Server aus)
    Settings_Navigate(SettingsPage::System);   // Zeile neu aufbauen
}

// Neustart mit Rueckfrage im Knopf selbst: erstes Tippen fragt, zweites
// (innerhalb von 4 s) startet neu. Kein Dialog, der den Ablauf unterbricht.
// Mit kurzer Stromlos-Phase (ColdRestart_RestartCold): nach einem Warmstart
// bliebe der Lautsprecher stumm.
static void rebootClicked(lv_event_t*) {
    if (s_rebootArmedMs != 0 && millis() - s_rebootArmedMs < 4000) {
        playToneI2S(800, 100, true);
        ColdRestart_RestartCold("Einstellungen");
    }
    playToneI2S(600, 80, true);
    s_rebootArmedMs = millis();
    lv_label_set_text(lv_obj_get_child(s_btnReboot, 0), "Wirklich neu starten?");
}

void SettingsSystem_Build(lv_obj_t* parent) {
    s_rebootArmedMs = 0;
    lv_obj_t* pg = page(parent);
    header(pg, "System", "Geraet, Werkzeuge und Neustart");

    lv_obj_t* card = section(pg, "GERAET");
    Row r = row(card, "Firmware");
    addValue(r, "Build " __DATE__);
    r = row(card, "IP-Adresse");
    s_ipVal = addValue(r, "");
    r = row(card, "Akku");
    s_batVal = addValue(r, "");
    r = row(card, "Freier Speicher (intern)");
    s_heapVal = addValue(r, "");
    r = row(card, "Laufzeit");
    s_upVal = addValue(r, "");
    linkRow(card, "Diagnose", "Bluetooth, MQTT-Proxy, Alarmzustand", "", openPage,
            (void*)(uintptr_t)SettingsPage::Diag);

    card = section(pg, "WERKZEUGE");
    r = row(card, "Bildschirmfoto im Browser", "Zeigt einen QR-Code; der Browser nimmt die gerade offene Ansicht auf");
    if (pendingScreenshotMode == 1) {
        chip(r, "Aktiv", Chip::Ok);
        addButton(r.obj, "Beenden", Btn::Tonal, stopScreenshot);
    } else {
        addButton(r.obj, "Starten", Btn::Tonal, openPage, (void*)(uintptr_t)SettingsPage::Screenshot);
    }
    r = row(card, "Neustart", "Kurz stromlos (ca. 3 s), damit der Ton sicher geht");
    s_btnReboot = addButton(r.obj, "Neu starten", Btn::Danger, rebootClicked);

    refreshSystem();
}

// Live-Werte ohne Drossel (Aufbau) bzw. gedrosselt (Hauptschleife).
static void refreshSystem() {
    if (!s_ipVal) return;

    char buf[48];
    const String ip = ipText();
    if (strcmp(lv_label_get_text(s_ipVal), ip.c_str()) != 0) lv_label_set_text(s_ipVal, ip.c_str());
    snprintf(buf, sizeof(buf), "%d %%%s", batteryPercent, isBatteryCharging ? ", laedt" : "");
    lv_label_set_text(s_batVal, buf);
    snprintf(buf, sizeof(buf), "%u KB", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    lv_label_set_text(s_heapVal, buf);
    formatUptime(buf, sizeof(buf));
    lv_label_set_text(s_upVal, buf);
}

void SettingsSystem_Update() {
    // Rueckfrage des Neustart-Knopfs zuruecknehmen.
    if (s_rebootArmedMs != 0 && millis() - s_rebootArmedMs >= 4000 && s_btnReboot) {
        s_rebootArmedMs = 0;
        lv_label_set_text(lv_obj_get_child(s_btnReboot, 0), "Neu starten");
    }

    static uint32_t last = 0;
    if (millis() - last < 1000 && last != 0) return;
    last = millis();
    refreshSystem();
}

// ==============================================================================
// Unterseite "Diagnose"
// ==============================================================================
enum DiagField : uint8_t { D_MAT, D_INTERVAL, D_KIPPY, D_WIFI, D_MQTT, D_PROXY, D_ALARM, D_COUNT };
static lv_obj_t* s_diag[D_COUNT] = { nullptr };

#if DIAG_TEST_TOOLS
// Testwerkzeuge (DebugFlags.h). Der Absturz-Knopf fragt im Knopf selbst nach
// (zweimal tippen innerhalb von 4 s), wie der Neustart unter System.
static lv_obj_t* s_btnCrash = nullptr;
static uint32_t  s_crashArmedMs = 0;

// user_data = AudioResetKind. Der Testton folgt aus dem Audio-Task.
static void audioResetClicked(lv_event_t* e) {
    AudioStreamLogic_ResetAudio((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

// Kamera Stufe 1: Sensor ansprechen; Ergebnis im Knopf und im Monitor.
static void cameraProbeClicked(lv_event_t* e) {
    const CameraProbeResult r = CameraTab5_Probe();
    char text[40];
    if (!r.clockOk)        snprintf(text, sizeof(text), "Takt-Fehler");
    else if (!r.ack)       snprintf(text, sizeof(text), "Keine Antwort");
    else if (r.isSc202cs)  snprintf(text, sizeof(text), "OK: SC2356");
    else                   snprintf(text, sizeof(text), "ID 0x%04X", r.chipId);
    lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), text);
}

static void crashClicked(lv_event_t*) {
    if (s_crashArmedMs != 0 && millis() - s_crashArmedMs < 4000) {
        CrashDiag_TriggerCrash();
    }
    playToneI2S(600, 80, true);
    s_crashArmedMs = millis();
    lv_label_set_text(lv_obj_get_child(s_btnCrash, 0), "Wirklich abstuerzen?");
}
#endif

void SettingsDiag_Build(lv_obj_t* parent) {
    memset(s_diag, 0, sizeof(s_diag));
    lv_obj_t* pg = page(parent);
    header(pg, "Diagnose", "Live-Werte, jede Sekunde aktualisiert", "System", Settings_BackCb);

    lv_obj_t* card = section(pg, "BLUETOOTH");
    s_diag[D_MAT]      = addValue(row(card, "Matte"), "");
    s_diag[D_INTERVAL] = addValue(row(card, "Paketabstand"), "");
    s_diag[D_KIPPY]    = addValue(row(card, "Kippy"), "");

    card = section(pg, "NETZWERK");
    s_diag[D_WIFI]  = addValue(row(card, "WLAN"), "");
    s_diag[D_MQTT]  = addValue(row(card, "MQTT"), "");
    s_diag[D_PROXY] = addValue(row(card, "Proxy"), "");

    card = section(pg, "ALARM");
    s_diag[D_ALARM] = addValue(row(card, "Mattenalarm"), "");

    // Letzter Start: Kalt- oder Warmstart und Reset-Grund (CrashDiag).
    card = section(pg, "START",
                   "Warmstart: Audio-Chip und Verstaerker waren nicht stromlos.");
    char start[96];
    snprintf(start, sizeof(start), "%s, %s", CrashDiag_WasColdStart() ? "Kaltstart" : "Warmstart",
             CrashDiag_ResetReasonText());
    addValue(row(card, "Letzter Start"), start);
    snprintf(start, sizeof(start), "%lu", (unsigned long)CrashDiag_GetPanicCount());
    addValue(row(card, "Abstuerze seit Kaltstart"), start);

    // Letzter eingefrorener Zustand, den der Waechter beendet hat (HangGuard).
    const uint32_t hangs = HangGuard_Count();
    Row hangRow = row(card, "Letzter Haenger",
                      hangs ? HangGuard_LastReport() : "Keiner aufgezeichnet (Waechter HangGuard)");
    snprintf(start, sizeof(start), "%lu insgesamt", (unsigned long)hangs);
    addValue(hangRow, start);

#if DIAG_TEST_TOOLS
    card = section(pg, "TESTWERKZEUGE",
                   "Nur zur Fehlersuche (DebugFlags.h: DIAG_TEST_TOOLS). Registerauszug des "
                   "Audio-Chips vor und nach dem Reset im seriellen Monitor.");
    // Je Theorie ein Knopf (AudioCodecTab5.h). Danach ertoent ein Testton.
    struct AudioReset { const char* title; const char* desc; AudioResetKind kind; };
    static const AudioReset RESETS[] = {
        { "Codec ES8388",        "Register und Ablaufsteuerung",           AudioResetKind::Codec  },
        { "I2S im ESP32-P4",     "Peripherie, Takt und Pins",              AudioResetKind::I2s    },
        { "Mikrofon-ADC ES7210", "Teilt die Takt-Leitungen",               AudioResetKind::MicAdc },
        { "Verstaerker NS4150B", "SPK_EN 200 ms aus, Pin neu",             AudioResetKind::Amp    },
        { "USB-Pads GPIO26/27",  "DOUT/BCLK von der USB-PHY loesen",       AudioResetKind::UsbPads },
        { "Codec ES8388 mit Takt", "Ruhezustand bei laufendem Takt, VMID entladen", AudioResetKind::CodecClocked },
        { "Pad-Hold GPIO26-30",  "Eingefrorene I2S-Pads freigeben",        AudioResetKind::PadHold },
        { "I2S-Leitungen messen", "Kein Reset: Pegel an MCLK/BCLK/LRCK/DOUT im Monitor", AudioResetKind::LineCheck },
        { "Hoertest",            "Je 1,5 s: Lautsprecher aus, an, Klick, 1-kHz-Ton, Klick", AudioResetKind::SoundTest },
        { "Erster Ton",          "Lautsprecher neu, dann 6 Pieptoene bei 0/0,15/0,4/0,8/1,5/3 s", AudioResetKind::FirstTone },
        { "Frequenztest",        "Je 1 s: 150 bis 6400 Hz (300 ms), dann 800 und 150 Hz kurz", AudioResetKind::ToneSweep },
        { "Alles nacheinander",  "USB-Pads, I2S, ES7210, ES8388, Verstaerker", AudioResetKind::All },
    };
    Row r;
    for (const AudioReset& a : RESETS) {
        r = row(card, a.title, a.desc);
        addButton(r.obj, "Zuruecksetzen", Btn::Tonal, audioResetClicked, (void*)(uintptr_t)a.kind);
    }
    r = row(card, "Aus- und Einschalten", "Kurz stromlos, Wiedereinschalten nach ca. 3 s (am Akku)");
    addButton(r.obj, "Stromlos", Btn::Tonal, [](lv_event_t*) { ColdRestart_Now("Diagnose", ColdRestartReport::None); });
    r = row(card, "Neustart ueber Tiefschlaf", "1 s Tiefschlaf: schaltet den HP-Bereich des Chips ab");
    addButton(r.obj, "Tiefschlaf", Btn::Tonal, [](lv_event_t*) { CrashDiag_DeepSleepRestart(); });
    r = row(card, "Absturz ausloesen", "Schreibzugriff auf Adresse 0 - zweimal tippen");
    s_crashArmedMs = 0;
    s_btnCrash = addButton(r.obj, "Absturz", Btn::Danger, crashClicked);

    card = section(pg, "KAMERA",
                   "QR-Einrichtung, Ausbaustufen siehe CameraTab5.h. Meldungen im Monitor ([Kamera]).");
    r = row(card, "Sensor pruefen", "Takt 24 MHz an G36, Reset, Chip-ID (SC2356 = 0xEB52)");
    addButton(r.obj, "Pruefen", Btn::Tonal, cameraProbeClicked);
    r = row(card, "Vorschau", "1280x720 ueber CSI + ISP, verkleinert angezeigt");
    addButton(r.obj, "Oeffnen", Btn::Tonal, [](lv_event_t*) {
        CameraViewConfig cfg = {};
        cfg.title     = "Kamera-Vorschau (Test)";
        cfg.closeText = "Schliessen";
        cfg.showStats = true;
        ViewCameraPreview_Open(cfg);
    });
#endif

    refreshDiag();
}

// Live-Werte ohne Drossel (Aufbau) bzw. gedrosselt (Hauptschleife).
static void refreshDiag() {
    if (!s_diag[D_MAT]) return;

    char buf[64];
    lv_label_set_text(s_diag[D_MAT], !matEnabled ? "Aus" : (connected ? "Verbunden" : "Getrennt"));
    snprintf(buf, sizeof(buf), "%d ms", (int)avgInterval);
    lv_label_set_text(s_diag[D_INTERVAL], buf);

    const bool seen = lastCatSeenTime != 0 && millis() - lastCatSeenTime <= 30000;
    if (!kippyEnabled) snprintf(buf, sizeof(buf), "Aus");
    else if (seen)     snprintf(buf, sizeof(buf), "Aktiv, %d dBm", catRssi);
    else               snprintf(buf, sizeof(buf), "Abwesend");
    lv_label_set_text(s_diag[D_KIPPY], buf);

    if (WiFi.status() == WL_CONNECTED) snprintf(buf, sizeof(buf), "Verbunden, %d dBm", (int)WiFi.RSSI());
    else                               snprintf(buf, sizeof(buf), wifiEnabled ? "Getrennt" : "Aus");
    lv_label_set_text(s_diag[D_WIFI], buf);
    lv_label_set_text(s_diag[D_MQTT], mqttClient.connected() ? "Verbunden" : "Getrennt");

    const bool proxyOnline = millis() - proxyLastUpdate < 15000;
    snprintf(buf, sizeof(buf), "%s, WLAN %d dBm", proxyOnline ? proxyStatusMsg.c_str() : "Offline", proxyWifiRssi);
    lv_label_set_text(s_diag[D_PROXY], buf);
    lv_label_set_text(s_diag[D_ALARM], alarmActive ? "Aktiv" : "Bereit");
}

void SettingsDiag_Update() {
#if DIAG_TEST_TOOLS
    // Rueckfrage des Absturz-Knopfs zuruecknehmen.
    if (s_crashArmedMs != 0 && millis() - s_crashArmedMs >= 4000 && s_btnCrash) {
        s_crashArmedMs = 0;
        lv_label_set_text(lv_obj_get_child(s_btnCrash, 0), "Absturz");
    }
#endif
    static uint32_t last = 0;
    if (millis() - last < 1000 && last != 0) return;
    last = millis();
    refreshDiag();
}

// ==============================================================================
// Unterseite "Bildschirmfoto im Browser"
// ==============================================================================
static void screenshotStop(lv_event_t*) {
    playToneI2S(600, 100, true);
    pendingScreenshotMode = 2;   // ViewSettings::update: Server aus, zurueck
}

void SettingsScreenshot_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Bildschirmfoto", "Aufnahmen der Anzeige im Browser", "System", Settings_BackCb);

    if (WiFi.status() != WL_CONNECTED) {
        lv_obj_t* card = section(pg, nullptr);
        row(card, "Kein WLAN", "Fuer Bildschirmfotos muss das Tablet im Heimnetz sein.");
        return;
    }

    pendingScreenshotMode = 1;   // WebSetupLogic startet den Webserver
    const String url = "http://" + WiFi.localIP().toString() + "/screenshot";
    const String steps = "1. QR-Code scannen oder oeffnen:\n    " + url +
                         "\n2. Zur gewuenschten Ansicht wechseln\n3. Im Browser aufnehmen";
    qrCard(pg, url.c_str(), steps.c_str(),
           "Der Modus laeuft weiter, wenn du diese Seite verlaesst - beenden hier oder im Browser.");

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Beenden", Btn::Tonal, screenshotStop);
    addButton(bar, "Fertig", Btn::Primary, Settings_BackCb);
}

void SettingsScreenshot_Update() {}
