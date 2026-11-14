// ==============================================================================
// MapTileService - Implementierung (siehe Header)
// ==============================================================================

#include "MapTileService.h"
#include "MapTileConfig.h"
#include "MapTileStore.h"
#include "MapPngDecoder.h"
#include "DebugFlags.h"
#include "HeapMonitor.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <memory>

// Zertifikats-Bundle des ESP-IDF (alle gaengigen Wurzelzertifikate). Es ist in
// libmbedtls.a eingebettet (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL)
// und liegt im Flash - kein RAM-Bedarf. Gegenueber einem fest hinterlegten
// Wurzelzertifikat uebersteht es einen Zertifikatswechsel des Anbieters.
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// Fruehestens gueltige Uhrzeit (Nov. 2023). Davor ist NTP noch nicht durch -
// ohne Uhrzeit keine Zertifikatspruefung und keine Ablaufzeiten.
static const time_t MIN_VALID_EPOCH = 1700000000;

// Wie lange ein Auftrag auf das Speicherventil wartet, bevor er verworfen
// wird. Die Ansicht fordert sichtbare Kacheln bei der naechsten
// Aktualisierung ohnehin erneut an.
static const uint32_t VALVE_WAIT_MS = 10000;

// Groesse einer dekodierten Kachel in Pixeln bzw. Bytes.
static const size_t TILE_PIXELS = (size_t)MAP_TILE_PX * MAP_TILE_PX;
static const size_t TILE_BYTES  = TILE_PIXELS * sizeof(uint16_t);

// Hintergrund fuer transparente Pixel und noch fehlende Bereiche (hellgrau).
static const uint16_t TILE_BACKGROUND = 0xDEFB;

// ------------------------------------------------------------------------------
// Zustand (geteilt, unter s_mutex)
// ------------------------------------------------------------------------------
struct RamTile {
    MapTileKey key;
    uint16_t*  pixels;     // nullptr = Platz frei
    uint32_t   lastUse;
};

static SemaphoreHandle_t       s_mutex = nullptr;
static QueueHandle_t           s_queue = nullptr;
static bool                    s_taskRunning = false;
static RamTile                 s_ram[MAP_RAM_CACHE_TILES] = {};
static uint32_t                s_ramTick = 0;
static std::vector<MapTileKey> s_pending;          // in der Warteschlange oder in Arbeit
static volatile uint32_t       s_version = 0;
static volatile bool           s_memoryPaused = false;

// Nur Kachel-Task
static uint32_t s_netBlockedUntil = 0;   // millis(); 0 = frei

// Messwerte. Nur der Kachel-Task schreibt; der Messlauf in der Hauptschleife
// liest sie fuer den Bericht (einzelne Woerter, Unschaerfe ist hier egal).
struct MapStats {
    uint32_t fromFlash;
    uint32_t fromNet;
    uint32_t notModified;
    uint32_t failed;
    uint32_t connections;
    uint32_t netBytes;
    uint32_t netMs;
    uint32_t decodeMs;
    uint32_t decoded;
};
static MapStats s_stats = {};

// ------------------------------------------------------------------------------
// Hilfsfunktionen
// ------------------------------------------------------------------------------
static bool lock()   { return s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE; }
static void unlock() { xSemaphoreGive(s_mutex); }

static void formatKey(MapTileKey key, char* out, size_t len) {
    snprintf(out, len, "%u/%lu/%lu", (unsigned)MapTileKey_Z(key),
             (unsigned long)MapTileKey_X(key), (unsigned long)MapTileKey_Y(key));
}

static bool timeValid(time_t& now) {
    now = time(nullptr);
    return now >= MIN_VALID_EPOCH;
}

// Nur unter s_mutex.
static int findRamLocked(MapTileKey key) {
    for (int i = 0; i < MAP_RAM_CACHE_TILES; i++) {
        if (s_ram[i].pixels && s_ram[i].key == key) return i;
    }
    return -1;
}

// Offene Auftraege (Warteschlange + in Arbeit). Derzeit nur im Messlauf benutzt.
[[maybe_unused]] static size_t pendingCount() {
    if (!lock()) return 0;
    const size_t n = s_pending.size();
    unlock();
    return n;
}

