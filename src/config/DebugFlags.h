#pragma once
// ==============================================================================
// DebugFlags - Schalter fuer Diagnose und Betrieb
// ==============================================================================
// Hintergrund und Messreihen: src/doc/video_refactoring.md
// ==============================================================================

// ------------------------------------------------------------------------------
// NET_QUIET_DURING_VIDEO - kein paralleler Netzverkehr waehrend des Streams
//
// 1 = Solange isStreamActive gilt, schweigen die BabyCam-Statusabfrage, MQTT
//     und der Webserver. Der BabyCam-Heartbeat (alle 10 s) bleibt aktiv - ohne
//     ihn schliesst die Kamera laut api_RE.md den Stream.
//     ACHTUNG: Ohne MQTT gibt es in diesem Modus keinen Baby-Alarm und keine
//     Kamera-Trigger von aussen.
// 0 = Normalbetrieb (empfohlen, seit die Messungen gezeigt haben, dass
//     paralleler Netzverkehr nicht der Ausloeser war)
// ------------------------------------------------------------------------------
#define NET_QUIET_DURING_VIDEO 0

// ------------------------------------------------------------------------------
// VIDEO_PROFILE - Messzeile [VideoProf] alle 5 Sekunden
//
// Zeigt Bildrate und die Zeitanteile von Download, Dekodierung und Anzeige.
// Kostet nichts Messbares und hilft bei jeder Bildratenfrage.
//
// Fuer den Dauerbetrieb AUS. Drei Gruende:
//  * Jede Profilzeile ist eine Ausgabe ueber die USB-Serial-JTAG-Einheit. Deren
//    Interrupt-Handler stuerzt ab, wenn der USB-Host mitten im Schreiben
//    verschwindet (Kabel gezogen, PC geht schlafen) - siehe main.cpp.
//    Weniger Ausgaben heissen weniger Gelegenheiten dafuer.
//  * Die Feldstaerke (RSSI) in der Zeile ist mit esp_hosted ein RPC-Aufruf
//    zum WLAN-Chip ueber SDIO - also genau ueber die Leitung, an der das
//    Video haengt.
//  * diagFrame() durchsucht alle 5 s einen ganzen 80-KB-Block Byte fuer Byte.
//
// Zur Fehlersuche einfach wieder auf 1 stellen.
// ------------------------------------------------------------------------------
#define VIDEO_PROFILE 0

// ------------------------------------------------------------------------------
// VIDEO_USE_PPA - Hardware-Skalierung und -Drehung fuer den Vollbildmodus
//
// Das Tab5-Panel ist physisch 720x1280 HOCHKANT (M5GFX: memory_width = 720,
// memory_height = 1280). Das Querformat entsteht durch Rotation. M5GFX nimmt
// seinen schnellen memcpy-Weg aber nur bei Rotation 0
// (Panel_FrameBufferBase::writeImage: `if (r == 0 && ... && no_convert)`).
// Mit Rotation geht JEDES Pixel einzeln und transponiert in den Framebuffer -
// gemessen 2 bis 3 MB/s, also rund 930 ms fuer ein Bild in 1280x720. Sichtbar
// als senkrechter Strich, der von links nach rechts ueber den Schirm wandert.
//
// Damit ist Vollbild ohne Hardware-Hilfe rechnerisch auf etwa 1 bis 3 fps
// begrenzt - unabhaengig von Netz und Decoder.
//
// Der PPA (Pixel Processing Accelerator) dreht und skaliert per DMA. Er ist
// der einzige Weg zu zweistelligen Bildraten im Vollbild.
//
// 1 = PPA im Vollbild (Standard). Der PPA ist ein DMA2D-Nutzer, siehe
//     video_refactoring.md 2.8/2.12. Das Panel ist es dank use_dma2d = false
//     nicht mehr, es bleiben also zwei Nutzer: JPEG-Decoder und PPA.
// 0 = ohne PPA. Sicher, aber im Vollbild sehr langsam. Der Fenstermodus ist
//     davon nicht betroffen.
// ------------------------------------------------------------------------------
#define VIDEO_USE_PPA 1

