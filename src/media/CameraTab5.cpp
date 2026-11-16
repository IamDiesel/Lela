// ==============================================================================
// CameraTab5 - Implementierung (siehe CameraTab5.h)
// ==============================================================================
#include "CameraTab5.h"
#include "CameraTab5Sc202cs.h"
#include "CameraExposure.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_clock_output.h>
#include <esp_cam_ctlr.h>
#include <esp_cam_ctlr_csi.h>
#include <driver/isp.h>
#include <driver/isp_ccm.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>

namespace {

// ------------------------------------------------------------------------------
// Hardware
// ------------------------------------------------------------------------------
constexpr gpio_num_t kPinMclk     = GPIO_NUM_36;
constexpr uint32_t   kMclkDivider = 20;          // SPLL 480 MHz / 20 = 24 MHz
constexpr uint8_t    kSccbAddr    = 0x36;
constexpr uint32_t   kSccbFreq    = 400000;
constexpr uint8_t    kExpanderIdx = 0;           // U6 (0x43)
constexpr uint8_t    kPinCamRst   = 6;           // U6 P6, aktiv low

constexpr uint16_t kRegChipIdH    = 0x3107;
constexpr uint16_t kRegChipIdL    = 0x3108;
constexpr uint16_t kChipIdSc202cs = 0xEB52;

// Bildweg (passend zu kSc202csInit720p).
constexpr int      kLaneBitRateMbps = 576;
constexpr int      kLanes           = 1;
constexpr uint32_t kIspClockHz      = 80 * 1000 * 1000;   // wie IDF-Beispiel mipi_isp_dsi
constexpr size_t   kFrameBytes      = (size_t)CAMERA_WIDTH * CAMERA_HEIGHT * 2;
constexpr size_t   kBufferAlign     = 64;                  // Cache-Zeile PSRAM
constexpr int      kBuffers         = 2;

// Weissabgleich ("Grauwelt"): Im Mittel ist eine Szene grau, also R = G = B.
// Der Sensor liefert ohne Abgleich stark gruen (doppelt so viele Gruen-Pixel
// im Bayer-Raster, hohe Gruen-Empfindlichkeit). Die Verstaerkung von Rot und
// Blau steht in der Farbkorrektur-Matrix (CCM) des ISP; sie wird aus dem
// fertigen Bild nachgefuehrt (Regelkreis, gedaempft).
constexpr uint32_t kAwbEveryFrames = 6;      // jedes 6. abgeholte Bild auswerten
constexpr int      kAwbStep        = 16;     // Stichprobe: jeder 16. Punkt/Zeile
constexpr float    kAwbDamping     = 0.5f;   // Exponent: Haelfte des Fehlers je Schritt
constexpr float    kGainMin        = 0.25f;
constexpr float    kGainMax        = 3.9f;   // CCM erlaubt (-4, 4)
constexpr int      kAwbDarkMin     = 16;     // zu dunkle/helle Punkte nicht werten
constexpr int      kAwbBrightMax   = 235;

// Belichtungsautomatik: Zeit und Verstaerkung des Sensors aus dem fertigen
// Bild nachfuehren (Rechnung und Begruendung: CameraExposure.h). Ausgewertet
// wird jedes zweite abgeholte Bild - eine neue Einstellung wirkt im Sensor erst
// ein bis zwei Bilder spaeter, und bewertet werden soll ihr Ergebnis.
constexpr uint32_t kAeEveryFrames  = 2;
constexpr float    kAeStartProduct = (float)AE_FLICKER_LINES;   // 10 ms, Verstaerkung 1
constexpr float    kLineTimeMs     = 1000.0f / (30.0f * 1250.0f);   // 30 Bilder/s, 1250 Zeilen je Bild
// Register des SC202CS (Sensortreiber esp_cam_sensor, sc202cs_regs.h).
constexpr uint16_t kRegExposureH   = 0x3e00;   // Zeilen, Bits 15..12
constexpr uint16_t kRegExposureM   = 0x3e01;   // Bits 11..4
constexpr uint16_t kRegExposureL   = 0x3e02;   // Bits 3..0 im oberen Halbbyte
constexpr uint16_t kRegGainDigCoarse = 0x3e06;
constexpr uint16_t kRegGainDigFine   = 0x3e07;
constexpr uint16_t kRegGainAnalog    = 0x3e09;

// Wartezeiten (Datenblatt-Richtwerte grosszuegig aufgerundet).
constexpr uint32_t kClockSettleMs   = 5;     // Takt stabil vor Reset-Freigabe
constexpr uint32_t kResetToSccbMs   = 20;    // Sensor bereit fuer SCCB
constexpr uint32_t kSoftResetMs     = 10;    // nach Register 0x0103

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
esp_clock_output_mapping_handle_t s_mclk = nullptr;
esp_cam_ctlr_handle_t s_cam     = nullptr;
isp_proc_handle_t     s_isp     = nullptr;
bool                  s_camOn   = false;   // esp_cam_ctlr_enable erfolgt
bool                  s_ispOn   = false;   // esp_isp_enable erfolgt
bool                  s_started = false;   // esp_cam_ctlr_start erfolgt
bool                  s_running = false;

// Bildpuffer und ihre Rollen (Index 0..kBuffers-1, -1 = keiner). Geschrieben
// im Interrupt und im Task, deshalb nur unter s_lock.
uint16_t*         s_buf[kBuffers] = {};
portMUX_TYPE      s_lock      = portMUX_INITIALIZER_UNLOCKED;
volatile int      s_filling   = -1;   // wird gerade vom DMA beschrieben
volatile int      s_completed = -1;   // eben fertig, Cache-Abgleich laeuft noch
volatile int      s_ready     = -1;   // fertig, noch nicht abgeholt
volatile int      s_reading   = -1;   // vom Verbraucher geliehen
volatile uint32_t s_frames    = 0;

// Weissabgleich: Verstaerkung Rot/Blau (Gruen = 1). Bleibt ueber Stop/Start
// erhalten, damit die naechste Vorschau gleich mit passenden Farben beginnt.
float    s_gainR    = 1.6f;   // Startwert: typischer Gruenstich dieses Sensors
float    s_gainB    = 1.6f;
uint32_t s_acquired = 0;

// Belichtungsautomatik. Das Belichtungsprodukt (Zeilen x Verstaerkung) bleibt
// wie der Weissabgleich ueber Stop/Start erhalten. s_expLines/s_expRegs halten
// fest, was im Sensor steht - geschrieben wird nur, was sich aendert.
float            s_expProduct = kAeStartProduct;
uint32_t         s_expLines   = 0;                        // 0 = unbekannt (nach Sensor-Reset)
ExposureGainRegs s_expRegs    = { 0xFF, 0xFF, 0xFF };     // 0xFF = unbekannt
int              s_expBrightness = -1;                    // letzte Messung (95. Perzentil)

// ------------------------------------------------------------------------------
// Takt (MCLK)
// ------------------------------------------------------------------------------
bool clockStart() {
    if (s_mclk) return true;
    esp_err_t err = esp_clock_output_start(CLKOUT_SIG_SPLL, kPinMclk, &s_mclk);
    if (err == ESP_OK) err = esp_clock_output_set_divider(s_mclk, kMclkDivider);
    if (err != ESP_OK) {
        Serial.printf("[Kamera] Takt an G36 fehlgeschlagen: %s\n", esp_err_to_name(err));
        if (s_mclk) { esp_clock_output_stop(s_mclk); s_mclk = nullptr; }
        return false;
    }
    return true;
}

void clockStop() {
    if (!s_mclk) return;
    esp_clock_output_stop(s_mclk);
    s_mclk = nullptr;
}

// ------------------------------------------------------------------------------
// Reset und SCCB
// ------------------------------------------------------------------------------
// Reihenfolge laut Sensor-Datenblatt: Reset halten, Takt starten, Reset
// freigeben. Waehrend des Reset stoert auch das kurze Umschalten des
// Takt-Ausgangs (erst SPLL ungeteilt, dann Teiler) nicht.
void resetHold(bool hold) {
    M5.getIOExpander(kExpanderIdx).digitalWrite(kPinCamRst, !hold);
}

// stop() immer aufrufen - start() sperrt den Bus auch dann, wenn der Sensor
// nicht antwortet (M5GFX gibt die Sperre erst in endTransaction frei).
bool sccbRead(uint16_t reg, uint8_t& value) {
    const uint8_t addr[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    auto& i2c = M5.In_I2C;
    const bool ok = i2c.start(kSccbAddr, false, kSccbFreq)
                 && i2c.write(addr, sizeof(addr))
                 && i2c.restart(kSccbAddr, true, kSccbFreq)
                 && i2c.read(&value, 1, true);
    i2c.stop();
    return ok;
}

bool sccbWrite(uint16_t reg, uint8_t value) {
    const uint8_t data[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value };
    auto& i2c = M5.In_I2C;
    const bool ok = i2c.start(kSccbAddr, false, kSccbFreq) && i2c.write(data, sizeof(data));
    return i2c.stop() && ok;
}

bool readChipId(uint16_t& id) {
    uint8_t hi = 0, lo = 0;
    const bool ok = sccbRead(kRegChipIdH, hi) && sccbRead(kRegChipIdL, lo);
    id = (uint16_t)((hi << 8) | lo);
    return ok;
}

// Sensor einschalten: Reset, Takt, Freigabe. Ohne Takt antwortet er nicht.
bool sensorPowerUp() {
    resetHold(true);
    if (!clockStart()) { resetHold(false); return false; }
    delay(kClockSettleMs);
    resetHold(false);
    delay(kResetToSccbMs);
    return true;
}

// Startsequenz schreiben (Datenstrom bleibt aus).
bool sensorConfigure() {
    for (const Sc202csReg& r : kSc202csInit720p) {
        if (!sccbWrite(r.reg, r.val)) {
            Serial.printf("[Kamera] Sensor-Register 0x%04X nicht geschrieben.\n", r.reg);
            return false;
        }
        if (r.reg == kSc202csRegSoftReset) delay(kSoftResetMs);
    }
    return true;
}

// ------------------------------------------------------------------------------
// Pufferverwaltung (Interrupt des CSI-Treibers, siehe Kopf von CameraTab5.h)
// ------------------------------------------------------------------------------
// Bildende: Ziel fuer das NAECHSTE Bild liefern. Beim Start (esp_cam_ctlr_start)
// wird diese Funktion im Task aufgerufen, sonst im Interrupt - daher _SAFE.
IRAM_ATTR bool onGetNewTrans(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void*) {
    portENTER_CRITICAL_SAFE(&s_lock);
    s_completed = s_filling;            // eben fertig (Cache-Abgleich folgt)
    int next = -1;
    for (int i = 0; i < kBuffers; i++) {
        if (i != s_reading && i != s_completed) { next = i; break; }
    }
    if (next >= 0 && next == s_ready) s_ready = -1;   // altes Bild wird ueberschrieben
    s_filling = next;
    portEXIT_CRITICAL_SAFE(&s_lock);

    if (next >= 0) {
        trans->buffer = s_buf[next];
        trans->buflen = kFrameBytes;
    }
    // Sonst kein Puffer: Der Treiber nimmt seinen Reservepuffer (Bild entfaellt).
    return false;
}

// Nach dem Cache-Abgleich: das eben fertige Bild zum Abholen freigeben.
IRAM_ATTR bool onTransFinished(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void*) {
    portENTER_CRITICAL_SAFE(&s_lock);
    if (s_completed >= 0 && trans->buffer == s_buf[s_completed]) {
        s_ready = s_completed;
        s_frames++;
    }
    s_completed = -1;
    portEXIT_CRITICAL_SAFE(&s_lock);
    return false;
}

// ------------------------------------------------------------------------------
// Weissabgleich
// ------------------------------------------------------------------------------
bool applyGains() {
    esp_isp_ccm_config_t ccm = {};
    ccm.matrix[0][0] = s_gainR;
    ccm.matrix[1][1] = 1.0f;
    ccm.matrix[2][2] = s_gainB;
    ccm.saturation   = true;
    return esp_isp_ccm_configure(s_isp, &ccm) == ESP_OK;
}

// Mittelwerte R, G, B einer Stichprobe (0..255) und daraus neue Verstaerkungen.
void autoWhiteBalance(const uint16_t* frame) {
    uint32_t sumR = 0, sumG = 0, sumB = 0, n = 0;
    for (int y = kAwbStep / 2; y < CAMERA_HEIGHT; y += kAwbStep) {
        const uint16_t* row = frame + (size_t)y * CAMERA_WIDTH;
        for (int x = kAwbStep / 2; x < CAMERA_WIDTH; x += kAwbStep) {
            const uint16_t p = row[x];
            const int r = ((p >> 11) & 0x1F) * 255 / 31;
            const int g = ((p >> 5)  & 0x3F) * 255 / 63;
            const int b = ( p        & 0x1F) * 255 / 31;
            const int luma = (r + 2 * g + b) / 4;
            if (luma < kAwbDarkMin || luma > kAwbBrightMax) continue;
            sumR += r; sumG += g; sumB += b; n++;
        }
    }
    if (n < 100 || sumR == 0 || sumB == 0) return;   // zu dunkel/hell fuer eine Aussage

    s_gainR = fminf(kGainMax, fmaxf(kGainMin, s_gainR * powf((float)sumG / sumR, kAwbDamping)));
    s_gainB = fminf(kGainMax, fmaxf(kGainMin, s_gainB * powf((float)sumG / sumB, kAwbDamping)));
    applyGains();
}

// ------------------------------------------------------------------------------
// Belichtungsautomatik
// ------------------------------------------------------------------------------
// Schreibt Belichtungszeit und Verstaerkung zum Belichtungsprodukt in den
// Sensor - nur die Register, deren Wert sich aendert.
bool applyExposure() {
    const ExposureSetting  set  = CameraExposure_Split(s_expProduct);
    const ExposureGainRegs regs = CameraExposure_GainRegs(set.gain);
    bool ok = true;

    if (set.lines != s_expLines) {
        ok = sccbWrite(kRegExposureH, (uint8_t)((set.lines >> 12) & 0x0F))
          && sccbWrite(kRegExposureM, (uint8_t)((set.lines >> 4) & 0xFF))
          && sccbWrite(kRegExposureL, (uint8_t)((set.lines & 0x0F) << 4));
        if (ok) s_expLines = set.lines;
    }
    if (ok && (regs.analog != s_expRegs.analog || regs.digitalCoarse != s_expRegs.digitalCoarse ||
               regs.digitalFine != s_expRegs.digitalFine)) {
        ok = sccbWrite(kRegGainDigFine, regs.digitalFine)
          && sccbWrite(kRegGainDigCoarse, regs.digitalCoarse)
          && sccbWrite(kRegGainAnalog, regs.analog);
        if (ok) s_expRegs = regs;
    }
    return ok;
}

// Helligkeitsverteilung derselben Stichprobe wie beim Weissabgleich, daraus
// das naechste Belichtungsprodukt.
void autoExposure(const uint16_t* frame) {
    uint32_t hist[AE_HIST_BINS] = {};
    for (int y = kAwbStep / 2; y < CAMERA_HEIGHT; y += kAwbStep) {
        const uint16_t* row = frame + (size_t)y * CAMERA_WIDTH;
        for (int x = kAwbStep / 2; x < CAMERA_WIDTH; x += kAwbStep) {
            const uint16_t p = row[x];
            const int r = ((p >> 11) & 0x1F) * 255 / 31;
            const int g = ((p >> 5)  & 0x3F) * 255 / 63;
            const int b = ( p        & 0x1F) * 255 / 31;
            const int luma = (r + 2 * g + b) / 4;
            hist[luma * AE_HIST_BINS / 256]++;
        }
    }
    s_expBrightness = CameraExposure_Percentile95(hist);

    const float next = CameraExposure_Next(s_expProduct, s_expBrightness);
    if (next == s_expProduct) return;   // im Totband
    s_expProduct = next;
    applyExposure();
}

bool allocBuffers() {
    for (int i = 0; i < kBuffers; i++) {
        s_buf[i] = (uint16_t*)heap_caps_aligned_calloc(kBufferAlign, 1, kFrameBytes, MALLOC_CAP_SPIRAM);
        if (!s_buf[i]) return false;
    }
    return true;
}

void freeBuffers() {
    for (int i = 0; i < kBuffers; i++) {
        heap_caps_free(s_buf[i]);
        s_buf[i] = nullptr;
    }
}

// ------------------------------------------------------------------------------
// CSI und ISP
// ------------------------------------------------------------------------------
bool pipelineCreate() {
    esp_cam_ctlr_csi_config_t csi = {};
    csi.ctlr_id                = 0;
    csi.clk_src                = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
    csi.h_res                  = CAMERA_WIDTH;
    csi.v_res                  = CAMERA_HEIGHT;
    csi.data_lane_num          = kLanes;
    csi.lane_bit_rate_mbps     = kLaneBitRateMbps;
    csi.input_data_color_type  = CAM_CTLR_COLOR_RAW8;
    csi.output_data_color_type = CAM_CTLR_COLOR_RGB565;
    csi.queue_items            = 1;
    csi.byte_swap_en           = false;
    csi.bk_buffer_dis          = false;     // Reservepuffer, wenn keiner frei ist

    esp_err_t err = esp_cam_new_csi_ctlr(&csi, &s_cam);
    if (err != ESP_OK) { Serial.printf("[Kamera] CSI: %s\n", esp_err_to_name(err)); return false; }

    const esp_cam_ctlr_evt_cbs_t cbs = { onGetNewTrans, onTransFinished };
    err = esp_cam_ctlr_register_event_callbacks(s_cam, &cbs, nullptr);
    if (err == ESP_OK) err = esp_cam_ctlr_enable(s_cam);
    if (err != ESP_OK) { Serial.printf("[Kamera] CSI aktivieren: %s\n", esp_err_to_name(err)); return false; }
    s_camOn = true;

    esp_isp_processor_cfg_t isp = {};
    isp.clk_src                = ISP_CLK_SRC_DEFAULT;
    isp.clk_hz                 = kIspClockHz;
    isp.input_data_source      = ISP_INPUT_DATA_SOURCE_CSI;
    isp.input_data_color_type  = ISP_COLOR_RAW8;
    isp.output_data_color_type = ISP_COLOR_RGB565;
    isp.has_line_start_packet  = false;
    isp.has_line_end_packet    = false;
    isp.h_res                  = CAMERA_WIDTH;
    isp.v_res                  = CAMERA_HEIGHT;
    isp.bayer_order            = COLOR_RAW_ELEMENT_ORDER_BGGR;

    err = esp_isp_new_processor(&isp, &s_isp);
    if (err == ESP_OK) err = applyGains() ? esp_isp_ccm_enable(s_isp) : ESP_FAIL;
    if (err == ESP_OK) err = esp_isp_enable(s_isp);
    if (err != ESP_OK) { Serial.printf("[Kamera] ISP: %s\n", esp_err_to_name(err)); return false; }
    s_ispOn = true;

    err = esp_cam_ctlr_start(s_cam);
    if (err != ESP_OK) { Serial.printf("[Kamera] CSI-Start: %s\n", esp_err_to_name(err)); return false; }
    s_started = true;
    return true;
}

// Raeumt alles auf, was angelegt wurde - auch nach einem halben Start.
void pipelineDestroy() {
    if (s_started) { esp_cam_ctlr_stop(s_cam); s_started = false; }
    if (s_camOn)   { esp_cam_ctlr_disable(s_cam); s_camOn = false; }
    if (s_cam)     { esp_cam_ctlr_del(s_cam); s_cam = nullptr; }
    if (s_ispOn)   { esp_isp_disable(s_isp); s_ispOn = false; }
    if (s_isp)     { esp_isp_del_processor(s_isp); s_isp = nullptr; }
}

void resetFrameState() {
    portENTER_CRITICAL(&s_lock);
    s_filling = s_completed = s_ready = s_reading = -1;
    s_frames  = 0;
    portEXIT_CRITICAL(&s_lock);
}

} // namespace

// ==============================================================================
// Oeffentliche Funktionen
// ==============================================================================
CameraProbeResult CameraTab5_Probe() {
    CameraProbeResult r = {};

    if (s_running) {
        r.clockOk = true;
        r.ack     = readChipId(r.chipId);
    } else {
        r.clockOk = sensorPowerUp();
        if (r.clockOk) r.ack = readChipId(r.chipId);
        clockStop();
        resetHold(false);   // Grundzustand wie nach M5.begin(): Reset freigegeben
    }
    r.isSc202cs = r.ack && r.chipId == kChipIdSc202cs;

    Serial.printf("[Kamera] Pruefung: Takt %s, I2C 0x%02X %s, Chip-ID 0x%04X%s\n",
                  r.clockOk ? "an" : "FEHLER", kSccbAddr, r.ack ? "antwortet" : "KEINE ANTWORT",
                  r.chipId, r.isSc202cs ? " = SC202CS (SC2356)" : "");
    return r;
}

bool CameraTab5_Start() {
    if (s_running) return true;
    const uint32_t t0 = millis();
    resetFrameState();

    uint16_t id = 0;
    bool ok = allocBuffers();
    if (!ok) Serial.println("[Kamera] Kein PSRAM fuer die Bildpuffer.");
    ok = ok && sensorPowerUp();
    if (ok && (!readChipId(id) || id != kChipIdSc202cs)) {
        Serial.printf("[Kamera] Sensor nicht erkannt (ID 0x%04X).\n", id);
        ok = false;
    }
    // Nach der Registertabelle (feste Belichtung) gleich die zuletzt passende
    // Einstellung setzen - der Sensor wurde zurueckgesetzt, also alles schreiben.
    s_expLines = 0;
    s_expRegs  = { 0xFF, 0xFF, 0xFF };
    s_expBrightness = -1;
    ok = ok && sensorConfigure() && applyExposure() && pipelineCreate()
            && sccbWrite(kSc202csRegStream, 0x01);

    if (!ok) {
        CameraTab5_Stop();
        return false;
    }
    s_running = true;
    Serial.printf("[Kamera] Laeuft: %dx%d RAW8 -> RGB565, %d Lane %d Mbit/s (Start %lu ms).\n",
                  CAMERA_WIDTH, CAMERA_HEIGHT, kLanes, kLaneBitRateMbps,
                  (unsigned long)(millis() - t0));
    return true;
}

void CameraTab5_Stop() {
    if (s_mclk) sccbWrite(kSc202csRegStream, 0x00);   // Datenstrom aus (Sensor hat Takt)
    pipelineDestroy();
    clockStop();
    resetHold(false);
    freeBuffers();
    resetFrameState();
    if (s_running) Serial.println("[Kamera] Angehalten.");
    s_running = false;
}

bool CameraTab5_IsRunning() {
    return s_running;
}

const uint16_t* CameraTab5_AcquireFrame() {
    if (!s_running) return nullptr;
    int idx;
    portENTER_CRITICAL(&s_lock);
    idx = s_ready;
    if (idx >= 0) { s_reading = idx; s_ready = -1; }
    portEXIT_CRITICAL(&s_lock);
    if (idx < 0) return nullptr;

    // Weissabgleich und Belichtungsautomatik laufen beim Abholen mit: so
    // bekommt sie jeder Verbraucher (Vorschau, QR-Leser), ohne eigenen Task.
    // Kosten je < 1 ms (Stichprobe von 3600 Punkten; Sensor-Register werden
    // nur bei einer Aenderung geschrieben).
    s_acquired++;
    if (s_acquired % kAeEveryFrames == 0)  autoExposure(s_buf[idx]);
    if (s_acquired % kAwbEveryFrames == 0) autoWhiteBalance(s_buf[idx]);
    return s_buf[idx];
}

void CameraTab5_ReleaseFrame() {
    portENTER_CRITICAL(&s_lock);
    s_reading = -1;
    portEXIT_CRITICAL(&s_lock);
}

uint32_t CameraTab5_FrameCount() {
    return s_frames;
}

void CameraTab5_WhiteGains(float& red, float& blue) {
    red  = s_gainR;
    blue = s_gainB;
}

void CameraTab5_Exposure(float& timeMs, float& gain, int& brightness) {
    timeMs     = (float)s_expLines * kLineTimeMs;
    gain       = (s_expRegs.analog == 0xFF) ? 1.0f : CameraExposure_GainOf(s_expRegs);
    brightness = s_expBrightness;
}
