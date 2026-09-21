#pragma once
// ==============================================================================
// Dma2dGuard - Zaehler des begrenzten 2D-DMA-Kanal-Resets
// ==============================================================================
// Hintergrund und Begruendung stehen in Dma2dGuard.c.
//
// Bleiben alle Werte auf null, hat der Schutz nie gegriffen. Steigt
// g_dma2dGuardTxTimeouts, ist der Geraeteneustart aus performance.md 9.2
// vermieden worden - jeder Zaehlerschritt entspricht einem Absturz, der nicht
// stattgefunden hat.
// ==============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint32_t g_dma2dGuardTxTimeouts;   // Fristablauf Sendeseite
extern volatile uint32_t g_dma2dGuardRxTimeouts;   // Fristablauf Empfangsseite
extern volatile uint32_t g_dma2dGuardMaxCycles;    // laengste Wartezeit in CPU-Zyklen

#ifdef __cplusplus
}
#endif
