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
    
    // WICHTIGSTER FIX HIER: Der riesige Render-Buffer (76 KB) liegt jetzt im PSRAM!
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_aligned_alloc(64, 1280 * 30 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 1280 * 30);

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