#pragma once
// ==============================================================================
// CameraTab5 - Kamera des Tab5 (SC2356 / SC202CS, MIPI-CSI)
// ==============================================================================
// Ziel: QR-Codes scannen fuer die WLAN-Einrichtung (Weg B, Projekt-Notiz
// camera_qr_2026-11-15). Eigenes schlankes Modul auf den IDF-Treibern, die das
// Arduino-Framework 3.2.1 bereits mitbringt (esp_driver_cam, esp_driver_isp) -
// ohne Versionswechsel. Die Startsequenz des Sensors stammt aus Espressifs
// esp_cam_sensor (CameraTab5Sc202cs.h, Apache-2.0).
//
// Hardware (Schaltplan / M5Stack Tab5-BSP):
//   - Sensor SC202CS (Handelsname SC2356), 2 MP, MIPI-CSI 1 Lane
//   - Steuerung SCCB = I2C an G31/G32 (Systembus, M5.In_I2C), Adresse 0x36,
//     16-Bit-Registeradressen, Chip-ID in 0x3107/0x3108 = 0xEB52
//   - Takt MCLK 24 MHz an G36. M5Stack erzeugt ihn per LEDC; hier per
//     Takt-Ausgang des ESP32-P4 (SPLL 480 MHz / 20 = genau 24 MHz). Grund: Die
//     LEDC-Taktquelle ist global, Arduino hat sie fuer die Hintergrund-
//     beleuchtung auf XTAL (40 MHz) gestellt - damit sind 24 MHz nicht moeglich.
//   - Reset CAM_RST an Expander U6 (0x43) P6, aktiv low
//   - MIPI-PHY-Versorgung (LDO 3, 2,5 V) teilt sich die Kamera mit dem
//     Display; M5GFX schaltet sie beim Start ein.
//
// Bildweg: Sensor (RAW8, Bayer BGGR, 1280x720, 30 fps) -> CSI -> ISP (Demosaic
// -> RGB565) -> DMA (DW-GDMA, NICHT der 2D-DMA mit dem Haenger) in einen von
// zwei Bildpuffern im PSRAM. Der Treiber gleicht den Cache selbst ab.
//
// Bildpuffer: Der Treiber fragt bei jedem Bildende im Interrupt nach dem
// naechsten Puffer. Vergeben wird nie der Puffer, den gerade jemand liest
// (AcquireFrame), und nie der gerade fertig gewordene. Ist keiner frei,
// schreibt der Treiber in seinen eigenen Reservepuffer - das Bild entfaellt.
// So liest der Verbraucher immer ein vollstaendiges, ungestoertes Bild.
//
// Ausbaustufen:
//   1. Machbarkeit: Takt, Reset, Chip-ID              (erledigt, "OK: SC2356")
//   2. Bilder ueber CSI + ISP, Vorschau in der Diagnose  <- dieser Stand
//   3. QR-Erkennung und WLAN-Einrichtung
// ==============================================================================

#include <stdint.h>

constexpr int CAMERA_WIDTH  = 1280;
constexpr int CAMERA_HEIGHT = 720;

// Der Sensor ist im Tab5 um 180 Grad gedreht eingebaut: Die Bilder kommen
// auf dem Kopf (Befund 15.11.2026). Gedreht wird erst bei der Anzeige
// (ViewCameraPreview); die QR-Erkennung braucht keine Drehung. Bewusst nicht
// ueber die Spiegel-Register des Sensors (0x3221): Das verschiebt das
// Bayer-Raster und damit die Farben.
constexpr bool CAMERA_UPSIDE_DOWN = true;

// Ergebnis der Pruefung (Stufe 1).
struct CameraProbeResult {
    bool     clockOk;   // 24-MHz-Takt an G36 gestartet
    bool     ack;       // Sensor hat auf I2C geantwortet
    uint16_t chipId;    // Inhalt 0x3107/0x3108 (SC202CS: 0xEB52)
    bool     isSc202cs; // chipId passt
};

// Takt starten, Sensor zuruecksetzen, Chip-ID lesen, Takt wieder aus. Laeuft
// die Kamera schon, wird nur die ID gelesen. Ergebnis auch im Log ([Kamera]).
CameraProbeResult CameraTab5_Probe();

// Kamera starten: Puffer, Takt, Sensor, CSI, ISP, Datenstrom. Dauert ca.
// 100 ms. false bei Fehler (Grund im Log); dann ist alles wieder aus.
bool CameraTab5_Start();

// Kamera anhalten und alles freigeben (Sensor, Treiber, Takt, Puffer).
void CameraTab5_Stop();

bool CameraTab5_IsRunning();

// Neuestes vollstaendiges Bild leihen: RGB565 (Little Endian, wie LVGL mit
// LV_COLOR_16_SWAP 0), CAMERA_WIDTH x CAMERA_HEIGHT, Zeilen ohne Luecke.
// nullptr, wenn seit dem letzten Abholen kein neues Bild fertig ist. Nach dem
// Lesen CameraTab5_ReleaseFrame() aufrufen. Nur EIN Verbraucher zur Zeit.
const uint16_t* CameraTab5_AcquireFrame();
void CameraTab5_ReleaseFrame();

// Fertige Bilder seit dem Start (fuer eine Bildraten-Anzeige).
uint32_t CameraTab5_FrameCount();

// Aktuelle Verstaerkung des Weissabgleichs fuer Rot und Blau (Gruen = 1).
// Der Abgleich ("Grauwelt") laeuft selbsttaetig beim Abholen der Bilder mit.
void CameraTab5_WhiteGains(float& red, float& blue);

// Stand der Belichtungsautomatik: Belichtungszeit in Millisekunden,
// Verstaerkung und die zuletzt gemessene Helligkeit (95. Perzentil, 0..255;
// -1 = noch keine Messung). Auch sie laeuft beim Abholen der Bilder mit - die
// Rechnung steht in CameraExposure.h.
void CameraTab5_Exposure(float& timeMs, float& gain, int& brightness);
