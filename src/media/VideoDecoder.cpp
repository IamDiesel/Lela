// ==============================================================================
// VideoDecoder - Implementierung
// ==============================================================================

#include "VideoDecoder.h"
#include "DebugFlags.h"
#include "esp_heap_caps.h"
#include <string.h>

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
static jpeg_decoder_handle_t s_engine = nullptr;
static bool s_ready = false;

// Ausgabepuffer (RGB565)
static uint8_t* s_outBuf[VIDEO_OUT_SLOTS] = { nullptr };
static size_t   s_outSize[VIDEO_OUT_SLOTS] = { 0 };
static uint8_t  s_nextOut = 0;

// Eingabepuffer (JPEG). Anzahl steht in VideoDecoder.h.
static uint8_t* s_inBuf[VIDEO_IN_SLOTS] = { nullptr };
static size_t   s_inSize[VIDEO_IN_SLOTS] = { 0 };
static bool     s_inInternal = false;   // liegen die Eingabepuffer im SRAM?

static uint32_t s_errors  = 0;
static uint32_t s_rejects = 0;

// Kleinster tatsaechlich reservierter Ausgabepuffer. Gegen DIESEN Wert wird
// geprueft, nicht gegen den von Slot 0: der Treiber rundet je Aufruf auf, die
// Slots koennen sich also unterscheiden.
static size_t s_outUsable = 0;

// Kanalreihenfolge. Der PPA schreibt unveraendert in den Framebuffer und
// braucht deshalb RGB; der Weg ueber M5GFX/LVGL braucht BGR. Voreinstellung
// ist BGR, damit der Fenstermodus ohne Zutun stimmt.
static volatile bool s_bgrOrder = true;

void VideoDecoder_SetBgrOrder(bool bgr) { s_bgrOrder = bgr; }

// ------------------------------------------------------------------------------
// Speicheranforderung des groessten erlaubten Bildes
//
// Bei YUV420/YUV422-Quellen padded der Decoder die Ausgabe auf Vielfache von
// 16 Pixeln. Der Puffer muss deshalb fuer die aufgerundeten Masse reichen,
// nicht nur fuer die sichtbaren.
// ------------------------------------------------------------------------------
static inline size_t alignUp16(size_t v) { return (v + 15u) & ~((size_t)15u); }

static size_t maxOutputBytes() {
    return alignUp16(VIDEO_MAX_WIDTH) * alignUp16(VIDEO_MAX_HEIGHT) * VIDEO_BYTES_PER_PX;
}

// ------------------------------------------------------------------------------
// Speicher anlegen
// ------------------------------------------------------------------------------
static uint8_t* allocDecoderMem(size_t bytes, jpeg_dec_buffer_alloc_direction_t dir,
                                size_t* actual, const char* what) {
    jpeg_decode_memory_alloc_cfg_t cfg = {};
    cfg.buffer_direction = dir;

    size_t got = 0;
    uint8_t* p = (uint8_t*)jpeg_alloc_decoder_mem(bytes, &cfg, &got);

    if (p) {
        // Die tatsaechlich reservierte Groesse zurueckgeben, nicht die
        // angeforderte - der Treiber rundet auf.
        if (actual) *actual = got;
        Serial.printf("[VideoDecoder] %s: %u KB bei %p\n",
                      what, (unsigned)(got / 1024), p);
    } else {
        Serial.printf("[VideoDecoder] FEHLER: %s (%u KB) konnte nicht angelegt werden.\n",
                      what, (unsigned)(bytes / 1024));
        if (actual) *actual = 0;
    }
    return p;
}

