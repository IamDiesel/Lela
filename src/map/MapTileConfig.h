#pragma once
// ==============================================================================
// MapTileConfig - Parameter des Kartenkachel-Dienstes
// ==============================================================================
// Alle Zahlen des Kartenmoduls an einer Stelle. Begruendungen und Mengengeruest
// siehe Konzept "Karten-Kachel" (Projektdoku map_tile_konzept_2026-11-13.md).
//
// Mengengeruest (Mitteleuropa, ~50 Grad Nord): eine Kachel ist bei z15 rund
// 790 m breit, bei z16 rund 390 m, bei z17 rund 200 m. Ein Revier von 1 km^2
// braucht ueber z15-z17 rund 90 Kacheln (~2 MB PNG). Das passt dauerhaft auf
// den Flash und dekodiert ins PSRAM.
// ==============================================================================

#include <Arduino.h>

// ------------------------------------------------------------------------------
// Kachelquelle
// ------------------------------------------------------------------------------
// Das Schema entscheidet ueber den Transport: https:// -> TLS, http:// -> TCP.
// So bleibt ein LAN-Proxy oder ein anderer Anbieter ohne Codeaenderung moeglich.
// Platzhalter: {z} {x} {y}.
#define MAP_TILE_URL_TEMPLATE "https://tile.openstreetmap.org/{z}/{x}/{y}.png"

// OSM-Tile-Usage-Policy: eindeutiger User-Agent der Anwendung. Bewusst ohne
// persoenliche Angaben.
#define MAP_HTTP_USER_AGENT "Lela-Tab5/2.0 (private home dashboard)"

// Kantenlaenge einer Kachel in Pixeln (Standard aller Raster-Kachelanbieter).
static const uint16_t MAP_TILE_PX = 256;

// Hoechste Zoomstufe, die angefordert wird (OSM liefert bis 19).
static const uint8_t MAP_MAX_ZOOM = 19;

// Hoechste Zoomstufe der Darstellung - wie die HA-Kartenkarte, deren Grenze
// ebenfalls 20 ist (Daniel, Massstabsbalken 5 m). OSM liefert nur bis 19;
// fuer 20 vergroessert die Ansicht die Kacheln von MAP_MAX_ZOOM um Faktor 2
// (Pixelverdopplung). Weiter vergroessert wird bewusst nicht: das Bild wird
// nur unschaerfer, ohne mehr zu zeigen.
static const uint8_t MAP_MAX_VIEW_ZOOM = 20;

// ------------------------------------------------------------------------------
// Netz
// ------------------------------------------------------------------------------
// Groesste akzeptierte Kachel. OSM-Kacheln liegen bei 5-40 KB; alles darueber
// ist keine Kartenkachel (Fehlerseite, falscher Anbieter).
static const size_t   MAP_TILE_MAX_BYTES        = 96 * 1024;

static const uint32_t MAP_HTTP_TIMEOUT_MS       = 10000;
static const uint32_t MAP_TLS_HANDSHAKE_TIMEOUT_S = 10;

// Mindestgueltigkeit einer Kachel. Kartenbilder aendern sich selten; ein
// kuerzerer Wert aus Cache-Control wird auf diese Frist angehoben, ein
// laengerer uebernommen. Danach wird nur nachgefragt (If-None-Match), die
// Antwort ist fast immer "304 Not Modified" ohne Bilddaten.
static const uint32_t MAP_MIN_CACHE_SECONDS     = 7UL * 24UL * 3600UL;

// Nach einem Netzfehler bzw. einer Ablehnung durch den Server (4xx) wird so
// lange nicht mehr angefragt. Schuetzt Server und WLAN vor Wiederholschleifen.
static const uint32_t MAP_NET_ERROR_BACKOFF_MS  = 30UL * 1000UL;
static const uint32_t MAP_HTTP_REJECT_BACKOFF_MS = 10UL * 60UL * 1000UL;

