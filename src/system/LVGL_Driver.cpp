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

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;

    M5.Display.startWrite();
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
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
    // Kosten: 76 KB von rund 375 KB freiem internen Speicher. Das ist der
    // knappe Topf (der SDIO-WLAN-Treiber bedient sich daraus), deshalb bleibt
    // es bei EINEM Puffer statt zweier - ein zweiter braechte ohne DMA im
    // Flush ohnehin nichts, weil dort synchron gearbeitet wird.
    //
    // Faellt die Reservierung aus, weichen wir auf PSRAM zurueck: langsamer,
    // aber lauffaehig. Ein Absturz beim Start waere die schlechtere Antwort.
    static const size_t DRAW_BUF_PX = 1280 * 30;

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