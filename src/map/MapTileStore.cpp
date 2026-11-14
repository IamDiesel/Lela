// ==============================================================================
// MapTileStore - Implementierung (siehe Header)
// ==============================================================================

#include "MapTileStore.h"
#include "MapTileConfig.h"
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <vector>
#include <stdio.h>

// ------------------------------------------------------------------------------
// Dateiformat
// ------------------------------------------------------------------------------
static const uint32_t TILE_FILE_MAGIC   = 0x31544D4C;   // "LMT1" (little endian)
static const uint16_t TILE_FILE_VERSION = 1;

struct __attribute__((packed)) MapTileFileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t headerSize;
    uint32_t payloadLen;
    uint32_t fetchedAt;
    uint32_t expiresAt;
    char     etag[64];
    char     lastModified[40];
};
static_assert(sizeof(MapTileFileHeader) == 124, "Kopfgroesse ist Teil des Dateiformats");

// Partition, die LittleFS ohne Angabe einbindet (Arduino-Standard).
static const char* const FS_PARTITION_LABEL = "spiffs";

// ------------------------------------------------------------------------------
// Index
// ------------------------------------------------------------------------------
struct StoreEntry {
    MapTileKey key;
    uint32_t   fileBytes;   // Kopf + Nutzdaten
    uint32_t   fetchedAt;
    uint32_t   lastUse;     // Zaehlerstand der letzten Benutzung in diesem Lauf, 0 = nie
};

static bool                    s_begun     = false;
static bool                    s_available = false;
static std::vector<StoreEntry> s_entries;
// s_usedBytes und s_count liest die Diagnose auch aus anderen Tasks: je ein
// ausgerichtetes Wort, also ohne Sperre konsistent lesbar.
static size_t                  s_usedBytes = 0;
static size_t                  s_count     = 0;   // = s_entries.size()
static uint32_t                s_useTick   = 0;
static bool                    s_fullReported = false;

static void tilePath(MapTileKey key, char* out, size_t outLen) {
    snprintf(out, outLen, "%s/%u_%lu_%lu.bin", MAP_STORE_DIR,
             (unsigned)MapTileKey_Z(key),
             (unsigned long)MapTileKey_X(key),
             (unsigned long)MapTileKey_Y(key));
}

static int findEntry(MapTileKey key) {
    for (size_t i = 0; i < s_entries.size(); i++) {
        if (s_entries[i].key == key) return (int)i;
    }
    return -1;
}

static void dropEntry(int idx) {
    if (idx < 0) return;
    s_usedBytes -= s_entries[idx].fileBytes;
    s_entries.erase(s_entries.begin() + idx);
    s_count = s_entries.size();
}

static bool headerValid(const MapTileFileHeader& h) {
    return h.magic == TILE_FILE_MAGIC
        && h.version == TILE_FILE_VERSION
        && h.headerSize == sizeof(MapTileFileHeader)
        && h.payloadLen > 0
        && h.payloadLen <= MAP_TILE_MAX_BYTES;
}

// Verdraengt den am wenigsten wertvollen Eintrag: zuerst die in diesem Lauf
// nicht benutzten (aelteste zuerst), dann den am laengsten nicht benutzten.
static bool evictOne(MapTileKey keep) {
    int victim = -1;
    for (size_t i = 0; i < s_entries.size(); i++) {
        const StoreEntry& e = s_entries[i];
        if (e.key == keep) continue;
        if (victim < 0) { victim = (int)i; continue; }
        const StoreEntry& v = s_entries[victim];
        if (e.lastUse < v.lastUse || (e.lastUse == v.lastUse && e.fetchedAt < v.fetchedAt)) {
            victim = (int)i;
        }
    }
    if (victim < 0) return false;

    char path[48];
    tilePath(s_entries[victim].key, path, sizeof(path));
    LittleFS.remove(path);
    dropEntry(victim);
    return true;
}

static size_t fsFreeBytes() {
    const size_t total = LittleFS.totalBytes();
    const size_t used  = LittleFS.usedBytes();
    return (total > used) ? (total - used) : 0;
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
bool MapTileStore_Begin() {
    if (s_begun) return s_available;
    s_begun = true;

    if (!esp_littlefs_mounted(FS_PARTITION_LABEL)) {
        Serial.println("[Map] Kein Dateisystem - Kacheln werden nur im PSRAM gehalten.");
        return false;
    }
    if (!LittleFS.exists(MAP_STORE_DIR) && !LittleFS.mkdir(MAP_STORE_DIR)) {
        Serial.println("[Map] FEHLER: Kachelordner laesst sich nicht anlegen - nur PSRAM-Cache.");
        return false;
    }

    // Index aus den Dateikoepfen aufbauen. Defekte oder fremde Dateien loeschen.
    std::vector<String> broken;
    File dir = LittleFS.open(MAP_STORE_DIR);
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        unsigned z = 0;
        unsigned long x = 0, y = 0;
        const char* name = f.name();
        MapTileFileHeader h;
        const bool ok = sscanf(name, "%u_%lu_%lu.bin", &z, &x, &y) == 3
                     && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h)
                     && headerValid(h)
                     && f.size() == sizeof(h) + h.payloadLen;
        if (ok) {
            StoreEntry e = { MapTileKey_Make((uint8_t)z, x, y), (uint32_t)f.size(), h.fetchedAt, 0 };
            s_entries.push_back(e);
            s_usedBytes += e.fileBytes;
            s_count = s_entries.size();
        } else {
            broken.push_back(String(MAP_STORE_DIR) + "/" + name);
        }
        f.close();
    }
    dir.close();
    for (const String& p : broken) LittleFS.remove(p);

    s_available = true;
    Serial.printf("[Map] Kachelspeicher: %u Kacheln, %u KB (Quote %u KB)%s\n",
                  (unsigned)s_entries.size(), (unsigned)(s_usedBytes / 1024),
                  (unsigned)(MAP_STORE_QUOTA_BYTES / 1024),
                  broken.empty() ? "" : ", defekte Eintraege entfernt");
    return true;
}

