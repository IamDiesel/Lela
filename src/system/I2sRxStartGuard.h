#pragma once
// ==============================================================================
// I2sRxStartGuard - begrenzt das Warten beim Start des I2S-Empfangs (Mikrofon)
// ==============================================================================
// Hintergrund und Begruendung stehen in I2sRxStartGuard.c.
//
// Der Schutz ersetzt i2s_channel_enable() (Schalter -Wl,--wrap in
// platformio.ini). Diese Schnittstelle liefert nur das Ergebnis: Wer einen
// Empfangskanal startet, liest die Zaehler vorher und nachher und erkennt so,
// ob der Start geprueft und ob er abgelehnt wurde.
// ==============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ergebnis der Pruefung vor dem Start.
typedef enum {
    I2S_RX_GUARD_OK = 0,     // Hardware quittiert, der echte Start lief
    I2S_RX_GUARD_PENDING,    // Stufe 1: eine schon angeforderte Uebernahme blieb haengen
    I2S_RX_GUARD_PROBE,      // Stufe 2: die Probe-Uebernahme wurde nicht quittiert
    I2S_RX_GUARD_TEST        // kuenstlich abgelehnt (Testwerkzeug)
} I2sRxGuardStage;

typedef struct {
    uint32_t starts;         // gepruefte Empfangsstarts seit dem Einschalten
    uint32_t refused;        // davon abgelehnt (= vermiedene Haenger)
    uint32_t maxWaitUs;      // laengste Quittungszeit eines gesunden Starts
    uint8_t  lastResult;     // I2sRxGuardStage des juengsten Starts (auch OK)

    // Zustand beim letzten abgelehnten Start - Material fuer die Ursachensuche.
    uint8_t  lastStage;      // I2sRxGuardStage
    uint32_t lastRxConf;     // I2S rx_conf (Bit 8 = rx_update)
    uint32_t lastClk[4];     // HP_SYS_CLKRST peri_clk_ctrl11..14 (Takt-Freigabe, Quelle, Teiler)
} I2sRxGuardStats;

// Momentaufnahme der Zaehler. Aus jedem Task aufrufbar.
void I2sRxStartGuard_GetStats(I2sRxGuardStats* out);

// Kurztext zu einer Stufe, fuer Logzeilen.
const char* I2sRxStartGuard_StageText(uint8_t stage);

// Testwerkzeug: lehnt den naechsten Empfangsstart ab, ohne die Hardware zu
// fragen. Jeder Aufruf zaehlt einen weiteren Start dazu - so laesst sich der
// Wiederholversuch (1x) und der Abbruch nach zwei Fehlstarts (2x) pruefen.
void I2sRxStartGuard_TestRefuseNext(void);

#ifdef __cplusplus
}
#endif