static void removePending(MapTileKey key) {
    if (!lock()) return;
    auto it = std::find(s_pending.begin(), s_pending.end(), key);
    if (it != s_pending.end()) s_pending.erase(it);
    unlock();
}

// Uebernimmt einen dekodierten Puffer in den PSRAM-Cache (Besitz geht ueber).
// Verdraengt bei Bedarf die am laengsten nicht benutzte Kachel.
static void publishTile(MapTileKey key, uint16_t* pixels) {
    if (!lock()) { heap_caps_free(pixels); return; }

    int slot = findRamLocked(key);
    if (slot < 0) {
        for (int i = 0; i < MAP_RAM_CACHE_TILES; i++) {
            if (!s_ram[i].pixels) { slot = i; break; }
            if (slot < 0 || s_ram[i].lastUse < s_ram[slot].lastUse) slot = i;
        }
    }
    uint16_t* old = s_ram[slot].pixels;
    s_ram[slot].key     = key;
    s_ram[slot].pixels  = pixels;
    s_ram[slot].lastUse = ++s_ramTick;
    s_version = s_version + 1;   // volatile: kein ++ (C++20-Warnung)
    unlock();

    if (old) heap_caps_free(old);
}

// Speicherventil mit Hysterese (gleiche Logik wie beim Video, BabyService).
static bool memoryOk() {
    const size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    if (s_memoryPaused) {
        if (block >= MAP_DMA_RESUME_ABOVE) {
            s_memoryPaused = false;
            Serial.printf("[Map] Speicher wieder ausreichend (DMA-Block %u KB) - Kachelabruf laeuft weiter.\n",
                          (unsigned)(block / 1024));
        }
    } else if (block < MAP_DMA_PAUSE_BELOW) {
        s_memoryPaused = true;
        Serial.printf("[Map] Speicher knapp (DMA-Block %u KB) - Kachelabruf pausiert.\n",
                      (unsigned)(block / 1024));
    }
    return !s_memoryPaused;
}

