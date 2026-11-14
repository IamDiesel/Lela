#pragma once
// ==============================================================================
// MapTileStore - dauerhafter Kachelspeicher auf LittleFS
// ==============================================================================
// Warum dauerhaft: Ein Revier von 1-10 km^2 umfasst ueber die sinnvollen
// Zoomstufen nur einige Dutzend bis wenige hundert Kacheln (1-5 MB). Einmal
// geladen, kommen sie nach jedem Neustart vom Flash - ohne Netz, ohne
// TLS-Handshake. Das Netz wird danach nur noch zum gelegentlichen Nachfragen
// ("hat sich die Kachel geaendert?") gebraucht.
//
// Ablage: eine Datei je Kachel, MAP_STORE_DIR/<z>_<x>_<y>.bin =
//   Kopf (MapTileFileHeader, feste Groesse) + PNG-Daten unveraendert.
// Ein unvollstaendig geschriebener Eintrag (Stromausfall) faellt an der
// Laengenpruefung auf und wird verworfen.
//
// Grenzen:
//   * Quote MAP_STORE_QUOTA_BYTES; darueber wird verdraengt (zuletzt in diesem
//     Lauf benutzt zuerst behalten, sonst die aeltesten zuerst).
//   * Nach dem Schreiben bleiben mindestens MAP_STORE_FS_RESERVE_BYTES auf dem
//     Dateisystem frei - die Dashboard-Konfiguration hat Vorrang.
//
// Das Dateisystem bindet HaConfigLogic::Init() ein (beim Start, gui.init()).
// Ist es nicht verfuegbar, arbeitet der Kachel-Dienst nur mit dem PSRAM-Cache.
//
// Threading: NUR aus dem Kachel-Task (MapTileService) aufrufen. Der Index ist
// deshalb ohne eigene Sperre. LittleFS selbst ist threadsicher.
// ==============================================================================

#include <Arduino.h>
#include "MapProjection.h"

// Cache-Angaben einer Kachel (aus den HTTP-Antwortkoepfen).
struct MapTileMeta {
    uint32_t fetchedAt = 0;       // Unix-Zeit des letzten Abrufs bzw. der letzten Bestaetigung (304)
    uint32_t expiresAt = 0;       // Unix-Zeit, ab der nachgefragt wird
    char     etag[64] = {0};      // fuer If-None-Match (leer = unbekannt)
    char     lastModified[40] = {0}; // fuer If-Modified-Since (leer = unbekannt)
};

// Bindet den Speicher ein und liest den Index (einmalig, danach sofort true).
// false, wenn kein Dateisystem verfuegbar ist.
bool MapTileStore_Begin();

// Liest eine Kachel in 'buf' (Kapazitaet 'cap'). false, wenn nicht vorhanden
// oder defekt (defekte Eintraege werden dabei geloescht).
bool MapTileStore_Load(MapTileKey key, uint8_t* buf, size_t cap, size_t& len, MapTileMeta& meta);

// Legt eine Kachel ab (ersetzt eine vorhandene). Verdraengt bei Bedarf.
// false, wenn trotz Verdraengung kein Platz ist - die Kachel bleibt dann nur
// im PSRAM-Cache.
bool MapTileStore_Save(MapTileKey key, const uint8_t* data, size_t len, const MapTileMeta& meta);

// Schreibt nur den Kopf neu (nach "304 Not Modified": neue Ablaufzeit).
bool MapTileStore_UpdateMeta(MapTileKey key, const MapTileMeta& meta);

void MapTileStore_Remove(MapTileKey key);

// Fuer Diagnose-Ausgaben. Duerfen (als einzige) aus jedem Task gerufen werden.
size_t MapTileStore_Count();
size_t MapTileStore_UsedBytes();