bool MapTileStore_Load(MapTileKey key, uint8_t* buf, size_t cap, size_t& len, MapTileMeta& meta) {
    if (!s_available) return false;
    const int idx = findEntry(key);
    if (idx < 0) return false;

    char path[48];
    tilePath(key, path, sizeof(path));
    File f = LittleFS.open(path, "r");
    MapTileFileHeader h;
    bool ok = f && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && headerValid(h)
           && h.payloadLen <= cap
           && f.read(buf, h.payloadLen) == h.payloadLen;
    if (f) f.close();

    if (!ok) {
        LittleFS.remove(path);
        dropEntry(idx);
        return false;
    }

    len = h.payloadLen;
    meta.fetchedAt = h.fetchedAt;
    meta.expiresAt = h.expiresAt;
    memcpy(meta.etag, h.etag, sizeof(meta.etag));
    memcpy(meta.lastModified, h.lastModified, sizeof(meta.lastModified));
    meta.etag[sizeof(meta.etag) - 1] = '\0';
    meta.lastModified[sizeof(meta.lastModified) - 1] = '\0';

    s_entries[idx].lastUse = ++s_useTick;
    return true;
}

static void fillHeader(MapTileFileHeader& h, size_t payloadLen, const MapTileMeta& meta) {
    memset(&h, 0, sizeof(h));
    h.magic      = TILE_FILE_MAGIC;
    h.version    = TILE_FILE_VERSION;
    h.headerSize = sizeof(MapTileFileHeader);
    h.payloadLen = (uint32_t)payloadLen;
    h.fetchedAt  = meta.fetchedAt;
    h.expiresAt  = meta.expiresAt;
    strncpy(h.etag, meta.etag, sizeof(h.etag) - 1);
    strncpy(h.lastModified, meta.lastModified, sizeof(h.lastModified) - 1);
}

bool MapTileStore_Save(MapTileKey key, const uint8_t* data, size_t len, const MapTileMeta& meta) {
    if (!s_available || !data || len == 0 || len > MAP_TILE_MAX_BYTES) return false;

    char path[48];
    tilePath(key, path, sizeof(path));

    // Vorhandene Fassung zaehlt nicht gegen die Quote - sie wird ersetzt.
    const int old = findEntry(key);
    if (old >= 0) {
        LittleFS.remove(path);
        dropEntry(old);
    }

    const size_t need = sizeof(MapTileFileHeader) + len;
    while (s_usedBytes + need > MAP_STORE_QUOTA_BYTES && evictOne(key)) {}
    while (fsFreeBytes() < need + MAP_STORE_FS_RESERVE_BYTES && evictOne(key)) {}

    if (s_usedBytes + need > MAP_STORE_QUOTA_BYTES || fsFreeBytes() < need + MAP_STORE_FS_RESERVE_BYTES) {
        if (!s_fullReported) {
            s_fullReported = true;
            Serial.printf("[Map] Kachelspeicher voll (Dateisystem frei %u KB) - weitere Kacheln nur im PSRAM.\n",
                          (unsigned)(fsFreeBytes() / 1024));
        }
        return false;
    }

    MapTileFileHeader h;
    fillHeader(h, len, meta);

    File f = LittleFS.open(path, "w");
    bool ok = f && f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h)
                && f.write(data, len) == len;
    if (f) f.close();
    if (!ok) {
        LittleFS.remove(path);
        Serial.printf("[Map] FEHLER: Kachel %s nicht gespeichert.\n", path);
        return false;
    }

    StoreEntry e = { key, (uint32_t)need, meta.fetchedAt, ++s_useTick };
    s_entries.push_back(e);
    s_usedBytes += need;
    s_count = s_entries.size();
    return true;
}

bool MapTileStore_UpdateMeta(MapTileKey key, const MapTileMeta& meta) {
    if (!s_available) return false;
    const int idx = findEntry(key);
    if (idx < 0) return false;

    char path[48];
    tilePath(key, path, sizeof(path));
    File f = LittleFS.open(path, "r+");
    if (!f) return false;

    MapTileFileHeader h;
    bool ok = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && headerValid(h);
    if (ok) {
        const uint32_t payloadLen = h.payloadLen;
        fillHeader(h, payloadLen, meta);
        ok = f.seek(0) && f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h);
    }
    f.close();

    if (ok) s_entries[idx].fetchedAt = meta.fetchedAt;
    return ok;
}

void MapTileStore_Remove(MapTileKey key) {
    if (!s_available) return;
    const int idx = findEntry(key);
    if (idx < 0) return;
    char path[48];
    tilePath(key, path, sizeof(path));
    LittleFS.remove(path);
    dropEntry(idx);
}

size_t MapTileStore_Count()     { return s_count; }
size_t MapTileStore_UsedBytes() { return s_usedBytes; }