static bool waitForMemory() {
    const uint32_t start = millis();
    while (!memoryOk()) {
        if (millis() - start > VALVE_WAIT_MS) return false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return true;
}

// "public, max-age=604800" -> 604800; 0, wenn nicht vorhanden.
static uint32_t parseMaxAge(const String& cacheControl) {
    const int pos = cacheControl.indexOf("max-age=");
    if (pos < 0) return 0;
    const long v = atol(cacheControl.c_str() + pos + 8);
    return v > 0 ? (uint32_t)v : 0;
}

static void copyHeader(char* dst, size_t dstLen, const String& value) {
    if (value.length() == 0) return;          // unbekannt: alten Wert behalten
    strncpy(dst, value.c_str(), dstLen - 1);
    dst[dstLen - 1] = '\0';
}

// URL aus der Vorlage: {z} {x} {y} ersetzen.
static String buildUrl(MapTileKey key) {
    String url = MAP_TILE_URL_TEMPLATE;
    url.replace("{z}", String((unsigned)MapTileKey_Z(key)));
    url.replace("{x}", String((unsigned long)MapTileKey_X(key)));
    url.replace("{y}", String((unsigned long)MapTileKey_Y(key)));
    return url;
}

// ------------------------------------------------------------------------------
// Senke fuer HTTPClient::writeToStream (nur bei Antworten ohne Content-Length,
// also "chunked"). Schreibt in einen festen Puffer und merkt sich Ueberlauf.
// ------------------------------------------------------------------------------
class BufferSink : public Stream {
public:
    BufferSink(uint8_t* buf, size_t cap) : m_buf(buf), m_cap(cap) {}
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* data, size_t n) override {
        if (m_len + n > m_cap) { m_overflow = true; return 0; }
        memcpy(m_buf + m_len, data, n);
        m_len += n;
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t length() const   { return m_len; }
    bool   overflow() const { return m_overflow; }

private:
    uint8_t* m_buf;
    size_t   m_cap;
    size_t   m_len = 0;
    bool     m_overflow = false;
};

// ------------------------------------------------------------------------------
// Kachel-Task
// ------------------------------------------------------------------------------
// Alles, was nur waehrend der Lebenszeit des Tasks existiert. Der Destruktor
// gibt Verbindung, Puffer und Decoder frei, bevor der Task sich loescht.
class TileWorker {
public:
    TileWorker() {
        m_buf = (uint8_t*)heap_caps_malloc(MAP_TILE_MAX_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        m_tls = String(MAP_TILE_URL_TEMPLATE).startsWith("https://");
    }

    // Verbindung und Client gibt die Reihenfolge der Mitglieder frei (siehe
    // m_client / m_http), nicht dieser Rumpf.
    ~TileWorker() {
        m_http.end();
        if (m_buf) heap_caps_free(m_buf);
    }

    bool ready() const { return m_buf != nullptr; }

    uint32_t processed() const { return m_processed; }

    void process(MapTileKey key);

private:
    uint8_t*       m_buf = nullptr;      // Rohdaten einer Kachel (PSRAM)
    bool           m_tls = true;

    // REIHENFOLGE NICHT AENDERN (Absturz 13.11.2026, Instruction access fault
    // in ~TileWorker): HTTPClient merkt sich den Client fuer Keep-Alive und
    // ruft in SEINEM Destruktor _client->stop() auf. Frueher wurde der Client
    // im Rumpf von ~TileWorker geloescht - der Destruktor von m_http lief
    // danach und sprang ueber die vtable eines freigegebenen Objekts.
    // Mitglieder werden in umgekehrter Reihenfolge zerstoert: m_http zuerst,
    // dann m_client - der Client lebt also, solange HTTPClient ihn anfasst.
    std::unique_ptr<NetworkClient> m_client;   // NetworkClientSecure bei https
    HTTPClient     m_http;
    MapPngDecoder  m_decoder;
    uint32_t       m_processed = 0;

    bool decodeAndPublish(MapTileKey key, const uint8_t* data, size_t len);
    bool networkAvailable();
    void ensureClient();
    bool readBody(size_t& len);
    void fetch(MapTileKey key, bool haveStored, MapTileMeta& meta);
};

bool TileWorker::decodeAndPublish(MapTileKey key, const uint8_t* data, size_t len) {
    uint16_t* pixels = (uint16_t*)heap_caps_malloc(TILE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels) return false;

    const uint32_t t0 = millis();
    const bool ok = m_decoder.decode(data, len, pixels, MAP_TILE_PX, MAP_TILE_PX, TILE_BACKGROUND);
    s_stats.decodeMs += millis() - t0;

    if (!ok) {
        heap_caps_free(pixels);
        return false;
    }
    s_stats.decoded++;
    publishTile(key, pixels);
    return true;
}

bool TileWorker::networkAvailable() {
    if (WiFi.status() != WL_CONNECTED) return false;
    time_t now;
    if (!timeValid(now)) return false;
    if (s_netBlockedUntil != 0 && (int32_t)(millis() - s_netBlockedUntil) < 0) return false;
    s_netBlockedUntil = 0;
    return true;
}

void TileWorker::ensureClient() {
    if (m_client) return;
    if (m_tls) {
        NetworkClientSecure* secure = new NetworkClientSecure();
        secure->setCACertBundle(x509_crt_bundle_start, x509_crt_bundle_end - x509_crt_bundle_start);
        secure->setHandshakeTimeout(MAP_TLS_HANDSHAKE_TIMEOUT_S);
        m_client.reset(secure);
    } else {
        m_client.reset(new NetworkClient());
    }
}

// Liest den Rumpf der laufenden Antwort nach m_buf. Mit Content-Length direkt
// in den PSRAM-Puffer (HTTPClient wuerde dafuer einen 4-KB-Zwischenpuffer im
// internen RAM anlegen); ohne Content-Length ueber writeToStream.
bool TileWorker::readBody(size_t& len) {
    len = 0;
    const int size = m_http.getSize();

    if (size < 0) {
        BufferSink sink(m_buf, MAP_TILE_MAX_BYTES);
        const int n = m_http.writeToStream(&sink);
        if (n <= 0 || sink.overflow()) return false;
        len = sink.length();
        return true;
    }

    if (size == 0 || (size_t)size > MAP_TILE_MAX_BYTES) return false;

    NetworkClient* stream = m_http.getStreamPtr();
    if (!stream) return false;

    uint32_t lastData = millis();
    while (len < (size_t)size) {
        const int n = stream->read(m_buf + len, (size_t)size - len);
        if (n > 0) {
            len += (size_t)n;
            lastData = millis();
            continue;
        }
        if (!stream->connected() && stream->available() <= 0) break;
        if (millis() - lastData > MAP_HTTP_TIMEOUT_MS) break;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return len == (size_t)size;
}

void TileWorker::fetch(MapTileKey key, bool haveStored, MapTileMeta& meta) {
    char name[24];
    formatKey(key, name, sizeof(name));

    ensureClient();
    const bool newConnection = !m_client->connected();

    // collectHeaders() erwartet ein nicht-konstantes Zeigerfeld.
    static const char* HEADERS[] = { "Cache-Control", "ETag", "Last-Modified" };
    m_http.setReuse(true);
    m_http.setUserAgent(MAP_HTTP_USER_AGENT);
    m_http.setTimeout(MAP_HTTP_TIMEOUT_MS);
    m_http.setConnectTimeout(MAP_HTTP_TIMEOUT_MS);
    m_http.collectHeaders(HEADERS, sizeof(HEADERS) / sizeof(HEADERS[0]));

    if (!m_http.begin(*m_client, buildUrl(key))) {
        s_stats.failed++;
        return;
    }
    if (haveStored) {
        if (meta.etag[0])         m_http.addHeader("If-None-Match", meta.etag);
        if (meta.lastModified[0]) m_http.addHeader("If-Modified-Since", meta.lastModified);
    }

    const uint32_t t0 = millis();
    const int code = m_http.GET();

    if (newConnection && code > 0) {
        s_stats.connections++;
#if MAP_PROFILE
        Serial.printf("[Map] Neue %s-Verbindung: erste Antwort nach %u ms\n",
                      m_tls ? "TLS" : "TCP", (unsigned)(millis() - t0));
        HeapMonitor_Log("Map nach Verbindungsaufbau");
#endif
    }

    // Antwortkoepfe sofort sichern - vor end() und vor dem naechsten Auftrag.
    const String etagHeader         = m_http.header("ETag");
    const String lastModifiedHeader = m_http.header("Last-Modified");
    const uint32_t maxAge = std::max(parseMaxAge(m_http.header("Cache-Control")), MAP_MIN_CACHE_SECONDS);

    time_t now;
    timeValid(now);

    if (code == HTTP_CODE_OK) {
        size_t len = 0;
        const bool bodyOk = readBody(len);
        const uint32_t netMs = millis() - t0;
        m_http.end();

        if (!bodyOk) {
            s_stats.failed++;
            Serial.printf("[Map] Kachel %s unvollstaendig empfangen.\n", name);
            m_client->stop();
            s_netBlockedUntil = millis() + MAP_NET_ERROR_BACKOFF_MS;
            return;
        }

        MapTileMeta fresh;
        fresh.fetchedAt = (uint32_t)now;
        fresh.expiresAt = (uint32_t)now + maxAge;
        copyHeader(fresh.etag, sizeof(fresh.etag), etagHeader);
        copyHeader(fresh.lastModified, sizeof(fresh.lastModified), lastModifiedHeader);

        if (!decodeAndPublish(key, m_buf, len)) {
            s_stats.failed++;
            Serial.printf("[Map] Antwort fuer %s ist keine gueltige Kachel (%u Byte).\n", name, (unsigned)len);
            return;
        }
        MapTileStore_Save(key, m_buf, len, fresh);

        s_stats.fromNet++;
        s_stats.netBytes += len;
        s_stats.netMs += netMs;
#if MAP_PROFILE
        Serial.printf("[Map] Kachel %s aus dem Netz (%u KB, %u ms)\n", name, (unsigned)((len + 512) / 1024), (unsigned)netMs);
#endif
        return;
    }

    if (code == HTTP_CODE_NOT_MODIFIED && haveStored) {
        m_http.end();
        meta.fetchedAt = (uint32_t)now;
        meta.expiresAt = (uint32_t)now + maxAge;
        copyHeader(meta.etag, sizeof(meta.etag), etagHeader);
        copyHeader(meta.lastModified, sizeof(meta.lastModified), lastModifiedHeader);
        MapTileStore_UpdateMeta(key, meta);
        s_stats.notModified++;
#if MAP_PROFILE
        Serial.printf("[Map] Kachel %s unveraendert (304)\n", name);
#endif
        return;
    }

    // Fehler: Verbindung verwerfen und eine Weile Ruhe geben.
    m_http.end();
    m_client->stop();
    s_stats.failed++;
    if (code < 0) {
        Serial.printf("[Map] Netzfehler fuer %s: %s\n", name, HTTPClient::errorToString(code).c_str());
        s_netBlockedUntil = millis() + MAP_NET_ERROR_BACKOFF_MS;
    } else {
        Serial.printf("[Map] HTTP %d fuer %s\n", code, name);
        // 4xx heisst: der Server lehnt uns ab (z.B. Policy) - laenger warten.
        s_netBlockedUntil = millis() + ((code >= 400 && code < 500) ? MAP_HTTP_REJECT_BACKOFF_MS
                                                                    : MAP_NET_ERROR_BACKOFF_MS);
    }
}

void TileWorker::process(MapTileKey key) {
    m_processed++;
    if (!waitForMemory()) return;

    // 1. Flash: sofort zeigen, auch wenn abgelaufen.
    MapTileMeta meta;
    size_t len = 0;
    [[maybe_unused]] const uint32_t t0 = millis();   // nur fuer MAP_PROFILE
    bool stored = MapTileStore_Load(key, m_buf, MAP_TILE_MAX_BYTES, len, meta);
    if (stored) {
        if (decodeAndPublish(key, m_buf, len)) {
            s_stats.fromFlash++;
#if MAP_PROFILE
            char name[24];
            formatKey(key, name, sizeof(name));
            Serial.printf("[Map] Kachel %s aus dem Flash (%u KB, Lesen + Dekodieren %u ms)\n",
                          name, (unsigned)((len + 512) / 1024), (unsigned)(millis() - t0));
#endif
        } else {
            MapTileStore_Remove(key);
            stored = false;
        }
    }

    // 2. Netz nur, wenn noetig: Kachel fehlt oder ist abgelaufen.
    time_t now;
    const bool clockOk = timeValid(now);
    if (stored && (!clockOk || meta.expiresAt > (uint32_t)now)) return;
    if (!networkAvailable()) return;

    fetch(key, stored, meta);
}

static void workerTask(void*) {
    {
        TileWorker worker;
        if (!worker.ready()) {
            Serial.println("[Map] FEHLER: kein PSRAM fuer den Kachelpuffer.");
        }
        MapTileStore_Begin();

        uint32_t idleSince = millis();
        while (true) {
            MapTileKey key;
            if (xQueueReceive(s_queue, &key, pdMS_TO_TICKS(1000)) == pdTRUE) {
                if (worker.ready()) worker.process(key);
                removePending(key);
                idleSince = millis();
                vTaskDelay(pdMS_TO_TICKS(MAP_WORKER_TILE_PAUSE_MS));
                continue;
            }
            if (millis() - idleSince < MAP_WORKER_IDLE_EXIT_MS) continue;

            // Beenden nur, wenn wirklich nichts mehr ansteht. Unter derselben
            // Sperre, unter der MapTileService_Request() den Task startet -
            // so geht kein Auftrag zwischen Pruefen und Beenden verloren.
            if (!lock()) continue;
            if (uxQueueMessagesWaiting(s_queue) == 0) {
                s_taskRunning = false;
                unlock();
                Serial.printf("[Map] Kachel-Task beendet (%u Auftraege, Stack-Reserve %u Byte)\n",
                              (unsigned)worker.processed(),
                              (unsigned)uxTaskGetStackHighWaterMark(nullptr));
                break;
            }
            unlock();
        }
    }   // ~TileWorker: Verbindung, Puffer, Decoder freigeben
    vTaskDelete(nullptr);
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void MapTileService_Init() {
    if (s_mutex) return;
    s_mutex = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(MAP_QUEUE_LEN, sizeof(MapTileKey));
    if (!s_mutex || !s_queue) {
        Serial.println("[Map] FEHLER: Mutex/Warteschlange fehlen, Kartendienst bleibt aus.");
        if (s_mutex) { vSemaphoreDelete(s_mutex); s_mutex = nullptr; }
        if (s_queue) { vQueueDelete(s_queue); s_queue = nullptr; }
    }
}

bool MapTileService_Request(MapTileKey key) {
    if (!s_queue || !lock()) return false;

    const int idx = findRamLocked(key);
    if (idx >= 0) {
        s_ram[idx].lastUse = ++s_ramTick;
        unlock();
        return true;
    }
    if (std::find(s_pending.begin(), s_pending.end(), key) != s_pending.end()) {
        unlock();
        return false;
    }
    if (xQueueSend(s_queue, &key, 0) != pdTRUE) {   // voll: spaeter erneut
        unlock();
        return false;
    }
    s_pending.push_back(key);

    const bool start = !s_taskRunning;
    if (start) s_taskRunning = true;
    unlock();

    if (start && xTaskCreatePinnedToCore(workerTask, "MapTiles", MAP_WORKER_STACK, nullptr,
                                         MAP_WORKER_PRIORITY, nullptr, MAP_WORKER_CORE) != pdPASS) {
        Serial.println("[Map] FEHLER: Kachel-Task laesst sich nicht anlegen.");
        if (lock()) {
            s_taskRunning = false;
            xQueueReset(s_queue);
            s_pending.clear();
            unlock();
        }
    }
    return false;
}

bool MapTileService_CopyTile(MapTileKey key, uint16_t* dst, uint16_t dstStridePx,
                             uint32_t offX, uint32_t offY, uint16_t w, uint16_t h, uint8_t shift) {
    const uint32_t edge = (uint32_t)MAP_TILE_PX << shift;
    if (!dst || shift > 4 || offX + w > edge || offY + h > edge) return false;
    if (!lock()) return false;

    const int idx = findRamLocked(key);
    if (idx < 0) { unlock(); return false; }
    const uint16_t* tile = s_ram[idx].pixels;

    if (shift == 0) {
        const uint16_t* src = tile + (size_t)offY * MAP_TILE_PX + offX;
        for (uint16_t row = 0; row < h; row++) {
            memcpy(dst + (size_t)row * dstStridePx, src + (size_t)row * MAP_TILE_PX, (size_t)w * sizeof(uint16_t));
        }
    } else {
        // Pixelverdopplung: Zielpixel (offX + c, offY + r) der vergroesserten
        // Kachel stammt von Quellpixel ((offX + c) >> shift, (offY + r) >> shift).
        for (uint16_t row = 0; row < h; row++) {
            const uint16_t* srcRow = tile + (size_t)((offY + row) >> shift) * MAP_TILE_PX;
            uint16_t* out = dst + (size_t)row * dstStridePx;
            for (uint16_t col = 0; col < w; col++) {
                out[col] = srcRow[(offX + col) >> shift];
            }
        }
    }
    s_ram[idx].lastUse = ++s_ramTick;
    unlock();
    return true;
}

void MapTileService_CancelPending() {
    if (!s_queue || !lock()) return;
    xQueueReset(s_queue);
    s_pending.clear();
    unlock();
}

uint32_t MapTileService_GetVersion() { return s_version; }

bool MapTileService_IsMemoryPaused() { return s_memoryPaused; }

// ------------------------------------------------------------------------------
// Messlauf (MAP_PROFILE, siehe DebugFlags.h)
// ------------------------------------------------------------------------------
#if MAP_PROFILE
// Oeffentlicher Ort statt Wohnort: die Messung braucht keine privaten Daten,
// und Koordinaten im Quelltext landen auf GitHub. Brandenburger Tor, Berlin.
static const double   PROFILE_LAT        = 52.5163;
static const double   PROFILE_LON        = 13.3777;
static const double   PROFILE_HALF_SIZE_M = 750.0;   // Ausschnitt 1,5 x 1,5 km
static const uint8_t  PROFILE_ZOOMS[]    = { 15, 16 };
// Wartezeit nach WLAN + Uhrzeit: genug, um vor dem Messlauf HA-Ansicht,
// Video-Overlay und Ton einzuschalten (Messung unter Last).
static const uint32_t PROFILE_SETTLE_MS  = 60000;
static const uint32_t PROFILE_TIMEOUT_MS = 180000;

static void profileUpdate() {
    enum class State { Waiting, Running, Done };
    static State                   state = State::Waiting;
    static uint32_t                readySince = 0;
    static uint32_t                startedAt = 0;
    static uint32_t                lastCheck = 0;
    static std::vector<MapTileKey> keys;

    if (state == State::Done) return;

    if (state == State::Waiting) {
        time_t now;
        if (WiFi.status() != WL_CONNECTED || !timeValid(now)) { readySince = 0; return; }
        if (readySince == 0) readySince = millis();
        if (millis() - readySince < PROFILE_SETTLE_MS) return;

        for (uint8_t z : PROFILE_ZOOMS) {
            const MapPoint c    = MapProjection_LatLonToPixel(PROFILE_LAT, PROFILE_LON, z);
            const double   half = PROFILE_HALF_SIZE_M / MapProjection_MetersPerPixel(PROFILE_LAT, z);
            const MapPoint tl   = { c.x - half, c.y - half };
            const MapPoint br   = { c.x + half, c.y + half };
            uint32_t x0, y0, x1, y1;
            MapProjection_TileRange(tl, br, z, x0, y0, x1, y1);
            for (uint32_t y = y0; y <= y1; y++) {
                for (uint32_t x = x0; x <= x1; x++) keys.push_back(MapTileKey_Make(z, x, y));
            }
        }

        // Pruefwert der Projektion (Header): Berlin z10 -> (550, 335).
        const MapPoint check = MapProjection_LatLonToPixel(52.52, 13.405, 10);
        Serial.printf("[MapProf] Projektion: Berlin z10 -> Kachel (%u, %u), erwartet (550, 335)\n",
                      (unsigned)(check.x / MAP_TILE_PX), (unsigned)(check.y / MAP_TILE_PX));
        Serial.printf("[MapProf] Start: %u Kacheln (z15-z16, 1,5 x 1,5 km)\n", (unsigned)keys.size());
        HeapMonitor_Log("MapProf Start");

        s_stats = MapStats();
        for (MapTileKey k : keys) MapTileService_Request(k);
        startedAt = millis();
        state = State::Running;
        return;
    }

    // Running: jede Kachel wurde genau einmal angefordert. Fertig, sobald
    // keine Auftraege mehr offen sind.
    //
    // Frueher wurden hier alle 2 s alle Kacheln erneut angefordert, bis
    // alle gleichzeitig im PSRAM lagen. Der Ausschnitt hat aber 34 Kacheln,
    // der Cache nur 32 (MAP_RAM_CACHE_TILES): zwei waren immer verdraengt,
    // also wurde 3 Minuten lang ununterbrochen aus dem Flash nachgeladen und
    // dekodiert (Messung 13.11.2026: 206 Flash-Ladevorgaenge statt 34).
    if (millis() - lastCheck < 2000) return;
    lastCheck = millis();

    const bool timeout = millis() - startedAt > PROFILE_TIMEOUT_MS;
    if (pendingCount() > 0 && !timeout) return;

    const uint32_t netTiles = s_stats.fromNet;
    const uint32_t handled  = s_stats.fromFlash + netTiles + s_stats.notModified + s_stats.failed;
    Serial.printf("[MapProf] Ende nach %u ms%s: %u/%u bearbeitet | Flash %u, Netz %u (%u KB), 304: %u, Fehler %u | Verbindungen %u\n",
                  (unsigned)(millis() - startedAt), timeout ? " (Zeitlimit)" : "",
                  (unsigned)handled, (unsigned)keys.size(),
                  (unsigned)s_stats.fromFlash, (unsigned)netTiles, (unsigned)(s_stats.netBytes / 1024),
                  (unsigned)s_stats.notModified, (unsigned)s_stats.failed, (unsigned)s_stats.connections);
    Serial.printf("[MapProf] Mittelwerte: Netz %u ms/Kachel, Dekodierung %u ms/Kachel | Kachelspeicher %u Kacheln, %u KB\n",
                  (unsigned)(netTiles ? s_stats.netMs / netTiles : 0),
                  (unsigned)(s_stats.decoded ? s_stats.decodeMs / s_stats.decoded : 0),
                  (unsigned)MapTileStore_Count(), (unsigned)(MapTileStore_UsedBytes() / 1024));
    HeapMonitor_Log("MapProf Ende");
    state = State::Done;
}
#endif

void MapTileService_Update() {
#if MAP_PROFILE
    profileUpdate();
#endif
}
