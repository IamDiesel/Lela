#include "LVGL_Driver.h"
#include "DebugFlags.h"
#include "PanelBlit.h"
#include <M5Unified.h>

static SemaphoreHandle_t lvgl_mux = NULL;

bool lvgl_port_lock(uint32_t timeout_ms) {
    const uint32_t timeout_ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void) {
    xSemaphoreGiveRecursive(lvgl_mux);
}

const char* Lvgl_LockHolderName() {
    if (!lvgl_mux) return "-";
    const TaskHandle_t holder = xSemaphoreGetMutexHolder(lvgl_mux);
    return holder ? pcTaskGetName(holder) : "-";
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

#if PERF_PROFILE
// Zeiten des laufenden Bildaufbaus; ausgewertet und zurueckgesetzt in
// disp_monitor. Eigene Messung statt des 'time'-Werts von LVGL: lv_tick
// zaehlt die Hauptschleife weiter (lv_tick_inc), steht also waehrend eines
// langen Bildaufbaus still.
static uint32_t s_flushUs = 0;          // Summe Uebertragen + Drehen ins Panel
static uint32_t s_syncUs = 0;           // davon Abgleich Cache -> Bildspeicher (nur schneller Weg)
static uint32_t s_firstFlushUs = 0;     // Beginn des ersten Streifens (0 = keiner)
static uint32_t s_lastFlushEndUs = 0;

// Nach jedem Bildaufbau (LVGL). Dauer vom ersten bis zum letzten Streifen -
// das Zeichnen des ersten Streifens fehlt darin (~1/45 des Bildes).
static void disp_monitor(lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px) {
    (void)time;
    const uint32_t screenPx = (uint32_t)disp_drv->hor_res * disp_drv->ver_res;
    if (px * 2 >= screenPx && s_firstFlushUs != 0) {
        Serial.printf("[Perf] Bild: %lu ms, davon Flush %lu ms (Abgleich %lu ms), %lu%% der Flaeche\n",
                      (unsigned long)((s_lastFlushEndUs - s_firstFlushUs) / 1000),
                      (unsigned long)(s_flushUs / 1000), (unsigned long)(s_syncUs / 1000),
                      (unsigned long)(px * 100 / screenPx));
    }
    s_flushUs = 0;
    s_syncUs = 0;
    s_firstFlushUs = 0;
}
#endif

// Schneller Weg: Bereich, der seit dem letzten Abgleich Cache -> Bildspeicher
// beschrieben wurde (ein Bild besteht aus vielen Streifen).
static lv_area_t s_unsynced;
static bool      s_unsyncedValid = false;

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
#if PERF_PROFILE
    const uint32_t flushStart = micros();
    if (s_firstFlushUs == 0) s_firstFlushUs = flushStart;
#endif
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    const uint16_t* data = (const uint16_t *)&color_p->full;

    lv_area_t hole;
    const bool clip = s_reservedActive && _lv_area_intersect(&hole, area, &s_reserved);

    // Schneller Weg: direkt und gedreht in den Bildspeicher (PanelBlit.h).
    // Faellt von selbst auf M5GFX zurueck, wenn sein Selbsttest nicht passt.
#if LVGL_FAST_FLUSH
    const bool fast = PanelBlit_Ready();
#else
    const bool fast = false;
#endif

    M5.Display.startWrite();
    if (fast) {
        if (!clip) {
            PanelBlit_Push(data, w, area->x1, area->y1, area->x1, area->y1, area->x2, area->y2);
        } else {
            // Dieselben vier Streifen um die Aussparung wie unten.
            PanelBlit_Push(data, w, area->x1, area->y1, area->x1, area->y1,    area->x2, hole.y1 - 1);
            PanelBlit_Push(data, w, area->x1, area->y1, area->x1, hole.y2 + 1, area->x2, area->y2);
            PanelBlit_Push(data, w, area->x1, area->y1, area->x1, hole.y1,     hole.x1 - 1, hole.y2);
            PanelBlit_Push(data, w, area->x1, area->y1, hole.x2 + 1, hole.y1,  area->x2, hole.y2);
        }
        // Cache -> Bildspeicher (M5.Display.display) EINMAL je Bild, mit dem
        // letzten Streifen. Ein Streifen ist im hochkant liegenden Speicher ein
        // schmales, hohes Band ueber fast alle Zeilen: Der Abgleich je Streifen
        // liefe jedes Mal ueber den ganzen Bildspeicher (so arbeitet der Weg
        // ueber M5GFX bei jedem endWrite). Geschrieben werden dabei ohnehin nur
        // veraenderte Cache-Zeilen; was der Cache vorher verdraengt, steht
        // schon im Speicher.
        if (s_unsyncedValid) _lv_area_join(&s_unsynced, &s_unsynced, area);
        else { s_unsynced = *area; s_unsyncedValid = true; }
        if (lv_disp_flush_is_last(disp_drv)) {
#if PERF_PROFILE
            const uint32_t syncStart = micros();
#endif
            M5.Display.display(s_unsynced.x1, s_unsynced.y1,
                               s_unsynced.x2 - s_unsynced.x1 + 1, s_unsynced.y2 - s_unsynced.y1 + 1);
            s_unsyncedValid = false;
#if PERF_PROFILE
            s_syncUs += micros() - syncStart;
#endif
        }
    } else if (!clip) {
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

#if PERF_PROFILE
    s_lastFlushEndUs = micros();
    s_flushUs += s_lastFlushEndUs - flushStart;
#endif
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
    // Geschichte der Groesse:
    // - Frueher 30 Zeilen (75 KB). Messung 14.10.2026: HA-Bildschirm + Video-
    //   Overlay drueckten den groessten DMA-Block auf 4-14 KB, einmal folgte
    //   "assert failed: sdio_rx_get_buffer". Deshalb 16 Zeilen (40 KB).
    // - 17.11.2026, Versuch mit 32 Zeilen (80 KB): Scrollen in den
    //   Einstellungen 85 statt 111 ms je Bild (halb so viele Streifen =
    //   halb so viele Gaenge durch den Objektbaum). Aber: HA-App + Overlay +
    //   Karte drueckten den groessten DMA-Block auf 7-11 KB, das Video wurde
    //   pausiert ("Speicher knapp"). Nach einer HA-Sitzung mit Karte bleibt
    //   der interne Speicher zerstueckelt (101 KB frei, groesster DMA-Block
    //   16 KB); die 40 KB Polster fehlen dann. Deshalb zurueck auf 16.
    //   32 Zeilen erst wieder, wenn die langlebigen HA-/Karten-Daten nicht
    //   mehr im internen RAM liegen (Projekt-Doku "Scroll-Messung").
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
#if PERF_PROFILE
    disp_drv.monitor_cb = disp_monitor;
#endif
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}
