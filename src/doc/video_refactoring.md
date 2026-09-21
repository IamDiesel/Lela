# Video-Refactoring: Fehleranalyse und Umbau

**Stand:** 2026-09-20 · **Betrifft:** `src/media/`, `src/gui/baby/`, Teile von `src/system/`

Dieses Dokument begleitet den Umbau der Babycam-/Video-App. Es ist als lebendes
Arbeitsdokument gedacht: Befunde bekommen eine feste ID, behalten sie ueber den
gesamten Umbau hinweg und werden hier abgehakt, nicht geloescht. Erledigte
Punkte wandern mit Datum und Kurzbegruendung in Abschnitt 7.

---

## 1. Statusuebersicht

Schwere: **A** = Absturz/Datenverlust · **B** = deutlich spuerbar · **C** = Sauberkeit

| ID | Thema | Schwere | Status |
| --- | --- | --- | --- |
| V-01 | Interrupt-WDT: Endlosschleife im 2D-DMA-Treiber | A | Ursache belegt, Massnahmen offen |
| V-02 | Vollbild-Push ohne `lvgl_port_lock()` auf gemeinsamem Framebuffer | A | offen (laut 2.7 nicht der Ausloeser von V-01, bleibt aber ein Fehler) |
| V-03 | `VideoLogic_Stop()` ohne Join: Stream bleibt nach Pause/Play tot | B | offen |
| V-04 | Triple-Buffer degeneriert bei Lock-Timeout, toter Schutzvergleich | B | offen |
| V-05 | `camStatus` / `camResolutions[]`: String-Race zwischen Tasks | A | offen |
| V-06 | Kamera-Befehle blockieren den UI-Thread (HTTP, 1500 ms) | B | offen |
| V-07 | PTT blockiert die UI bis ca. 3 s | B | offen |
| V-08 | `delay(50)` statt Handshake beim I2S-Umschalten | A | offen (laut 2.7 nicht der Ausloeser von V-01, bleibt aber ein Fehler) |
| V-09 | `camDropFrames` wird destruktiv dekrementiert | B | offen |
| V-10 | `VideoOverlay`: Guard `pic_w < 150` statt `< 160`, weitere Annahmen | C | offen |
| V-11 | `reset_jpeg_engine()` im Decode-Fehlerpfad | A | **umgesetzt** 2026-09-20 |
| V-12 | Keine Validierung von `pic_info` gegen `MAX_PIXEL_BUF_SIZE` | A | **umgesetzt** 2026-09-20 |
| V-13 | JPEG-Endmarker wird rueckwaerts gesucht (letztes `FFD9`) | B | offen |
| V-14 | Gelieferte Aufloesung wird nie zurueckgelesen | B | offen |
| V-15 | Core 1 ueberbucht, LVGL laeuft auf Prioritaet 1 | B | offen |
| V-16 | Jeder Fehlerpfad erzwingt vollen Re-Login (ca. 5,5 s) | B | offen |
| V-17 | `processStream<true>`: Template-Parameter ungenutzt | C | offen |
| V-18 | `/api/v1/client/offline` wird nie aufgerufen | B | offen |
| V-19 | Login-Logik dupliziert, Magic Constants mehrfach | C | offen |
| V-20 | `ViewBaby_SetStatus()` erzeugt pro Aufruf einen String | C | offen |
| V-21 | `StaticJsonDocument` / `containsKey()` in ArduinoJson 7 veraltet | C | offen |
| V-22 | `new lgfx::PPASrm(...)` ohne Null-Pruefung | C | offen |
| V-23 | LVGL-Zeichenpuffer `buf1` ohne Null-Pruefung | C | offen |
| V-24 | `WebSetupLogic_Update()` wurde doppelt pro Durchlauf gerufen | C | **umgesetzt** 2026-09-20 |

---

## 2. V-01 — Grundursache der sporadischen Abstuerze

### 2.1 Symptom

Guru Meditation, `Interrupt wdt timeout on CPU1`, anschliessend Neustart. Tritt
nach Sekunden bis Minuten Videobetrieb auf, an manchen Tagen gar nicht. Nach dem
ersten Auftreten haeufen sich die Abstuerze.

### 2.2 Beweislage

Drei Dumps (zwei im Vollbild, einer im Fenstermodus) zeigen **byteweise
identische** Register:

    MEPC 0x4015b270   RA 0x40174b72
    A0   0x481c3014   A1 0x200   S0/FP 0x200   A5 0x50088200
    S2   0x481c30ac   S3 0x481c302c   S4 0x481c30bc   S7 0x481c3010

Symbolisiert (`llvm-addr2line` gegen `firmware.elf`, ELF-SHA256 `5654`):

| Adresse | Symbol |
| --- | --- |
| `0x4015b270` | `dma2d_hal_tx_reset_channel` — `hal/dma2d_hal.c:18` |
| `0x40174b72` | `dma2d_connect` — `esp_hw_support/dma/dma2d.c:575` |

Disassembly der Absturzstelle:

    4015b26a:  lw    a4, 0x24(a5)      ; DMA2D-Kanalregister lesen (a5 = 0x50088200)
    4015b26c:  slli  a3, a4, 0x7       ; Statusbit isolieren
    4015b270:  bgez  a3, 0x4015b26a    ; Bit noch 0? -> zurueck

Eine **Polling-Schleife ohne Timeout**, die auf die Reset-Bestaetigung eines
2D-DMA-Kanals wartet. Der Aufruf erfolgt aus einem `transaction_on_picked`-
Callback, also mit gesperrten Interrupts — daher der Interrupt-Watchdog statt
des Task-Watchdogs.

### 2.3 Aufrufpfade

Fenstermodus:

    videoTask                             VideoLogic.cpp:356
    +- processStream                      VideoLogic.cpp:239  (jpeg_decoder_process)
       +- jpeg_decoder_process            jpeg_decode.c:248
          +- dma2d_enqueue                dma2d.c:904
             +- jpeg_dec_transaction_on_picked   jpeg_decode.c:515   <- Interrupts aus
                +- dma2d_connect          dma2d.c:575
                   +- dma2d_hal_tx_reset_channel      <- Endlosschleife

Vollbild — gleiche Endstelle, anderer Aufrufer:

    +- draw_and_push_fullscreen           VideoLogic.cpp:254
       +- PPASrm::pushImageSRM            LGFX_PPA.cpp:402
          +- PPABase::exec                LGFX_PPA.hpp:241
             +- ppa_do_scale_rotate_mirror      ppa_srm.c:262
                +- ppa_dma2d_enqueue     ppa_core.c:338
                   +- ppa_srm_transaction_on_picked -> dma2d_connect -> dieselbe Schleife

**Gemeinsamer Nenner:** Auf dem ESP32-P4 haengen der Hardware-JPEG-Decoder und
die PPA am selben 2D-DMA-Controller. Weder der Vollbildmodus noch die PPA sind
die Ursache — beide sind nur zwei Wege in denselben Engpass.

### 2.4 Mechanismus

Ein 2D-DMA-Kanal wird uebernommen, waehrend er aus einem vorherigen Transfer
noch verklemmt ist. Der Reset wird dann nie bestaetigt.

Wahrscheinlichster Ausloeser im eigenen Code ist **V-11**: bei jedem
fehlgeschlagenen Decode wird die JPEG-Engine komplett abgerissen und neu
angelegt, inklusive Ab- und Neuanmeldung beim DMA2D-Treiber. Laeuft dabei noch
ein Transfer oder ist einer in den 1000-ms-Timeout gelaufen, bleibt der Kanal
schmutzig zurueck.

Das erklaert die Tagesabhaengigkeit: schlechtes WLAN -> mehr korrupte Frames ->
mehr Decode-Fehler -> mehr Engine-Resets -> hoehere Trefferwahrscheinlichkeit.
Beguenstigt wird das durch **V-12** und **V-13**, die ungeprueft moeglicherweise
unvollstaendige JPEG-Daten an die Hardware geben.

### 2.5 Geplante Massnahmen

1. `reset_jpeg_engine()` aus dem Decode-Fehlerpfad entfernen; fehlerhafte Frames
   werden uebersprungen. Falls ein Reset noetig bleibt: rate-limitiert und erst
   nach einer Pause ohne laufenden Transfer. (V-11)
2. Vor dem Decode validieren: plausible Dimensionen und
   `pic_info.width * pic_info.height * 2 <= MAX_PIXEL_BUF_SIZE`. (V-12)
3. `actual_jpeg_size` gegen die `Content-Length` gegenpruefen statt rueckwaerts
   zu raten. (V-13)
4. JPEG-Decode und PPA-Push entzerren, statt beide im Vollbild unmittelbar
   hintereinander auf denselben DMA2D-Controller zu werfen.

Die Schleife selbst ist ein ESP-IDF-Problem (Hardware-Poll ohne Timeout in einem
kritischen Abschnitt). Aus eigenem Code ist sie nicht mehr verlassbar, sobald
sie betreten wurde — es bleibt nur, sie gar nicht erst zu betreten. Ein
IDF-Update koennte helfen, waere angesichts der hart gepinnten Bibliotheks-
versionen aber ein eigenes Risiko und wird bewusst zurueckgestellt.

### 2.6 Umsetzung V-11 und V-12 (2026-09-20)

Beide Aenderungen sitzen in `processStream()` in `src/media/VideoLogic.cpp`.

**V-12 - Bildmasse pruefen, bevor die Hardware sie bekommt.** Direkt nach
`jpeg_decoder_get_info()` wird geprueft, ob das Bild ueberhaupt in den
Zielpuffer passt:

    const size_t requiredBytes = (size_t)pic_info.width * (size_t)pic_info.height * 2u;
    if (pic_info.width == 0 || pic_info.height == 0 || requiredBytes > MAX_PIXEL_BUF_SIZE) {
        // gedrosselte Warnung, Frame ueberspringen
        continue;
    }

Notwendig, weil die Kamera eine angeforderte Aufloesung ablehnen darf
(`forceChangeResolution: false`, siehe V-14) und die gelieferten Masse damit
nicht vorhersagbar sind.

**V-11 - kein Engine-Neuaufbau mehr im Fehlerpfad.** Der Aufruf
`reset_jpeg_engine()` ist aus dem `else`-Zweig von `jpeg_decoder_process()`
entfernt. Er existiert nur noch einmalig beim Start von `videoTask()`.
Stattdessen:

* Ein einzelner Decode-Fehler wird gezaehlt und der Frame uebersprungen.
* Die Warnung ist auf hoechstens eine Ausgabe pro 2 s gedrosselt.
* Nach fuenf Fehlern **in Folge** wird die HTTP-Verbindung neu aufgebaut
  (`return` aus `processStream()`). Das behandelt den wahrscheinlicheren Fall -
  kaputter Stream statt kaputter Engine - ohne die Hardware anzufassen.
* Ein erfolgreicher Decode setzt den Zaehler zurueck.

