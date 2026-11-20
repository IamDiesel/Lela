// ==============================================================================
// ViewQuickSettings - Schnelleinstellungen (Wischen nach unten)
// ==============================================================================
// Schwebende Karte auf der System-Ebene, auf jedem Bildschirm gleich:
//
//   Schnelleinstellungen
//   +---------------------------------------------+
//   | Helligkeit        [------o------]  [ 80 % ] |
//   | Lautstaerke       [----o--------]  [ 60 % ] [Ton]
//   | Mixer                         UI, Alarm, Baby > |
//   +---------------------------------------------+
//   (Mixer: UI / Alarm / Baby, je Regler + Stumm)
//   BABYPHONE  [ Aus | Ton | Video | Video+Ton ]
//                          [Bildschirm aus] [Ausschalten]
//
// Bausteine und Farben wie in den Einstellungen (SettingsUi, UiTheme). Tippen
// neben die Karte schliesst sie, Wischen nach oben ebenfalls (GuiManager).
// ==============================================================================

#include "ViewQuickSettings.h"
#include "ViewPower.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "BabyService.h"
#include "SettingsUi.h"
#include "UiTheme.h"
#include "DisplayBrightness.h"
#include <M5Unified.h>
#include <Preferences.h>

using namespace SettingsUi;

static lv_obj_t* quickOverlay   = nullptr;
static lv_obj_t* mixer_section  = nullptr;
static lv_obj_t* s_brightPill   = nullptr;

// Babyphone-Modus: ein Segment je Modus, Reihenfolge wie BabyMode.
static const int BABY_MODE_COUNT = 4;
static_assert((int)BabyMode::VideoAudio == BABY_MODE_COUNT - 1,
              "Segmentreihenfolge muss der Reihenfolge von BabyMode entsprechen");
static const char* const BABY_MODE_TEXT[BABY_MODE_COUNT] = {
    "Aus", LV_SYMBOL_AUDIO, LV_SYMBOL_VIDEO, LV_SYMBOL_VIDEO " " LV_SYMBOL_AUDIO,
};

// ------------------------------------------------------------------------------
// Lautstaerke-Zeilen (Master und Mixer)
// ------------------------------------------------------------------------------
// Je Zeile: Regler, Prozentfeld, Stummknopf. Die Kennung waehlt den Mixer-Kanal
// fuer den Probeton beim Loslassen (0 = Master).
struct VolumeRow {
    int*      vol;
    bool*     mute;
    int       id;
    lv_obj_t* pill;
};
static VolumeRow s_rows[4];

static void setPercent(lv_obj_t* pill, int v) { lv_label_set_text_fmt(pill, "%d %%", v); }

static void volumeChanged(lv_event_t* e) {
    VolumeRow* r = (VolumeRow*)lv_event_get_user_data(e);
    *r->vol = lv_slider_get_value(lv_event_get_target(e));
    setPercent(r->pill, *r->vol);
}

// Probeton im eingestellten Pegel, dann speichern.
static void volumeReleased(lv_event_t* e) {
    const VolumeRow* r = (const VolumeRow*)lv_event_get_user_data(e);
    float vol = 0.0f;
    if      (r->id == 0 && !muteMaster)              vol = volMaster / 100.0f;
    else if (r->id == 1 && !muteMaster && !muteUI)    vol = (volMaster / 100.0f) * (volUI / 100.0f);
    else if (r->id == 2 && !muteMaster && !muteAlarm) vol = (volMaster / 100.0f) * (volAlarm / 100.0f);
    else if (r->id == 3 && !muteMaster && !muteBaby)  vol = (volMaster / 100.0f) * (volBaby / 100.0f);
    const uint8_t chan = (r->id == 0) ? 0 : (uint8_t)(r->id - 1);
    if (vol > 0.01f) playBingI2S(chan);
    SharedData::Save();
}

