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
// AUDIO_PROFILE - Messzeile [AudioProf] alle 5 Sekunden, solange das
// Babyphone laeuft
//
// Zeigt, wo der Ton verloren geht: Kommen UDP-Pakete der Kamera an (Pakete,
// KB/s)? Fuellt sich der Jitterpuffer? Werden Haeppchen an den Lautsprecher
// gegeben, oder laeuft die Wiedergabe leer? Mit welcher Lautstaerke?
//
// Erste Pruefung bei "kein Babyphone-Ton". Liegen Pakete und Haeppchen im
// Soll (~86 KB/s, ~86 Haeppchen/s), die Lautstaerke > 0 und der Pegel
// ("Spitze") > 0, liegt das Problem hinter M5.Speaker - z.B. nach einem
// Absturz mit Warmstart (Audio-Chips nicht stromlos, siehe CrashDiag): dann
// hilft nur Stromlos-Machen.
//
// Fuer den Dauerbetrieb AUS - jede Zeile ist eine Ausgabe ueber USB-CDC
// (siehe VIDEO_PROFILE).
// ------------------------------------------------------------------------------
#define AUDIO_PROFILE 0

// ------------------------------------------------------------------------------
// HEAP_PROFILE - [Heap]-Zeile alle 10 s statt alle 60 s, mit Zusammenhang
//
// Die Zeile nennt Bildschirm, Babyphone-Modus und Ziel des Videos, z.B.
//   [Heap] HA | Video+Audio | Overlay: intern frei ...
// So laesst sich ablesen, welcher Betriebszustand wie viel internes RAM
// braucht (Messreihe Overlay, 14.10.2026).
//
// Fuer den Dauerbetrieb AUS: dann alle 60 s, ebenfalls mit Zusammenhang.
// ------------------------------------------------------------------------------
#define HEAP_PROFILE 0

// ------------------------------------------------------------------------------
// MAP_PROFILE - Messlauf des Kartenkachel-Dienstes (Schritt 0 Karten-Kachel)
//
// 60 s nachdem WLAN und Uhrzeit stehen (Zeit, um HA-Ansicht, Video-Overlay
// und Ton fuer die Messung unter Last einzuschalten), fordert der Dienst die Kacheln
// eines Ausschnitts von 1,5 x 1,5 km an (z15 und z16, ~30 Kacheln) - bewusst
// an einem oeffentlichen Ort (Brandenburger Tor), nicht am Wohnort.
// Ausgaben:
//   [Map]     je Kachel: Quelle (Netz/Flash), Groesse, Zeit; neue Verbindung
//             mit [Heap]-Zeile direkt nach dem TLS-Handshake
//   [MapProf] Start (mit Projektions-Pruefwert) und Bericht am Ende
// Erwartung beim ersten Lauf: alle Kacheln aus dem Netz, eine Verbindung.
// Nach einem Neustart: alle aus dem Flash, keine Verbindung.
//
// Fuer den Dauerbetrieb AUS (0): Ausgaben ueber USB-CDC, siehe VIDEO_PROFILE.
// ------------------------------------------------------------------------------
#define MAP_PROFILE 0

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
// VIDEO_PPA_SAFE_FIT - PPA-Auftraege ohne winzige Rest-Kachel (media/PpaGeometry.h)
//
// Befund 15.11.2026: Im Vollbild blieb ein Skalier-Auftrag an den PPA haengen
// (Bild steht, Touch tot, Ton laeuft) - bei 800x450, nie bei 1280x720. Der
// Verdacht: eine sehr kleine letzte Kachel am Bildrand (bekannter Hardware-
// fehler, in IDF 5.5.3 umgangen; hier IDF 5.4.2). PpaGeometry kuerzt den
// Bildausschnitt deshalb bei Bedarf um wenige Zeilen (450 -> 448).
//
// 1 = Absicherung aktiv (Standard).
// 0 = Geometrie wie vor der Absicherung - nur, um den Haenger gezielt
//     nachzustellen (der HangGuard faengt ihn ab und nennt die Stelle).
// ------------------------------------------------------------------------------
#define VIDEO_PPA_SAFE_FIT 1

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

