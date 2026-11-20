// ==============================================================================
// AudioCodecTab5 - siehe AudioCodecTab5.h
// ==============================================================================
#include "AudioCodecTab5.h"
#include <M5Unified.hpp>
#include <driver/gpio.h>
#include "esp_private/periph_ctrl.h"
#include "hal/i2s_ll.h"
#include "soc/soc.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_struct.h"
#include "soc/usb_wrap_struct.h"
#include "soc/usb_serial_jtag_struct.h"
#include "soc/lp_system_struct.h"
#include "soc/hp_system_struct.h"
#include "soc/pmu_struct.h"

// Adressen auf dem internen I2C-Bus (wie in M5Unified fuer den Tab5).
static constexpr uint8_t  ES8388_ADDR = 0x10;
static constexpr uint8_t  ES7210_ADDR = 0x40;
static constexpr uint8_t  PI4IO1_ADDR = 0x43;   // SPK_EN, LCD/TP/CAM-Reset
static constexpr uint8_t  PI4IO2_ADDR = 0x44;   // WLAN-Strom, Laden
static constexpr uint32_t I2C_FREQ    = 400000;

// PI4IOE5V6408: Register.
static constexpr uint8_t PI4IO_REG_DIR    = 0x03;   // 1 = Ausgang
static constexpr uint8_t PI4IO_REG_OUTPUT = 0x05;
static constexpr uint8_t PI4IO_REG_HIGHZ  = 0x07;   // 1 = Ausgang hochohmig
static constexpr uint8_t PI4IO_REG_PULLEN = 0x0B;
static constexpr uint8_t PI4IO_BIT_SPK_EN = 0b00000010;   // P1

// I2S-Pins des Tab5 (MCLK, BCLK, LRCK, DOUT zum ES8388, DIN vom ES7210).
static constexpr gpio_num_t I2S_PINS[] = { GPIO_NUM_30, GPIO_NUM_27, GPIO_NUM_29, GPIO_NUM_26, GPIO_NUM_28 };

static bool wr(uint8_t addr, uint8_t reg, uint8_t val) {
    return M5.In_I2C.writeRegister8(addr, reg, val, I2C_FREQ);
}
static uint8_t rd(uint8_t addr, uint8_t reg) {
    return M5.In_I2C.readRegister8(addr, reg, I2C_FREQ);
}

// ------------------------------------------------------------------------------
// Einzelne Theorien
// ------------------------------------------------------------------------------

// ES8388: CHIPPOWER (0x02) = 0xF3 nimmt Digitalteil und beide Ablaufsteuerungen
// in den Reset (wie der ESP-ADF-Treiber), dann Register-Reset. M5Unified
// schreibt beim begin() wieder 0x0A und gibt damit alles frei.
static void resetCodec() {
    const bool ok = wr(ES8388_ADDR, 0x02, 0xF3);
    delay(10);
    const bool okReg = wr(ES8388_ADDR, 0x00, 0x80) && wr(ES8388_ADDR, 0x00, 0x00);
    delay(10);
    Serial.printf("[AudioCodec] ES8388: CHIPPOWER %s, Register %s.\n", ok ? "ok" : "FEHLER", okReg ? "ok" : "FEHLER");
}

// ESP32-P4: I2S-Peripherie des Lautsprechers vollstaendig zuruecksetzen
// (Bus-Takt aus, Register-Reset, TX-Takt aus) und die I2S-Pins in den
// Grundzustand. Der Treiber baut beim naechsten begin() alles neu auf.
static void resetI2s() {
    const int port = (int)M5.Speaker.config().i2s_port;
    i2s_dev_t* hw = (port == 1) ? &I2S1 : (port == 2) ? &I2S2 : &I2S0;
    PERIPH_RCC_ATOMIC() {
        i2s_ll_tx_disable_clock(hw);
        i2s_ll_reset_register(port);
        i2s_ll_enable_bus_clock(port, false);
    }
    for (gpio_num_t pin : I2S_PINS) gpio_reset_pin(pin);
    delay(20);
    Serial.printf("[AudioCodec] I2S%d: Peripherie und Pins zurueckgesetzt.\n", port);
}

