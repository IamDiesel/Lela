// ==============================================================================
// SessionResume - Implementierung (siehe SessionResume.h)
// ==============================================================================
#include "SessionResume.h"
#include "BabyService.h"
#include "CrashDiag.h"
#include "GuiManager.h"
#include "HangGuard.h"
#include "UiTheme.h"
#include <Arduino.h>
#include <lvgl.h>

namespace {

// Vorheriger Lauf muss mindestens so lange gelaufen sein (Absturzschleifen).
constexpr uint32_t kMinPrevUptimeMs = 60000;
// Erst kurz nach dem Wechsel vom Startbildschirm fortsetzen: der Bildschirm-
// wechsel ins Dashboard soll abgeschlossen sein, bevor der naechste folgt.
constexpr uint32_t kApplyDelayMs    = 500;

// Aufbau des Merkworts (CrashDiag_SetResumeWord).
constexpr uint32_t kValidBit    = 0x80000000;   // unterscheidet "Aus, Dashboard" von "nichts gemerkt"
constexpr uint32_t kModeMask    = 0x00000003;   // BabyMode
constexpr uint32_t kBabyViewBit = 0x00000004;   // Baby-Ansicht war offen

// Der Startbildschirm hat keine ScreenID; GuiManager fuehrt ihn als 99.
constexpr ScreenID kBootScreen = (ScreenID)99;

uint32_t s_bootDoneMs = 0;      // 0 = Startbildschirm laeuft noch
bool     s_applied    = false;
uint32_t s_lastWord   = 0;

// ------------------------------------------------------------------------------
// Hinweis-Kasten
// ------------------------------------------------------------------------------
// Auf der obersten Ebene, damit er jeden Bildschirmwechsel uebersteht. Bleibt
// stehen, bis er mit "OK" bestaetigt wird - ein Neustart in der Nacht soll am
// Morgen noch zu sehen sein. Solange er steht, blendet sich das Video-Overlay
// nicht ein (ViewVideoPip: oberste Ebene belegt).
void closeNotice(lv_event_t* e) {
    lv_obj_del_async((lv_obj_t*)lv_event_get_user_data(e));
}

void showNotice(bool afterHang) {
    const UiColors& c = UiTheme::c();

    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(box, 820, LV_SIZE_CONTENT);
    lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(box, c.surface, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, c.warn, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 16, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_style_pad_row(box, 10, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    // Quer zur Spalte rechtsbuendig: Titel und Text sind volle Breite, es
    // wirkt also nur auf den Knopf.
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(box);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, c.text, 0);
    lv_label_set_text(title, afterHang ? LV_SYMBOL_WARNING "  Babyphone nach Haenger fortgesetzt"
                                       : LV_SYMBOL_WARNING "  Babyphone nach Absturz fortgesetzt");

    lv_obj_t* body = lv_label_create(box);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(body, c.muted, 0);
    lv_label_set_text(body, afterHang
        ? "Das Tablet war eingefroren und hat sich selbst neu gestartet.\n"
          "Bericht: Einstellungen > System > Diagnose > Letzter Haenger.\n"
          "Details am PC: tools\\read_coredump.ps1 liest den Coredump aus."
        : "Das Tablet ist abgestuerzt und hat sich selbst neu gestartet.\n"
          "Bericht: Einstellungen > System > Diagnose > Letzter Start.\n"
          "Details am PC: tools\\read_coredump.ps1 liest den Coredump aus.");

    lv_obj_t* btn = lv_btn_create(box);
    lv_obj_set_style_bg_color(btn, c.primary, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 32, 0);
    lv_obj_set_style_pad_ver(btn, 12, 0);
    lv_obj_add_event_cb(btn, closeNotice, LV_EVENT_CLICKED, box);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_obj_set_style_text_color(lbl, c.onPrimary, 0);
    lv_label_set_text(lbl, "OK");
}

// ------------------------------------------------------------------------------
// Merken und Fortsetzen
// ------------------------------------------------------------------------------
uint32_t pack(BabyMode mode, bool babyView) {
    return kValidBit | ((uint32_t)mode & kModeMask) | (babyView ? kBabyViewBit : 0);
}

void apply() {
    if (!CrashDiag_WasUnplannedRestart()) return;

    const uint32_t prev = CrashDiag_PrevResumeWord();
    const BabyMode mode = (BabyMode)(prev & kModeMask);
    if (!(prev & kValidBit) || mode == BabyMode::Off) return;   // nichts lief

    if (CrashDiag_PrevUptimeMs() < kMinPrevUptimeMs) {
        Serial.printf("[Resume] Keine Fortsetzung: vorheriger Lauf nur %lu s (Schleifenschutz).\n",
                      (unsigned long)(CrashDiag_PrevUptimeMs() / 1000));
        return;
    }

    const bool babyView = (prev & kBabyViewBit) != 0;
    Serial.printf("[Resume] Setze fort: Babyphone %s%s.\n", BabyMode_Name(mode),
                  babyView ? ", Baby-Ansicht" : "");
    BabyService_SetMode(mode);
    if (babyView) gui.switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
    showNotice(HangGuard_TrippedLastRun());
}

} // namespace

void SessionResume_Update() {
    const ScreenID screen = gui.getCurrentScreen();
    if (screen == kBootScreen) return;

    const uint32_t now = millis();
    if (s_bootDoneMs == 0) s_bootDoneMs = now ? now : 1;
    if (!s_applied) {
        // Bis dahin nichts merken: sonst ueberschriebe "Aus, Dashboard" nach
        // einem erneuten Absturz in dieser halben Sekunde den alten Stand
        // (unschaedlich, aber irrefuehrend).
        if (now - s_bootDoneMs < kApplyDelayMs) return;
        s_applied = true;
        apply();
    }

    const uint32_t word = pack(BabyService_GetMode(), gui.getCurrentScreen() == SCREEN_BABY);
    if (word != s_lastWord) {
        s_lastWord = word;
        CrashDiag_SetResumeWord(word);
    }
}