static void muteClicked(lv_event_t* e) {
    playToneI2S(800, 15, true);
    VolumeRow* r = (VolumeRow*)lv_event_get_user_data(e);
    *r->mute = !*r->mute;
    lv_obj_t* btn = lv_event_get_target(e);
    lv_label_set_text(lv_obj_get_child(btn, 0), *r->mute ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    setButtonKind(btn, *r->mute ? Btn::Danger : Btn::Tonal);
    SharedData::Save();
}

static void volumeRow(lv_obj_t* card, const char* label, int* vol, bool* mute, int maxVol, int id) {
    VolumeRow& vr = s_rows[id];
    vr = { vol, mute, id, nullptr };
    Row r = row(card, label);
    lv_obj_t* sl = addSlider(r, 0, maxVol, *vol, nullptr, &vr.pill);
    lv_obj_set_width(sl, 180);
    lv_obj_add_event_cb(sl, volumeChanged, LV_EVENT_VALUE_CHANGED, &vr);
    lv_obj_add_event_cb(sl, volumeReleased, LV_EVENT_RELEASED, &vr);
    setPercent(vr.pill, *vol);
    lv_obj_t* b = addButton(r.obj, *mute ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX,
                            *mute ? Btn::Danger : Btn::Tonal, muteClicked, &vr);
    lv_obj_set_style_pad_left(b, 18, 0);
    lv_obj_set_style_pad_right(b, 18, 0);
}

// ------------------------------------------------------------------------------
// Uebrige Bedienung
// ------------------------------------------------------------------------------
static void brightnessChanged(lv_event_t* e) {
    DisplayBrightness_Set(lv_slider_get_value(lv_event_get_target(e)));
    setPercent(s_brightPill, brightnessPercent);
}

static void brightnessReleased(lv_event_t*) {
    playToneI2S(1000, 15, true);
    DisplayBrightness_Save();
}

static void mixerToggle(lv_event_t*) {
    playToneI2S(800, 15, true);
    if (lv_obj_has_flag(mixer_section, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(mixer_section, LV_OBJ_FLAG_HIDDEN);
    else                                                    lv_obj_add_flag(mixer_section, LV_OBJ_FLAG_HIDDEN);
}

// Gilt sofort und auf jedem Bildschirm; Video ohne Ton laeuft im Hintergrund,
// bis das Overlay es anzeigt.
static void babyModeChanged(lv_event_t* e) {
    playToneI2S(800, 15, true);
    BabyService_SetMode((BabyMode)segmentedSelected(lv_event_get_target(e)));
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
bool ViewQuickSettings::isActive() {
    return quickOverlay != nullptr;
}

void ViewQuickSettings::hide() {
    if (quickOverlay != nullptr) {
        lv_obj_del_async(quickOverlay);
        quickOverlay = nullptr;
        mixer_section = nullptr;
        s_brightPill = nullptr;
    }
}

void ViewQuickSettings::toggle(GuiManager* guiManager) {
    if (quickOverlay != nullptr) {
        hide();
        return;
    }
    initStyles();
    const UiColors& c = UiTheme::c();

    // Vollflaechig und unsichtbar: Tippen neben die Karte schliesst sie.
    quickOverlay = lv_obj_create(lv_disp_get_layer_sys(NULL));
    lv_obj_remove_style_all(quickOverlay);
    lv_obj_set_size(quickOverlay, 1280, 720);
    lv_obj_clear_flag(quickOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(quickOverlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(quickOverlay, [](lv_event_t*) { ViewQuickSettings::hide(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(quickOverlay, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, guiManager);

    // Karte. Undurchsichtig: Transparenz ueber einem laufenden Bild waere bei
    // jedem Bildwechsel teuer nachzuzeichnen.
    lv_obj_t* panel = lv_obj_create(quickOverlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_width(panel, 680);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(panel, 690, 0);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_color(panel, c.side, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, c.divider, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 24, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_row(panel, 16, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);       // Klicks nicht zum Overlay durchreichen
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);  // Wischen nach oben schliesst

    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_text(title, "Schnelleinstellungen");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, c.text, 0);

    // Helligkeit und Lautstaerke
    lv_obj_t* card = section(panel, nullptr);
    Row r = row(card, "Helligkeit");
    lv_obj_t* sl = addSlider(r, DISPLAY_BRIGHTNESS_MIN_PERCENT, DISPLAY_BRIGHTNESS_MAX_PERCENT,
                             brightnessPercent, brightnessChanged, &s_brightPill);
    lv_obj_set_width(sl, 180);
    lv_obj_add_event_cb(sl, brightnessReleased, LV_EVENT_RELEASED, nullptr);
    setPercent(s_brightPill, brightnessPercent);

    volumeRow(card, "Lautstaerke", &volMaster, &muteMaster, 100, 0);
    linkRow(card, "Mixer", nullptr, "UI, Alarm, Baby", mixerToggle);

    // Mixer (eingeklappt)
    lv_obj_t* mixCard = section(panel, "MIXER");
    mixer_section = lv_obj_get_parent(mixCard);
    volumeRow(mixCard, "UI", &volUI, &muteUI, 100, 1);
    volumeRow(mixCard, "Alarm", &volAlarm, &muteAlarm, 100, 2);
    volumeRow(mixCard, "Baby", &volBaby, &muteBaby, 150, 3);
    lv_obj_add_flag(mixer_section, LV_OBJ_FLAG_HIDDEN);

    // Babyphone-Modus
    lv_obj_t* babyCard = section(panel, "BABYPHONE");
    r = row(babyCard, "Modus");
    segmented(r.obj, BABY_MODE_TEXT, BABY_MODE_COUNT, (uint8_t)BabyService_GetMode(), babyModeChanged);

    // Bildschirm aus / Ausschalten
    lv_obj_t* bar = buttonRow(panel);
    addButton(bar, LV_SYMBOL_EYE_CLOSE "  Bildschirm aus", Btn::Tonal, ViewPower::btn_screen_off_event_cb, guiManager);
    addButton(bar, LV_SYMBOL_POWER "  Ausschalten", Btn::Danger, ViewPower::btn_shutdown_event_cb);
}