// ------------------------------------------------------------------------------
// Eingabepuffer: internes SRAM bevorzugt
// ------------------------------------------------------------------------------
// Warum nicht einfach jpeg_alloc_decoder_mem()? Weil der Treiber fuer die
// Eingaberichtung grundsaetzlich PSRAM waehlt. Das kostet zweimal Bandbreite
// je Bild: einmal beim Hineinschreiben durch die CPU (memcpy aus dem Socket)
// und einmal beim Herauslesen durch die DMA2D des JPEG-Decoders. Beide Wege
// teilen sich den PSRAM zusaetzlich mit dem PPA und dem DSI-Scanout, der den
// Framebuffer mit 60 Hz dauerhaft liest.
//
// Internes SRAM hat keine dieser Konkurrenzen. Der Preis ist Platz: bei
// VIDEO_MAX_JPEG_BYTES = 96 KiB und zwei Slots sind das 192 KiB von rund
// 300 KiB freiem internem Speicher.
//
// Ausrichtung: 64 Byte deckt die Cachezeile des P4 und die Anforderung der
// DMA2D ab; die Groesse wird ebenfalls auf 64 aufgerundet, damit ein
// Cache-Rueckschreiben nie in fremden Speicher hineinreicht.
//
// Faellt die interne Belegung aus (zu wenig Platz), wird ohne Drama auf den
// bisherigen Weg zurueckgeschaltet. Das Video laeuft dann wie vorher.
static uint8_t* allocInputMem(size_t bytes, size_t* actual, const char* what) {
#if VIDEO_JPEG_IN_SRAM
    const size_t aligned = (bytes + 63u) & ~((size_t)63u);

    // NICHT einfach belegen, solange es geht. Der interne Speicher gehoert
    // nicht uns allein: esp_hosted zieht bei jedem SDIO-Empfang einen
    // DMA-faehigen Puffer nach und bricht mit einem assert ab, wenn keiner
    // mehr kommt. Deshalb erst fragen, ob hinterher noch genug uebrig bleibt.
    const size_t freeNow = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t reserve = (size_t)VIDEO_SRAM_RESERVE_KB * 1024u;

    if (freeNow >= aligned + reserve) {
        uint8_t* p = (uint8_t*)heap_caps_aligned_alloc(64, aligned,
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (p) {
            if (actual) *actual = aligned;
            Serial.printf("[VideoDecoder] %s: %u KB im internen SRAM bei %p\n",
                          what, (unsigned)(aligned / 1024), p);
            return p;
        }
    }

    Serial.printf("[VideoDecoder] %s: interner Platz reicht nicht (%u KB frei, "
                  "%u KB noetig plus %u KB Reserve) - nutze PSRAM.\n",
                  what, (unsigned)(freeNow / 1024), (unsigned)(aligned / 1024),
                  (unsigned)VIDEO_SRAM_RESERVE_KB);
#endif
    s_inInternal = false;
    return allocDecoderMem(bytes, JPEG_DEC_ALLOC_INPUT_BUFFER, actual, what);
}

// Gibt alles frei, was bis zum Fehlerpunkt schon belegt wurde. Ein halb
// aufgebauter Decoder waere die schlechteste aller Welten: Speicher belegt,
// Engine als DMA2D-Client angemeldet, aber nicht benutzbar.
static void initCleanup() {
    for (int i = 0; i < VIDEO_OUT_SLOTS; i++) {
        if (s_outBuf[i]) { heap_caps_free(s_outBuf[i]); s_outBuf[i] = nullptr; }
        s_outSize[i] = 0;
    }
    for (int i = 0; i < VIDEO_IN_SLOTS; i++) {
        if (s_inBuf[i]) { heap_caps_free(s_inBuf[i]); s_inBuf[i] = nullptr; }
        s_inSize[i] = 0;
    }
    if (s_engine) { jpeg_del_decoder_engine(s_engine); s_engine = nullptr; }
    s_outUsable = 0;
}

bool VideoDecoder_Init() {
    if (s_ready) return true;

    Serial.printf("[VideoDecoder] Start. Frei: intern %u KB, PSRAM %u KB\n",
                  (unsigned)(ESP.getFreeHeap() / 1024),
                  (unsigned)(ESP.getFreePsram() / 1024));

    // --- Engine: genau einmal, ueber die gesamte Laufzeit ---------------------
    // Ein Neuaufbau meldet den DMA2D-Client ab und wieder an; laeuft dabei noch
    // ein Transfer, bleibt ein Kanal verklemmt zurueck.
    s_outUsable = 0;

    jpeg_decode_engine_cfg_t engCfg = {};
    engCfg.intr_priority = 0;      // Treiber waehlt
    engCfg.timeout_ms    = 400;    // grosszuegig: PPA und Display teilen sich
                                   // die PSRAM-Bandbreite, ein knapper Wert
                                   // erzeugt sporadische Scheinfehler.
    if (jpeg_new_decoder_engine(&engCfg, &s_engine) != ESP_OK || !s_engine) {
        Serial.println("[VideoDecoder] FEHLER: Decoder-Engine nicht verfuegbar.");
        return false;
    }

    // --- Ausgabepuffer -------------------------------------------------------
    const size_t outBytes = maxOutputBytes();
    for (int i = 0; i < VIDEO_OUT_SLOTS; i++) {
        char what[32];
        snprintf(what, sizeof(what), "Ausgabepuffer %d", i);
        s_outBuf[i] = allocDecoderMem(outBytes, JPEG_DEC_ALLOC_OUTPUT_BUFFER,
                                      &s_outSize[i], what);
        if (!s_outBuf[i]) { initCleanup(); return false; }

        if (s_outUsable == 0 || s_outSize[i] < s_outUsable) s_outUsable = s_outSize[i];
    }

    // --- Eingabepuffer -------------------------------------------------------
    s_inInternal = (VIDEO_JPEG_IN_SRAM != 0);   // allocInputMem() setzt zurueck
    for (int i = 0; i < VIDEO_IN_SLOTS; i++) {
        char what[32];
        snprintf(what, sizeof(what), "Eingabepuffer %d", i);
        s_inBuf[i] = allocInputMem(VIDEO_MAX_JPEG_BYTES, &s_inSize[i], what);
        if (!s_inBuf[i]) { initCleanup(); return false; }
    }

    s_ready = true;
    Serial.printf("[VideoDecoder] Eingabe liegt im %s.\n",
                  s_inInternal ? "internen SRAM" : "PSRAM");
    Serial.printf("[VideoDecoder] Bereit. Frei danach: intern %u KB, PSRAM %u KB\n",
                  (unsigned)(ESP.getFreeHeap() / 1024),
                  (unsigned)(ESP.getFreePsram() / 1024));
    return true;
}

bool VideoDecoder_IsReady() { return s_ready; }

uint8_t VideoDecoder_InputSlotCount() { return VIDEO_IN_SLOTS; }

uint8_t* VideoDecoder_GetInputBuffer(uint8_t slot, size_t* capacity) {
    if (!s_ready || slot >= VIDEO_IN_SLOTS) {
        if (capacity) *capacity = 0;
        return nullptr;
    }
    // Nutzbar ist die angeforderte Groesse; der Treiber hat ggf. mehr
    // reserviert, dieser Ueberhang gehoert ihm.
    if (capacity) *capacity = VIDEO_MAX_JPEG_BYTES;
    return s_inBuf[slot];
}

// ------------------------------------------------------------------------------
// Pruefung vor der Dekodierung
//
// Der Hardware-Decoder bekommt nur Bilder, die garantiert in den Zielpuffer
// passen. Die Kamera darf eine angeforderte Aufloesung ablehnen, die
// gelieferten Masse sind also nicht vorhersagbar.
// ------------------------------------------------------------------------------
bool VideoDecoder_Probe(const uint8_t* jpeg, size_t len, uint16_t* w, uint16_t* h) {
    if (!s_ready || !jpeg || len < 128 || len > VIDEO_MAX_JPEG_BYTES) {
        s_rejects++;
        return false;
    }

    jpeg_decode_picture_info_t info = {};
    if (jpeg_decoder_get_info(jpeg, len, &info) != ESP_OK) {
        s_rejects++;
        return false;
    }

    if (info.width == 0 || info.height == 0 ||
        info.width > VIDEO_MAX_WIDTH || info.height > VIDEO_MAX_HEIGHT) {
        s_rejects++;
        return false;
    }

    // Auch die gepaddeten Masse muessen passen - und zwar in JEDEN Slot.
    const size_t need = alignUp16(info.width) * alignUp16(info.height) * VIDEO_BYTES_PER_PX;
    if (s_outUsable == 0 || need > s_outUsable) {
        s_rejects++;
        return false;
    }

    if (w) *w = (uint16_t)info.width;
    if (h) *h = (uint16_t)info.height;
    return true;
}

// ------------------------------------------------------------------------------
// Zeilen zusammenschieben
// ------------------------------------------------------------------------------
// Der Hardware-Decoder legt die Ausgabe mit einer auf 16 Pixel aufgerundeten
// Zeilenlaenge ab (MCU-Breite). Ist die Bildbreite selbst kein Vielfaches von
// 16, stimmt die Zeilenlaenge im Puffer nicht mit der Bildbreite ueberein.
// M5GFX und LVGL kennen keinen getrennten Stride - sie wuerden das Bild
// schraeg auslesen (die bekannten Diagonalstreifen).
//
// Statt solche Bilder zu verwerfen, schieben wir die Zeilen einmalig zusammen.
// Das kostet einen memmove ueber das Bild, faellt aber nur bei ungewoehnlichen
// Aufloesungen an: 1280, 640, 320, 800, 1920 sind alle durch 16 teilbar.
//
// Richtung: Ziel liegt immer VOR der Quelle, deshalb ist die Schleife von oben
// nach unten korrekt und die Bereiche ueberlappen nie rueckwaerts.
static void compactRows(uint8_t* buf, uint16_t w, uint16_t h) {
    const size_t rowBytes    = (size_t)w * VIDEO_BYTES_PER_PX;
    const size_t strideBytes = alignUp16(w) * VIDEO_BYTES_PER_PX;
    if (strideBytes == rowBytes) return;

    for (uint16_t y = 1; y < h; y++) {
        memmove(buf + (size_t)y * rowBytes, buf + (size_t)y * strideBytes, rowBytes);
    }
}

// ------------------------------------------------------------------------------
// Dekodierung
// ------------------------------------------------------------------------------
bool VideoDecoder_Decode(const uint8_t* jpeg, size_t len, DecodedFrame* out) {
    if (!s_ready || !out) return false;

    uint16_t w = 0, h = 0;
    if (!VideoDecoder_Probe(jpeg, len, &w, &h)) return false;

    const uint8_t slot = s_nextOut;

    jpeg_decode_cfg_t cfg = {};
    cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    // Fenster- und Vollbildmodus gehen beide ueber M5GFX, also durchgaengig
    // dieselbe Byte-Reihenfolge.
    cfg.rgb_order     = s_bgrOrder ? JPEG_DEC_RGB_ELEMENT_ORDER_BGR
                                   : JPEG_DEC_RGB_ELEMENT_ORDER_RGB;
    cfg.conv_std      = JPEG_YUV_RGB_CONV_STD_BT601;

    // out_size darf nicht null sein: in IDF 5.4.x/5.5.1 dereferenziert der
    // Treiber den Zeiger ungeprueft.
    uint32_t produced = 0;

    esp_err_t r = jpeg_decoder_process(s_engine, &cfg, jpeg, len,
                                       s_outBuf[slot], s_outSize[slot], &produced);
    if (r != ESP_OK) {
        s_errors++;
        return false;
    }

    // Weniger Bytes als eine Zeile: der Transfer ist abgebrochen, der Inhalt
    // ist unbrauchbar. Lieber ein Bild auslassen als Muell anzeigen.
    if (produced < (uint32_t)w * VIDEO_BYTES_PER_PX) {
        s_errors++;
        return false;
    }

    compactRows(s_outBuf[slot], w, h);

    s_nextOut = (uint8_t)((s_nextOut + 1) % VIDEO_OUT_SLOTS);

    out->pixels = s_outBuf[slot];
    out->width  = w;
    out->height = h;
    out->slot   = slot;
    return true;
}

uint32_t VideoDecoder_GetErrorCount()  { return s_errors; }
uint32_t VideoDecoder_GetRejectCount() { return s_rejects; }