**Erwartete Wirkung.** Der Pfad, der einen 2D-DMA-Kanal verklemmt zuruecklassen
konnte, wird im laufenden Betrieb nicht mehr betreten. Bleibt V-01 danach aus,
ist die Kausalkette aus Abschnitt 2.4 bestaetigt.

**Messung.** Vergleichsbasis ist das Feld `prev_ms` aus dem
MQTT-Bericht (Abschnitt 6.1) vor und nach dieser Aenderung. Die neuen Logzeilen
`Decode-Fehler ...` und `Frame verworfen: ...` zeigen ausserdem, wie oft die
Schutzpfade ueberhaupt greifen - bleiben sie stumm, war der Decoder-Pfad nicht
das Problem.

---

### 2.7 Coredump-Auswertung (2026-09-20)

Die Partitionstabelle enthaelt eine **64-KB-Coredump-Partition** (`0xFF0000`),
und sie wird beschrieben. Ein Upload fasst sie nicht an, der Inhalt ueberlebt
also das Flashen. Auslesen:

    $py  = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
    $env:PYTHONPATH = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy"
    & $py -m esptool --chip esp32p4 --port COM13 read_flash 0xFF0000 0x10000 coredump.bin

Format: 24 Byte Header (`data_len`, `version = 0x00120102`), danach ein
RISC-V-CORE-ELF. Der enthaelt Register und Stack **aller** Tasks.

**Befund des ausgelesenen Dumps: 25 Tasks.**

Der `VideoTask` stand im PPA-Pfad; der Stack ist eindeutig:

    vPortTaskWrapper
    +- processStream                    VideoLogic.cpp:220
       +- VideoOverlay::applyOverlay
       +- PPASrm::pushImageSRM
          +- PPABase::exec              LGFX_PPA.hpp:241
             +- ppa_do_scale_rotate_mirror    ppa_srm.c:173
                +- ppa_srm_transaction_on_picked  ppa_srm.c:95
                   +- HAL-Ebene: Warteschleife auf ein Hardware-Bit

Die genaue Instruktion ist symbolisch mehrdeutig (die `*_hal.c` / `*_ll.h`
Inline-Funktionen liegen dicht beieinander, die Symbolgrenzen sind nach dem
Inlining nicht zuverlaessig). Der Stack ist es nicht.

**Alle 24 uebrigen Tasks waren blockiert.** Das ist der eigentliche Gewinn
dieses Dumps, denn es widerlegt mehrere Vermutungen auf einen Schlag:

| Task | Zustand im Moment des Absturzes | Folgerung |
| --- | --- | --- |
| `loopTask` (LVGL) | `vPortYield` | **V-02 war nicht beteiligt** - kein gleichzeitiger Framebuffer-Zugriff |
| `Aud_Cons` | `xQueueSemaphoreTake` | **V-08 war nicht beteiligt** - kein I2S-Zugriff offen |
| `sdio_read`, `sdio_rx_buf`, `sdio_process_rx`, `sdio_write` | alle `xQueueSemaphoreTake` | **keine WLAN-/SDIO-Buslast** |
| `Aud_Prod`, `ApiTask`, `AudioTask_UI`, `esp_timer`, `Tmr Svc`, ... | blockiert | unbeteiligt |

> **Korrektur vom 2026-09-20 (siehe 2.10):** Dieser Schluss war falsch. Die
> DMA2D-Nutzung des Displays haengt an keinem FreeRTOS-Task und taucht in
> keiner Taskliste auf. Ein spaeterer Coredump hat die Kollision direkt
> belegt. Der folgende Absatz bleibt zur Nachvollziehbarkeit stehen.

Der Absturz ist damit **kein Nebenlaeufigkeitsproblem**. Zum Zeitpunkt des
Haengers war ausschliesslich der `VideoTask` aktiv und wartete auf eine
Hardware-Quittung, die nicht kam. Die frueher vermutete Konkurrenz um PSRAM-
und Display-Bandbreite scheidet als Ausloeser aus.

Das staerkt V-11 als richtige Massnahme: ein Kanal, den eine **vorherige**
Operation verklemmt zurueckgelassen hat, passt zu diesem Bild - gleichzeitige
Zugriffe passen nicht.

**Fuer kuenftige Abstuerze:** Die Partition haelt nur den jeweils letzten Dump.
Nach einem Absturz also zeitnah auslesen, bevor der naechste ihn ueberschreibt.

---

### 2.8 V-01 ist ein bekannter ESP-IDF-Bug (2026-09-20)

Zwei offene Meldungen in Espressifs Issue-Tracker beschreiben exakt unsere
Absturzstelle.

**esp-idf Issue #19108** - `dma2d_hal_tx_reset_channel()` pollt ein
`reset_avail`-Bit, das fuer Kanaele mit `peri_sel == 7` (kein Peripheriegeraet
verbunden) **niemals** gesetzt wird. Der Zustand rastet dauerhaft ein, es ist
keine Race Condition. Der Reset laeuft im Kanal-Spinlock mit gesperrten
Interrupts gegen den 300-ms-Watchdog - deckt sich Wort fuer Wort mit unserem
Befund aus Abschnitt 2.2.

**esp-idf Issue #18999** - `dma2d_connect()` haengt unter anhaltender PPA-Last.
Die dortige Arbeitslast ist unsere: **JPEG-Decode -> PPA-Skalierung ->
DPI-Panel-Draw**, also dieselben drei DMA2D-Clients. Absturz nach ca. 1000
Bildern bei 14 fps.

Zwei Details mit unmittelbarer Konsequenz:

* **Silizium-revisionsabhaengig.** ESP32-P4 rev v1.3 ist betroffen, rev v1.0
  nicht. Die eigene Revision meldet esptool beim Verbinden.
* **Die IDF-Version wirkt stark.** v6.0 stuerzt nach ~15 Bildern ab,
  v6.1-RC1 nach ~1000, v6.2-master nach ~8000. Der JPEG-Pfad wurde in v6.1
  teilweise repariert, **der PPA-Pfad nicht**.

