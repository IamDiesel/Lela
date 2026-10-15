#include "LVGL_Driver.h"
#include <M5Unified.h>

static SemaphoreHandle_t lvgl_mux = NULL;

bool lvgl_port_lock(uint32_t timeout_ms) {
    const uint32_t timeout_ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void) {
    xSemaphoreGiveRecursive(lvgl_mux);
}

// Ausgesparte Flaeche, siehe Header. Nur von der Hauptschleife geschrieben,
// nur im Flush (ebenfalls Hauptschleife) gelesen.
static lv_area_t s_reserved;
static bool      s_reservedActive = false;

void Lvgl_SetReservedArea(const lv_area_t* area) {
    if (area) {
        s_reserved = *area;
        s_reservedActive = true;
    } else {
        s_reservedActive = false;
    }
}

// Schreibt den Flush-Puffer, beschnitten auf ein Teilrechteck. M5GFX
// uebernimmt das Beschneiden selbst (Clip-Rechteck) und rechnet den Versatz im
// Quellpuffer korrekt mit - die Quelle bleibt der volle Flush-Bereich.
static void pushClipped(const lv_area_t* area, int w, int h, const uint16_t* data,
                        int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2) {
    if (cx1 > cx2 || cy1 > cy2) return;
    M5.Display.setClipRect(cx1, cy1, cx2 - cx1 + 1, cy2 - cy1 + 1);
    M5.Display.pushImage(area->x1, area->y1, w, h, data);
}

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    const uint16_t* data = (const uint16_t *)&color_p->full;

    lv_area_t hole;
    const bool clip = s_reservedActive && _lv_area_intersect(&hole, area, &s_reserved);

    M5.Display.startWrite();
    if (!clip) {
        M5.Display.pushImage(area->x1, area->y1, w, h, data);
    } else {
        // Bis zu vier Streifen um die Aussparung herum: oben, unten (volle
        // Breite), links und rechts (Hoehe der Aussparung).
        pushClipped(area, w, h, data, area->x1, area->y1,     area->x2, hole.y1 - 1);
        pushClipped(area, w, h, data, area->x1, hole.y2 + 1,  area->x2, area->y2);
        pushClipped(area, w, h, data, area->x1, hole.y1,      hole.x1 - 1, hole.y2);
        pushClipped(area, w, h, data, hole.x2 + 1, hole.y1,   area->x2, hole.y2);
        M5.Display.clearClipRect();
    }
    M5.Display.endWrite();

    lv_disp_flush_ready(disp_drv);
}

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    M5.update();
    auto touch = M5.Touch.getDetail();
    
    if (touch.isPressed()) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void Lvgl_Init() {
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    
    // Der Zeichenpuffer gehoert ins INTERNE SRAM, nicht ins PSRAM.
    //
    // LVGL liest und schreibt ihn beim Rendern staendig - Pixel fuer Pixel,
    // mit schlechter Lokalitaet. Das ist der denkbar unguenstigste Zugriff auf
    // PSRAM, wo jeder Fehlgriff eine 64-Byte-Cachezeile nachzieht. Internes
    // SRAM hat weder Cache-Umweg noch Bus-Konkurrenz mit dem Videopfad, der
    // PSRAM ohnehin dauerhaft belastet (Decoderausgabe, Framebuffer).
    //
    // Kosten: 40 KB (16 Zeilen) vom internen Speicher. Das ist der knappe
    // Topf: der SDIO-WLAN-Treiber braucht daraus fuer jeden Empfangsblock bis
    // zu 7,5 KB zusammenhaengenden, DMA-faehigen Speicher. Deshalb EIN Puffer
    // statt zweier - ein zweiter braechte ohne DMA im Flush ohnehin nichts,
    // weil dort synchron gearbeitet wird.
    //
    // Frueher 30 Zeilen (75 KB). Messung 14.10.2026: HA-Bildschirm + Video-
    // Overlay drueckten den groessten DMA-Block auf 4-14 KB, einmal folgte
    // "assert failed: sdio_rx_get_buffer". 16 Zeilen geben 35 KB zurueck.
    // LVGL zeichnet dafuer in mehr, kleineren Streifen; die Gesamtzeit
    // bestimmt ohnehin der Flush auf das gedrehte Panel (Pixel fuer Pixel).
    //
    // Faellt die Reservierung aus, weichen wir auf PSRAM zurueck: langsamer,
    // aber lauffaehig. Ein Absturz beim Start waere die schlechtere Antwort.
    static const size_t DRAW_BUF_PX = 1280 * 16;

    static lv_color_t *buf1 = (lv_color_t *)heap_caps_aligned_alloc(
        64, DRAW_BUF_PX * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (buf1 == nullptr) {
        Serial.println("[LVGL] WARNUNG: Zeichenpuffer passt nicht ins interne RAM, nutze PSRAM.");
        buf1 = (lv_color_t *)heap_caps_aligned_alloc(
            64, DRAW_BUF_PX * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    } else {
        Serial.printf("[LVGL] Zeichenpuffer im internen SRAM (%u KB).\n",
                      (unsigned)(DRAW_BUF_PX * sizeof(lv_color_t) / 1024));
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DRAW_BUF_PX);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 1280;
    disp_drv.ver_res = 720;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

void Set_Backlight(uint8_t val) {
    M5.Display.setBrightness(val);
}