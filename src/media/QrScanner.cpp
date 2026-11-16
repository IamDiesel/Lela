// ==============================================================================
// QrScanner - Implementierung (siehe QrScanner.h)
// ==============================================================================
#include "QrScanner.h"
#include "CameraTab5.h"
#include <quirc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

namespace {

constexpr uint32_t kTaskStack    = 20 * 1024;
constexpr UBaseType_t kTaskPrio  = 2;          // ueber Leerlauf, unter allem Wichtigen
constexpr BaseType_t  kTaskCore  = 0;          // LVGL und Audio laufen auf Kern 1
constexpr uint32_t kStopWaitMs   = 2000;       // laengste Auswertung abwarten
constexpr int      kLogFirstRuns = 3;          // Dauer der ersten Auswertungen loggen

struct quirc*        s_q     = nullptr;
struct quirc_code*   s_code  = nullptr;   // ~4 KB, im Heap statt auf dem Stapel
struct quirc_data*   s_data  = nullptr;   // ~9 KB, dito
TaskHandle_t         s_task  = nullptr;
portMUX_TYPE         s_lock  = portMUX_INITIALIZER_UNLOCKED;
volatile bool        s_busy  = false;     // Task wertet gerade aus
volatile bool        s_stop  = false;
volatile bool        s_taskDone = false;
String               s_result;            // unter s_lock
bool                 s_hasResult = false;
int                  s_runs  = 0;

// Graustufen nach ITU-R BT.601 (Gewichte 77/150/29 von 256).
void toGray(const uint16_t* src, uint8_t* dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const uint16_t p = src[i];
        const uint32_t r = (p >> 8) & 0xF8;
        const uint32_t g = (p >> 3) & 0xFC;
        const uint32_t b = (p << 3) & 0xF8;
        dst[i] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
    }
}

// Alle gefundenen Codes dekodieren; der erste lesbare wird gemeldet.
void decodeAll() {
    const int count = quirc_count(s_q);
    for (int i = 0; i < count; i++) {
        quirc_extract(s_q, i, s_code);
        quirc_decode_error_t err = quirc_decode(s_code, s_data);
        if (err == QUIRC_ERROR_DATA_ECC) {          // evtl. gespiegelt
            quirc_flip(s_code);
            err = quirc_decode(s_code, s_data);
        }
        if (err != QUIRC_SUCCESS) continue;

        const String text((const char*)s_data->payload, s_data->payload_len);
        portENTER_CRITICAL(&s_lock);
        s_result    = text;
        s_hasResult = true;
        portEXIT_CRITICAL(&s_lock);
        Serial.printf("[QR] Code erkannt (Version %d, %d Zeichen).\n",
                      s_data->version, s_data->payload_len);
        return;
    }
}

void task(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_stop) break;

        const uint32_t t0 = millis();
        quirc_end(s_q);
        decodeAll();
        if (s_runs < kLogFirstRuns) {
            s_runs++;
            Serial.printf("[QR] Auswertung %lu ms, %d Kandidaten.\n",
                          (unsigned long)(millis() - t0), quirc_count(s_q));
        }
        s_busy = false;
    }
    s_taskDone = true;
    vTaskDelete(nullptr);
}

void freeAll() {
    if (s_q) { quirc_destroy(s_q); s_q = nullptr; }
    heap_caps_free(s_code); s_code = nullptr;
    heap_caps_free(s_data); s_data = nullptr;
}

} // namespace

bool QrScanner_Start() {
    if (s_task) return true;

    s_q    = quirc_new();
    s_code = (struct quirc_code*)heap_caps_calloc(1, sizeof(struct quirc_code), MALLOC_CAP_SPIRAM);
    s_data = (struct quirc_data*)heap_caps_calloc(1, sizeof(struct quirc_data), MALLOC_CAP_SPIRAM);
    if (!s_q || !s_code || !s_data || quirc_resize(s_q, CAMERA_WIDTH, CAMERA_HEIGHT) < 0) {
        Serial.println("[QR] Kein Speicher fuer quirc.");
        freeAll();
        return false;
    }

    s_busy = false; s_stop = false; s_taskDone = false; s_runs = 0;
    portENTER_CRITICAL(&s_lock);
    s_result = ""; s_hasResult = false;
    portEXIT_CRITICAL(&s_lock);

    if (xTaskCreatePinnedToCore(task, "QrScan", kTaskStack, nullptr, kTaskPrio, &s_task, kTaskCore) != pdPASS) {
        Serial.println("[QR] Task nicht gestartet.");
        s_task = nullptr;
        freeAll();
        return false;
    }
    Serial.printf("[QR] Bereit (quirc %s, %dx%d).\n", quirc_version(), CAMERA_WIDTH, CAMERA_HEIGHT);
    return true;
}

void QrScanner_Stop() {
    if (!s_task) return;
    s_stop = true;
    xTaskNotifyGive(s_task);
    const uint32_t t0 = millis();
    while (!s_taskDone && millis() - t0 < kStopWaitMs) delay(5);
    if (!s_taskDone) {
        // Haengt quirc? Dann den Speicher stehen lassen statt ihn unter dem
        // laufenden Task freizugeben.
        Serial.println("[QR] Task beendet sich nicht - Speicher bleibt belegt.");
        return;
    }
    s_task = nullptr;
    freeAll();
}

bool QrScanner_Offer(const uint16_t* frame) {
    if (!s_task || s_busy || s_stop || !frame) return false;
    int w = 0, h = 0;
    uint8_t* img = quirc_begin(s_q, &w, &h);
    toGray(frame, img, (size_t)w * h);
    s_busy = true;
    xTaskNotifyGive(s_task);
    return true;
}

bool QrScanner_TakeResult(String& text) {
    bool has;
    portENTER_CRITICAL(&s_lock);
    has = s_hasResult;
    if (has) { text = s_result; s_hasResult = false; }
    portEXIT_CRITICAL(&s_lock);
    return has;
}
