// ==============================================================================
// Dma2dGuard - begrenzt den Kanal-Reset des 2D-DMA-Controllers
// ==============================================================================
// Problem (siehe src/doc/performance.md, 9.2 und video_refactoring.md, 2.8):
//
// Vor jeder Benutzung setzt der IDF-Treiber einen 2D-DMA-Kanal zurueck. Die
// HAL wartet dabei unbegrenzt darauf, dass die Hardware den Reset freigibt:
//
//     void dma2d_hal_tx_reset_channel(dma2d_hal_context_t *hal, uint32_t ch)
//     {
//         while (!dma2d_ll_tx_is_reset_avail(hal->dev, ch)) {}   // <- Zeile 18
//         dma2d_ll_tx_reset_channel(hal->dev, ch);
//     }
//
// Auf Silizium-Revision v1.3 wird `reset_avail` fuer bestimmte Kanaele nie
// gesetzt (esp-idf #19108, #18999, #19096). Der Aufrufer
// (`acquire_free_channels_for_trans`) haelt dabei eine Spinlock mit gesperrten
// Interrupts - aus der Warteschleife wird also kein haengender Task, sondern
// ein Interrupt-Watchdog-Panic und ein Neustart des Geraets.
//
// Loesung: dieselbe Funktion, aber mit begrenzter Wartezeit. Laeuft die Frist
// ab, wird der Reset trotzdem ausgeloest und die Fahrt fortgesetzt. Im
// schlimmsten Fall kostet das ein verworfenes oder fehlerhaftes Bild - statt
// eines Neustarts mitten in der Nachtueberwachung.
//
// Wie die Umleitung funktioniert:
// Die HAL liegt nur als Binaerbibliothek vor (`libhal.a:dma2d_hal.c.obj`), ist
// also nicht patchbar. Der einzige Aufrufer sitzt in einem ANDEREN Archiv
// (`libesp_hw_support.a:dma2d.c.obj`) und referenziert das Symbol als
// undefiniert. Genau dafuer gibt es `-Wl,--wrap=<symbol>`: der Linker leitet
// jede solche Referenz auf `__wrap_<symbol>` um. Der Schalter steht in
// platformio.ini.
//
// Wichtig: Die Umleitung gilt fuer ALLE Nutzer des 2D-DMA, also auch fuer den
// Hardware-JPEG-Decoder. Das ist beabsichtigt - es ist derselbe Fehler.
//
// Diese Datei ist bewusst C und nicht C++: `__wrap_...` muss C-Bindung haben.
// ==============================================================================

#include <stdint.h>
#include <stdbool.h>

#include "hal/dma2d_hal.h"
#include "hal/dma2d_ll.h"
#include "esp_cpu.h"

// ------------------------------------------------------------------------------
// Frist
// ------------------------------------------------------------------------------
// Ein gesunder Reset ist sofort fertig - die Schleife laeuft im Normalfall
// null- bis einmal. Die Frist muss also nur zwei Bedingungen erfuellen: gross
// genug, um keinen funktionierenden Reset abzuschneiden, und klar kleiner als
// der Interrupt-Watchdog (Voreinstellung 300 ms).
//
// Erste Fassung waren 400000 Zyklen (~1 ms). Die Messung hat gezeigt, dass
// jeder Fristablauf die Frist auch voll ausschoepft (max 400736 Zyklen) - der
// Reset kommt also entweder sofort oder gar nicht, ein Dazwischen gibt es
// nicht. Sobald der Fehler einmal auftritt, greift er bei JEDEM Bild; 1 ms je
// Bild waren damit reine Wartezeit. 20000 Zyklen (~50 us bei 400 MHz) lassen
// einem gesunden Reset immer noch das Zwanzigfache dessen, was er braucht.
#define DMA2D_GUARD_MAX_CYCLES  20000u

// ------------------------------------------------------------------------------
// Diagnose
// ------------------------------------------------------------------------------
// Bleiben diese Zaehler auf null, hat der Schutz nie gegriffen und kostet
// nichts. Steigen sie, wissen wir, wie oft dem Fehler ausgewichen wurde - das
// ist die Messgroesse fuer die Frage, ob der PPA dauerhaft tragfaehig ist.
volatile uint32_t g_dma2dGuardTxTimeouts = 0;
volatile uint32_t g_dma2dGuardRxTimeouts = 0;

// Laengste beobachtete Wartezeit in CPU-Zyklen. Zeigt, ob wir nah an der Frist
// liegen oder ob der Reset im Normalfall sofort durchgeht.
volatile uint32_t g_dma2dGuardMaxCycles = 0;

static inline void noteCycles(uint32_t cycles)
{
    if (cycles > g_dma2dGuardMaxCycles) g_dma2dGuardMaxCycles = cycles;
}

// ------------------------------------------------------------------------------
// Ersatz fuer dma2d_hal_tx_reset_channel()
// ------------------------------------------------------------------------------
void __wrap_dma2d_hal_tx_reset_channel(dma2d_hal_context_t *hal, uint32_t channel)
{
    const uint32_t start = esp_cpu_get_cycle_count();
    uint32_t waited = 0;

    while (!dma2d_ll_tx_is_reset_avail(hal->dev, channel)) {
        waited = esp_cpu_get_cycle_count() - start;   // Ueberlauf ist unschaedlich
        if (waited > DMA2D_GUARD_MAX_CYCLES) {
            g_dma2dGuardTxTimeouts++;
            break;
        }
    }

    noteCycles(waited);

    // Auch nach Fristablauf zuruecksetzen: die Absicht des Aufrufers ist ein
    // Kanal im definierten Zustand. Ihn unangetastet zu lassen waere die
    // schlechtere Wahl - dann arbeitete der naechste Transfer garantiert auf
    // einem Kanal mit altem Zustand.
    dma2d_ll_tx_reset_channel(hal->dev, channel);
}

// ------------------------------------------------------------------------------
// Ersatz fuer dma2d_hal_rx_reset_channel()
// ------------------------------------------------------------------------------
// Die Empfangsseite hat dieselbe Warteschleife. Bisher ist sie nie in einem
// Absturzbericht aufgetaucht, aber sie mit demselben Fehlerbild ungeschuetzt zu
// lassen waere nachlaessig.
void __wrap_dma2d_hal_rx_reset_channel(dma2d_hal_context_t *hal, uint32_t channel)
{
    const uint32_t start = esp_cpu_get_cycle_count();
    uint32_t waited = 0;

    while (!dma2d_ll_rx_is_reset_avail(hal->dev, channel)) {
        waited = esp_cpu_get_cycle_count() - start;
        if (waited > DMA2D_GUARD_MAX_CYCLES) {
            g_dma2dGuardRxTimeouts++;
            break;
        }
    }

    noteCycles(waited);

    dma2d_ll_rx_reset_channel(hal->dev, channel);
}
