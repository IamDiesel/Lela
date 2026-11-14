// ==============================================================================
// SettingsPageBaby - Bereich "Babyphone": Kamera-Verbindung, Aufloesung, Ton
// ==============================================================================

#include "SettingsPages.h"
#include "SettingsUi.h"
#include "SharedData.h"
#include "CamResolutionUi.h"
#include "CameraScanner.h"
#include "AudioStreamLogic.h"
#include "secrets.h"
#include <Preferences.h>
#include <WiFi.h>

using namespace SettingsUi;

static void refreshBaby();   // Live-Werte, siehe unten

// Liest einen gespeicherten Text (ohne Rueckfall aus secrets.h): Felder zeigen
// nur, was im Geraet eingestellt ist - sonst schriebe "Uebernehmen" den
// Rueckfall ungefragt fest (gleiche Regel wie im Web-Setup).
static String storedText(const char* key) {
    Preferences p;
    p.begin("catmat", true);
    String v = p.isKey(key) ? p.getString(key, "") : String();
    p.end();
    return v;
}

// Speichert ein Feld mit Rueckfall: leer -> Schluessel loeschen.
static void saveOrClear(Preferences& p, const char* key, const String& v) {
    if (v.length() > 0) p.putString(key, v);
    else if (p.isKey(key)) p.remove(key);
}

static String trimmed(lv_obj_t* ta) {
    String s = lv_textarea_get_text(ta);
    s.trim();
    return s;
}

// ==============================================================================
// Bereich "Babyphone"
// ==============================================================================
static lv_obj_t* s_resChip = nullptr;

static void openPage(lv_event_t* e) {
    playToneI2S(800, 60, true);
    Settings_Navigate((SettingsPage)(uintptr_t)lv_event_get_user_data(e));
}

static void fpsChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    showFps = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    Preferences p; p.begin("catmat", false); p.putBool("showFps", showFps); p.end();
}

static const char* audioSourceText() {
    if (audioSource == AUDIO_SRC_BABYCAM_UDP) return "BabyCam (UDP)";
    return audioFormat == AUDIO_FMT_AAC ? "HTTP, AAC" : "HTTP, PCM";
}

void SettingsBaby_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Babyphone", "Kamera, Bild und Ton");

    lv_obj_t* card = section(pg, "KAMERA");
    linkRow(card, "Verbindung", useCustomUrls ? "Andere Kamera (eigene URL)" : "BabyCam",
            useCustomUrls ? "" : streamIp.c_str(), openPage, (void*)(uintptr_t)SettingsPage::Camera);

    // Aufloesung: dieselbe Einstellung und Bedienung wie im Zahnrad-Dialog der
    // Baby-App (CamResolutionUi).
    Row r = row(card, "Aufloesung");
    s_resChip = chip(r, "", Chip::Warn);
    lv_obj_t* dd = addDropdown(r, "", 0, nullptr);
    CamResolutionUi_Fill(dd);
    lv_obj_add_event_cb(dd, CamResolutionUi_OnChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    r = row(card, "Bildrate einblenden");
    addSwitch(r, showFps, fpsChanged);

    card = section(pg, "TON");
    linkRow(card, "Tonquelle",
            audioSource == AUDIO_SRC_BABYCAM_UDP ? "Gegensprechen verfuegbar" : "Ohne Gegensprechen",
            audioSourceText(), openPage, (void*)(uintptr_t)SettingsPage::Audio);

    refreshBaby();
}

// Live-Werte ohne Drossel (Aufbau) bzw. gedrosselt (Hauptschleife).
static void refreshBaby() {
    if (!s_resChip) return;

    const String hint = CamResolutionUi_Hint();
    if (hint.length() == 0) {
        lv_obj_add_flag(s_resChip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_resChip, LV_OBJ_FLAG_HIDDEN);
        setChip(s_resChip, hint.c_str(), Chip::Warn);
    }
}

void SettingsBaby_Update() {
    static uint32_t last = 0;
    if (millis() - last < 1000 && last != 0) return;
    last = millis();
    refreshBaby();
}

// ==============================================================================
// Unterseite "Kamera-Verbindung"
// ==============================================================================
static lv_obj_t* s_taIp      = nullptr;
static lv_obj_t* s_taUrl     = nullptr;
static lv_obj_t* s_swForeign = nullptr;
static lv_obj_t* s_btnFind   = nullptr;
static lv_obj_t* s_foundCard = nullptr;

static void findClicked(lv_event_t*) {
    if (WiFi.status() != WL_CONNECTED) {
        playToneI2S(400, 150, true);
        lv_label_set_text(lv_obj_get_child(s_btnFind, 0), "Kein WLAN");
        return;
    }
    playToneI2S(800, 60, true);
    CameraScanner::scanFinishedEvent = false;
    lv_label_set_text(lv_obj_get_child(s_btnFind, 0), "Sucht ...");
    lv_obj_add_state(s_btnFind, LV_STATE_DISABLED);
    CameraScanner::startScan();
}