// ES7210: teilt sich MCLK, BCLK und LRCK mit dem Lautsprecher. Steht er nach
// einem Absturz im Master-Betrieb, treibt er die Takte mit. Reset (0xFF) und
// im Reset halten - wie M5Unified beim Abschalten des Mikrofons.
static void resetMicAdc() {
    const uint8_t mode = rd(ES7210_ADDR, 0x08);
    const bool ok = wr(ES7210_ADDR, 0x00, 0xFF);
    delay(10);
    Serial.printf("[AudioCodec] ES7210: MODE_CFG vorher %02X (Bit0 = Master), Reset %s.\n", mode, ok ? "ok" : "FEHLER");
}

// NS4150B ueber SPK_EN (Expander 0x43, P1): Pin sicher als Push-Pull-Ausgang
// ohne Pull konfigurieren, Verstaerker > 80 ms abschalten (Datenblatt:
// Abschaltzeit 80 ms), wieder ein und die Anlaufzeit (120 ms) abwarten.
static void resetAmp() {
    const uint8_t dir = rd(PI4IO1_ADDR, PI4IO_REG_DIR);
    const uint8_t hiz = rd(PI4IO1_ADDR, PI4IO_REG_HIGHZ);
    const uint8_t pen = rd(PI4IO1_ADDR, PI4IO_REG_PULLEN);
    wr(PI4IO1_ADDR, PI4IO_REG_DIR,    dir | PI4IO_BIT_SPK_EN);
    wr(PI4IO1_ADDR, PI4IO_REG_HIGHZ,  hiz & ~PI4IO_BIT_SPK_EN);
    wr(PI4IO1_ADDR, PI4IO_REG_PULLEN, pen & ~PI4IO_BIT_SPK_EN);
    M5.In_I2C.bitOff(PI4IO1_ADDR, PI4IO_REG_OUTPUT, PI4IO_BIT_SPK_EN, I2C_FREQ);
    delay(200);
    M5.In_I2C.bitOn(PI4IO1_ADDR, PI4IO_REG_OUTPUT, PI4IO_BIT_SPK_EN, I2C_FREQ);
    delay(150);
    Serial.printf("[AudioCodec] Verstaerker: SPK_EN 200 ms aus/ein (vorher Richtung %02X HighZ %02X Pull %02X).\n",
                  dir, hiz, pen);
}

// ESP32-P4: Pads GPIO26/27 von der internen USB-PHY 1 (USB_OTG FS) loesen.
// Der USB-Host fuer den Bluetooth-Dongle laeuft an der USB-A-Buchse ueber die
// High-Speed-PHY (eigene Pins) und ist davon nicht betroffen.
static void releaseUsbPads() {
    const uint32_t before = USB_WRAP.otg_conf.val;
    USB_WRAP.otg_conf.usb_pad_enable    = 0;
    USB_WRAP.otg_conf.pad_pull_override = 0;
    USB_WRAP.otg_conf.dp_pullup   = 0;
    USB_WRAP.otg_conf.dp_pulldown = 0;
    USB_WRAP.otg_conf.dm_pullup   = 0;
    USB_WRAP.otg_conf.dm_pulldown = 0;
    gpio_reset_pin(GPIO_NUM_26);
    gpio_reset_pin(GPIO_NUM_27);
    Serial.printf("[AudioCodec] USB-PHY-Pads GPIO26/27: otg_conf %08lX -> %08lX.\n",
                  (unsigned long)before, (unsigned long)USB_WRAP.otg_conf.val);
}

