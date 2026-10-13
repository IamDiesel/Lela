#pragma once
// ==============================================================================
// HeapMonitor - Diagnose des internen RAMs
// ==============================================================================
// Das interne RAM ist auf dem Tab5 der Engpass: Der WLAN-Treiber (ESP-Hosted
// ueber SDIO) braucht fuer jeden Empfangsblock 4,6 KB zusammenhaengenden,
// DMA-faehigen Speicher. Fehlt der, bricht er mit
//     assert failed: sdio_rx_get_buffer sdio_drv.c:674
// ab. Entscheidend ist deshalb nicht nur der freie Speicher insgesamt, sondern
// der groesste zusammenhaengende Block.
//
// Ausgabe:
//   [Heap] <Anlass>: intern frei X KB, groesster Block Y KB, DMA-Block Z KB, Minimum M KB
// ==============================================================================

#include <Arduino.h>

// Schreibt sofort eine Zeile mit dem aktuellen Stand.
void HeapMonitor_Log(const char* context);

// Periodische Ausgabe (hoechstens alle 'intervalMs'); im Hauptloop aufrufen.
void HeapMonitor_Update(uint32_t intervalMs = 60000);
