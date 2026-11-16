// ==============================================================================
// ViewCameraPreview - Implementierung (siehe ViewCameraPreview.h)
// ==============================================================================
#include "ViewCameraPreview.h"
#include "CameraTab5.h"
#include "SettingsUi.h"
#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

using namespace SettingsUi;

namespace {

constexpr int      kPrevW  = CAMERA_WIDTH / 2;    // 640
constexpr int      kPrevH  = CAMERA_HEIGHT / 2;   // 360
constexpr uint32_t kTickMs = 40;                  // hoechstens 25 Bilder/s anzeigen

CameraViewConfig s_cfg   = {};
lv_obj_t*    s_root  = nullptr;
lv_obj_t*    s_img   = nullptr;
lv_obj_t*    s_hint  = nullptr;
lv_obj_t*    s_stats = nullptr;
lv_timer_t*  s_timer = nullptr;
uint16_t*    s_pix   = nullptr;     // angezeigtes Bild (RGB565, PSRAM)
lv_img_dsc_t s_dsc;

// Bildraten-Messung
uint32_t s_rateT0     = 0;
uint32_t s_rateCam0   = 0;
uint32_t s_shown      = 0;
uint32_t s_rateShown0 = 0;

// 2:1 verkleinern und dabei ausrichten. Der Sensor liefert auf dem Kopf
// (beide Achsen gespiegelt); ein Spiegelbild kehrt die waagrechte Achse
// zusaetzlich um.
void downscale(const uint16_t* src) {
    const bool flipY = CAMERA_UPSIDE_DOWN;
    const bool flipX = CAMERA_UPSIDE_DOWN != s_cfg.mirror;
    for (int y = 0; y < kPrevH; y++) {
        const int       sy  = flipY ? (CAMERA_HEIGHT - 1 - 2 * y) : 2 * y;
        const uint16_t* in  = src + (size_t)sy * CAMERA_WIDTH;
        uint16_t*       out = s_pix + (size_t)y * kPrevW;
        if (flipX) {
            for (int x = 0; x < kPrevW; x++) out[x] = in[CAMERA_WIDTH - 1 - 2 * x];
        } else {
            for (int x = 0; x < kPrevW; x++) out[x] = in[2 * x];
        }
    }
}

void updateStats() {
    if (!s_stats) return;
    const uint32_t now = millis();
    if (now - s_rateT0 < 1000) return;
    const uint32_t cam = CameraTab5_FrameCount();
    const float    s   = (now - s_rateT0) / 1000.0f;
    float gainR = 0, gainB = 0;
    CameraTab5_WhiteGains(gainR, gainB);
    float expMs = 0, expGain = 1;
    int   expBright = -1;
    CameraTab5_Exposure(expMs, expGain, expBright);
    char buf[160];
    snprintf(buf, sizeof(buf), "Kamera %.0f/s  |  Anzeige %.0f/s  |  Weissabgleich R %.2f B %.2f  |  "
                               "Belichtung %.1f ms x%.1f (Helligkeit %d)",
             (cam - s_rateCam0) / s, (s_shown - s_rateShown0) / s, gainR, gainB, expMs, expGain, expBright);
    lv_label_set_text(s_stats, buf);
    s_rateT0 = now; s_rateCam0 = cam; s_rateShown0 = s_shown;
}

void close(bool byUser) {
    if (!s_root) return;
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    CameraTab5_Stop();
    // Asynchron loeschen: close() laeuft im Klick-Ereignis bzw. Timer der Ansicht.
    lv_obj_del_async(s_root);
    s_root = s_img = s_hint = s_stats = nullptr;
    heap_caps_free(s_pix);
    s_pix = nullptr;

    void (*onClosed)(bool) = s_cfg.onClosed;
    s_cfg = {};
    if (onClosed) onClosed(byUser);
}

void tick(lv_timer_t*) {
    const uint16_t* frame = CameraTab5_AcquireFrame();
    if (frame) {
        downscale(frame);
        const bool done = s_cfg.onFrame && s_cfg.onFrame(frame);
        CameraTab5_ReleaseFrame();
        lv_obj_invalidate(s_img);
        s_shown++;
        if (done) { close(false); return; }
    }
    updateStats();
}

void closeClicked(lv_event_t*) {
    close(true);
}

lv_obj_t* label(lv_obj_t* parent, const char* text, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

} // namespace

void ViewCameraPreview_Open(const CameraViewConfig& cfg) {
    if (s_root) return;
    s_cfg = cfg;

    s_pix = (uint16_t*)heap_caps_calloc((size_t)kPrevW * kPrevH, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_pix) {
        Serial.println("[Kamera] Kein PSRAM fuer die Anzeige.");
        if (s_cfg.onClosed) s_cfg.onClosed(false);
        s_cfg = {};
        return;
    }

    // Deckende Ebene ueber allem; faengt alle Beruehrungen ab.
    s_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = label(s_root, cfg.title ? cfg.title : "Kamera", lv_color_white());
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    memset(&s_dsc, 0, sizeof(s_dsc));
    s_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_dsc.header.w  = kPrevW;
    s_dsc.header.h  = kPrevH;
    s_dsc.data_size = (uint32_t)kPrevW * kPrevH * sizeof(uint16_t);
    s_dsc.data      = (const uint8_t*)s_pix;
    s_img = lv_img_create(s_root);
    lv_img_set_src(s_img, &s_dsc);
    lv_obj_align(s_img, LV_ALIGN_CENTER, 0, -20);

    s_hint = label(s_root, cfg.hint ? cfg.hint : "", lv_color_white());
    lv_obj_set_width(s_hint, kPrevW);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(s_hint, s_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    if (cfg.showStats) {
        s_stats = label(s_root, "Kamera startet ...", lv_color_hex(0xAAAAAA));
        lv_obj_align(s_stats, LV_ALIGN_BOTTOM_LEFT, 32, -36);
    }

    lv_obj_t* btn = addButton(s_root, cfg.closeText ? cfg.closeText : "Schliessen", Btn::Primary, closeClicked);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -32, -24);

    if (!CameraTab5_Start()) {
        lv_label_set_text(s_hint, "Kamera konnte nicht starten - Grund im Monitor ([Kamera]).");
        return;
    }
    s_rateT0 = millis(); s_rateCam0 = 0; s_shown = 0; s_rateShown0 = 0;
    s_timer = lv_timer_create(tick, kTickMs, nullptr);
}

void ViewCameraPreview_SetHint(const char* text) {
    if (s_hint) lv_label_set_text(s_hint, text ? text : "");
}

bool ViewCameraPreview_IsOpen() {
    return s_root != nullptr;
}