// ES8388 mit laufendem Takt in den Ruhezustand und neu. Waehrenddessen spielt
// ein eigener Kanal Stille, damit der I2S-Treiber MCLK/BCLK/LRCK sicher
// ausgibt. Verstaerker aus, damit es nicht knackt; M5.Speaker.begin()
// danach schaltet ihn wieder ein und schreibt die Betriebswerte.
static constexpr uint8_t SILENCE_CHANNEL = 7;   // von der App sonst ungenutzt
static void resetCodecClocked() {
    static int16_t silence[512] = { 0 };
    M5.In_I2C.bitOff(PI4IO1_ADDR, PI4IO_REG_OUTPUT, PI4IO_BIT_SPK_EN, I2C_FREQ);
    M5.Speaker.playRaw(silence, 512, 44100, false, 400, SILENCE_CHANNEL, true);   // ~4,6 s Stille
    delay(50);
    wr(ES8388_ADDR, 0x19, 0x24);   // DAC stumm
    wr(ES8388_ADDR, 0x04, 0xC0);   // DAC und alle Ausgaenge aus
    wr(ES8388_ADDR, 0x02, 0xFF);   // Digitalteil, DLLs, Referenzen aus, Ablaufsteuerung im Reset
    wr(ES8388_ADDR, 0x00, 0x00);   // Referenz und VMID aus
    delay(500);                    // VMID-Kondensator entladen lassen
    wr(ES8388_ADDR, 0x00, 0x80);   // Register-Reset ...
    wr(ES8388_ADDR, 0x00, 0x00);
    wr(ES8388_ADDR, 0x00, 0x07);   // Referenz an, VMID schnell laden (5k)
    delay(200);
    M5.Speaker.stop(SILENCE_CHANNEL);
    Serial.println("[AudioCodec] ES8388 mit Takt: Ruhezustand, VMID 500 ms entladen, Register-Reset, VMID geladen.");
}

// Hold der I2S-Pads loesen (HP_SYSTEM.gpio_o_hold_ctrl0 fuer GPIO16-47).
static void releasePadHold() {
    const uint32_t before = HP_SYSTEM.gpio_o_hold_ctrl0.val;
    // Je Pin; ein globales gpio_deep_sleep_hold_dis() gibt es auf dem P4 nicht.
    for (gpio_num_t pin : I2S_PINS) gpio_hold_dis(pin);
    Serial.printf("[AudioCodec] Pad-Hold: HP %08lX -> %08lX (Bit = GPIO-16), PMU pad_hold_all %08lX.\n",
                  (unsigned long)before, (unsigned long)HP_SYSTEM.gpio_o_hold_ctrl0.val,
                  (unsigned long)PMU.imm.pad_hold_all.val);
}

// Messung: Pegel an den I2S-Pads waehrend eines Tons (1 kHz Rechteck auf dem
// freien Kanal). Abtastung direkt im GPIO-Eingangsregister; dafuer wird an den
// Pads der Eingang zugeschaltet (aendert den Ausgang nicht).
static void checkI2sLines() {
    static int16_t square[44];
    for (int i = 0; i < 44; i++) square[i] = (i < 22) ? 3000 : -3000;
    static constexpr struct { gpio_num_t pin; const char* name; uint32_t reg; } LINES[] = {
        { GPIO_NUM_30, "MCLK", IO_MUX_GPIO30_REG }, { GPIO_NUM_27, "BCLK", IO_MUX_GPIO27_REG },
        { GPIO_NUM_29, "LRCK", IO_MUX_GPIO29_REG }, { GPIO_NUM_26, "DOUT", IO_MUX_GPIO26_REG },
    };
    for (const auto& l : LINES) PIN_INPUT_ENABLE(l.reg);

    M5.Speaker.setChannelVolume(SILENCE_CHANNEL, 128);
    M5.Speaker.playRaw(square, 44, 44100, false, 400, SILENCE_CHANNEL, true);   // ~0,4 s Ton
    delay(100);                                     // Treiber laeuft an

    static constexpr uint32_t SAMPLES = 20000;
    uint32_t high[4] = { 0 }, toggles[4] = { 0 };
    uint32_t prev = GPIO.in.val;
    for (uint32_t s = 0; s < SAMPLES; s++) {
        const uint32_t now = GPIO.in.val;
        for (int k = 0; k < 4; k++) {
            const uint32_t bit = 1u << LINES[k].pin;
            if (now & bit) high[k]++;
            if ((now ^ prev) & bit) toggles[k]++;
        }
        prev = now;
    }
    M5.Speaker.stop(SILENCE_CHANNEL);

    Serial.println("[AudioCodec] I2S-Leitungen waehrend eines Tons (20000 Abtastungen):");
    for (int k = 0; k < 4; k++) {
        Serial.printf("  GPIO%-2d %s: high %3lu %%, %5lu Wechsel -> %s\n",
                      (int)LINES[k].pin, LINES[k].name,
                      (unsigned long)(high[k] * 100 / SAMPLES), (unsigned long)toggles[k],
                      toggles[k] > 10 ? "aktiv" : (high[k] > SAMPLES / 2 ? "steht auf 1" : "steht auf 0"));
    }
}

