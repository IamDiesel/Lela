// ==============================================================================
// HeapMonitor - Implementierung (siehe Header)
// ==============================================================================

#include "HeapMonitor.h"
#include <esp_heap_caps.h>

void HeapMonitor_Log(const char* context) {
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    Serial.printf("[Heap] %s: intern frei %u KB, groesster Block %u KB, DMA-Block %u KB, Minimum %u KB\n",
                  context ? context : "",
                  (unsigned)(heap_caps_get_free_size(caps) / 1024),
                  (unsigned)(heap_caps_get_largest_free_block(caps) / 1024),
                  (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_DMA) / 1024),
                  (unsigned)(heap_caps_get_minimum_free_size(caps) / 1024));
}

void HeapMonitor_Update(uint32_t intervalMs, const char* context) {
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (last != 0 && now - last < intervalMs) return;
    last = now;
    HeapMonitor_Log(context);
}