// ------------------------------------------------------------------------------
// Speicher
// ------------------------------------------------------------------------------
// Dekodierte Kacheln im PSRAM (je 256 x 256 x 2 Byte = 128 KB).
// Muss MEHR fassen als gleichzeitig sichtbar sein kann - sonst verdraengen
// sich die Kacheln eines Bildes gegenseitig und es wird endlos nachgeladen
// (genau das passierte im ersten Messlauf mit 34 Kacheln bei 32 Plaetzen).
// Sichtbar hoechstens: Vollbild 1280x720 -> 6x4, versetzt 7x5 = 35 Kacheln,
// dazu die Kachel-Ansicht im Dashboard (520x340 -> bis 9). 56 Plaetze = 7 MB.
static const uint8_t  MAP_RAM_CACHE_TILES       = 56;

// Dauerhafter Kachelspeicher auf LittleFS (Partition 3,375 MB, darin sonst
// nur ha_config.json).
#define MAP_STORE_DIR "/tiles"
static const size_t   MAP_STORE_QUOTA_BYTES     = 2UL * 1024UL * 1024UL;

// So viel muss auf dem Dateisystem nach dem Schreiben einer Kachel frei
// bleiben. Das atomare Speichern der Dashboard-Konfiguration (Datei + Kopie)
// hat Vorrang vor dem Kachelspeicher.
static const size_t   MAP_STORE_FS_RESERVE_BYTES = 512UL * 1024UL;

// ------------------------------------------------------------------------------
// Speicherventil (interner DMA-Speicher, siehe HeapMonitor.h)
// ------------------------------------------------------------------------------
// Unterhalb dieser Groesse des groessten freien DMA-Blocks wird weder eine
// Verbindung aufgebaut noch dekodiert. Hysterese wie beim Video-Ventil.
static const size_t   MAP_DMA_PAUSE_BELOW       = 24 * 1024;
static const size_t   MAP_DMA_RESUME_ABOVE      = 32 * 1024;

// ------------------------------------------------------------------------------
// Arbeits-Task
// ------------------------------------------------------------------------------
// Der Task entsteht bei Bedarf und beendet sich nach dieser Leerlaufzeit
// wieder. Damit gibt er seinen Stack (internes RAM) und die TLS-Verbindung
// frei, solange keine Karte etwas braucht.
static const uint32_t MAP_WORKER_IDLE_EXIT_MS   = 30UL * 1000UL;

// Stack: TLS-Handshake mit Zertifikatspruefung braucht ~6-7 KB. Der
// Hochwasserstand wird beim Beenden geloggt.
static const uint32_t MAP_WORKER_STACK          = 8192;

// Prioritaet des Leerlauf-Tasks (0). Grund (Absturz 13.11.2026):
// PNG-Dekodierung ist reine Rechenarbeit. Kommen viele Kacheln aus dem Flash,
// rechnet der Task ohne eine einzige Wartezeit. Mit Prioritaet 1 kam der
// Leerlauf-Task IDLE0 dann nicht mehr zum Zug; nach 5 s
// (CONFIG_ESP_TASK_WDT_TIMEOUT_S) brach der Task-Watchdog das System ab.
// Auf Prioritaet 0 teilen sich Kachel-Task und IDLE0 die Rechenzeit im
// Zeitscheibenverfahren - der Watchdog wird bedient, und jeder andere Task
// (Video, WLAN, HA) hat ohnehin Vorrang.
static const UBaseType_t MAP_WORKER_PRIORITY    = tskIDLE_PRIORITY;
static const BaseType_t  MAP_WORKER_CORE        = 0;

// Pause nach jeder Kachel. Zweite Sicherung neben der Prioritaet: verteilt
// einen Schub (z.B. 34 Kacheln aus dem Flash) ueber die Zeit, statt Kern 0
// und die PSRAM-Bandbreite am Stueck zu belegen.
static const uint32_t MAP_WORKER_TILE_PAUSE_MS  = 20;

// Laenge der Auftragswarteschlange.
static const uint16_t MAP_QUEUE_LEN             = 64;