// ------------------------------------------------------------------------------
// Schnittstelle
// ------------------------------------------------------------------------------
const char* AudioCodec_KindName(AudioResetKind kind) {
    switch (kind) {
        case AudioResetKind::Codec:  return "Codec ES8388";
        case AudioResetKind::I2s:    return "I2S ESP32-P4";
        case AudioResetKind::MicAdc: return "Mikrofon-ADC ES7210";
        case AudioResetKind::Amp:    return "Verstaerker NS4150B";
        case AudioResetKind::UsbPads: return "USB-Pads GPIO26/27";
        case AudioResetKind::CodecClocked: return "Codec ES8388 mit Takt";
        case AudioResetKind::PadHold:      return "Pad-Hold GPIO26-30";
        case AudioResetKind::LineCheck:    return "I2S-Leitungen messen";
        case AudioResetKind::SoundTest:    return "Hoertest";
        case AudioResetKind::ToneSweep:    return "Frequenztest";
        case AudioResetKind::FirstTone:    return "Erster Ton";
        case AudioResetKind::All:    return "Alles";
    }
    return "?";
}

bool AudioCodec_NeedsClock(AudioResetKind kind) {
    return kind == AudioResetKind::CodecClocked || kind == AudioResetKind::LineCheck;
}

void AudioCodec_Reset(AudioResetKind kind) {
    Serial.printf("[AudioCodec] Reset: %s\n", AudioCodec_KindName(kind));
    switch (kind) {
        case AudioResetKind::Codec:  resetCodec();  break;
        case AudioResetKind::I2s:    resetI2s();    break;
        case AudioResetKind::MicAdc: resetMicAdc(); break;
        case AudioResetKind::Amp:    resetAmp();    break;
        case AudioResetKind::UsbPads: releaseUsbPads(); break;
        case AudioResetKind::CodecClocked: resetCodecClocked(); break;
        case AudioResetKind::PadHold:      releasePadHold();    break;
        case AudioResetKind::LineCheck:    checkI2sLines();     break;
        case AudioResetKind::SoundTest:    break;   // laeuft in AudioStreamLogic (runSoundTest)
        case AudioResetKind::ToneSweep:    break;   // laeuft in AudioStreamLogic (runToneSweep)
        case AudioResetKind::FirstTone:    break;   // laeuft in AudioStreamLogic (runFirstTone)
        case AudioResetKind::All:
            // Reihenfolge: erst die Takt-Erzeuger/-Teilnehmer, dann der Codec,
            // zuletzt der Verstaerker (der beim begin() ohnehin geschaltet wird).
            releaseUsbPads();
            resetI2s();
            resetMicAdc();
            resetCodec();
            resetAmp();
            break;
    }
}

static void dumpRange(const char* name, uint8_t addr, uint8_t from, uint8_t to) {
    char line[96];
    for (uint16_t base = from; base <= to; base += 8) {
        int n = snprintf(line, sizeof(line), "  %s %02X:", name, (unsigned)base);
        for (uint16_t r = base; r < base + 8 && r <= to; r++) {
            n += snprintf(line + n, sizeof(line) - n, " %02X", rd(addr, (uint8_t)r));
        }
        Serial.println(line);
    }
}