// ------------------------------------------------------------------------------
// VIDEO_JPEG_IN_SRAM - JPEG-Eingabepuffer im internen SRAM
// ------------------------------------------------------------------------------
// Der Gedanke ist richtig: die Eingabepuffer werden je Bild einmal von der CPU
// beschrieben und einmal von der DMA2D des JPEG-Decoders gelesen. Im internen
// SRAM kostet beides ein Fuenftel der Zeit und belastet die PSRAM-Bandbreite
// nicht, die sich PPA und DSI-Scanout ohnehin schon teilen.
//
// Die Rechnung geht trotzdem nicht auf. Gemessen beim Start:
//
//   frei vor dem Decoder                    298 KB
//   zwei Eingabepuffer a 96 KB             -192 KB
//   frei danach                             105 KB
//
// 105 KB klingen nach genug. Sind sie nicht - denn mehrere Treiber holen sich
// ihren Speicher ERST SPAETER und koennen ihn NUR intern bekommen, weil DMA
// dort hineinschreibt:
//
//   * esp_hosted zieht bei jedem SDIO-Empfang einen Puffer nach
//     (sdio_drv.c:129, heap_caps_aligned_alloc mit MALLOC_CAP_DMA). Schlaegt
//     das fehl, liefert sdio_rx_get_buffer() einen Nullzeiger und der Treiber
//     bricht mit einem assert ab: "assert failed: sdio_rx_get_buffer
//     sdio_drv.c:674 (*buf)". Das ist kein Absturz unseres Codes - das ist der
//     WLAN-Treiber, dem der Speicher unter den Fuessen weggezogen wurde.
//   * Der I2S-Treiber legt seine DMA-Deskriptoren ebenfalls intern an. Fehlt
//     der Platz, bleibt der Lautsprecher stumm
//     ("i2s_alloc_dma_desc: allocate DMA buffer failed").
//
// Beides stand im Protokoll, unmittelbar bevor das Geraet neu startete.
//
// Der Gewinn waere ohnehin klein gewesen: rund 2 ms von 145 ms je Bild. Dafuer
// den WLAN-Treiber zu verhungern, ist kein Handel.
//
// 0 = Eingabepuffer im PSRAM (Standard, stabil).
// 1 = Versuch im internen SRAM. Greift nur, wenn nach der Belegung noch
//     VIDEO_SRAM_RESERVE_KB frei bleiben - sonst wird ohne Drama auf PSRAM
//     zurueckgeschaltet. Erst sinnvoll, wenn intern deutlich mehr frei ist
//     (z. B. nachdem der LVGL-Zeichenpuffer wieder ins PSRAM gewandert ist).
// ------------------------------------------------------------------------------
#define VIDEO_JPEG_IN_SRAM 0

// So viel internes SRAM muss nach der Belegung uebrig bleiben. Der Wert ist
// aus dem Absturz abgeleitet: bei 105 KB Rest ist der SDIO-Treiber gestorben,
// bei 297 KB lief das System tagelang. 220 KB liegen mit Sicherheitsabstand
// oberhalb der Grenze.
#define VIDEO_SRAM_RESERVE_KB 220

// ==============================================================================
// NICHT per Schalter: Panel-Patch
//
// In .pio/libdeps/esp32p4_m5stack/M5GFX/src/lgfx/v1/platforms/esp32p4/
// Panel_DSI.cpp steht Zeile 86 auf `use_dma2d = false`, damit das Display nicht
// ueber DMA2D flusht. Das Original liegt als Panel_DSI.cpp.orig daneben.
//
// Der Patch geht verloren, sobald die Bibliotheken neu geholt werden
// (.pio/libdeps ist ein Build-Artefakt).
// Tritt Displayflackern auf: Panel_DSI.cpp.orig zurueckkopieren.
// ==============================================================================
