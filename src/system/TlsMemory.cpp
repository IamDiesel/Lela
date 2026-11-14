// ==============================================================================
// TlsMemory - Implementierung (siehe Header)
// ==============================================================================

#include "TlsMemory.h"
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

// mbedTLS erwartet calloc-Semantik (genullter Speicher, nullptr bei Fehler).
static void* psramCalloc(size_t count, size_t size) {
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

// heap_caps_free() gibt Bloecke beider Speicherarten frei. Falls vor dem
// Umschalten schon etwas intern angelegt wurde, wird es trotzdem korrekt
// freigegeben.
static void psramFree(void* ptr) {
    heap_caps_free(ptr);
}

void TlsMemory_UsePsram() {
    static bool s_done = false;
    if (s_done) return;

    if (mbedtls_platform_set_calloc_free(psramCalloc, psramFree) == 0) {
        s_done = true;
        Serial.println("[TLS] mbedTLS-Speicher liegt im PSRAM.");
    } else {
        Serial.println("[TLS] FEHLER: mbedTLS-Allokator laesst sich nicht umstellen - TLS bleibt im internen RAM.");
    }
}
