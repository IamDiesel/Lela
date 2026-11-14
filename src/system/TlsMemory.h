#pragma once
// ==============================================================================
// TlsMemory - mbedTLS-Speicher ins PSRAM
// ==============================================================================
// Die vorkompilierten Arduino-Bibliotheken fuer den P4 sind mit
// CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1 gebaut: mbedTLS legt JEDE Allokation im
// internen RAM an - auch die beiden Datensatzpuffer einer Verbindung
// (CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN 16384, zusammen rund 35-40 KB).
//
// Internes RAM ist auf dem Tab5 der Engpass (siehe HeapMonitor.h): der
// SDIO-WLAN-Treiber braucht fuer jeden Empfangsblock zusammenhaengenden
// DMA-faehigen Speicher. Eine TLS-Verbindung mit internen Puffern waere ein
// direkter Konkurrent - genau die Lage, die zu
//     assert failed: sdio_rx_get_buffer sdio_drv.c:674
// fuehrt.
//
// Die Bibliotheken definieren MBEDTLS_PLATFORM_MEMORY ohne Calloc-Makro
// (mbedtls/port/include/mbedtls/esp_config.h). Damit laesst sich der
// Allokator zur Laufzeit ueber mbedtls_platform_set_calloc_free() tauschen -
// ohne sdkconfig-Aenderung und ohne Neubau der Bibliotheken.
//
// Wirkung: systemweit fuer alle mbedTLS-Nutzer. Auf dem P4 ist das nur unser
// eigener Code (Kartenkacheln); der WLAN-Supplicant laeuft auf dem C6
// (ESP-Hosted), HA-WebSocket und MQTT sind unverschluesselt.
//
// Kryptobeschleuniger: AES/SHA arbeiten per DMA. Liegen Puffer nicht im
// DMA-faehigen Bereich, nimmt der IDF-Treiber selbst kurzlebige interne
// Zwischenpuffer - das ist klein und wird in Schritt 0 gemessen.
// ==============================================================================

#include <Arduino.h>

// Einmalig in setup() aufrufen, bevor die erste TLS-Verbindung entsteht.
// Mehrfacher Aufruf ist harmlos.
void TlsMemory_UsePsram();
