// ==============================================================================
// MapPngDecoder - Implementierung (siehe Header)
// ==============================================================================

#include "MapPngDecoder.h"
#include <lgfx/utility/lgfx_pngle.h>
#include <string.h>

static const uint8_t PNG_SIGNATURE[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

bool MapPngDecoder_IsPng(const uint8_t* data, size_t len) {
    return data && len >= sizeof(PNG_SIGNATURE) && memcmp(data, PNG_SIGNATURE, sizeof(PNG_SIGNATURE)) == 0;
}

// ------------------------------------------------------------------------------
// Rueckrufe fuer lgfx_pngle
// ------------------------------------------------------------------------------
namespace {

// Gemeinsamer Kontext fuer Lese- und Zeichen-Rueckruf (pngle reicht denselben
// user_data-Zeiger an beide).
struct DecodeContext {
    const uint8_t* src;
    size_t         srcLen;
    size_t         srcPos;
    uint16_t*      dst;
    uint16_t       dstW;
    uint16_t       dstH;
    uint16_t       background;
};

// Liefert bis zu 'len' Bytes. buf == nullptr bedeutet: 'len' Bytes ueberspringen.
uint32_t readFromMemory(void* userData, uint8_t* buf, uint32_t len) {
    DecodeContext* ctx = static_cast<DecodeContext*>(userData);
    const size_t remaining = ctx->srcLen - ctx->srcPos;
    if (len > remaining) len = (uint32_t)remaining;
    if (buf) memcpy(buf, ctx->src + ctx->srcPos, len);
    ctx->srcPos += len;
    return len;
}

// pngle liefert je Aufruf 'len' Pixel einer Zeile ab (x, y), Abstand div_x
// (Interlace). Pixelformat: 4 Byte je Pixel in der Reihenfolge A, R, G, B.
void drawToBuffer(void* userData, uint32_t x, uint32_t y, uint_fast8_t divX, size_t len, const uint8_t* argb) {
    DecodeContext* ctx = static_cast<DecodeContext*>(userData);
    if (y >= ctx->dstH) return;

    uint16_t* row = ctx->dst + (size_t)y * ctx->dstW;
    for (size_t i = 0; i < len; i++, x += divX, argb += 4) {
        if (x >= ctx->dstW) break;
        if (argb[0] == 0) {
            row[x] = ctx->background;
            continue;
        }
        const uint8_t r = argb[1];
        const uint8_t g = argb[2];
        const uint8_t b = argb[3];
        row[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
}

} // namespace

// ------------------------------------------------------------------------------
// MapPngDecoder
// ------------------------------------------------------------------------------
MapPngDecoder::~MapPngDecoder() {
    if (m_pngle) lgfx_pngle_destroy(m_pngle);
    m_pngle = nullptr;
}

bool MapPngDecoder::decode(const uint8_t* png, size_t len, uint16_t* dst, uint16_t dstW, uint16_t dstH,
                           uint16_t background) {
    if (!dst || !MapPngDecoder_IsPng(png, len)) return false;

    if (!m_pngle) {
        m_pngle = lgfx_pngle_new();
        if (!m_pngle) return false;
    }

    DecodeContext ctx = { png, len, 0, dst, dstW, dstH, background };

    if (lgfx_pngle_prepare(m_pngle, readFromMemory, &ctx) < 0) return false;
    if (lgfx_pngle_get_width(m_pngle) != dstW || lgfx_pngle_get_height(m_pngle) != dstH) return false;

    // Vorbelegen: bricht die Dekodierung mittendrin ab, bleibt kein Rest
    // eines frueheren Bildes stehen.
    for (size_t i = 0, n = (size_t)dstW * dstH; i < n; i++) dst[i] = background;

    return lgfx_pngle_decomp(m_pngle, drawToBuffer) >= 0;
}
