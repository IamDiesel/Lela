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
#include <stdlib.h>

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

// Dasselbe fuer Standard-Container, z. B. std::vector<T, HaPsramStdAllocator<T>>.
// Ohne ihn landen kleine Vektoren (<= 4 KB, SPIRAM_MALLOC_ALWAYSINTERNAL) im
// internen RAM - bei vielen kleinen, wachsenden Listen (Positionsverlauf)
// genau die Zerstueckelung, die der WLAN-Treiber nicht vertraegt.
template <typename T>
struct HaPsramStdAllocator {
    using value_type = T;
    HaPsramStdAllocator() = default;
    template <typename U> HaPsramStdAllocator(const HaPsramStdAllocator<U>&) {}

    T* allocate(size_t n) {
        void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM);
        if (!p) abort();   // wie std::allocator ohne Ausnahmen
        return static_cast<T*>(p);
    }
    void deallocate(T* p, size_t) { heap_caps_free(p); }

    template <typename U> bool operator==(const HaPsramStdAllocator<U>&) const { return true; }
    template <typename U> bool operator!=(const HaPsramStdAllocator<U>&) const { return false; }
};
