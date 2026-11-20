// ==============================================================================
// SettingsPageBaby - Bereich "Babyphone": Kamera-Verbindung, Aufloesung, Ton
// ==============================================================================

#include "SettingsPages.h"
#include "SettingsUi.h"
#include "SharedData.h"
#include "CamResolutionUi.h"
#include "CameraScanner.h"
#include "BabyService.h"
#include "AudioStreamLogic.h"
#include "VoiceProcessor.h"
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

// Kurzfassung der Sprachaufbereitung fuer die Zeile "Gegensprechen".
static String talkSummary() {
    String s;
    if (pttHighpassHz > 0 || pttLowpassHz > 0) {
        s += pttHighpassHz > 0 ? String(pttHighpassHz) + " Hz" : String("offen");
        s += " - ";
        s += pttLowpassHz > 0 ? String(pttLowpassHz / 1000) + " kHz" : String("offen");
    } else {
        s += "Ohne Filter";
    }
    if (pttAutoGain)        s += ", Automatik";
    else if (pttGainDb > 0) s += ", +" + String(pttGainDb) + " dB";
    return s;
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
    linkRow(card, "Gegensprechen", "Klang der eigenen Stimme an der Kamera",
            talkSummary().c_str(), openPage, (void*)(uintptr_t)SettingsPage::Talk);

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
    saveOrClear(p, "camEntity", url);
    p.end();

    // Die IP gehoert dem BabyService (speichern, wirksam machen, Empfang neu
    // aufbauen) - derselbe Weg wie die Kamerasuche in der Baby-Ansicht. Die
    // BabyCam-URL baut VideoLogic bei jedem Verbindungsaufbau aus der IP.
    BabyService_SetCameraIp(ip);
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
    addButton(bar, "Verwerfen", Btn::Tonal, Settings_DiscardCb);
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
    addButton(bar, "Verwerfen", Btn::Tonal, Settings_DiscardCb);
    addButton(bar, "Uebernehmen", Btn::Primary, audioSave);

    audioVisibility();
}

// ==============================================================================
// Unterseite "Gegensprechen"
// ==============================================================================
// Klang der eigenen Stimme an der Kamera (VoiceProcessor.h). Jede Aenderung
// gilt sofort und wird gleich gespeichert - ohne "Uebernehmen", damit man
// zwischen zwei Durchsagen probieren kann. Wirksam wird sie mit dem naechsten
// Druck auf die Sprechtaste (AudioStreamLogic uebernimmt die Werte beim Start).
static const int HIGHPASS_HZ[]  = { 0, 80, 100, 120, 150, 200, 300 };
static const char HIGHPASS_TEXT[] = "Aus\n80 Hz\n100 Hz\n120 Hz\n150 Hz\n200 Hz\n300 Hz";
static const int LOWPASS_HZ[]   = { 0, 10000, 8000, 7000, 6000, 5000, 4000 };
static const char LOWPASS_TEXT[]  = "Aus\n10 kHz\n8 kHz\n7 kHz\n6 kHz\n5 kHz\n4 kHz";
static const int FILTER_STEPS   = sizeof(HIGHPASS_HZ) / sizeof(HIGHPASS_HZ[0]);

static Row       s_talkGainRow;
static lv_obj_t* s_talkGainVal = nullptr;

// Eintrag der Auswahl, der dem gespeicherten Wert am naechsten liegt.
static uint16_t nearestStep(const int* list, int value) {
    int best = 0;
    for (int i = 1; i < FILTER_STEPS; i++) {
        if (abs(list[i] - value) < abs(list[best] - value)) best = i;
    }
    return (uint16_t)best;
}

static void saveTalk() {
    Preferences p;
    p.begin("catmat", false);
    p.putInt("pttHp", pttHighpassHz);
    p.putInt("pttLp", pttLowpassHz);
    p.putBool("pttAgc", pttAutoGain);
    p.putInt("pttGain", pttGainDb);
    p.end();
}