void AudioCodec_DumpI2sRx(const char* tag) {
    // Register der I2S-Peripherie nur bei laufendem Bus-Takt lesen.
    if (!HP_SYS_CLKRST.soc_clk_ctrl2.reg_i2s0_apb_clk_en) {
        Serial.printf("[AudioCodec] I2S-Empfang (%s): Bus-Takt aus - Register nicht lesbar\n", tag);
        return;
    }
    Serial.printf("[AudioCodec] I2S-Empfang (%s): rx_conf %08lX rx_conf1 %08lX tdm %08lX timing %08lX | "
                  "Takt ctrl11 %08lX 12 %08lX 13 %08lX 14 %08lX\n",
                  tag,
                  (unsigned long)I2S0.rx_conf.val, (unsigned long)I2S0.rx_conf1.val,
                  (unsigned long)I2S0.rx_tdm_ctrl.val, (unsigned long)I2S0.rx_timing.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl11.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl12.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl13.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl14.val);
}

void AudioCodec_Dump(const char* tag) {
    Serial.printf("[AudioCodec] Auszug (%s)\n", tag ? tag : "");
    dumpRange("ES8388", ES8388_ADDR, 0x00, 0x3F);   // 0x35-0x3F undokumentiert
    dumpRange("ES7210", ES7210_ADDR, 0x00, 0x14);
    dumpRange("ES7210", ES7210_ADDR, 0x40, 0x4C);
    dumpRange("PI4IO1", PI4IO1_ADDR, 0x01, 0x11);
    dumpRange("PI4IO2", PI4IO2_ADDR, 0x01, 0x11);
    // I2S-Takt des ESP32-P4 (HP_SYS_CLKRST: Takt-Freigaben, Quelle, Teiler;
    // LP_AON: MCLK-Pad-Freigabe - Always-on, ueberlebt den Software-Reset).
    Serial.printf("  CLKRST soc2 %08lX | ctrl11 %08lX 12 %08lX 13 %08lX 14 %08lX | LP_AON hp_clk %08lX\n",
                  (unsigned long)HP_SYS_CLKRST.soc_clk_ctrl2.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl11.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl12.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl13.val,
                  (unsigned long)HP_SYS_CLKRST.peri_clk_ctrl14.val,
                  (unsigned long)LP_AON_CLKRST.hp_clk_ctrl.val);
    // Pads der I2S-Pins: USB-PHY 1 (GPIO26/27), USB-Serial-JTAG, PHY-Auswahl
    // (LP, Always-on), IO-MUX und Ausgangs-Signal der GPIO-Matrix.
    Serial.printf("  USB_WRAP otg_conf %08lX | USJ conf0 %08lX | LP_SYS usb_ctrl %08lX\n",
                  (unsigned long)USB_WRAP.otg_conf.val,
                  (unsigned long)USB_SERIAL_JTAG.conf0.val,
                  (unsigned long)LP_SYS.usb_ctrl.val);
    Serial.printf("  IO_MUX 26 %08lX 27 %08lX 29 %08lX 30 %08lX | OUT_SEL 26 %03lX 27 %03lX 29 %03lX 30 %03lX\n",
                  (unsigned long)REG_READ(IO_MUX_GPIO26_REG), (unsigned long)REG_READ(IO_MUX_GPIO27_REG),
                  (unsigned long)REG_READ(IO_MUX_GPIO29_REG), (unsigned long)REG_READ(IO_MUX_GPIO30_REG),
                  (unsigned long)GPIO.func_out_sel_cfg[26].val, (unsigned long)GPIO.func_out_sel_cfg[27].val,
                  (unsigned long)GPIO.func_out_sel_cfg[29].val, (unsigned long)GPIO.func_out_sel_cfg[30].val);
    Serial.printf("  Pad-Hold HP %08lX (Bit = GPIO-16) | PMU pad_hold_all %08lX\n",
                  (unsigned long)HP_SYSTEM.gpio_o_hold_ctrl0.val, (unsigned long)PMU.imm.pad_hold_all.val);
    // Register der I2S-Peripherie nur bei laufendem Bus-Takt lesen.
    if (HP_SYS_CLKRST.soc_clk_ctrl2.reg_i2s0_apb_clk_en) {
        Serial.printf("  I2S0 tx_conf %08lX tx_conf1 %08lX\n",
                      (unsigned long)I2S0.tx_conf.val, (unsigned long)I2S0.tx_conf1.val);
    } else {
        Serial.println("  I2S0 Bus-Takt aus - Register nicht lesbar");
    }
}