static void foundClicked(lv_event_t* e) {
    playToneI2S(800, 60, true);
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (i < CameraScanner::foundCameras.size()) {
        lv_textarea_set_text(s_taIp, CameraScanner::foundCameras[i].ip.c_str());
    }
}

static void cameraSave(lv_event_t*) {
    playToneI2S(800, 100, true);
    useCustomUrls = lv_obj_has_state(s_swForeign, LV_STATE_CHECKED);
    const String ip  = trimmed(s_taIp);
    const String url = trimmed(s_taUrl);

    Preferences p;
    p.begin("catmat", false);
    p.putBool("useCstUrl", useCustomUrls);
    saveOrClear(p, "strIp", ip);
    saveOrClear(p, "camEntity", url);
    p.end();

    // Wirksame Werte wie beim Laden (SharedData: prefOrSecret). Die BabyCam-URL
    // baut VideoLogic bei jedem Verbindungsaufbau selbst aus der IP.
    streamIp  = ip.length()  ? ip  : String(SECRET_STREAM_IP);
    camEntity = url.length() ? url : String(SECRET_CAM_SNAPSHOT_PATH);
    Settings_Back();
}

void SettingsCamera_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Kamera-Verbindung", "BabyCam im Heimnetz oder eine andere MJPEG-Kamera",
           "Babyphone", Settings_BackCb);

    lv_obj_t* card = section(pg, "BABYCAM");
    Row r = row(card, "IP-Adresse", "Leer = Standard aus secrets.h");
    s_taIp = addTextarea(r, storedText("strIp").c_str(), SECRET_STREAM_IP, 300);
    r = row(card, "Im Netz suchen", "Findet BabyCams im selben WLAN");
    s_btnFind = addButton(r.obj, "Suchen", Btn::Tonal, findClicked);

    s_foundCard = section(pg, "GEFUNDEN");
    lv_obj_add_flag(lv_obj_get_parent(s_foundCard), LV_OBJ_FLAG_HIDDEN);

    card = section(pg, "ANDERE KAMERA", "Bei einer anderen Kamera steht die Aufloesung in ihrer URL. "
                                        "Die BabyCam-IP bleibt fuer die Kamerasteuerung erhalten.");
    r = row(card, "Eigene Video-URL verwenden");
    s_swForeign = addSwitch(r, useCustomUrls, nullptr);
    r = row(card, "Video-URL");
    s_taUrl = addTextarea(r, storedText("camEntity").c_str(), "http://...  (leer = secrets.h)", 480);

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Verwerfen", Btn::Tonal, Settings_BackCb);
    addButton(bar, "Uebernehmen", Btn::Primary, cameraSave);
}

void SettingsCamera_Update() {
    if (!CameraScanner::scanFinishedEvent || !s_btnFind) return;
    CameraScanner::scanFinishedEvent = false;

    lv_obj_clear_state(s_btnFind, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(s_btnFind, 0), "Erneut suchen");

    lv_obj_clean(s_foundCard);
    lv_obj_clear_flag(lv_obj_get_parent(s_foundCard), LV_OBJ_FLAG_HIDDEN);
    if (CameraScanner::foundCameras.empty()) {
        row(s_foundCard, "Keine BabyCam gefunden", "Laeuft die App? Ist das Handy im selben WLAN?");
        return;
    }
    for (size_t i = 0; i < CameraScanner::foundCameras.size(); i++) {
        const CamDevice& d = CameraScanner::foundCameras[i];
        linkRow(s_foundCard, d.name.c_str(), d.ip.c_str(), "Uebernehmen", foundClicked, (void*)(uintptr_t)i);
    }
}

// ==============================================================================
// Unterseite "Tonquelle"
// ==============================================================================
static const char* const SOURCES[] = { "BabyCam (UDP)", "HTTP-Stream" };
static const int  RATES[]   = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
static const int  RATE_COUNT = sizeof(RATES) / sizeof(RATES[0]);

static lv_obj_t* s_seg       = nullptr;
static lv_obj_t* s_secUdp    = nullptr;
static lv_obj_t* s_secHttp   = nullptr;
static lv_obj_t* s_taPortIn  = nullptr;
static lv_obj_t* s_taPortOut = nullptr;
static lv_obj_t* s_taAudUrl  = nullptr;
static lv_obj_t* s_ddFormat  = nullptr;
static lv_obj_t* s_ddRate    = nullptr;
static lv_obj_t* s_ddCh      = nullptr;
static lv_obj_t* s_rowRate   = nullptr;
static lv_obj_t* s_rowCh     = nullptr;