// ------------------------------------------------------------------------------
// LVGL_FAST_FLUSH - LVGL-Bilder direkt und gedreht in den Bildspeicher kopieren
//
// Messung 16.11.2026 (Scrollen in den Einstellungen): Ein Bild ueber zwei
// Drittel der Flaeche dauert 110-220 ms, davon 75-115 ms nur fuer das
// Uebertragen ins hochkant liegende Panel ueber M5.Display.pushImage. PanelBlit
// (system/PanelBlit.h) kopiert stattdessen direkt - gleicher Speicher, weiter
// mit der CPU, kein PPA und kein 2D-DMA; der Panel-Patch unten bleibt gueltig.
//
// 1 = schneller Weg (Standard). Ein Selbsttest gegen M5GFX entscheidet beim
//     ersten Bild, ob er benutzt wird; Logzeile "[LVGL] Schneller Flush ...".
// 0 = wie bisher ueber M5.Display.pushImage - zum Vergleich, oder falls doch
//     Bildfehler auftreten.
// ------------------------------------------------------------------------------
#define LVGL_FAST_FLUSH 1

// ------------------------------------------------------------------------------
// PERF_PROFILE - Messzeilen [Perf] fuer grosse Bildaufbauten
//
// Nach jedem Neuzeichnen von mindestens der halben Bildschirmflaeche (Wechsel
// des Bildschirms, Vollbildkarte) eine Zeile: Gesamtzeit und davon die Zeit
// fuer das Uebertragen/Drehen ins Panel (disp_flush). Daraus folgt, ob sich
// eine Hardware-Drehung (PPA) lohnen wuerde - siehe Panel-Patch unten.
// Kleine Aenderungen (Uhr, Zustaende) erzeugen keine Zeile.
//
// Fuer den Dauerbetrieb AUS (USB-Ausgaben, siehe VIDEO_PROFILE). Messrunde
// Startzeit 2026-11 abgeschlossen. Die [Boot]- und [HA-Start]-Zeilen sind
// davon unabhaengig.
// ------------------------------------------------------------------------------
#define PERF_PROFILE 1

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

// ------------------------------------------------------------------------------
// HANG_GUARD - Waechter gegen ein eingefrorenes Geraet (system/HangGuard.h)
//
// Erkennt, wenn Oberflaeche, Hauptschleife oder Video-Decoder haengen, sichert
// einen Kurzbericht und startet das Geraet neu (Coredump + ColdRestart).
// Kein Testwerkzeug, sondern Schutz fuer den Dauerbetrieb (Babyphone).
//
// 1 = aktiv (Standard).
// 0 = aus, z.B. wenn beim Debuggen absichtlich lange angehalten wird.
// ------------------------------------------------------------------------------
#define HANG_GUARD 1

// ------------------------------------------------------------------------------
// DIAG_TEST_TOOLS - Testwerkzeuge auf der Seite Einstellungen > System > Diagnose
//
//  * "Audio-Chip zuruecksetzen": Codec-Reset ohne Stromlos-Machen (siehe
//    AudioCodecTab5.h), mit Registerauszug vorher/nachher auf der Konsole.
//  * "Absturz ausloesen": absichtlicher Absturz (CrashDiag_TriggerCrash), um
//    die Absturz-Behandlung reproduzierbar zu testen.
//  * "Mikrofonstart ablehnen": laesst den naechsten Start des Mikrofons
//    kuenstlich scheitern (I2sRxStartGuard), um Neuaufbau und Abbruch des
//    Gegensprechens zu testen.
//  * "Senderate Gegensprechen": rechnet die Sprachpakete auf eine andere
//    Abtastrate um (PcmResampler), um zu pruefen, welche die Kamera erwartet.
//  * "Paketgroesse Gegensprechen": 1024, 512 oder 256 Bytes je Sprachpaket.
//  * Mitschnitt des Gegensprechens (PttCapture): die ersten 10 s jeder
//    Sitzung, abholbar als WAV unter http://<Geraet>/ptt_capture, solange der
//    Bildschirmfoto-Modus den Webserver offen haelt. Dazu Registerzeilen im
//    Monitor ([AudioCodec] I2S-Empfang ...).
//
// Nur zur Fehlersuche. Fuer den Dauerbetrieb auf 0 - dann fehlt der Abschnitt.
// ------------------------------------------------------------------------------
#define DIAG_TEST_TOOLS 1