Gescheiterte Workarounds anderer: Mutex-Serialisierung, abgeschaltete Overlays,
IRAM-safe-Konfiguration, veraenderte PPA-Blockgroessen. Wirksam war nur, die
Warteschleife selbst zu begrenzen (Issue #19108): aus dem Haenger wird ein
geloggtes Ereignis, die Bildrate bleibt erhalten.

**Folgerungen fuer dieses Projekt**

1. Den PPA zu entfernen bleibt der staerkste selbst kontrollierbare Hebel -
   gerade weil der PPA-Pfad auch in neueren IDF-Versionen kaputt geblieben ist.
2. Eine begrenzte Warteschleife ist ein realistischer Plan B.
   `dma2d_hal_tx_reset_channel` ist ein globales Symbol; per
   `-Wl,--wrap=dma2d_hal_tx_reset_channel` liesse sich eine eigene, begrenzte
   Fassung unterschieben, ohne die gepinnten Bibliotheken anzufassen.
3. Ein IDF-Sprung wuerde die Haeufigkeit stark senken, kollidiert aber mit der
   Versionspinnung, die wegen des Displayflackerns existiert.

**Hebel 1, bereits umgesetzt:** `M5.Display.startWrite()/endWrite()` um den
PPA-Aufruf in `draw_and_push_fullscreen()` sind entfernt. Der PPA schreibt per
DMA direkt in den Framebuffer und braucht die Schreibtransaktion nicht; M5GFX
benutzt fuer seine eigenen Uebertragungen aber ebenfalls DMA2D
(`Panel_DSI.cpp:86`, `use_dma2d = true`). Die Klammer machte das Display damit
zu einem zusaetzlichen DMA2D-Nutzer in derselben Schleifeniteration.
Erwartung gedaempft - vergleichbare Eingriffe haben in Issue #18999 nicht
gereicht.

### 2.9 Frame-Zeitmessung (2026-09-20)

Espressif gibt fuer den Hardware-JPEG-Decoder bei 360 MHz **109 fps bei
1280x720** an, also rund 9 ms pro Bild. Gemessen werden real ~7 fps, also
143 ms pro Bild - **93 % der Zeit liegen woanders**. Drosselung scheidet aus:
`camSpeed = 0` (maximale Rate), `camDropFrames = 0` (kein Verwerfen).

`processStream()` misst deshalb jetzt vier Abschnitte je Bild und gibt alle
5 Sekunden Mittelwerte aus:

    [VideoProf] 800x450 | 34 Bilder in 5003 ms = 6.8 fps | je Bild:
    Header 2.1 ms, Download 118.4 ms, Decode 4.2 ms, Anzeige 11.3 ms
    (Summe 136.0 ms) | 96 KB/Bild = 5.2 Mbit/s

Damit laesst sich unterscheiden:

| Beobachtung | Schlussfolgerung |
| --- | --- |
| Download dominiert | Netzwerk oder Kamera sind die Grenze; die sequenzielle Schleife verschenkt zusaetzlich Zeit |
| Anzeige dominiert | PPA bzw. LVGL-Pfad ist der Engpass |
| Summe << 1000/fps | Die Zeit geht ausserhalb der gemessenen Abschnitte verloren (Wartezeiten, Scheduling) |

**Strukturelle Vermutung.** Die Schleife ist streng sequenziell: ein Bild wird
komplett geladen, dann dekodiert, dann angezeigt, erst dann beginnt das
naechste. Waehrend des Downloads steht der Decoder still, waehrend der
Verarbeitung wird der Socket nicht gelesen. Ueberlappung von Netz und
Verarbeitung ist ein Entwurfsziel des Rewrites.

---

### 2.10 Kollision belegt, Hebel 2 (2026-09-20)

Ein Absturz im **Fenstermodus** - also ganz ohne PPA - lieferte den Beweis.

Der `VideoTask` stand in:

    dma2d_hal_tx_reset_channel        dma2d_hal.c:18
    <- dma2d_connect
    <- jpeg_dec_transaction_on_picked jpeg_decode.c:542
    <- acquire_free_channels_for_trans dma2d.c:147
    <- dma2d_enqueue                  dma2d.c:904
    <- jpeg_decoder_process           jpeg_decode.c:248
    <- processStream                  VideoLogic.cpp:293

Der `loopTask` **im selben Moment** in:

    lv_draw_sw_img_decoded  <- lv_draw_sw_blend <- map_normal <- lv_memcpy
    _lv_img_cache_open      <- decode_and_draw

LVGL hat also gerade das Kamerabild gerendert - und unmittelbar danach folgt
`disp_flush()` -> `M5.Display.pushImage()` -> `esp_lcd_panel_draw_bitmap()` ->
**DMA2D**. Zwei Clients, zeitgleich, waehrend der eine einen Kanal anfordert.

**Zwei Folgerungen:**

1. Den PPA zu entfernen haette **nicht** gereicht. Zwei Clients genuegen, und im
   Fenstermodus sind das JPEG-Decoder und Display.
2. Anders als in den Upstream-Issues kontrollieren wir in diesem Fall **beide**
   Nutzer. Das eroeffnet eine Moeglichkeit, die dort niemand probiert hat.

**Hebel 2 - Decode unter dem LVGL-Lock.** `jpeg_decoder_process()` lief bisher
ausserhalb des Locks. Jetzt wird er darin ausgefuehrt: solange der Lock gehalten
wird, kann LVGL nicht flushen, die beiden DMA2D-Nutzer sind serialisiert.
Kosten: ~4,5 ms Haltezeit pro Bild - bei 167 ms Downloadzeit je Bild
vernachlaessigbar. Ein Zaehler `LockMiss` in der `[VideoProf]`-Zeile zeigt, ob
der Lock je nicht rechtzeitig zu bekommen war.

Wirkt Hebel 2, ist das zugleich die Bestaetigung der These - und ein Weg, der
ohne IDF-Patch und ohne Verzicht auf den Hardware-Decoder auskommt.

### 2.11 Messergebnis: der Engpass ist das Netz

Erste Messung im Fenstermodus:

    28 Bilder in 5079 ms = 5.5 fps | Header 0.4 ms | Download 167.3 ms |
    Decode 4.5 ms | Anzeige 7.4 ms | Summe 179.6 ms | 80 KB/Bild = 3.6 Mbit/s

Bildabstand real 181,4 ms, gemessene Summe 179,6 ms - **99 %**, es geht keine
Zeit an ungemessener Stelle verloren.

**Der Download frisst 93 % der Zeit.** Der Decoder liegt mit 4,5 ms exakt auf
Espressifs Spezifikation, die Anzeige kostet 7,4 ms. Offen bleibt, ob die
Kamera langsam sendet oder unsere Leseseite bremst - beides sieht in dieser
Messung gleich aus. Gegenprobe: denselben Stream vom PC ziehen.

**Wichtige Nebenfolge:** Wir sind netzwerkgebunden, nicht rechengebunden. Pro
Bild liegt die CPU 167 ms untaetig. Ein Software-JPEG-Decode (geschaetzt
40-80 ms fuer 800x450) passt vollstaendig in dieses Fenster, ohne die Bildrate
anzutasten - und wuerde den JPEG-Decoder als DMA2D-Client entfernen. Was vorher
als zu teuer galt, ist damit bezahlbar geworden.

---

### 2.12 Chip-Revision, Vollbild-Absturz und Hebel 3 (2026-09-20)

**Chip-Revision bestaetigt.** Der Coredump-Header fuehrt `chip_rev` als sechstes
Feld: `0x67 = 103` -> **ESP32-P4 rev v1.3**. Das ist genau die Revision, die
esp-idf #18999 als betroffen nennt (v1.0 ist es nicht). Auslesen ohne esptool:

    struct.unpack_from("<6I", coredump_bin, 0)[5]

**Absturz im Vollbild.** `VideoTask` in `ppa_do_scale_rotate_mirror` ->
`ppa_srm_transaction_on_picked` -> `dma2d_connect`, `loopTask` dabei untaetig in
`vPortYield`. Die beiden DMA2D-Nutzer waren also **JPEG-Decoder und PPA,
nacheinander im selben Task** - gar keine Gleichzeitigkeit, und es verklemmt
trotzdem.

Zwei Folgerungen:

* **Hebel 2 kann im Vollbild nicht wirken.** Er serialisiert gegen LVGL, und
  LVGL ist dort gar nicht aktiv. Fuer den Fenstermodus bleibt er relevant.
* Es erklaert, warum Mutex-Serialisierung in #18999 gescheitert ist: wo nichts
  gleichzeitig laeuft, hilft Serialisieren nicht.

**Neue Leithypothese.** #19108 nennt als Bedingung Kanaele mit `peri_sel == 7`,
also ohne angeschlossenes Peripheriegeraet - der Fall einer reinen
Speicher-zu-Speicher-Kopie. Genau so arbeitet `esp_lcd_panel_draw_bitmap()` mit
`use_dma2d = true`. Nicht der PPA vergiftet die Kanaele, sondern womoeglich das
**Display**: es hinterlaesst `peri_sel == 7`-Kanaele, und wer sie danach
anfordert - JPEG oder PPA - haengt im Reset.

**Hebel 3 (Experiment).** In
`.pio/libdeps/esp32p4_m5stack/M5GFX/src/lgfx/v1/platforms/esp32p4/Panel_DSI.cpp`
Zeile 86 auf `use_dma2d = false` gesetzt. Sicherung liegt als
`Panel_DSI.cpp.orig` daneben.

* **Achtung:** Das Verzeichnis `.pio/libdeps` ist ein Build-Artefakt. Der Patch
  geht verloren, sobald die Bibliotheken neu geholt werden. Bewaehrt er sich,
  gehoert er als `extra_scripts`-Patch in die `platformio.ini` oder in einen
  eigenen M5GFX-Fork.
* **Risiko:** Die Versionspinnung dieses Projekts existiert wegen
  Displayflackerns. Genau dieser Schalter koennte es zurueckbringen. Tritt
  Flackern auf, `Panel_DSI.cpp.orig` zurueckkopieren.

### 2.13 Netzmessung gegen den PC (2026-09-20)

Gegenprobe zu 2.11, derselbe Stream vom Laptop gezogen:

    curl --max-time 15 "http://<cam>:8080/api-stream/v1/video?..."
    11.878.400 Bytes in 15,0 s = 791.435 B/s

| | Durchsatz | bei 80 KB/Bild |
| --- | --- | --- |
| PC (curl) | 791 KB/s | ~9,9 fps moeglich |
| Tablet | 479 KB/s | 5,5 fps erreicht |

**Die Kamera ist die Hauptgrenze**, nicht unsere Leseseite - aber wir holen nur
60 % dessen ab, was sie hergibt. Zwei Hebel fuer die Bildrate:

1. **Ueberlappung.** Die sequenzielle Schleife laesst den Socket waehrend
   Decode und Anzeige ungelesen. Pipelining hebt uns Richtung 9-10 fps.
2. **Weniger Bytes je Bild.** Bei ~790 KB/s Deckel kauft halbe Bildgroesse
   (Qualitaet 50 -> 30) etwa doppelt so viele Bilder.

Mehr als ~10 fps sind mit dieser Kamera bei dieser Qualitaet nicht zu holen.

---

### 2.14 Neustart-Ausloeser, V-03 und Hebel 4 (2026-09-20)

**Ein Absturz mit bekanntem Ausloeser.** Gemeldeter Ablauf: Video gestartet,
versehentlich gestoppt, beim erneuten Starten abgestuerzt. Das ist kein
Zufallstreffer, sondern V-03 - und es erklaert eine ganze Klasse der bisher
statistisch wirkenden Abstuerze.

Beim Stoppen lief der `videoTask` noch bis zu drei Sekunden weiter (er haengt in
der Netzwerk-Leseschleife). Beim Neustart rief der neue Task
`reset_jpeg_engine()` auf: `jpeg_del_decoder_engine()` plus Neuanlage, also Ab-
und Neuanmeldung des DMA2D-Clients - genau der Vorgang, der einen Kanal
schmutzig zuruecklaesst.

**Zwei Korrekturen:**

1. **Engine-Lebensdauer.** Die JPEG-Engine wird jetzt **einmal** in
   `VideoLogic_Init()` angelegt und ueber die gesamte Laufzeit behalten. Weder
   Taskstart noch Taskende fassen sie an.
2. **Start ist pegel- statt flankengesteuert.** `VideoLogic_Start()` ist
   idempotent (legt nur an, wenn kein Task laeuft) und wird in
   `SystemLogic_Update()` in jedem Durchlauf aufgerufen. Damit holt sich der
   Start automatisch nach, sobald ein auslaufender Vorgaenger weg ist. Vorher
   feuerte die Flanke einmal ins Leere und der Stream blieb dauerhaft tot.

**Hebel 3 wirkt - aber nicht im Vollbild.** Messbar: `Anzeige` fiel von 7,4 ms
auf **0,4 ms**, der LVGL-Lock ist sofort frei. Ein Absturz im Vollbild trat
danach trotzdem auf, was konsistent ist: dort bleiben nach Hebel 3 der
JPEG-Decoder und der PPA als DMA2D-Nutzer uebrig, beide von uns.

**Hebel 4 - Vollbild ohne PPA.** Neuer Schalter `VIDEO_USE_PPA` in
`VideoLogic.cpp`, per Vorgabe auf `0`:

* **0** - Das Bild wird in Originalgroesse mittig per `M5.Display.pushImage()`
  ausgegeben (Letterbox, 800x450 auf schwarzem Grund). Zusammen mit Hebel 3 ist
  dieser Weg **vollstaendig DMA2D-frei**. Der JPEG-Decoder bleibt der einzige
  DMA2D-Nutzer im System, ein Kanalwechsel zwischen verschiedenen
  Peripheriegeraeten kann nicht mehr auftreten.
* **1** - Bisheriges Verhalten mit PPA-Skalierung, zum Vergleich erhalten.

Die Byte-Reihenfolge des Decoders ist bei `VIDEO_USE_PPA 0` durchgaengig BGR,
weil Fenster- und Vollbildmodus dann beide ueber M5GFX laufen.

**Erwartung.** Bleibt es damit stabil, ist die Leithypothese aus 2.12
bestaetigt: nicht ein einzelner Client ist schuld, sondern der **Wechsel
zwischen zwei verschiedenen DMA2D-Clients**. Stuerzt es weiter ab, obwohl nur
noch der JPEG-Decoder DMA2D benutzt, bleibt als Ausweg der Software-Decoder
(2.11) oder die gekappte Warteschleife per `-Wl,--wrap=` (2.8).

---

### 2.15 Selbsttest: Netz oder Grafik? (2026-09-20)

Nach vier Hebeln ohne durchschlagenden Erfolg steht die Richtungsfrage im Raum:
Ist die Ursache wirklich die Grafik, oder haengt sie doch am SDIO-/WLAN-Pfad?

**Was fuer die Grafik spricht:** Ueber sieben Dumps hinweg steht der PC immer in
`dma2d_hal_tx_reset_channel`. Im Stack taucht kein WLAN- oder SDIO-Rahmen auf,
immer nur JPEG oder PPA. SDIO benutzt auf dem P4 seinen eigenen SDMMC-DMA, nicht
DMA2D.

**Was dagegen spricht:** esp-idf #19108 haelt ausdruecklich fest, der Ausloeser
korreliere mit *network/UI activity* statt mit der Grafiklast selbst. Ein
gemeinsamer Speicherbus fuer DMA2D, JPEG, PSRAM und SDIO macht Buskonkurrenz zu
einer plausiblen Erklaerung - dann waeren alle Grafikhebel Symptombehandlung.

**Der Test.** `#define VIDEO_SELFTEST 1` in `VideoLogic.cpp`:

1. Der erste gueltige Frame wird in `download_buf` gesichert, der Stream endet.
2. Das WLAN wird vollstaendig abgeschaltet (`WiFi.mode(WIFI_OFF)`).
3. Derselbe Frame wird endlos mit ~6 fps dekodiert und angezeigt - gleiche
   Decoder-Last, gleicher Anzeigepfad, gleiche Serialisierung wie im
   Echtbetrieb, **null Netzwerkverkehr**.
4. Alle 5 s: Durchlaeufe, Laufzeit, Decode-Fehler, freier Heap.

Ausgabe:

    [Selftest] Frame gesichert (81234 Bytes, 800x450) - Stream wird beendet.
    [Selftest] WLAN wird jetzt abgeschaltet - kein SDIO-Verkehr mehr.
    [Selftest] Bildmasse 800x450, Schleife laeuft mit ~6 fps.
    [Selftest] 180 Durchlaeufe | 30 s ohne Absturz | 0 Decode-Fehler | Heap 198 KB

**Auswertung**

| Ergebnis | Schlussfolgerung | Naechste Schritte |
| --- | --- | --- |
| Absturz nach Minuten | Netz unbeteiligt, Ursache in der Grafik | Hebel 3+4 weiterverfolgen, sonst Software-Decoder oder `--wrap` |
| Laeuft stundenlang durch | SDIO/WLAN ist beteiligt | esp-hosted-Version, SDIO-Puffer, internes RAM (nur 231 KB frei) |

Wichtig fuer die Bewertung: der Selbsttest deckt Decode und Anzeige ab, aber
**nicht** den Neustart-Pfad aus 2.14 und nicht das Multipart-Parsing. Ein
Durchlaufen beweist also nicht, dass die Grafik fehlerfrei ist - nur, dass sie
unter Dauerlast allein nicht ausreicht, um den Hanger auszuloesen.

Rueckbau: `#define VIDEO_SELFTEST 0`.

---

### 2.16 Paralleler Netzverkehr als Verdaechtiger (2026-09-20)

**Erfahrungswert aus dem Projekt:** Solange der HA-Websocket waehrend des
Videobetriebs mitlief, stuerzte das Video reproduzierbar staendig ab. Das deckt
sich mit esp-idf #19108, wo der Ausloeser mit *network/UI activity* korreliert
statt mit der Grafiklast.

**Inventar - wer funkt waehrend des Babyphone-Betriebs parallel zum Stream?**

| Nutzer | Aktivitaet | Takt |
| --- | --- | --- |
| `BabyCamApi::apiTask` | HTTP-Statusabfrage `/api/v1/status` | **alle 3 s** |
| `BabyCamApi::apiTask` | HTTP-Heartbeat `/client/online` (neue Verbindung) | alle 10 s |
| `MqttLogic` | Dauerverbindung, `mqttClient.loop()` | jeder Durchlauf |
| `WebSetupLogic` | `handleClient()` | jeder Durchlauf, **doppelt gerufen** |
| `HaWebsocketLogic` | - | aus (laeuft nur auf dem HA-Screen) |

Der HA-Websocket ist also heute schon aus, wenn das Babyphone laeuft. Uebrig
bleiben die BabyCam-Statusabfrage alle drei Sekunden und MQTT.

**Nebenbefund (V-24):** `WebSetupLogic_Update()` wurde sowohl aus `loop()` in
`main.cpp` als auch aus `SystemLogic_Update()` gerufen - doppelt pro Durchlauf.
Der Aufruf in `main.cpp` ist entfernt.

**Schalter `NET_QUIET_DURING_VIDEO`** (in `SharedData.h`, Vorgabe `1`):
Solange `isStreamActive` gilt, schweigen

* die BabyCam-Statusabfrage (der Heartbeat bleibt - ohne ihn schliesst die
  Kamera laut `api_RE.md` den Stream),
* MQTT,
* der Webserver.

**Einschraenkung:** Ohne MQTT gibt es in diesem Modus keinen Baby-Alarm per
MQTT und keine Kamera-Trigger von aussen. Fuer einen Diagnoselauf vertretbar,
fuer den Dauerbetrieb muesste MQTT feiner behandelt werden (etwa nur
`loop()` ohne Reconnect-Versuche).

**Reihenfolge der Tests**

1. `VIDEO_SELFTEST 1` - Grafiklast ohne Netz. Beantwortet: reicht die Grafik
   allein als Ausloeser?
2. `VIDEO_SELFTEST 0` + `NET_QUIET_DURING_VIDEO 1` - Video mit Netz, aber ohne
   jeden Nebenverkehr. Beantwortet: reicht paralleler Netzverkehr als Ausloeser?
3. Je nach Ergebnis: `NET_QUIET_DURING_VIDEO 0` zum Gegenvergleich.

Laeuft Schritt 1 durch und Schritt 2 ebenfalls, aber Schritt 3 stuerzt ab, ist
paralleler Netzverkehr als Ausloeser belegt - und die Loesung liegt in der
Netz-Architektur, nicht in der Grafik.

---

### 2.17 Wendepunkt: das Netz ist beteiligt (2026-09-20)

Zwei Laeufe mit identischer Grafiklast:

| Lauf | Decoder | Anzeige | Netz | Ergebnis |
| --- | --- | --- | --- | --- |
| Schritt 2 (`NET_QUIET=1`, kein PPA, Panel-Patch) | aktiv | aktiv | **an** | Absturz im JPEG-Pfad |
| Selbsttest Stufe 1 | aktiv | aktiv | **aus** | **~400 s stabil** |

Derselbe Decoder, derselbe Anzeigepfad, derselbe DMA2D-Kanal. Der einzige
Unterschied ist das Netzwerk.

**Damit ist die Grafik als alleinige Ursache widerlegt.** Die Hebel 1 bis 4
haben an der falschen Stelle angesetzt. Der urspruengliche Verdacht des
Projektinhabers - SDIO/WLAN ist beteiligt - trifft zu, und esp-idf #19108
beschreibt mit *"correlates with network/UI activity"* dasselbe.

**Bewertung der Aussagekraft.** 400 s stehen gegen Abstaende von Sekunden bis
wenigen Minuten. Das ist ein deutliches Signal, aber kein Beweis; fuer
Gewissheit braeuchte Stufe 1 einen Lauf von 30 Minuten oder mehr.

**Offene Zweideutigkeit.** Stufe 1 hat nicht nur das Netz entfernt, sondern auch
die **Variation der Eingabedaten** - sie dekodiert immer denselben Frame. Beide
Unterschiede muessen getrennt werden.

**Stufentest.** `VIDEO_SELFTEST` kennt jetzt vier Stufen. Decoder- und
Anzeigelast sind in allen identisch, variiert wird nur das Netzwerk:

| Stufe | Netz | Prueft |
| --- | --- | --- |
| 0 | Normalbetrieb | - |
| 1 | WLAN aus | reine Grafiklast |
| 2 | WLAN verbunden, kein Stream | genuegt schon der aktive Funk/SDIO? |
| 3 | WLAN + Stream laeuft (Frames verworfen) | genuegt der Netzverkehr? |

In Stufe 3 laeuft die Decode-Schleife in einem eigenen Task (`Selftest`,
Prio 5, Core 1), damit der Stream parallel weiterlaufen kann. Dekodiert wird
durchgaengig der gesicherte Frame aus einem eigenen PSRAM-Puffer - die
Decoder-Eingabe bleibt also konstant, waehrend der volle Netzverkehr anliegt.

**Auswertung**

| Ergebnis | Schlussfolgerung |
| --- | --- |
| 3 stuerzt ab | Der Netz**verkehr** ist der Ausloeser - Buslast, Speicherdruck oder esp-hosted |
| 3 stabil, 2 stuerzt ab | Schon der aktive Funk/SDIO genuegt |
| 2 und 3 stabil | Nicht das Netz, sondern die **wechselnden Eingabedaten** des Decoders |

Die letzte Zeile ist die unerwartete Moeglichkeit, die der Stufentest mit
abdeckt: wechselnde JPEG-Groessen und -Inhalte koennten Decoder-Pfade treffen,
die ein statischer Frame nie erreicht.

**Konsequenz fuer die bisherigen Hebel.** Sie bleiben vorerst aktiv, sind aber
nicht mehr begruendet. Insbesondere kostet Hebel 3 messbar Leistung
(`Anzeige` 0,4 ms -> 40 ms, 5,5 -> 4,4 fps), weil das Display ohne DMA2D mit der
CPU kopiert. Bestaetigt sich 2.17, gehoert er zurueckgebaut.

---

---

### 2.18 Umbau der Medienschicht (2026-09-20)

Nach 2.17 stand die Richtung fest: die Grafik allein erklaert die Abstuerze
nicht, und die belastbarste Spur ist der Unterschied zwischen Selbsttest und
Normalbetrieb. Der Selbsttest dekodierte aus einem **PSRAM**-Puffer und lief
491 s stabil - mit doppelter Decodelast und vollem Netzverkehr. Der
Normalbetrieb dekodierte aus **internem RAM** (`download_buf`, nur
adressaligniert) und stuerzte nach Sekunden bis Minuten ab.

Daraus ist kein Flickwerk mehr geworden, sondern ein Umbau der gesamten
Medienschicht. Dieser Abschnitt beschreibt den Zustand danach; er ist der
Bezugspunkt, auf den die neuen Dateikoepfe verweisen.

#### 2.18.1 Leitsaetze

1. **Jede Decoderpuffer-Anforderung laeuft ueber `jpeg_alloc_decoder_mem()`.**
   Die Funktion kapselt drei Dinge, die sich von Hand leicht falsch machen
   lassen und die zwischen IDF-Versionen wechseln: die zur Laufzeit ermittelte
   Cache-Line-Groesse (auf dem P4 typisch 64 B), das Aufrunden der *Groesse*
   (nicht nur der Adresse) und die Wahl der unverschluesselten PSRAM-Region.
   Interner RAM als JPEG-Eingabe ist nirgends zugesichert.
2. **Genau ein Besitzer je Hardwareeinheit.** Der Hardware-JPEG-Decoder gehoert
   `VideoDecoder`, die I2S-Einheit gehoert dem Audio-Besitzer-Task. Wer sonst
   noch heran muss, fragt - er greift nicht selbst zu.
3. **Kein Neuaufbau im Fehlerpfad.** Engine und Puffer entstehen einmal beim
   Systemstart. Ein Neuaufbau meldet den DMA2D-Client ab und wieder an; laeuft
   dabei noch ein Transfer, bleibt ein Kanal verklemmt zurueck - genau der
   Zustand, in dem `dma2d_hal_tx_reset_channel()` endlos dreht.
4. **Ein defektes Bild kostet ein Bild, nicht die Verbindung.** Jede Stufe
   verwirft einzeln und macht weiter.

#### 2.18.2 Videopfad: zwei Tasks statt einer Schleife

```
  [VideoNet, Prio 5, Core 0]                [VideoDec, Prio 4, Core 1]
   HTTP/MJPEG lesen                          Slot aus s_fullQ holen
        |                                          |
        |-- Slot aus s_freeQ  <--------------------+---- Slot zurueck in s_freeQ
        |-- Kopfzeilen + Rumpf in den Slot         |
        +-- Slot in s_fullQ  ---------------------→+---- dekodieren -> anzeigen
```

Der Grund ist gemessen, nicht geraten (2.11): 167 ms Download, 12 ms
Verarbeitung. Sequenziell sind das 179 ms je Bild - 5,5 fps. Ueberlappt bleibt
nur das Maximum der beiden uebrig, also die Downloadzeit. Zusammen mit der in
2.13 gemessenen Kameraobergrenze von 791 KB/s ergibt das bei ~80 KB je Bild den
angestrebten Bereich von rund 10 fps.

Drei Ein- und drei Ausgabeslots entkoppeln die beiden Tasks. Die Slotzahl ist
bewusst klein: sie begrenzt die Latenz. Fuellt der Netzpfad schneller, als der
Decoder leert, blockiert er am leeren `s_freeQ` - gewollte Rueckstaufuehrung
statt wachsender Verzoegerung zwischen Realitaet und Bild.

Bemerkenswert an der Slotverwaltung ist, dass **jeder** Pfad den Slot genau
einmal zurueckgibt: bei zu kleiner Kapazitaet, bei Abbruch im Rumpf, bei
fehlgeschlagener Plausibilitaetspruefung, bei voller Ausgabewarteschlange und
im Normalfall nach der Dekodierung. Ein verlorener Slot waere nicht sofort
sichtbar - die Bildrate saenke nur um ein Drittel und nach drei Verlusten
stuende das Video still.

#### 2.18.3 Anzeige

Der Anzeigepfad kommt ohne PPA aus (Hebel 4). Im Vollbild wird das Bild in
Originalgroesse mittig ausgegeben; zieht die Aushandlung die Kamera auf die
Panelaufloesung, fuellt es den Schirm ohnehin 1:1.

Beide Darstellungswege laufen unter dem LVGL-Lock (V-02). `M5.Display` wird
sonst von zwei Tasks ueber denselben MIPI-DSI-Bus bespielt: vom LVGL-Flush und
vom Decoder-Task. Der Lock ist bereits die Klammer um jeden Displayzugriff der
Oberflaeche, also nutzen wir ihn auch hier. Was bewusst **nicht** unter dem Lock
laeuft, ist die Dekodierung selbst - sie dauert ~12 ms und wuerde die
Oberflaeche in genau diesem Mass ausbremsen. Das ist die bewusste Ruecknahme
von Hebel 2: Hebel 2 hat die Abstuerze ohnehin nicht verhindert (2.10), und
Hebel 3 hat den zweiten DMA2D-Nutzer inzwischen ganz entfernt.

Der Hardware-Decoder legt die Ausgabe mit einer auf 16 Pixel aufgerundeten
Zeilenlaenge ab (MCU-Breite). Ist die Bildbreite kein Vielfaches von 16, stimmt
die Zeilenlaenge im Puffer nicht mit der Bildbreite ueberein, und M5GFX wie
LVGL lesen das Bild schraeg aus. `VideoDecoder::compactRows()` schiebt die
Zeilen in diesem Fall einmalig zusammen - bei allen gaengigen Aufloesungen
(1280, 800, 640, 320) faellt der Schritt gar nicht erst an.

#### 2.18.4 Overlay im Vollbild

Bei gesetztem FPS-Schalter blendet `VideoOverlay` Bildrate, Kamera-Akku und
Tablet-Akku links oben ein. Gezeichnet wird in ein Sprite und zeilenweise per
`memcpy` in den RGB565-Bildpuffer kopiert - ohne Annahme ueber Ausrichtung oder
Bildbreite. Die Aufteilung des Sprites steht als benannte Konstanten im Header,
weil sich die Bloecke sonst ueberlappen: mit frei gewaehlten Pixelwerten schob
sich "100 %" des ersten Akkublocks genau ueber die Beschriftung des zweiten.

Das Sprite liegt in PSRAM. Interner RAM ist auf diesem Board das knappste Gut -
der SDIO-WLAN-Treiber braucht ihn -, und das Sprite wird hoechstens zweimal pro
Sekunde beschrieben.

#### 2.18.5 Audio: ein Besitzer, drei Zustaende

Mic und Lautsprecher haengen auf dem Tab5 am selben I2S-Port und teilen sich
den MCLK-Pin (GPIO 30). Halbduplex ist damit kein Entwurf, sondern Hardware.
M5Unified 0.2.13 serialisiert `begin()`/`end()` nicht: faellt ein zweiter Task
waehrend eines Teardowns in `tone()` oder `playRaw()`, wird ihm der Kanal unter
den Fuessen weggezogen (Core Panic).

Der Lautsprecher kennt deshalb drei Zustaende:

| Zustand | Wann | Bedeutung |
| --- | --- | --- |
| `SPK_UI` | Grundzustand nach dem Boot | Default-Abtastrate, Kanal 1 spielt Klicks und den Mattenalarm |
| `SPK_STREAM` | Babyphone laeuft | 44100 Hz, sonst resampled M5Unified jedes Sample linear auf 48000 |
| `SPK_OFF` | PTT laeuft | abgeschaltet, das Mikrofon belegt den Port |

Der Rueckweg nach `SPK_UI` ist der Punkt, der beim ersten Entwurf gefehlt hat:
ohne ihn waeren Tastenklicks **und der Mattenalarm** nach der ersten
Babyphone-Sitzung dauerhaft stumm geblieben.

Zugriffe von aussen laufen ueber `AudioHw_Lock()` / `AudioHw_Unlock()` und
fragen vorher `AudioHw_SpeakerUsable()`. Zwei Stellen tun das: der UI-Ton-Task
in `SharedData.cpp` und die Bootmelodie in `main.cpp`. Beide halten das Schloss
nur ueber den Aufruf, nicht ueber die Tondauer - der Ton laeuft asynchron
weiter.

#### 2.18.6 Jitterpuffer und die SPSC-Regel

Der Netzempfang (`Audio_RX`, Prio 11) fuellt einen 64-KB-Ring in PSRAM
(~740 ms), der Besitzer-Task (Prio 12) leert ihn. Ohne Puffer startet jeder
Stream im Underrun: die DMA-Kette haelt nur ~43 ms Audio vor.

Die Regel dabei: **`s_ringHead` gehoert dem Schreiber, `s_ringTail` dem Leser.**
Keiner fasst die Variable des anderen an. Der erste Entwurf verletzte das an
zwei Stellen, und beide waren tueckisch:

* `ringFree()` wurde bei Ueberlauf zweimal ausgewertet. Zwischen den beiden
  Aufrufen laeuft der Leser weiter, die Differenz `len - ringFree()` unterlaeuft
  als `size_t` ins Riesenhafte - und wird als Sprungweite benutzt.
* Der Schreiber zog bei Ueberlauf den Tail nach. Damit konnte `ringRead()`
  mitten im `memcpy` einen Tail sehen, den es nie gesetzt hat.

Jetzt meldet der Schreiber einen Ueberlauf nur noch per Flag und verwirft das
Paket; das Nachziehen erledigt der Leser in `ringTrim()`. Zusaetzlich werden
alle Laengen auf gerade Byteanzahlen gerundet - bei 16-Bit-Samples verschiebt
ein einzelnes Restbyte sonst alle folgenden Samples, und aus Audio wird
Rauschen.

#### 2.18.7 Mikrofon: `record()` ist asynchron

`M5.Mic.record()` reiht den Puffer nur in die DMA-Kette ein und kehrt sofort
zurueck - der Inhalt ist beim Ruecksprung noch nicht gueltig. Der erste Entwurf
verschickte ihn direkt: beim ersten Mal uninitialisierten Speicher, danach
dauerhaft veraltete Samples. Jetzt laufen zwei Puffer im Wechsel, und
`micRecord()` wartet explizit, bis `isRecording()` unter zwei faellt - erst dann
ist der andere Puffer garantiert vollstaendig.

#### 2.18.8 Kamera-API

Alle UI-seitigen Befehle laufen ueber eine Warteschlange und werden im
Hintergrundtask ausgefuehrt. Frueher wurden sie direkt aus den LVGL-Callbacks
gerufen - ein synchroner HTTP-Request mit 1,5 s Timeout, waehrend der Main-Loop
den LVGL-Lock hielt.

Der Heartbeat (`/client/online`) steht jetzt **vor** der Kommandoabarbeitung.
Ein Schieberegler fuellt die Warteschlange im Handumdrehen mit einem Dutzend
Befehlen; mit je bis zu 1,5 s waere der Heartbeat bis zu 18 s ausgeblieben, und
die Kamera haette mitten in der Bedienung Video- und Audio-Sockets gekappt.

Neu gegenueber dem Altstand:

* Die Abmeldung (api_RE.md Schritt 3) wird ueberhaupt erst gerufen - vorher
  blieb nach jedem Absturz eine Sitzung auf der Kamera zurueck. Sie sitzt im
  API-Task, also dort, wo auch der Heartbeat sitzt; die Begruendung steht in
  2.18.12.
* `forceChangeResolution` steht auf `true`. Mit `false` darf die Kamera die
  angeforderte Aufloesung ablehnen - was sie nachweislich getan hat.
* Die tatsaechlich eingestellte Aufloesung wird zurueckgelesen, statt sie nach
  dem Senden anzunehmen.

#### 2.18.9 Die Einheitenfalle

`WiFiClient::setTimeout()` erwartet **Sekunden** und ueberschreibt damit
`Stream::setTimeout(ms)` mit einer anderen Einheit. Ein gut gemeintes `50`
haette den Socket 50 Sekunden blockieren lassen und jede Stall-Erkennung
ausgehebelt. Solche Vorzeichen- und Einheitenfehler sind in diesem Umbau
zweimal aufgetreten (hier und beim `size_t`-Unterlauf in 2.18.6) - sie erzeugen
keine Compilerwarnung und werden erst im Dauerbetrieb sichtbar.

#### 2.18.10 Der Panel-Eingriff ist jetzt abgesichert

Hebel 3 (`use_dma2d = false`, 2.12) liegt in `.pio/libdeps` - einem
Verzeichnis, das PlatformIO jederzeit neu befuellen darf. `tools/patch_m5gfx.py`
laeuft als `pre:`-Skript vor jedem Build, ist idempotent, legt beim ersten Mal
eine Sicherung als `*.orig` an und meldet sich laut, wenn die Ankerzeile in
M5GFX verschwindet. Ohne dieses Skript waere der DMA2D-Haenger nach dem
naechsten Paketupdate stillschweigend zurueck - und niemand wuesste warum.

#### 2.18.11 Was der Umbau nicht kann

Der Upstream-Bug (#19108, #18999) ist damit **nicht behoben**; er ist umgangen.
Solange Espressif keinen Fix liefert, bleibt die Strategie: so wenige
DMA2D-Nutzer wie moeglich, und die verbleibenden mit treiberkonform angelegten
Puffern bedienen. Ob das reicht, entscheidet der Dauerbetrieb - `CrashDiag`
und der retained Bericht auf `lela/diag/boot` messen es mit.

#### 2.18.12 Was ein zweiter Blick noch gefunden hat

Der Umbau ist anschliessend noch einmal vollstaendig gegengelesen worden,
gezielt auf Verklemmungen, Slot-Lecks, Zustandsfehler und Ueberlaeufe. Zwei
Dinge vorweg, weil sie beruhigend sind: die Slotverwaltung im Videopfad ist
nachweislich leckfrei (jeder der sechs Ruecksprungpfade gibt den Slot genau
einmal zurueck), und eine Lock-Inversion zwischen `lvgl_port_lock` und
`AudioHw_Lock` gibt es nicht - kein Pfad nimmt beide.

Gefunden wurde dagegen Folgendes:

**Der Lautsprecher konnte stumm zurueckbleiben.** `speakerSet()` setzte den
Zustand erst auf `SPK_OFF` und danach auf das Ziel. Schlug `begin()` fehl,
blieb er auf `SPK_OFF` stehen - und damit lieferte `AudioHw_SpeakerUsable()`
dauerhaft `false`. **Der Mattenalarm waere ab diesem Moment stumm gewesen**,
ohne jede Meldung. `speakerSet()` meldet jetzt Erfolg oder Misserfolg zurueck,
jeder Aufrufer wertet das aus, und `speakerFallbackToUI()` ist die letzte
Rettung in den Grundzustand.

**Toene wurden waehrend jedes Umschaltvorgangs verworfen.** Der UI-Ton-Task
holte sich das Schloss mit 50 ms Wartezeit und liess die Nachricht bei
Misserfolg fallen. Ein Teardown haelt das Schloss aber 60 bis 600 ms. Ein
Alarmton wird genau einmal abgeschickt - faellt er in dieses Fenster, ist er
weg. Bei einer Katzenmatte und einem Babyphone ist das die falsche
Sparsamkeit, also wird jetzt bis zu 1,5 s gewartet statt verworfen. Die Grenze
bleibt, damit ein dauerhaft defekter Lautsprecher die Warteschlange nicht
aufstaut.

**Die Absenkung des Streams war wirkungslos.** Der Ton-Task setzte
`setChannelVolume(0, vol/3)` - und `feedSpeaker()` schrieb alle 5 ms den vollen
Wert zurueck. Die Absenkung hielt also hoechstens ein Haeppchen. Jetzt meldet
der Ton-Task ueber `AudioHw_DuckStream()` nur noch einen Zeitraum an; abgesenkt
wird dort, wo die Lautstaerke ohnehin gesetzt wird.

**Stop waehrend PTT verpuffte.** `CMD_STOP_BABY` beginnt mit
`if (!isAudioStreaming) break;` - waehrend PTT ist das immer wahr, weil PTT den
Stream vorher abgeschaltet hat. Das Kommando ging verloren, `resume` blieb
stehen, und nach dem Loslassen lief das Babyphone gegen den ausdruecklichen
Wunsch wieder an. Jetzt loescht ein Stop waehrend PTT die Wiederaufnahme.

**`record()` konnte einen unfertigen Puffer liefern.** Lief die 200-ms-Frist
ab, waehrend noch zwei Puffer in der DMA-Kette standen, wurde trotzdem
gesendet. Der Puffer wird jetzt nur zurueckgegeben, wenn die Frist eingehalten
wurde; die Indexrotation passiert unabhaengig davon, denn der eingereihte
Puffer darf kein zweites Mal eingereiht werden.

**Ein fehlgeschlagenes `M5.Mic.begin()` war unbehandelt.** Ergebnis waere PTT
"aktiv" ohne Mikrofon und mit abgeschaltetem Lautsprecher gewesen - wieder mit
stummem Mattenalarm.

**Die Abmeldung widerrief sich selbst.** `camLogoff()` lief im Video-Task,
waehrend der API-Task alle 8 s weiter `/client/online` schickte. Nach
spaetestens acht Sekunden war die Sitzung wieder offen. Schlimmer: das Beenden
des Videos haette das noch laufende Babyphone-Audio mitgekappt. Die Abmeldung
gehoert dorthin, wo der Heartbeat sitzt - jetzt meldet `BabyCamApi` ab, sobald
die Kamerasitzung insgesamt endet.

**Pause und sofort wieder Play konnte ~20 s dauern.** `s_running` wurde nur in
den Leseschleifen geprueft, nicht in den Backoff-Pausen (bis 8 s) und nicht
zwischen den Steuerungsaufrufen. `VideoLogic_Start()` legt aber erst dann einen
neuen Task an, wenn der alte verschwunden ist. Alle Wartezeiten laufen jetzt
ueber `waitInterruptible()`.

**Drei Fristen waren nicht ueberlaufsicher.** `millis() > deadline` schlaegt
nach 49,7 Tagen dauerhaft an und reisst dann bei jedem Bild die Verbindung ab.
Fuer ein Geraet, das monatelang durchlaeuft, ist das kein theoretischer Fall.
`deadlineReached()` rechnet jetzt mit der Differenz in einem
vorzeichenbehafteten Typ.

Bewusst nicht geaendert wurde eines: waehrend gedrueckter PTT-Taste bleibt ein
Alarmton aus. Das ist keine Nachlaessigkeit, sondern Halbduplex - das Mikrofon
belegt den I2S-Port, auf dem der Ton liegen muesste. PTT ist eine
Momenttaste, das Fenster also auf Sekunden begrenzt.

#### 2.18.13 Der erste Flash: kein einziges Bild

Der erste Lauf auf dem Geraet lief so: Verbindung steht, `[Video] Stream
laeuft.`, dann acht Sekunden nichts, `[Video] Keine Bilder mehr - Verbindung
wird erneuert.`, und das im Kreis. Kein Absturz, kein Decode-Fehler, keine
Profilzeile - und kein Bild.

Die Meldung war irrefuehrend. Sie zeigt auf das Netz, das Problem lag im
Parser:

```c
// falsch
return (len > 128 && b[0] == 0xFF && b[1] == 0xD8 &&
        b[len - 2] == 0xFF && b[len - 1] == 0xD9);
```

Die `Content-Length` der Kamera ist **nicht** die Laenge des JPEG. Hinter dem
EOI-Marker stehen noch Zeilenumbrueche und der Anfang der naechsten
Multipart-Grenze. Die Pruefung auf FF D9 an den letzten beiden Bytes schlug
damit bei **jedem** Bild fehl.

Der Pfad, der das Bild verwirft, aktualisiert `lastFrameMs` nicht - nach acht
Sekunden meldete die Stall-Erkennung pflichtgemaess eine tote Verbindung. Ein
systematischer Ausschuss sah damit exakt aus wie ein Netzwerkproblem.

Der Vorgaengercode hatte es richtig gemacht und den Marker immer rueckwaerts
gesucht (`d_buf[i] == 0xFF && d_buf[i+1] == 0xD9`, dann
`actual_jpeg_size = i + 2`). Beim Neuschreiben ist diese Eigenschaft verloren
gegangen - ein Lehrstueck darueber, dass beim Neubau nicht nur die Struktur,
sondern auch das gesammelte Detailwissen des alten Codes mitwandern muss.

`jpegLength()` sucht jetzt wieder rueckwaerts und liefert die wahre Laenge; nur
die wird an den Decoder weitergereicht, das Fuellmaterial dahinter sieht er
nicht mehr.

Zwei Lehren, die im Code stehen:

* Ein verworfenes Bild ist jetzt nicht mehr stumm. `logBadFrame()` meldet
  hoechstens alle 5 s Laenge sowie Anfangs- und Endbytes. Ein systematischer
  Ausschuss faellt damit in der ersten Logzeile auf, nicht nach einer Stunde
  Fehlersuche in die falsche Richtung.
* Eine Diagnosemeldung darf nicht mehr behaupten, als sie weiss. "Keine Bilder
  mehr" klang nach Netz und war es nie.

#### 2.18.14 Wo die Zeit bleibt

Die Bildratenfrage hat sich als eigenes Thema herausgestellt, das ueber die
Medienschicht hinausgeht - es beruehrt die Uebersetzungseinstellungen, die
PSRAM-Taktrate, den Netzwerkstapel und LVGL. Analyse, Massnahmen und deren
Verfolgung stehen deshalb in einem eigenen Dokument: `src/doc/performance.md`.

Die drei Befunde, die dort die Richtung vorgeben:

* Der Hardware-Decoder ist mit 10,4 ms je 720p-Bild bereits am Anschlag
  (Herstellerbenchmark: 109 fps). Dort ist nichts zu holen.
* Die Aufloesung senkt die Netzlast nicht - die Kamera liefert bei 1280x720 und
  bei 640x480 jeweils rund 80 KB je Bild und variiert stattdessen die
  Kompression. Zustaendig ist `quality`.
* Der Durchsatz liegt bei 320 KB/s, gegen 791 KB/s derselben Kamera am PC. Es
  fehlt mehr als die Haelfte, bevor ein Bild dekodiert wird.

## 3. Nebenlaeufigkeit

**V-02 — Vollbild-Push ohne LVGL-Lock.** `draw_and_push_fullscreen()` ruft
`M5.Display.clear/startWrite/pushImageSRM/endWrite` ohne `lvgl_port_lock()`.
`PPABase::getBuffer()` holt sich als Ziel
`((Panel_DSI*)base->getPanel())->config_detail().buffer` — also denselben
DSI-Framebuffer, in den LVGLs `disp_flush()` aus dem Main-Loop-Task schreibt.
Zwei Tasks, zwei DMA-Pfade, kein gemeinsames Schloss. Verschaerft dadurch, dass
`btn_fs_event_cb` `vidFSMode` aus `lv_timer_handler()` heraus setzt, waehrend
LVGL noch offene Flushes hat.

**V-03 — Kein Join beim Stoppen.** `VideoLogic_Stop()` setzt nur
`isStreamActive = false` und kehrt sofort zurueck. Der Task kann noch Sekunden
in Ping/Login/`vTaskDelay(1500)` haengen. Tippt der Nutzer in diesem Fenster
erneut auf Play, greift die Flanke in `SystemLogic.cpp:61`, `VideoLogic_Start()`
sieht `videoTaskHandle != NULL` und erzeugt keinen Task. Danach ist
`isStreamActive == true`, `lastStreamState == true`, kein Task — der Stream
bleibt tot, bis zweimal umgeschaltet wird.

**V-04 — Triple-Buffer.** Nach jedem Decode gilt
`write_idx = read_idx = next_write`, wodurch `if (next_write == read_idx)` nie
greift (toter Code). Schlaegt `lvgl_port_lock(15)` fehl, werden die Indizes
trotzdem weitergedreht, waehrend LVGL noch auf den alten Puffer zeigt — nach
drei Frames ueberschreibt der Decoder den gerade gerenderten Puffer.

**V-05 — String-Race.** `apiTask` schreibt `camStatus` und `camResolutions[]`
alle 3 s; `ViewBabySettings_Show()` und `BabyCamApi_ToggleCamera()` lesen sie
aus dem GUI-Task. Arduino-`String` reallokiert beim Zuweisen. Zusaetzlich wird
`camResCount = 0` gesetzt, bevor das Array neu gefuellt wird.

---

## 4. Blockierende Aufrufe im UI-Thread

**V-06.** `BabyCamApi_SetZoom/Quality/Speed/Resolution`, `ToggleFlash`,
`ToggleScreen`, `PowerOff` laufen direkt in LVGL-Callbacks
(`ViewBabySettings.cpp:33-45, 63`) und machen dort synchrone HTTP-Requests mit
1500 ms Timeout, waehrend der Main-Loop den LVGL-Lock haelt. Loesung: kleine
Befehls-Queue, die `apiTask` abarbeitet.

**V-07.** `AudioStreamLogic_StartPTT()` im Button-Callback: `StopBaby()`
(50 ms + HTTP bis 1,5 s) -> `delay(50)` -> `M5.Mic.begin()` ->
`SetParentAudio()` (nochmals bis 1,5 s). In Summe ueber 3 s Stillstand.

**V-08.** `StopBaby()`/`StopPTT()` loeschen das Flag, warten 50 ms und rufen
dann `M5.Speaker.end()` bzw. `M5.Mic.end()` — in der Hoffnung, der Consumer-Task
sei fertig. Der laeuft aber mit Prioritaet 4 auf Core 1, wo der Video-Task mit
Prioritaet 5 ihn verdraengen kann; 50 ms sind dann nicht garantiert. `api_RE.md`
beschreibt genau diesen Fall als sichere Panic-Ursache und schreibt einen
Handshake vor (Flag -> RTOS-Yield -> I2S deaktivieren). Korrekt waere eine
Task-Notification statt eines Delays.

---

## 5. Logik, Robustheit, Architektur

**V-09.** `if (camDropFrames >= 2) camDropFrames--;` dekrementiert die globale
Nutzereinstellung statt eines lokalen Zaehlers. Ein eingestellter Wert von 2
faellt nach dem ersten Drop dauerhaft auf 1. Es fehlt ein separater
`static int dropCounter`.

**V-10.** `VideoOverlay::applyOverlay()` steigt bei `pic_w < 150` aus, schreibt
aber ab x=10 volle 150 Pixel, braucht also `pic_w >= 160`. Der 32-Bit-Transfer
setzt zudem eine gerade `pic_w` voraus (sonst unaligned Zugriffe), und
`createSprite()` wird nicht auf Erfolg geprueft, obwohl `initialized = true`
unabhaengig davon gesetzt wird.

**V-14.** `BabyCamApi_SetResolution()` sendet `forceChangeResolution: false`,
die Kamera darf also ablehnen. Das Feld `resolution` aus `/api/v1/status` wird
nie zurueckgelesen. Belegt durch die Dumps: angefordert 1280x720, geliefert
800x450. Bei echten 1280x720 waere der Zoomfaktor 1.0 und die PPA im Vollbild
ganz vermeidbar.

**V-15.** Core 1 traegt `VideoTask` (5), `HA_WS_Task` (5), `cam_scan` (5),
`Aud_Cons` (4), `ApiTask` (2) — und darunter mit Prioritaet 1 den Arduino-Loop
mit LVGL. Core 0 nur `Aud_Prod` (5) und den Preloader (1). Die Frame-Drop-
Heuristik und die kurzen Lock-Timeouts sind Symptombehandlung dafuer.

**V-16.** Jedes `return` aus `processStream()` erzwingt im `videoTask` den
vollen Wiederaufbau: 2 s Pause + Ping + Login + Aufloesung + 1,5 s Wartezeit.
Ein kurzer Sync-Verlust kostet so ueber fuenf Sekunden Schwarzbild.

**V-18.** `/api/v1/client/offline` ist in `api_RE.md` dokumentiert, wird im Code
aber nirgends aufgerufen. Nach jedem Absturz bleibt eine Client-Session auf der
Kamera zurueck.

**V-19.** Die Login-Sequenz ist in `videoTask` handgeschrieben dupliziert,
obwohl `BabyCamApi` dieselben Endpunkte kapselt. Port `8080` und
`versionApp: 2.48` stehen als Literale an mehreren Stellen.

**V-20.** `ViewBaby_SetStatus()` baut pro Aufruf einen `String` mit 80
fuehrenden Leerzeichen — Heap-Churn auf einem Pfad aus dem Video-Task.

**V-21.** `StaticJsonDocument` und `containsKey()` sind in ArduinoJson 7
veraltet (`JsonDocument`, `doc["x"].is<T>()`). Die 1024 Bytes koennten bei
langer Aufloesungsliste zudem knapp werden; ein Overflow wird still verschluckt.

**V-22 / V-23.** `new lgfx::PPASrm(...)` und der LVGL-Zeichenpuffer `buf1`
(`heap_caps_aligned_alloc(..., MALLOC_CAP_SPIRAM)`) werden ohne Null-Pruefung
weiterverwendet.

---

## 6. Offene Experimente

| ID | Experiment | Erwartung |
| --- | --- | --- |
| E-1 | Nach einem Absturz **stromlos** machen statt durchlaufen lassen | **In Arbeit** - Messung eingebaut, siehe 6.1. Bricht die Haeufung ab, behaelt der DMA2D-Block seinen verklemmten Zustand ueber den Panic-Reset hinweg. Erklaert "einmal aufgetreten, dann oefter". |
| E-2 | `download_buf` von `MALLOC_CAP_INTERNAL` nach `MALLOC_CAP_SPIRAM` | Gibt dem SDIO-WLAN-Treiber 150 KB internes RAM zurueck (Bootlog: nur 231 KB frei). |
| E-3 | Aufloesung wirklich auf 1280x720 zwingen und per Status verifizieren | Zoom 1.0, PPA im Vollbild vermeidbar (siehe V-14). |
| E-4 | FPS-Overlay abschalten (`showFps`) | Entfernt CPU-Schreibzugriffe in den PPA-Quellpuffer; der Cache-Writeback der Bibliothek ist auskommentiert. |
| E-5 | PSRAM-Bandbreite senken (`camDropFrames`, kleinere Aufloesung) | Sinkt die Absturzrate messbar, ist Buslast beteiligt. |

### 6.1 E-1: Messaufbau und Testprotokoll

Damit E-1 eine Messung statt eines Eindrucks wird, protokolliert das Modul
`src/system/CrashDiag.*` den Zustand ueber Neustarts hinweg. Es legt seine
Daten in `RTC_NOINIT_ATTR`-Speicher: der ueberlebt einen Panic-Reset, geht bei
einem echten Stromausfall aber verloren. Genau diese Eigenschaft unterscheidet
Warm- von Kaltstart - dieselbe Unterscheidung, um die es bei E-1 geht.

Der Bericht ist bewusst in zwei Schritte geteilt: `CrashDiag_Init()` erfasst den
Zustand ganz frueh in `setup()` und gibt nichts aus, `CrashDiag_Report()` gibt ihn
spaeter aus. Grund: durch `ARDUINO_USB_CDC_ON_BOOT=1` ist der USB-CDC-Port
unmittelbar nach dem Reset noch nicht enumeriert - alles, was vor
`[USB] Starte USBHostSerial Treiber...` gesendet wird, geht verloren
(auch `--- SYSTEM BOOT ---` taucht deshalb in keinem Log auf).

Ausgabe beim Start, Beispiel nach einem Absturz:

    ==============================================
    [CrashDiag] Reset-Grund: Interrupt-Watchdog
    [CrashDiag] WARMSTART - Peripherie wurde NICHT stromlos!
    [CrashDiag] Vorheriger Lauf: 47.312 s | Video: AN | Vollbild: aus | 18 fps
    [CrashDiag] Seit Kaltstart: 3 Starts, 2 Abstuerze
    ==============================================

**Lesehilfe zum Reset-Grund**

* `Kaltstart (Power-On)` - das Geraet war wirklich stromlos.
* `Sonstiger Watchdog` / `Software-Reset` / `Externer Reset-Pin` - erscheint unter
  anderem direkt nach dem Flashen; der RTC-Block ist dabei ebenfalls verloren.
  Die Ausgabe weist diesen Fall gesondert aus, damit er nicht mit einem echten
  Kaltstart verwechselt wird.
* `Interrupt-Watchdog` / `PANIC / Exception` mit `WARMSTART` - das ist der Fall,
  um den es bei E-1 geht.

Abstuerze werden nur gezaehlt, wenn es einen vorherigen Lauf gibt, dem sie
zugeordnet werden koennen. Nach einem Kaltstart steht der Zaehler auf null.

**Wo die Daten landen**

Nicht auf der seriellen Konsole - die setzt einen angeschlossenen Rechner voraus
und verliert nach einem Absturz die Verbindung. Der Bericht wird stattdessen
einmalig nach jedem Start **retained** auf dem MQTT-Broker abgelegt:

    Topic:   lela/diag/boot
    Payload: {"reason":"Interrupt-Watchdog","cold":0,"prev_ms":47312,
              "video":1,"fs":0,"fps":18,"boots":3,"crashes":2}

Retained bedeutet: der letzte Stand liegt dauerhaft auf dem Broker. Es genuegt,
gelegentlich hineinzuschauen - egal ob Stunden oder Tage spaeter, mit einem
MQTT-Client oder als Sensor in Home Assistant. Kein Kabel, kein Mitschreiben.

Fuer eine Historie statt nur des letzten Stands reicht ein MQTT-Sensor in Home
Assistant auf diesem Topic; `crashes` und `prev_ms` sind dann als Verlauf
vorhanden.

**Durchfuehrung**

Das Geraet laeuft im Alltag einfach weiter. Zu tun ist nur:

1. Einmal zu Beginn **stromlos** machen (einige Sekunden warten, damit das RAM
   sicher verliert) und wieder einschalten. Das setzt den Nullpunkt; der Bericht
   meldet dann `cold: 1` und `crashes: 0`.
2. Danach normal benutzen.
3. Gelegentlich `lela/diag/boot` ansehen.

**Auswertung**

* Steigt `crashes` und wird `prev_ms` dabei von Mal zu Mal kleiner, haeufen
  sich die Abstuerze innerhalb einer Warmstart-Kette. Faellt `prev_ms` nach
  einem Kaltstart (`cold: 1`) wieder auf den urspruenglichen Wert zurueck, ist
  die These bestaetigt: der verklemmte Zustand ueberlebt den Panic-Reset.
* Bleibt `prev_ms` unabhaengig von Kalt- oder Warmstart gleich verteilt, ist
  die These widerlegt und die Haeufung hat eine andere Ursache (naheliegend
  dann: WLAN-Qualitaet ueber den Tag).

E-1 ist damit erklaerend, nicht entscheidend: die Massnahmen aus Abschnitt 2.5
sind in beiden Faellen dieselben. Der praktische Nutzen liegt darin, dass die
Werte zugleich die Vergleichsbasis fuer die Wirkung von V-11 und V-12 liefern.

Die Zeile `Video: AN | Vollbild: aus` haelt nebenbei fest, in welchem Zustand
das Geraet jeweils gestorben ist - das ergaenzt die Aufrufpfad-Analyse aus
Abschnitt 2.3 ohne weiteren Aufwand.

---

## 7. Werkzeuge und Vorgehen

**Panic symbolisieren.** In der `platformio.ini` ist
`monitor_filters = esp32_exception_decoder` eingetragen (reiner Monitor-Filter,
kein Build-Einfluss); kuenftige Panics kommen bereits aufgeloest an.
Nachtraeglich fuer einen vorhandenen Dump:

    & "$env:USERPROFILE\.platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-addr2line.exe" `
      -pfiaC -e ".pio\build\esp32p4_m5stack\firmware.elf" <adressen>

Die ELF muss zum Dump passen (`ELF file SHA256` im Panic). Ein Rebuild ohne
Quellaenderung aendert nur den eingebetteten Zeitstempel, nicht die Adressen.

**Build-Stolperstein.** Schlaegt der Build mit
`ParamType.get_metavar() missing 1 required positional argument: 'ctx'` fehl,
wurde der in `README.md` Abschnitt 5.2 beschriebene click-Downgrade durch ein
Update ueberschrieben:

    & "$env:USERPROFILE\.platformio\penv\Scripts\pip.exe" install "click==8.1.7"

**Weitere Beobachtungen aus den Logs**, noch nicht eingeordnet:

* `Reason: 2 - AUTH_EXPIRE` bei ca. 10,3 s in **jedem** Boot.
* `nvs_get_str len fail: mqttBaby / mqttCamTrig NOT_FOUND` bei jedem Boot.
* Freies internes RAM nach dem Reservieren: 231 KB.

---

## 8. Aenderungsprotokoll

Format: Datum · ID(s) · Kurzbeschreibung · Ergebnis.

| Datum | ID | Aenderung | Ergebnis |
| --- | --- | --- | --- |
| 2026-09-20 | — | `monitor_filters = esp32_exception_decoder` ergaenzt | Panics kommen symbolisiert an |
| 2026-09-20 | V-01 | Grundursache ueber drei Dumps belegt und dokumentiert | Aufrufpfad steht, Massnahmen offen |
| 2026-09-20 | E-1 | `src/system/CrashDiag.*` ergaenzt, in `main.cpp` eingebunden | Kalt-/Warmstart und Laufzeit bis zum Absturz werden protokolliert |
| 2026-09-20 | E-1 | CrashDiag in Erfassen (`_Init`) und Ausgeben (`_Report`) getrennt | Bericht ging vorher im CDC-Enumerierungsfenster verloren |
| 2026-09-20 | E-1 | Absturzzaehler zaehlt nach einem Kaltstart nicht mehr mit; Flash-Reset wird als Sonderfall ausgewiesen | Bericht ist jetzt in sich schluessig |
| 2026-09-20 | V-11, V-12 | Decoder-Eingaben validiert, Engine-Neuaufbau aus dem Fehlerpfad entfernt | siehe 2.6; Wirkung ueber CrashDiag messbar |
| 2026-09-20 | E-1 | Bericht wird retained auf `lela/diag/boot` veroeffentlicht | Auswertung ohne angeschlossenen Rechner moeglich |
| 2026-09-20 | V-01 | Coredump aus Flash ausgelesen und ausgewertet (25 Tasks) | Nebenlaeufigkeit als Ausloeser ausgeschlossen, siehe 2.7 |
| 2026-09-20 | V-01 | Hebel 1: `startWrite()/endWrite()` um den PPA-Aufruf entfernt | Ein DMA2D-Nutzer weniger pro Bild |
| 2026-09-20 | V-01 | Upstream-Bugs #19108 und #18999 recherchiert und dokumentiert | Bekannter, offener IDF-Bug; Strategie in 2.8 |
| 2026-09-20 | fps | Frame-Zeitmessung in `processStream()` ergaenzt | Engpass wird messbar statt vermutet |
| 2026-09-20 | V-01 | Kollision LVGL-Flush vs. JPEG-Decode per Coredump belegt | Korrektur zu 2.7, Details in 2.10 |
| 2026-09-20 | V-01 | Hebel 2: `jpeg_decoder_process()` unter den LVGL-Lock gelegt | Serialisiert die beiden DMA2D-Nutzer |
| 2026-09-20 | fps | Erste Messung: Download 93 %, Decode 4,5 ms | siehe 2.11 |
| 2026-09-20 | V-01 | Chip-Revision v1.3 aus dem Coredump-Header bestaetigt | betroffene Revision laut #18999 |
| 2026-09-20 | V-01 | Hebel 3: `use_dma2d = false` im DSI-Panel (Patch in libdeps) | Experiment, siehe 2.12 |
| 2026-09-20 | fps | Gegenmessung per curl: Kamera liefert 791 KB/s | Deckel ~10 fps, siehe 2.13 |
| 2026-09-20 | V-03 | JPEG-Engine einmalig in `VideoLogic_Init()`; Start pegelgesteuert | Neustart-Ausloeser beseitigt, siehe 2.14 |
| 2026-09-20 | V-01 | Hebel 4: `VIDEO_USE_PPA 0` - Vollbild per `pushImage()` statt PPA | zusammen mit Hebel 3 DMA2D-frei |
| 2026-09-20 | V-01 | Selbsttest `VIDEO_SELFTEST` gebaut: Decode-Dauerlast ohne WLAN | Richtungsentscheidung Netz vs. Grafik, siehe 2.15 |
| 2026-09-20 | V-01 | `NET_QUIET_DURING_VIDEO` gebaut: kein Nebenverkehr waehrend des Streams | Test 2 der Reihenfolge in 2.16 |
| 2026-09-20 | V-24 | Doppelaufruf von `WebSetupLogic_Update()` entfernt | ein Netzwerknutzer weniger pro Durchlauf |
| 2026-09-20 | V-01 | Selbsttest Stufe 1: ~400 s stabil ohne Netz | Grafik als alleinige Ursache widerlegt, siehe 2.17 |
| 2026-09-20 | V-01 | Selbsttest auf vier Stufen erweitert (Netz variabel) | trennt Netzverkehr, Funkbetrieb und Eingabevariation |
| 2026-09-20 | V-01 | Medienschicht neu gebaut: Decoderpuffer durchgaengig ueber `jpeg_alloc_decoder_mem()` | siehe 2.18 |
| 2026-09-20 | fps | Video in Netz- und Decodier-Task getrennt (Pipeline, 3 Slots) | Ziel >= 10 fps, siehe 2.18.2 |
| 2026-09-20 | V-02 | Vollbildausgabe unter den LVGL-Lock gelegt | zwei Tasks auf einem MIPI-DSI-Bus serialisiert |
| 2026-09-20 | Video | Overlay (fps, Akku Baby, Akku Tablet) im Vollbild bei gesetztem FPS-Schalter | Sprite in PSRAM, Aufteilung ueberlappungsfrei |
| 2026-09-20 | Video | Zeilenlaenge des Decoders (MCU-Padding) wird zusammengeschoben | kein schraeges Bild bei Breiten ungleich n*16 |
| 2026-09-20 | Video | `setTimeout()` nimmt Sekunden - von 50 auf 1 korrigiert | Stall-Erkennung war ausgehebelt |
| 2026-09-20 | Video | Verwurfsschleife fuer uebergrosse Bilder begrenzt (s_running + Deadline) | Netz-Task kann nicht mehr haengen bleiben |
| 2026-09-20 | Audio | Lautsprecher-Zustandsmaschine UI/Stream/Off, Rueckweg nach UI ergaenzt | Klicks und Mattenalarm bleiben nach Babyphone-Betrieb hoerbar |
| 2026-09-20 | Audio | `AudioHw_Lock()` eingefuehrt; UI-Toene und Bootmelodie nutzen es | einziger verbliebener Zweitnutzer der I2S-Hardware abgesichert |
| 2026-09-20 | Audio | Jitterpuffer auf saubere SPSC-Trennung umgestellt | `size_t`-Unterlauf und Tail-Race beseitigt, siehe 2.18.6 |
| 2026-09-20 | Audio | Mikrofon mit zwei Puffern im Wechsel | `record()` ist asynchron - PTT sendete veraltete Samples |
| 2026-09-20 | API | Heartbeat vor die Kommandoabarbeitung gezogen | Befehlsschwall konnte die 10-s-Frist reissen |
| 2026-09-20 | API | `camLogoff()` wird tatsaechlich gerufen, `forceChangeResolution` auf `true` | Sitzungsleichen und abgelehnte Aufloesungen |
| 2026-09-20 | V-01 | `tools/patch_m5gfx.py` als `pre:`-Skript: Hebel 3 ueberlebt Paketupdates | siehe 2.18.10 |
| 2026-09-20 | V-05 | Kameradaten ueber feste Zeichenpuffer; `BabyCamApi_Update()` uebertraegt sie in der Hauptschleife | String-Use-after-free zwischen API-Task und Oberflaeche beseitigt |
| 2026-09-20 | Audio | `speakerSet()` meldet Erfolg; `speakerFallbackToUI()` als Rueckfallebene | stummer Mattenalarm nach fehlgeschlagenem `begin()` ausgeschlossen |
| 2026-09-20 | Audio | UI-Ton-Task wartet bis 1,5 s statt zu verwerfen | Alarmtoene gingen im Umschaltfenster verloren |
| 2026-09-20 | Audio | Absenkung ueber `AudioHw_DuckStream()` statt `setChannelVolume(0,..)` | war wirkungslos, weil die Wiedergabe alle 5 ms ueberschrieb |
| 2026-09-20 | Audio | Stop waehrend PTT loescht die Wiederaufnahme | Babyphone lief nach PTT gegen den Benutzerwunsch wieder an |
| 2026-09-20 | Audio | `micRecord()` gibt bei Zeitueberschreitung keinen Puffer heraus; Mikrofon unter dem Hardware-Schloss | unfertige Samples wurden verschickt |
| 2026-09-20 | API | Abmeldung wandert vom Video- in den API-Task | Heartbeat widerrief sie nach 8 s und kappte sonst laufendes Audio |
| 2026-09-20 | Video | Alle Wartezeiten ueber `waitInterruptible()` | Pause/Play brauchte bis zu 20 s |
| 2026-09-20 | Video | Fristen ueberlaufsicher (`deadlineReached()`) | nach 49,7 Tagen riss sonst jede Verbindung ab |
| 2026-09-20 | Build | Gesamtprojekt uebersetzt und gelinkt: RAM 10,1 %, Flash 45,5 %, keine Warnungen in der Medienschicht | siehe 2.18 |
| 2026-09-20 | Video | `jpegLength()` sucht den EOI-Marker rueckwaerts und kuerzt auf die wahre Bildlaenge | erster Flash zeigte kein einziges Bild, siehe 2.18.13 |
| 2026-09-20 | Video | `logBadFrame()`: verworfene Bilder werden gemeldet statt still verworfen | systematischer Ausschuss war als Netzproblem getarnt |