// Beschriftung der Verstaerkung: je nach Automatik Obergrenze oder fester Wert.
static void talkGainTexts() {
    setDesc(s_talkGainRow, pttAutoGain ? "Wie weit die Automatik leises Sprechen hoechstens anhebt"
                                       : "Fester Wert; 0 dB laesst die Lautstaerke unveraendert");
    lv_label_set_text_fmt(s_talkGainVal, "+%d dB", pttGainDb);
}

static void talkHighpassChanged(lv_event_t* e) {
    playToneI2S(800, 50, true);
    pttHighpassHz = HIGHPASS_HZ[lv_dropdown_get_selected(lv_event_get_target(e)) % FILTER_STEPS];
    saveTalk();
}

static void talkLowpassChanged(lv_event_t* e) {
    playToneI2S(800, 50, true);
    pttLowpassHz = LOWPASS_HZ[lv_dropdown_get_selected(lv_event_get_target(e)) % FILTER_STEPS];
    saveTalk();
}

static void talkAutoChanged(lv_event_t* e) {
    playToneI2S(800, 60, true);
    pttAutoGain = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    talkGainTexts();
    saveTalk();
}

static void talkGainChanged(lv_event_t* e) {
    pttGainDb = lv_slider_get_value(lv_event_get_target(e));
    talkGainTexts();
}

// Gespeichert wird erst beim Loslassen - nicht bei jedem Schritt des Reglers.
static void talkGainReleased(lv_event_t*) {
    playToneI2S(1000, 40, true);
    saveTalk();
}

static void talkDefaults(lv_event_t*) {
    playToneI2S(800, 100, true);
    pttHighpassHz = VOICE_HIGHPASS_DEFAULT_HZ;
    pttLowpassHz  = VOICE_LOWPASS_DEFAULT_HZ;
    pttAutoGain   = VOICE_AUTO_GAIN_DEFAULT;
    pttGainDb     = VOICE_GAIN_DEFAULT_DB;
    saveTalk();
    Settings_Navigate(SettingsPage::Talk);   // Seite mit den neuen Werten neu aufbauen
}

void SettingsTalk_Build(lv_obj_t* parent) {
    lv_obj_t* pg = page(parent);
    header(pg, "Gegensprechen", "Klang der eigenen Stimme an der Kamera", "Babyphone", Settings_BackCb);

    lv_obj_t* card = section(pg, "KLANG", "Aenderungen gelten ab dem naechsten Druck auf die Sprechtaste.");
    Row r = row(card, "Tiefen abschneiden", "Entfernt Rumpeln, Griffgeraeusche und Brummen");
    addDropdown(r, HIGHPASS_TEXT, nearestStep(HIGHPASS_HZ, pttHighpassHz), talkHighpassChanged);
    r = row(card, "Hoehen abschneiden", "Entfernt Zischen und Rauschen; Sprache braucht bis etwa 7 kHz");
    addDropdown(r, LOWPASS_TEXT, nearestStep(LOWPASS_HZ, pttLowpassHz), talkLowpassChanged);

    card = section(pg, "LAUTSTAERKE", "Ein Begrenzer faengt Spitzen ab, sobald verstaerkt wird.");
    r = row(card, "Pegelautomatik", "Hebt leises Sprechen an, senkt lautes ab; Pausen bleiben leise");
    addSwitch(r, pttAutoGain, talkAutoChanged);
    s_talkGainRow = row(card, "Verstaerkung", "");
    lv_obj_t* sl = addSlider(s_talkGainRow, 0, VOICE_GAIN_MAX_DB, pttGainDb, talkGainChanged, &s_talkGainVal);
    lv_obj_add_event_cb(sl, talkGainReleased, LV_EVENT_RELEASED, nullptr);
    talkGainTexts();

    card = section(pg, nullptr);
    char defaults[72];
    snprintf(defaults, sizeof(defaults), "%u Hz, %u kHz, Automatik bis %u dB",
             (unsigned)VOICE_HIGHPASS_DEFAULT_HZ, (unsigned)(VOICE_LOWPASS_DEFAULT_HZ / 1000),
             (unsigned)VOICE_GAIN_DEFAULT_DB);
    r = row(card, "Empfohlene Werte", defaults);
    addButton(r.obj, "Zuruecksetzen", Btn::Tonal, talkDefaults);
}
