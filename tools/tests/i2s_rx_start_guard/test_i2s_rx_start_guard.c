// ==============================================================================
// PC-Test fuer system/I2sRxStartGuard
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   gcc -std=gnu17 -O1 -g -fsanitize=address,undefined
//       -I tools/tests/i2s_rx_start_guard/stubs -I src/system
//       src/system/I2sRxStartGuard.c
//       tools/tests/i2s_rx_start_guard/test_i2s_rx_start_guard.c -o /tmp/i2sguardtest
//   /tmp/i2sguardtest
//
// Der Schutz haengt an Treiber und Registern des ESP32-P4. Der Ordner stubs/
// ersetzt beides durch das Noetigste; die "Hardware" ist eine kleine
// Simulation in dieser Datei: Sie quittiert eine Uebernahme (rx_update) nach
// einer einstellbaren Zeit - oder nie.
//
// Geprueft wird:
//   * Lautsprecher und ungueltige Handles laufen unveraendert zum Original.
//   * Gesunder Start: Original wird gerufen, nichts abgelehnt.
//   * Haengende Uebernahme (Stufe 1) und unbeantwortete Probe (Stufe 2):
//     Start abgelehnt, Original NICHT gerufen, Wartezeit begrenzt,
//     Registerauszug festgehalten.
//   * Testwerkzeug: kuenstliche Ablehnung, ohne die Hardware anzufassen.
// ==============================================================================
#include "I2sRxStartGuard.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2s_common.h"
#include "hal/i2s_ll.h"
#include "soc/hp_sys_clkrst_struct.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

// Die Funktion unter Test (im Geraet setzt der Linker sie per --wrap ein).
esp_err_t __wrap_i2s_channel_enable(i2s_chan_handle_t handle);

static int g_errors = 0;

#define CHECK(cond, ...) \
    do { if (!(cond)) { g_errors++; printf("FEHLER: " __VA_ARGS__); printf("\n"); } } while (0)

// ------------------------------------------------------------------------------
// Simulierte Hardware
// ------------------------------------------------------------------------------
i2s_dev_t           I2S0, I2S1, I2S2;
hp_sys_clkrst_dev_t HP_SYS_CLKRST;
int                 g_stubCriticalDepth = 0;

#define NEVER  (-1)

static int64_t s_nowUs        = 0;      // simulierte Uhr
static int     s_ackAfterUs   = 0;      // Quittung so lange nach der Anforderung; NEVER = nie
static int     s_acksLeft     = 1000;   // danach quittiert die Hardware nicht mehr
static int64_t s_pendingSince = NEVER;  // seit wann rx_update ansteht
static int     s_realCalls    = 0;      // Aufrufe des Originals
static bool    s_realSawPending = false;

// Jeder Blick auf die Uhr ist ein Schritt der Simulation: 1 us vergeht, und
// eine anstehende Uebernahme wird quittiert, sobald ihre Zeit um ist.
int64_t esp_timer_get_time(void)
{
    s_nowUs++;

    if (I2S0.rx_conf.rx_update) {
        if (s_pendingSince == NEVER) s_pendingSince = s_nowUs;
        if (s_ackAfterUs != NEVER && s_acksLeft > 0 && s_nowUs - s_pendingSince >= s_ackAfterUs) {
            I2S0.rx_conf.rx_update = 0;
            s_pendingSince = NEVER;
            s_acksLeft--;
        }
    } else {
        s_pendingSince = NEVER;
    }
    return s_nowUs;
}

// Das Original. Im Geraet wuerde es bei anstehendem rx_update endlos warten -
// der Test haelt fest, ob der Schutz es in diesem Zustand je erreicht.
esp_err_t __real_i2s_channel_enable(i2s_chan_handle_t handle)
{
    (void)handle;
    s_realCalls++;
    if (I2S0.rx_conf.rx_update) s_realSawPending = true;
    return ESP_OK;
}

static void resetSim(int ackAfterUs, bool pendingAtEntry)
{
    memset(&I2S0, 0, sizeof(I2S0));
    I2S0.rx_conf.rx_update = pendingAtEntry ? 1 : 0;
    s_ackAfterUs     = ackAfterUs;
    s_acksLeft       = 1000;
    s_pendingSince   = NEVER;
    s_realCalls      = 0;
    s_realSawPending = false;
}