static void showFlag(lv_obj_t* o, bool show) {
    if (show) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Felder passend zu Quelle und Format. Abtastrate/Kanaele nur fuer rohes PCM.
static void audioVisibility() {
    const bool http = segmentedSelected(s_seg) == 1;
    const bool pcm  = lv_dropdown_get_selected(s_ddFormat) == AUDIO_FMT_PCM16;
    showFlag(s_secUdp, !http);
    showFlag(s_secHttp, http);
    showFlag(s_rowRate, pcm);
    showFlag(s_rowCh, pcm);
}

static void audioOptionChanged(lv_event_t*) {
    playToneI2S(800, 50, true);
    audioVisibility();
}

static int portFrom(lv_obj_t* ta, int current) {
    const long v = atol(lv_textarea_get_text(ta));
    return (v > 0 && v <= 65535) ? (int)v : current;
}

static void audioSave(lv_event_t*) {
    playToneI2S(800, 100, true);
    audioSource      = segmentedSelected(s_seg) == 1 ? AUDIO_SRC_HTTP : AUDIO_SRC_BABYCAM_UDP;
    audioFormat      = lv_dropdown_get_selected(s_ddFormat) == AUDIO_FMT_AAC ? AUDIO_FMT_AAC : AUDIO_FMT_PCM16;
    audioPcmRate     = RATES[lv_dropdown_get_selected(s_ddRate) % RATE_COUNT];
    audioPcmChannels = lv_dropdown_get_selected(s_ddCh) == 1 ? 2 : 1;
    audioUdpPortIn   = portFrom(s_taPortIn, audioUdpPortIn);
    audioUdpPortOut  = portFrom(s_taPortOut, audioUdpPortOut);
    const String url = trimmed(s_taAudUrl);

    Preferences p;
    p.begin("catmat", false);
    p.putInt("audSrc", audioSource);
    p.putInt("audioFmt", audioFormat);
    p.putInt("audRate", audioPcmRate);
    p.putInt("audCh", audioPcmChannels);
    p.putInt("audPIn", audioUdpPortIn);
    p.putInt("audPOut", audioUdpPortOut);
    saveOrClear(p, "babyUrl", url);
    p.end();
    babyStreamUrl = url.length() ? url : String(SECRET_BABY_STREAM_URL);

    // Ein laufender Ton startet mit der neuen Quelle neu.
    AudioStreamLogic_ApplyConfig();
    Settings_Back();
}

void SettingsAudio_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Tonquelle", "Woher der Babyphone-Ton kommt", "Babyphone", Settings_BackCb);

    s_seg = segmented(pg, SOURCES, 2, audioSource == AUDIO_SRC_HTTP ? 1 : 0, audioOptionChanged);

    lv_obj_t* card = section(pg, "PORTS", "Meldet die Kamera eigene Ports, gelten diese. "
                                          "Gegensprechen gibt es nur mit dieser Quelle.");
    s_secUdp = lv_obj_get_parent(card);
    Row r = row(card, "Empfangsport", "Kamera -> Tablet (Standard 50001)");
    s_taPortIn = addTextarea(r, String(audioUdpPortIn).c_str(), nullptr, 180, true);
    r = row(card, "Sprechport", "Tablet -> Kamera (Standard 50003)");
    s_taPortOut = addTextarea(r, String(audioUdpPortOut).c_str(), nullptr, 180, true);

    card = section(pg, "STROM", "Ein WAV-Kopf und AAC bringen Abtastrate und Kanaele selbst mit. "
                                "Leere URL = Standard aus secrets.h.");
    s_secHttp = lv_obj_get_parent(card);
    r = row(card, "URL");
    s_taAudUrl = addTextarea(r, storedText("babyUrl").c_str(), "http://...", 560);
    r = row(card, "Format");
    s_ddFormat = addDropdown(r, "PCM 16 Bit (roh/WAV)\nAAC (ADTS)", audioFormat == AUDIO_FMT_AAC ? 1 : 0, audioOptionChanged);

    uint16_t rateIdx = 5;   // 44100
    for (int i = 0; i < RATE_COUNT; i++) if (RATES[i] == audioPcmRate) rateIdx = (uint16_t)i;
    r = row(card, "Abtastrate");
    s_rowRate = r.obj;
    s_ddRate = addDropdown(r, "8000 Hz\n11025 Hz\n16000 Hz\n22050 Hz\n32000 Hz\n44100 Hz\n48000 Hz", rateIdx, nullptr);
    r = row(card, "Kanaele");
    s_rowCh = r.obj;
    s_ddCh = addDropdown(r, "Mono\nStereo", audioPcmChannels == 2 ? 1 : 0, nullptr);

    lv_obj_t* bar = actions(pg);
    addButton(bar, "Verwerfen", Btn::Tonal, Settings_BackCb);
    addButton(bar, "Uebernehmen", Btn::Primary, audioSave);

    audioVisibility();
}
