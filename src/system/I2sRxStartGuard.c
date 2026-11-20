// ==============================================================================
// I2sRxStartGuard - begrenzt das Warten beim Start des I2S-Empfangs (Mikrofon)
// ==============================================================================
// Problem (Befund 17.11.2026, zwei Coredumps):
//
// Beim Start eines I2S-Empfangskanals laesst der IDF-Treiber die Hardware die
// Einstellungen uebernehmen und wartet OHNE Zeitlimit auf die Quittung
// (ESP-IDF 5.4.2, hal/esp32p4/include/hal/i2s_ll.h):
//
//     static inline void i2s_ll_rx_start(i2s_dev_t *hw)
//     {
//         hw->rx_conf.rx_update = 1;
//         while (hw->rx_conf.rx_update);      // <- hier stand der Task
//         hw->rx_conf.rx_start = 1;
//     }
//
// Die Quittung kommt aus dem Empfangstakt des I2S. Bleibt sie aus, dreht der
// aufrufende Task fuer immer in dieser Schleife. Aufrufer ist der
// Mikrofon-Task von M5Unified, den AudioStreamLogic mit Prioritaet 13 auf
// Kern 1 startet - er verdraengt dort Audio-Task, Video-Decoder und
// Hauptschleife. Das Geraet friert ein, bis HangGuard es neu startet.
// Aufgetreten bei etwa jedem dritten Druck auf "Gegensprechen".
//
// Vermutete Ursache (nicht bewiesen): M5Unified 0.2.13 stellt den Takt im
// Mikrofon-Task von Hand um (Mic_Class.cpp, mic_task). Es fordert eine
// Uebernahme an (rx_update = 1), wartet sie nicht ab und programmiert sofort
// den Taktteiler neu. Der IDF-Treiber selbst aendert den Teiler nie, waehrend
// eine Uebernahme laeuft. Der Registerauszug, den dieser Schutz bei jedem
// abgelehnten Start festhaelt, soll das klaeren.
//
// Loesung: i2s_channel_enable() wird umgeleitet. Fuer Empfangskanaele prueft
// die Ersatzfunktion VOR dem echten Start mit Zeitlimit, ob die Hardware
// Uebernahmen quittiert:
//   Stufe 1  eine bereits angeforderte Uebernahme muss abgeschlossen sein,
//   Stufe 2  eine eigene Probe-Uebernahme muss quittiert werden.
// Erst dann laeuft der echte Start; seine Warteschleife ist damit sofort
// fertig. Bleibt eine Quittung aus, wird der Start abgelehnt
// (ESP_ERR_TIMEOUT), ohne die Warteschleife zu betreten. Der Aufrufer
// (AudioStreamLogic, micUp) baut den Kanal ab und einmal neu auf; dabei setzt
// der Treiber das I2S-Modul zurueck.
//
// Grenze: Zwischen Pruefung und echtem Start liegen wenige Mikrosekunden.
// Faellt die Quittung genau dort aus, greift weiterhin nur HangGuard.
//
// Sendekanaele (Lautsprecher) laufen unveraendert durch. Dieselbe
// Warteschleife gibt es dort auch (i2s_ll_tx_start). Sie ist bisher in keinem
// Absturzbericht aufgetaucht, und ein still gescheiterter Lautsprecherstart
// waere fuer den Mattenalarm schlimmer als der Neustart durch HangGuard.
//
// Wie die Umleitung funktioniert, steht in Dma2dGuard.c (-Wl,--wrap=<symbol>,
// Schalter in platformio.ini): Der Treiber liegt nur als Binaerbibliothek vor;
// M5Unified referenziert i2s_channel_enable als undefiniertes Symbol, der
// Linker leitet es auf __wrap_i2s_channel_enable um. Die Umleitung gilt fuer
// ALLE Nutzer des I2S-Treibers.
//
// Diese Datei ist bewusst C und nicht C++: __wrap_... muss C-Bindung haben.
// ==============================================================================

#include "I2sRxStartGuard.h"

#include <stdbool.h>

#include "driver/i2s_common.h"
#include "hal/i2s_ll.h"
#include "soc/hp_sys_clkrst_struct.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

// Das Original. Der Linker stellt es unter diesem Namen bereit.
esp_err_t __real_i2s_channel_enable(i2s_chan_handle_t handle);

// ------------------------------------------------------------------------------
// Frist
// ------------------------------------------------------------------------------
// Eine gesunde Quittung braucht wenige Perioden des Empfangstakts (bei 11 MHz
// unter einer Mikrosekunde); der gemessene Hoechstwert steht in maxWaitUs.
// 2 ms sind ein Vielfaches davon und trotzdem unschaedlich: so lange haelt der
// Mikrofon-Task die anderen Tasks auf Kern 1 im Fehlerfall hoechstens auf -
// je Stufe einmal.
#define I2S_RX_GUARD_LIMIT_US  2000

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
// Geschrieben vom startenden Task (Mikrofon-Task), gelesen von Audio-Task und
// Oberflaeche - deshalb unter einem Spinlock.
static portMUX_TYPE    s_mux          = portMUX_INITIALIZER_UNLOCKED;
static I2sRxGuardStats s_stats        = { 0 };
static uint32_t        s_testRefusals = 0;