int main(void)
{
    i2s_chan_info_t rx = { 0, I2S_DIR_RX };
    i2s_chan_info_t tx = { 0, I2S_DIR_TX };
    I2sRxGuardStats st;

    HP_SYS_CLKRST.peri_clk_ctrl11.val = 0x11111111;
    HP_SYS_CLKRST.peri_clk_ctrl12.val = 0x00060103;
    HP_SYS_CLKRST.peri_clk_ctrl13.val = 0x00406610;
    HP_SYS_CLKRST.peri_clk_ctrl14.val = 0x0008180B;

    // --- Lautsprecher: unveraendert zum Original, keine Zaehlung --------------
    resetSim(NEVER, true);                       // selbst bei "toter" Empfangsseite
    CHECK(__wrap_i2s_channel_enable(&tx) == ESP_OK, "Sendekanal: Rueckgabe");
    CHECK(s_realCalls == 1, "Sendekanal: Original %d-mal gerufen", s_realCalls);
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 0 && st.refused == 0, "Sendekanal wurde gezaehlt");

    // --- Ungueltiger Handle: das Original entscheidet -------------------------
    resetSim(0, false);
    CHECK(__wrap_i2s_channel_enable(NULL) == ESP_OK, "ungueltiger Handle: Rueckgabe des Originals");
    CHECK(s_realCalls == 1, "ungueltiger Handle: Original nicht gerufen");
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 0, "ungueltiger Handle wurde gezaehlt");

    // --- Gesunder Start, nichts steht an --------------------------------------
    resetSim(0, false);
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_OK, "gesunder Start: Rueckgabe");
    CHECK(s_realCalls == 1, "gesunder Start: Original %d-mal gerufen", s_realCalls);
    CHECK(!s_realSawPending, "gesunder Start: Original mit anstehender Uebernahme erreicht");
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 1 && st.refused == 0, "gesunder Start: Zaehler %u/%u", (unsigned)st.starts, (unsigned)st.refused);
    CHECK(st.lastResult == I2S_RX_GUARD_OK, "gesunder Start: juengstes Ergebnis %u", (unsigned)st.lastResult);

    // --- Gesunder Start, eine Uebernahme steht an und braucht 30 us -----------
    resetSim(30, true);
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_OK, "traege Quittung: Rueckgabe");
    CHECK(s_realCalls == 1 && !s_realSawPending, "traege Quittung: Original zu frueh oder gar nicht");
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 2 && st.refused == 0, "traege Quittung: Zaehler %u/%u", (unsigned)st.starts, (unsigned)st.refused);
    CHECK(st.maxWaitUs >= 29 && st.maxWaitUs < 2000, "traege Quittung: laengste Wartezeit %u us", (unsigned)st.maxWaitUs);

    // --- Stufe 1: die anstehende Uebernahme wird nie quittiert ----------------
    resetSim(NEVER, true);
    I2S0.rx_conf.rx_start = 0;
    int64_t t0 = s_nowUs;
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_ERR_TIMEOUT, "Stufe 1: Start nicht abgelehnt");
    CHECK(s_realCalls == 0, "Stufe 1: Original trotzdem gerufen - das waere der Haenger");
    CHECK(s_nowUs - t0 <= 2 * 2000 + 100, "Stufe 1: Wartezeit %lld us nicht begrenzt", (long long)(s_nowUs - t0));
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 3 && st.refused == 1, "Stufe 1: Zaehler %u/%u", (unsigned)st.starts, (unsigned)st.refused);
    CHECK(st.lastStage == I2S_RX_GUARD_PENDING, "Stufe 1: gemeldete Stufe %u", (unsigned)st.lastStage);
    CHECK(st.lastResult == I2S_RX_GUARD_PENDING, "Stufe 1: juengstes Ergebnis %u", (unsigned)st.lastResult);
    CHECK((st.lastRxConf & (1u << 8)) != 0, "Stufe 1: rx_update fehlt im Registerauszug (%08X)", (unsigned)st.lastRxConf);
    CHECK(st.lastClk[0] == 0x11111111 && st.lastClk[1] == 0x00060103 &&
          st.lastClk[2] == 0x00406610 && st.lastClk[3] == 0x0008180B, "Stufe 1: Taktregister nicht festgehalten");
    CHECK(st.maxWaitUs < 2000, "Stufe 1: Fristablauf zaehlt als gesunde Wartezeit");

    // --- Stufe 2: Anstehendes wird quittiert, die Probe danach nicht mehr -----
    resetSim(5, true);
    s_acksLeft = 1;
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_ERR_TIMEOUT, "Stufe 2: Start nicht abgelehnt");
    CHECK(s_realCalls == 0, "Stufe 2: Original trotzdem gerufen");
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 4 && st.refused == 2, "Stufe 2: Zaehler %u/%u", (unsigned)st.starts, (unsigned)st.refused);
    CHECK(st.lastStage == I2S_RX_GUARD_PROBE, "Stufe 2: gemeldete Stufe %u", (unsigned)st.lastStage);

    // --- Testwerkzeug: zwei kuenstliche Ablehnungen, dann wieder normal -------
    resetSim(0, false);
    I2sRxStartGuard_TestRefuseNext();
    I2sRxStartGuard_TestRefuseNext();
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_ERR_TIMEOUT, "Testwerkzeug: 1. Start nicht abgelehnt");
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_ERR_TIMEOUT, "Testwerkzeug: 2. Start nicht abgelehnt");
    CHECK(s_realCalls == 0, "Testwerkzeug: Original gerufen");
    CHECK(I2S0.rx_conf.val == 0, "Testwerkzeug: Hardware angefasst (%08X)", (unsigned)I2S0.rx_conf.val);
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.starts == 6 && st.refused == 4 && st.lastStage == I2S_RX_GUARD_TEST,
          "Testwerkzeug: Zaehler %u/%u, Stufe %u", (unsigned)st.starts, (unsigned)st.refused, (unsigned)st.lastStage);
    CHECK(__wrap_i2s_channel_enable(&rx) == ESP_OK, "Testwerkzeug: 3. Start nicht wieder normal");
    CHECK(s_realCalls == 1, "Testwerkzeug: Original beim 3. Start nicht gerufen");
    I2sRxStartGuard_GetStats(&st);
    CHECK(st.lastResult == I2S_RX_GUARD_OK && st.lastStage == I2S_RX_GUARD_TEST,
          "nach Erholung: juengstes Ergebnis %u, letzte Ablehnung %u", (unsigned)st.lastResult, (unsigned)st.lastStage);

    // --- Rahmen ---------------------------------------------------------------
    CHECK(g_stubCriticalDepth == 0, "Sperre nicht ausgeglichen (%d)", g_stubCriticalDepth);
    CHECK(strcmp(I2sRxStartGuard_StageText(I2S_RX_GUARD_PENDING), I2sRxStartGuard_StageText(I2S_RX_GUARD_PROBE)) != 0,
          "Stufentexte nicht unterscheidbar");
    I2sRxStartGuard_GetStats(NULL);              // darf nichts tun

    printf("%d Fehler\n", g_errors);
    return g_errors ? 1 : 0;
}
