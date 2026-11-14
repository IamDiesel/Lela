#pragma once
// ==============================================================================
// MapTileService - Kartenkacheln laden, zwischenspeichern, dekodieren
// ==============================================================================
// Muster wie bei den uebrigen Diensten: Arbeits-Task (kein LVGL) -> Cache mit
// Versionszaehler -> die Ansicht zieht sich, was sie braucht.
//
//   Ansicht --Request(key)--> Warteschlange --> Kachel-Task
//                                                 1. Flash (MapTileStore)
//                                                 2. Netz (HTTPS, nur wenn noetig)
//                                                 3. PNG -> RGB565 (PSRAM)
//   Ansicht <--CopyTile()---- PSRAM-Cache <-------+   Version++
//
// Sparsamkeit (Konzept, Abschnitt 3):
//   * Was einmal geladen ist, kommt vom Flash - auch nach einem Neustart.
//   * Abgelaufene Kacheln werden sofort aus dem Flash gezeigt und nur
//     nachgefragt (If-None-Match / If-Modified-Since -> meist 304 ohne Daten).
//   * Nur angeforderte (= sichtbare) Kacheln werden geladen, kein Vorausladen.
//   * Eine Verbindung, Keep-Alive waehrend eines Schubs; der Task beendet sich
//     nach MAP_WORKER_IDLE_EXIT_MS Leerlauf und gibt Stack und TLS frei.
//   * Speicherventil: unterhalb MAP_DMA_PAUSE_BELOW wird pausiert.
//
// TLS-Speicher: TlsMemory_UsePsram() muss vorher gelaufen sein (SystemLogic_Init).
//
// Threading: Request/CopyTile/CancelPending/GetVersion sind threadsicher und
// kehren sofort zurueck. Init und Update nur aus der Hauptschleife.
// ==============================================================================

#include <Arduino.h>
#include "MapProjection.h"

void MapTileService_Init();

// Hauptschleife. Treibt derzeit nur den Messlauf (MAP_PROFILE, DebugFlags.h).
void MapTileService_Update();

// Fordert eine Kachel an. true, wenn sie dekodiert bereitliegt (dann sofort
// per MapTileService_CopyTile abholbar); sonst wird sie im Hintergrund geladen
// und die Version steigt, sobald sie da ist. Mehrfache Anforderung derselben
// Kachel ist harmlos.
bool MapTileService_Request(MapTileKey key);

// Kopiert einen Ausschnitt einer bereitliegenden Kachel nach 'dst'
// (Zeilenabstand dstStridePx Pixel, RGB565 wie lv_color_t).
// 'shift' vergroessert die Kachel um 2^shift (Ueberzoom jenseits von
// MAP_MAX_ZOOM, Pixelverdopplung). offX/offY/w/h beziehen sich auf die
// VERGROESSERTE Kachel (Kantenlaenge MAP_TILE_PX << shift); bei shift 0 sind
// es einfach Pixel der Kachel.
// false, wenn die Kachel (nicht mehr) im PSRAM liegt.
bool MapTileService_CopyTile(MapTileKey key, uint16_t* dst, uint16_t dstStridePx,
                             uint32_t offX, uint32_t offY, uint16_t w, uint16_t h, uint8_t shift = 0);

// Verwirft alle noch nicht begonnenen Auftraege (Karte ausgeblendet).
void MapTileService_CancelPending();

// Steigt bei jeder neu bereitliegenden Kachel.
uint32_t MapTileService_GetVersion();

// true, solange das Speicherventil den Abruf anhaelt.
bool MapTileService_IsMemoryPaused();