// ------------------------------------------------------------------------------
// Warten auf die Quittung, mit Frist
// ------------------------------------------------------------------------------
// true = rx_update ist zurueckgesetzt. longestUs haelt die laengste Wartezeit
// der gesunden Stufen fest.
static bool waitAcknowledged(i2s_dev_t *hw, uint32_t *longestUs)
{
    const int64_t start = esp_timer_get_time();

    while (hw->rx_conf.rx_update) {
        if (esp_timer_get_time() - start > I2S_RX_GUARD_LIMIT_US) return false;
    }

    const uint32_t waited = (uint32_t)(esp_timer_get_time() - start);
    if (waited > *longestUs) *longestUs = waited;
    return true;
}

// Beide Stufen der Pruefung. Veraendert an den Einstellungen nichts: die
// Probe-Uebernahme laesst die Hardware nur uebernehmen, was ohnehin schon in
// den Registern steht - dasselbe tut der echte Start gleich danach.
static I2sRxGuardStage probe(i2s_dev_t *hw, uint32_t *longestUs)
{
    if (!waitAcknowledged(hw, longestUs)) return I2S_RX_GUARD_PENDING;

    hw->rx_conf.rx_update = 1;
    if (!waitAcknowledged(hw, longestUs)) return I2S_RX_GUARD_PROBE;

    return I2S_RX_GUARD_OK;
}

// ------------------------------------------------------------------------------
// Ersatz fuer i2s_channel_enable()
// ------------------------------------------------------------------------------
esp_err_t __wrap_i2s_channel_enable(i2s_chan_handle_t handle)
{
    // Lautsprecher (Sendekanal) und ungueltige Handles: unveraendert an das
    // Original - es prueft den Handle selbst.
    i2s_chan_info_t info;
    if (i2s_channel_get_info(handle, &info) != ESP_OK || info.dir != I2S_DIR_RX) {
        return __real_i2s_channel_enable(handle);
    }

    i2s_dev_t *hw = I2S_LL_GET_HW(info.id);

    portENTER_CRITICAL(&s_mux);
    const bool testRefusal = (s_testRefusals > 0);
    if (testRefusal) s_testRefusals--;
    portEXIT_CRITICAL(&s_mux);

    uint32_t longestUs = 0;
    const I2sRxGuardStage stage = testRefusal ? I2S_RX_GUARD_TEST : probe(hw, &longestUs);

    portENTER_CRITICAL(&s_mux);
    s_stats.starts++;
    s_stats.lastResult = (uint8_t)stage;
    if (stage == I2S_RX_GUARD_OK) {
        if (longestUs > s_stats.maxWaitUs) s_stats.maxWaitUs = longestUs;
    } else {
        s_stats.refused++;
        s_stats.lastStage  = (uint8_t)stage;
        s_stats.lastRxConf = hw->rx_conf.val;
        s_stats.lastClk[0] = HP_SYS_CLKRST.peri_clk_ctrl11.val;
        s_stats.lastClk[1] = HP_SYS_CLKRST.peri_clk_ctrl12.val;
        s_stats.lastClk[2] = HP_SYS_CLKRST.peri_clk_ctrl13.val;
        s_stats.lastClk[3] = HP_SYS_CLKRST.peri_clk_ctrl14.val;
    }
    portEXIT_CRITICAL(&s_mux);

    // Abgelehnt: Der Kanal bleibt im Zustand "bereit, nicht gestartet". Der
    // Aufrufer muss ihn abbauen (i2s_del_channel), bevor er es erneut versucht.
    if (stage != I2S_RX_GUARD_OK) return ESP_ERR_TIMEOUT;

    return __real_i2s_channel_enable(handle);
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void I2sRxStartGuard_GetStats(I2sRxGuardStats *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_mux);
    *out = s_stats;
    portEXIT_CRITICAL(&s_mux);
}

const char *I2sRxStartGuard_StageText(uint8_t stage)
{
    switch (stage) {
        case I2S_RX_GUARD_OK:      return "in Ordnung";
        case I2S_RX_GUARD_PENDING: return "angeforderte Uebernahme haengt";
        case I2S_RX_GUARD_PROBE:   return "Probe-Uebernahme ohne Quittung";
        case I2S_RX_GUARD_TEST:    return "Testwerkzeug";
        default:                   return "unbekannt";
    }
}

void I2sRxStartGuard_TestRefuseNext(void)
{
    portENTER_CRITICAL(&s_mux);
    s_testRefusals++;
    portEXIT_CRITICAL(&s_mux);
}
