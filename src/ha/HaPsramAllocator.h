#pragma once
// ==============================================================================
// HaPsramAllocator - ArduinoJson-Speicher im PSRAM
// ==============================================================================
// Alle JSON-Dokumente der HA-Schicht (Konfiguration, Service-Calls, Import,
// Live-Abo, Entitaetskatalog) legen ihren Speicher hier an. Internes SRAM ist
// knapp und wird vom Video- und WLAN-Pfad gebraucht.
//
// Frueher gab es drei identische Kopien dieser Klasse (Config, ServiceCaller,
// LovelaceParser). Die Klasse ist zustandslos, eine gemeinsame Instanz ist
// daher aus jedem Task heraus sicher nutzbar.
// ==============================================================================

#include <ArduinoJson.h>
#include <esp_heap_caps.h>

class HaPsramAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    void deallocate(void* pointer) override {
        heap_caps_free(pointer);
    }
    void* reallocate(void* pointer, size_t new_size) override {
        return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM);
    }
};

// Gemeinsame Instanz, z. B.: JsonDocument doc(HaPsramAlloc());
inline HaPsramAllocator* HaPsramAlloc() {
    static HaPsramAllocator instance;
    return &instance;
}
