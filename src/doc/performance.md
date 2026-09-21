# Durchlaufzeiten und Speicher: Analyse und Massnahmenverfolgung

Schwesterdokument zu `video_refactoring.md`. Dort steht, warum die Medienschicht
umgebaut wurde; hier steht, woran die Zeit und der Speicher verbraucht werden
und was dagegen zu tun ist.

Stand: 2026-09-20

---

## 1. Vorgehen

Regel dieses Dokuments: **keine Massnahme ohne Messung davor und danach.** Die
Fehlersuche im Videopfad hat dreimal gezeigt, wie teuer eine plausible
Vermutung ist. Jeder Eintrag in Abschnitt 4 hat deshalb eine erwartete und eine
gemessene Wirkung; solange die gemessene fehlt, gilt die Massnahme als offen.

### 1.1 Die Messzeile lesen

`VIDEO_PROFILE 1` gibt alle 5 s aus:

```
[VideoProf] 1280x720 | 6 Bilder in 5645 ms = 1 fps | je Bild: Download 845.9 ms,
Decode 10.0 ms, Anzeige 930.7 ms | (davon Sperre 0.0 ms, Transfer 929.4 ms) |
80 KB/Bild = 0.7 Mbit/s | verworfen 0 | Fehler 0/0
```

Zwei Fallen dabei:

* **`Download` enthaelt die Wartezeit auf einen freien Slot.** Ist die Anzeige
  der Engpass, staut sich der Netzpfad zurueck und meldet dieselbe Zahl wie die
  Anzeige. Zwei gleich grosse Werte heissen also nicht "zwei Probleme", sondern
  "eines, und das andere wartet darauf".
* **`Anzeige` misst nur, was im Decoder-Task passiert.** Im Fenstermodus ist das
  lediglich die LVGL-Buchhaltung; gezeichnet wird spaeter im Loop-Task und taucht
  in keiner Zahl auf. Deshalb sieht der Fenstermodus in der Messung besser aus,
  als er sich anfuehlt.

### 1.2 Was fehlt

Es gibt bisher keine Messung fuer den LVGL-Zeichenvorgang selbst
(`lv_timer_handler` im Loop-Task) und keine fuer die Heap-Entwicklung ueber die
Zeit. Beides gehoert ergaenzt, bevor die Punkte P-20 und P-21 bewertet werden
koennen.

---

## 2. Messbasis (2026-09-20, vor den Massnahmen)

| Aufloesung | Modus | Download | Decode | Transfer | Ergebnis |
| --- | --- | --- | --- | --- | --- |
| 1280x720 | Vollbild | 845-935 ms | 10.0-11.2 ms | 928-932 ms | 1 fps |
| 1280x720 | Fenster | 320 ms | 10.6 ms | 50 ms (nur Buchhaltung) | 1-3 fps |
| 640x480 | Vollbild | 250-330 ms | 4.0-4.5 ms | 90-107 ms | 2-4 fps |
| 640x480 | Fenster | 286 ms | 4.5 ms | 13 ms (nur Buchhaltung) | 2-3 fps |

Speicher beim Start:

```
[VideoDecoder] Start.          Frei: intern 376 KB, PSRAM 30814 KB
[VideoDecoder] Bereit. Frei danach: intern 375 KB, PSRAM 23022 KB
```

### 2.1 Drei Befunde, die die Richtung vorgeben

**Der Decoder ist bereits am Anschlag.** 10.4 ms fuer 1280x720 entsprechen
96 fps. Espressifs eigener Benchmark fuer den P4-Hardware-Decoder nennt 109 fps
bei derselben Aufloesung. Hier ist nichts zu holen - jede Minute, die in die
Dekodierung investiert wird, ist verschwendet.

**Die Aufloesung senkt die Netzlast nicht.** 1280x720 liefert 80-93 KB je Bild,
640x480 liefert ebenfalls 80 KB. Die Kamera haelt offenbar ein Byte-Budget ein
und variiert dafuer die Kompression. Wer die Downloadzeit senken will, muss
also an `quality` drehen, nicht an der Aufloesung. Die Aufloesung wirkt
ausschliesslich auf den Anzeigepfad.

**Der Durchsatz liegt bei 40 Prozent des Moeglichen.** 80 KB in 250-330 ms sind
250-320 KB/s. Dieselbe Kamera wurde per curl vom PC mit 791 KB/s gemessen
(`video_refactoring.md`, 2.13). Es fehlt also mehr als die Haelfte, und zwar
bevor irgendein Bild dekodiert wird.

---

## 3. Das Zeitbudget

Ziel sind 10 fps, also **100 ms je Bild**. Netz und Verarbeitung laufen
ueberlappt (zwei Tasks), es zaehlt daher das Maximum, nicht die Summe:

| Stufe | heute (720p) | Budget bei 10 fps | Bemerkung |
| --- | --- | --- | --- |
| Download | 250-330 ms* | < 100 ms | *ohne Rueckstau gemessen; braucht 800 KB/s bei 80 KB/Bild |
| Header-Parsing | nicht gemessen | < 2 ms | siehe P-30 |
| Dekodierung | 10.4 ms | 10 ms | erledigt, Hardwaregrenze |
| Transfer zur Anzeige | 929 ms | < 20 ms | nur mit PPA erreichbar, siehe P-10 |

Daraus folgt die Reihenfolge: erst die Anzeige (Faktor 45 daneben), dann das
Netz (Faktor 3), dann der Rest.

---

## 4. Massnahmen

Status: `offen` | `umgesetzt` | `zu messen` | `verworfen`

### 4.1 Anzeige

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-10 | PPA fuer den Vollbildmodus (Hardware-Drehung) | 929 ms -> < 20 ms | DMA2D-Nutzer, siehe 2.18 | **gemessen: 929 -> 46 ms, 1 -> 6 fps. Absturz nach Minuten, siehe 9.1** |
| P-11 | Fenstermodus direkt zeichnen statt ueber LVGL | zwei Softwarepaesse entfallen | GUI-Layout | offen |
| P-12 | LVGL-Zeichenpuffer in internen SRAM | LVGL rendert lesend/schreibend im Puffer | 76 KB intern | offen |
| P-13 | LVGL-Doppelpuffer (Ping-Pong) | Zeichnen und Flushen ueberlappen | weitere 76 KB | offen |
| P-14 | Panel-Doppelpuffer, Umschalten statt Kopieren | Tearing weg, kein Kopieraufwand | Panel_DSI-Eingriff | offen |

### 4.2 Build und Werkzeugkette

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-01 | `build_type = debug` -> `release` (bzw. `-O2` global) | betrifft M5GFX, LVGL, alles | Debuggen wird unschaerfer | offen |
| P-02 | `CORE_DEBUG_LEVEL` von 3 auf 1 | USB-CDC blockiert UND stuerzt ab | weniger Diagnose | **umgesetzt, siehe 9.6** |
| P-03 | PSRAM-Taktrate pruefen und auf 200 MHz festlegen | bis Faktor 2,5 auf allem PSRAM-gebundenen | keines, wenn Board es traegt | offen |

### 4.3 Speicher

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-20 | Ausgabepuffer an die echte Aufloesung anpassen | 5,4 MB PSRAM frei | Neuanlage bei Wechsel | offen |
| P-21 | `VIDEO_MAX_HEIGHT` 960 -> 720 | 1,8 MB PSRAM je Slot weniger | hochkant-Quellen fallen raus | offen |
| P-22 | Lautsprecher-Haeppchen in internen SRAM | I2S-DMA liest nicht mehr aus PSRAM | 3 KB intern | offen |
| P-23 | Heap-Verlauf mitschreiben (CrashDiag) | Fragmentierung sichtbar machen | keines | offen |

### 4.4 Netzwerk

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-40 | TCP-Empfangsfenster 5760 -> 28720 via `custom_sdkconfig` | Deckel 360 KB/s -> ~1,8 MB/s | interner RAM, langer Build | **blockiert durch pioarduino-Pfadfehler, siehe 9.7** |
| P-41 | Header blockweise lesen statt Byte fuer Byte | ~160 Socketaufrufe je Bild weniger | Parser umbauen | offen |
| P-42 | Netzpfad-Ursache der Einbrueche klaeren | Stabilitaet | - | **groesstenteils erklaert durch 9.7** |

### 4.5 Kamera

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-50 | `quality` als wirksamsten Regler dokumentieren und vorbelegen | Downloadzeit proportional | Bildqualitaet | **bestaetigt, siehe 9.4** |
| P-51 | `speed` auf 0 (maximale Bildrate) pruefen | Kamera liefert oefter | Akku der Kamera | **hinfaellig: nicht die Kamera begrenzt, siehe 9.4** |
| P-52 | Aufloesung an den Anzeigebereich koppeln | kein Skalieren noetig | Stream-Neustart je Wechsel | offen |

### 4.6 Parsing, Strings, Ausgaben

| ID | Massnahme | Erwartung | Risiko | Status |
| --- | --- | --- | --- | --- |
| P-30 | `String` im Videopfad durch feste Puffer ersetzen | keine Heap-Bewegung je Bild | - | offen |
| P-31 | JSON mit Filter parsen statt vollstaendig | weniger Allokation alle 3 s | - | offen |
| P-32 | Profilzeile nur bei Aenderung ausgeben | CDC-Blockaden | weniger Verlauf | offen |
| P-33 | Latenzmodus: aufgestaute Bilder ueberspringen | Verzoegerung statt Bildrate | Bilder fallen weg | umgesetzt |

### 4.7 Bereits erledigt (aus dem Umbau)

| ID | Massnahme | Wirkung |
| --- | --- | --- |
| P-60 | Netz und Dekodierung in getrennte Tasks (Pipeline) | Download und Verarbeitung ueberlappen |
| P-61 | Drei Ein- und Ausgabeslots (Ping-Pong) | kein Warten auf den jeweils anderen |
| P-62 | Decoderpuffer ueber `jpeg_alloc_decoder_mem()` | treiberkonform, Voraussetzung fuer DMA |
| P-63 | `download_buf` (150 KB intern) entfernt | 145 KB interner RAM zurueck |
| P-64 | Engine einmalig statt je Fehlerfall | kein DMA2D-Neuanschluss im Betrieb |

---

## 5. Begruendungen

### 5.1 P-01: Das Projekt wird unoptimiert uebersetzt

In `platformio.ini` steht `build_type = debug`. PlatformIO uebersetzt damit das
**gesamte** Projekt einschliesslich `lib_deps` mit `-Og` statt `-Os`/`-O2`.
Betroffen sind also auch M5GFX und LVGL - und damit genau die Pixelschleifen,
die heute die Zeit verbrauchen.

Dass in `VideoLogic.cpp`, `AudioStreamLogic.cpp` und `SystemLogic.cpp` ein
`#pragma GCC optimize ("O2")` steht, ist der beste Beleg dafuer, dass das
Problem schon einmal aufgefallen ist. Nur wirkt das Pragma ausschliesslich auf
die eigene Uebersetzungseinheit; die teuren Schleifen liegen in den
Bibliotheken.

Das ist der billigste Hebel im ganzen Dokument: eine Zeile, und er wirkt auf
alles gleichzeitig. Gegenargument ist die Fehlersuche - Backtraces werden
unschaerfer. Vorschlag: eine zweite Umgebung `[env:esp32p4_debug]` behalten und
im Alltag die optimierte bauen.

### 5.2 P-02: Die serielle Ausgabe blockiert

`-DCORE_DEBUG_LEVEL=3` schaltet die IDF-Protokollierung auf INFO. WLAN-, lwIP-
und Treibermeldungen laufen damit staendig ueber USB-CDC. Und CDC blockiert den
schreibenden Task, sobald der Puffer voll ist. Die abgeschnittenen Zeilen in den
Protokollen dieser Sitzung (`[Vide`, `[Videm laeuft.`) sind genau das.

Ein blockierender `Serial.print` im Netz- oder Decoder-Task kostet direkt
Bildrate. Stufe 1 (nur Fehler) reicht im Normalbetrieb; die eigenen Meldungen
des Projekts sind davon nicht betroffen.

### 5.3 P-03: PSRAM-Taktrate

M5GFX enthaelt eine ausdrueckliche Pruefung:

```c
#elif CONFIG_SPIRAM_SPEED <= 80
  ESP_LOGE(LIBRARY_NAME, "M5Tab5 need PSRAM SPEED 200MHz");
```

In `platformio.ini` steht dazu nichts, es gilt also die Voreinstellung des
Boards. Laeuft der PSRAM mit 80 statt 200 MHz, sind **alle** PSRAM-gebundenen
Schritte betroffen: der Framebuffer, die Decoderausgabe, der LVGL-Puffer, der
Jitterpuffer. Bei einem speicherbandbreitenbegrenzten Engpass wie der gedrehten
Kopie waere das unmittelbar ein Faktor.

Zu pruefen: ob die Warnung beim Start erscheint (CORE_DEBUG_LEVEL muss dafuer
Fehler zeigen), und was `CONFIG_SPIRAM_SPEED` im verwendeten sdkconfig steht.

### 5.4 P-10 bis P-14: Warum die Anzeige das eigentliche Thema ist

Der Grund steht ausfuehrlich in `video_refactoring.md`, 2.18: Das Panel ist
physisch 720x1280 hochkant, das Querformat entsteht durch Rotation, und M5GFX
nimmt seinen `memcpy`-Weg nur bei Rotation 0. Mit Rotation geht jedes Pixel
einzeln und transponiert in den Framebuffer.

Der Grund fuer die 2-3 MB/s ist nicht der Funktionszeiger je Pixel, sondern das
Zugriffsmuster: zwei im Quellbild benachbarte Pixel liegen im Ziel 1440 Bytes
auseinander (720 x 2 Byte). Jeder 2-Byte-Schreibzugriff zieht eine eigene
64-Byte-Cachezeile nach - Verstaerkungsfaktor rund 32. Die gemessenen 2-3 MB/s
Nutzdaten entsprechen damit 80-100 MB/s echtem PSRAM-Verkehr. Der Bus ist
ausgelastet; selbstgeschriebener Kopiercode aendert daran nichts.

Deshalb ist der PPA (P-10) keine Bequemlichkeit, sondern das einzige Bauteil,
das dieses Zugriffsmuster in Hardware beherrscht.

**P-11** betrifft den Fenstermodus, den der PPA nicht rettet: dort komponiert
LVGL das Bild zuerst in seinen Zeichenpuffer (Softwarepass 1) und flusht es
anschliessend durch dieselbe gedrehte Pipeline (Softwarepass 2). Das Video
direkt in den Fensterbereich zu zeichnen und LVGL nur die Umgebung zu
ueberlassen, spart beide. Kosten: LVGL uebermalt den Bereich bei jedem eigenen
Neuzeichnen, bis das naechste Bild kommt.

**P-12** ist unabhaengig davon und wirkt auf die gesamte Oberflaeche. Der
Zeichenpuffer (`1280 x 30 x 2 Byte = 76 KB`) liegt heute im PSRAM. LVGL liest
und schreibt ihn beim Rendern staendig - das ist der ungeeignetste Ort dafuer.
Interner RAM ist reichlich vorhanden (375 KB frei).

### 5.5 P-20 bis P-23: Speicher

**Die Ausgabepuffer sind fuer den schlechtesten Fall bemessen.** Sie werden auf
`alignUp16(1280) x alignUp16(960) x 2 = 2400 KB` angelegt, dreimal, also
7,2 MB - unabhaengig davon, was die Kamera tatsaechlich liefert. Bei 640x480
werden davon 600 KB genutzt, 1800 KB je Slot liegen brach.

Zwei Wege: `VIDEO_MAX_HEIGHT` auf 720 senken (P-21, spart sofort 1,8 MB, schliesst
aber hochkante Quellen aus), oder die Puffer bei einem Aufloesungswechsel neu
anlegen (P-20, flexibler, aber ein Neuanlegen zur Laufzeit ist genau die Sorte
Operation, die uns bei der Engine Abstuerze beschert hat - also nur im
gestoppten Zustand).

Dringend ist es nicht: 23 MB PSRAM sind frei. Es lohnt sich erst, wenn der
Speicher anderweitig gebraucht wird.

**Ping-Pong ist bereits an drei Stellen umgesetzt:** drei JPEG-Eingabeslots und
drei Ausgabeslots (P-61), zwei Mikrofonpuffer im Wechsel, drei
Lautsprecher-Haeppchen. Was fehlt, ist der Doppelpuffer bei LVGL (P-13) und beim
Panel (P-14).

**P-22:** Die Lautsprecher-Haeppchen (3 x 1 KB) liegen im PSRAM, aus ihnen liest
die I2S-DMA. Das funktioniert, ist aber die unguenstigere Wahl - 3 KB im
internen RAM sind nicht der Rede wert.

**Interner RAM ist das knappe Gut, nicht PSRAM.** Der SDIO-WLAN-Treiber braucht
ihn, und mit `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=0` im Arduino-P4-Build ist
die Reserve klein. Vor dem Entfernen von `download_buf` waren nur 231 KB frei,
jetzt sind es 375 KB. Jede Massnahme, die internen RAM belegt (P-12, P-13,
P-40), muss gegen den WLAN-Treiber abgewogen werden - und genau deshalb gehoert
P-23 dazu, bevor die anderen umgesetzt werden.

### 5.6 P-40 bis P-42: Netzwerk

Der Abstand zwischen 320 KB/s (Geraet) und 791 KB/s (PC, gleiche Kamera) ist zu
gross, um ihn dem Funk anzulasten. Der erste Verdacht ist das TCP-Empfangsfenster:
bei 80 KB je Bild und einem kleinen Fenster bestimmt die Umlaufzeit den
Durchsatz, nicht die Bandbreite. Relevante Schalter sind
`CONFIG_LWIP_TCP_WND_DEFAULT`, `CONFIG_LWIP_TCP_RECVMBOX_SIZE` und die
WLAN-Empfangspuffer; ausserdem `setRxBufferSize()` am Socket. Mit pioarduino
lassen sich diese ueber `custom_sdkconfig` setzen.

**P-41** ist eine klassische Mikrooptimierung mit realem Anteil:
`readBytesUntil()` liest **byteweise** ueber `Stream::timedRead()`. Bei vier
Kopfzeilen zu je rund 40 Zeichen sind das ueber 160 Einzelaufrufe je Bild, jeder
mit `available()`- und `read()`-Aufwand durch den lwIP-Stack. Ein einziger
Blockleseaufruf mit anschliessendem Parsen im RAM ersetzt das. Bei 10 fps sind
das 1600 Aufrufe je Sekunde, die ersatzlos entfallen.

**P-42:** Die Downloadzeit bricht im Betrieb auf 1200-4800 ms ein, bei
unveraenderter Bildgroesse. Das ist kein Engpass, sondern eine Stoerung -
Kandidaten sind der bekannte SDIO-Kanal, das Energiesparverhalten der Kamera
oder Funkstoerungen. Ohne Messung ueber laengere Zeit ist das nicht zu
entscheiden.

### 5.7 P-30 bis P-32: Strings, Parsing, Ausgaben

`VideoLogic.cpp` enthaelt 16 Stellen mit `String`. Kritisch sind die im
Verbindungsaufbau (`camBase()` erzeugt bei jedem Aufruf ein neues Objekt,
`streamUrl()` und `camLogin()` ebenso). Je Bild passiert das nicht, je
Verbindungsaufbau schon - und bei einem instabilen Netz ist das oft. Heap-Churn
ist auf einem Geraet, das monatelang laufen soll, ein Fragmentierungsrisiko.
`BabyCamApi.cpp` ist hier bereits sauber: feste `char`-Puffer und `snprintf`.

`camReadResolution()` parst die vollstaendige Statusantwort, um einen einzigen
Wert zu lesen. ArduinoJson 7 kann mit `DeserializationOption::Filter` nur die
benoetigten Schluessel uebernehmen - weniger Allokation, weniger Rechenzeit
(P-31).

Bei den Schleifen selbst ist wenig zu holen, und das ist erwaehnenswert, weil es
der naheliegendste Ort waere: `jpegLength()` findet den Marker praktisch immer
im ersten Durchlauf, `compactRows()` laeuft bei allen gaengigen Aufloesungen gar
nicht, und die Overlay-Kopie sind 28 `memcpy`-Aufrufe zweimal je Sekunde. Die
Zeit liegt woanders.

---

## 6. Was sich nicht lohnt

Festgehalten, damit es nicht doch jemand versucht:

* **Die Dekodierung optimieren.** 10.4 ms bei 96 fps gegen 109 fps im Benchmark
  des Herstellers. Der Hardware-Decoder ist am Anschlag.
* **Den Framebuffer-Kopiervorgang selbst schreiben.** Der Engpass ist die
  Cachezeilen-Verstaerkung beim transponierten Schreiben, nicht der Overhead von
  M5GFX. Eine kachelweise Drehung (32x32-Bloecke) kaeme vielleicht auf
  20-40 MB/s statt 2-3 - immer noch zu langsam und ein Kern dauerhaft belegt.
* **M5GFX ersetzen.** Die Bibliothek ist an dieser Stelle nicht der Fehler.
* **DMA2D an der IDF vorbei selbst programmieren.** Der Haenger steckt laut
  Beweislage im Silizium der Revision v1.3, nicht nur im Treiber; ausserdem
  verwaltet der JPEG-Treiber dieselben Kanaele weiter.
* **Die Aufloesung senken, um Netzlast zu sparen.** Die Kamera haelt ihr
  Byte-Budget ein (Abschnitt 2.1). Dafuer ist `quality` zustaendig.

---

## 7. Offene Fragen

1. Welche PSRAM-Taktrate laeuft tatsaechlich? (P-03)
2. Wie schnell ist `lv_timer_handler()` im Loop-Task wirklich - und wie oft
   kommt er gar nicht dran, weil `lvgl_port_lock(0)` fehlschlaegt?
3. Woher kommen die Netzeinbrueche auf 4800 ms? (P-42)
4. Wie verhaelt sich der interne Heap ueber Stunden? (P-23)
5. Traegt der PPA den Dauerbetrieb, oder kehren die DMA2D-Haenger zurueck?

---

## 9. Messungen

### 9.1 P-10, PPA im Vollbild (2026-09-20)

Wirkung eindeutig. Vorher/nachher bei 1280x720, Vollbild:

| Groesse | ohne PPA | mit PPA |
| --- | --- | --- |
| Transfer | 929 ms | 41-46 ms |
| Anzeige gesamt | 931 ms | 46 ms |
| Bildrate | 1 fps | 6 fps |
| Netzdurchsatz | 0,7 Mbit/s | 3,0-4,0 Mbit/s |

Bei 640x480 liegt der Transfer bei 27 ms.

Der Engpass ist damit **das Netz**, genau wie in Abschnitt 3 vorhergesagt:
165-195 ms Download je Bild bei 80 KB. Der scheinbare Sprung im Durchsatz von
320 auf 440 KB/s ist kein Netzeffekt - vorher hat der Rueckstau aus der Anzeige
die Messung verfaelscht (Abschnitt 1.1).

Rechnung fuer das 10-fps-Ziel: 100 ms je Bild bei aktuell 440 KB/s bedeutet
44 KB je Bild. Heute sind es 80 KB. Der Regler `quality` ist damit **heute schon**
der Weg zum Ziel (P-50), unabhaengig von allen Netzmassnahmen.

### 9.2 Der DMA2D-Haenger ist zurueck - diesmal mit vollstaendigem Pfad

Nach mehreren Minuten Betrieb, kurz nach einem Aufloesungswechsel:

```
Interrupt wdt timeout on CPU1
dma2d_hal_tx_reset_channel        (hal/dma2d_hal.c:18)
 <- acquire_free_channels_for_trans (esp_hw_support/dma/dma2d.c:147)
 <- dma2d_enqueue                   (dma2d.c:904)
 <- ppa_dma2d_enqueue / ppa_do_operation (ppa_core.c:338/438)
 <- ppa_do_scale_rotate_mirror      (ppa_srm.c:262/295)
 <- lgfx::PPABase::exec             (LGFX_PPA.hpp:241)
 <- lgfx::PPASrm::pushImageSRM      (LGFX_PPA.cpp:402)
 <- renderFrame / videoDecodeTask   (VideoLogic.cpp:634)
```

Das ist derselbe Haenger wie in 2.7/2.8 von `video_refactoring.md`, aber zum
ersten Mal mit lueckenlosem Aufrufpfad aus **unserem** Code. Damit ist belegt:

* Es ist der **PPA-Pfad**, nicht der JPEG-Pfad (esp-idf #19096, #18999).
* Die Stelle ist die **Kanalbeschaffung** (`acquire_free_channels_for_trans`),
  nicht die Uebertragung. Der Kanal wird vor der Benutzung zurueckgesetzt, und
  auf Revision v1.3 quittiert `reset_avail` diesen Reset nie (#19108).
* Der Reset laeuft in einem kritischen Abschnitt mit gesperrten Interrupts -
  deshalb der Interrupt-Watchdog statt eines einfachen Fehlers.

Die Serialisierung von Decoder und PPA im selben Task hilft hier **nicht**: der
Haenger sitzt in der Kanalvergabe, nicht in einer Kollision zweier laufender
Uebertragungen.

**Kandidaten fuer die Entschaerfung** (noch keiner umgesetzt):

| ID | Ansatz | Bewertung |
| --- | --- | --- |
| P-15 | `-Wl,--wrap=dma2d_hal_{tx,rx}_reset_channel`, eigene Fassung mit begrenzter Warteschleife | **umgesetzt**, siehe 9.3 |
| P-16 | PPA nur im Vollbild verwenden, Moduswechsel vermeiden | Der Absturz kam kurz nach einem Aufloesungswechsel - moeglicherweise loest der Wechsel der SRM-Parameter eine Neuvergabe aus. Zu pruefen. |
| P-17 | `VIDEO_USE_PPA 0` | Rueckfallebene. Kostet den gesamten Gewinn aus 9.1. |

P-15 ist der einzige Ansatz, der die Ursache trifft, statt ihr auszuweichen.

### 9.3 P-15: Der begrenzte Kanal-Reset (2026-09-20)

Die Voraussetzungen wurden vor dem Bau geprueft, nicht angenommen:

```
libhal.a:dma2d_hal.c.obj:          T dma2d_hal_tx_reset_channel   (Definition)
libesp_hw_support.a:dma2d.c.obj:   U dma2d_hal_tx_reset_channel   (Referenz)
```

Definition und Aufruf liegen in **verschiedenen** Archiven, die Referenz ist
undefiniert - genau die Konstellation, in der `--wrap` greift. (Waere der
Aufruf innerhalb derselben Uebersetzungseinheit erfolgt, haette der Linker
nichts umzuleiten gehabt.)

Die Original-HAL sieht laut Header und Disassemblat so aus:

```c
void dma2d_hal_tx_reset_channel(dma2d_hal_context_t *hal, uint32_t ch)
{
    while (!dma2d_ll_tx_is_reset_avail(hal->dev, ch)) {}   // Zeile 18
    dma2d_ll_tx_reset_channel(hal->dev, ch);
}
```

`src/system/Dma2dGuard.c` ersetzt genau das durch dieselbe Abfolge mit einer
Frist von rund 1 ms (400000 CPU-Zyklen). Nach Fristablauf wird der Reset
trotzdem ausgeloest und ein Zaehler erhoeht.

Nachweis am fertigen Abbild:

```
400ba306 T __wrap_dma2d_hal_tx_reset_channel
400ba36e T __wrap_dma2d_hal_rx_reset_channel

40175f38: jal 400ba306 <__wrap_dma2d_hal_tx_reset_channel>
4017603e: jal 400ba36e <__wrap_dma2d_hal_rx_reset_channel>
```

Die urspruengliche Fassung ist nicht mehr im Abbild - der Linker hat das
Archivmitglied verworfen, weil es niemand mehr referenziert. Die Umleitung ist
damit vollstaendig, auch fuer den Hardware-JPEG-Decoder.

**Was der Eingriff nicht ist:** eine Behebung. Der Fehler sitzt im Silizium,
der Kanal ist nach einem abgelaufenen Reset moeglicherweise in einem
undefinierten Zustand. Erwartbar ist dann ein fehlerhaftes oder verworfenes
Bild. Das ist die bewusste Abwaegung gegen einen Neustart des Babyphones im
laufenden Betrieb.

**Messgroesse:** Die Profilzeile fuehrt jetzt `DMA2D-Rettungen` mit. Bleibt der
Wert auf null, hat der Schutz nie gegriffen und kostet nichts. Jeder
Zaehlerschritt entspricht einem Absturz, der nicht stattgefunden hat.

### 9.4 Die Bildrate ist bandbreitenbegrenzt (2026-09-20)

Offene Frage aus 9.1 war: begrenzt das Netz, oder liefert die Kamera einfach
nicht mehr Bilder? Die Messung ueber verschiedene Qualitaetsstufen bei
durchgehend 320x240 entscheidet es:

| KB/Bild | Download | Durchsatz |
| --- | --- | --- |
| 80 | 230.8 ms | 347 KB/s |
| 85 | 286.7 ms | 296 KB/s |
| 117 | 297.6 ms | 393 KB/s |
| 160 | 428.8 ms | 373 KB/s |
| 160 | 471.5 ms | 339 KB/s |
| 160 | 508.4 ms | 315 KB/s |

Die Downloadzeit waechst mit der Bildgroesse, der Durchsatz bleibt bei
300-400 KB/s. **Es ist die Bandbreite, nicht die Bildrate der Kamera.** Damit
ist `quality` der direkte Regler fuer die Bildrate - P-50 bestaetigt.

Zwei Folgerungen:

* **Die Qualitaetseinstellung steht viel zu hoch.** 160 KB bei 320x240 sind
  ueber 2 Byte je Pixel - mehr als unkomprimiertes RGB565. Fuer eine
  Babyphone-Uebersicht ist das sinnlos. Bei 40 KB je Bild und 400 KB/s waeren
  100 ms je Bild erreicht, also die angestrebten 10 fps.
* **Der Netzstapel hat weiter Luft.** 300-400 KB/s stehen gegen 791 KB/s, die
  dieselbe Kamera am PC liefert (2.13). P-40 bleibt damit offen und ist nach
  `quality` der naechste Hebel - wert ist er grob den Faktor 2.

Nebenbefund: Der Durchsatz schwankt zwischen 296 und 500 KB/s bei sonst
gleichen Bedingungen. Das ist P-42 und weiterhin ungeklaert.

### 9.5 Entschieden: es ist die Bandbreite (2026-09-20)

Die Trennung von `Download` in Warten und Empfangen beendet die Debatte aus
9.4. Werte aus dem ungestoerten Betrieb:

| Aufloesung | Warten | Empfang |
| --- | --- | --- |
| 640x480 | 1.0 ms | 194.1 ms |
| 640x480 | 0.4 ms | 181.2 ms |
| 1280x720 | 0.6 ms | 157.8 ms |
| 1280x720 | 0.3 ms | 150.7 ms |
| 1280x720 | 0.3 ms | 143.8 ms |

**Warten liegt bei 0,3 bis 1 ms.** Die Kamera hat das naechste Bild also
praktisch sofort bereit - sie ist nicht die Grenze. Die gesamte Zeit steckt im
Empfangen der Bytes: 80 KB in rund 150 ms sind 533 KB/s.

Damit ist P-51 endgueltig hinfaellig und **P-40 der einzige verbliebene Hebel**
fuer die Bildrate - neben `quality`, das die zu uebertragende Menge senkt. Zum
Vergleich: derselbe Stream erreicht am PC 791 KB/s (2.13).

Die Ausreisser mit Warten von 44 bis 507 ms fallen genau mit den
`getStreamPtr: not connected`-Meldungen zusammen, gehoeren also zu P-42
(Stoerung), nicht zum Normalbetrieb.

### 9.6 Neuer Absturzpfad: der USB-CDC-Interrupt

```
Guru Meditation Error: Core 1 panic'ed (Store access fault)
MEPC  : hw_cdc_isr_handler (HWCDC.cpp:100 / usb_serial_jtag_ll.h:142)
MTVAL : 0x500D2000        <- Registerblock des USB-Serial-JTAG
```

Danach eine Kette von Watchdog-Neustarts (Laufzeiten 4,2 s / 11,4 s / 55 s).
Der Absturz trat auf, waehrend der serielle Monitor die Verbindung verlor und
neu aufbaute ("ClearCommError failed", "Reconnecting to COM13") - und die
Ausgabe mitten im Wort abbrach ("[USB] St", "G").

Das ist **nicht** der DMA2D-Haenger: anderer Fehlertyp (Store access fault
statt Watchdog im kritischen Abschnitt), andere Stelle, und die
DMA2D-Rettungen standen zu dem Zeitpunkt auf 0.

Mechanismus: `ARDUINO_USB_CDC_ON_BOOT=1` bedeutet, dass `Serial` der USB-CDC
ist. Wird die Verbindung getrennt, waehrend der Interrupt-Handler in die
Register des Peripherals schreibt, greift er auf abgeschaltete Hardware zu.
Das ist ein Fehler in arduino-esp32, aber die Auftrittswahrscheinlichkeit
haengt direkt daran, wie viel geschrieben wird.

Gegenmassnahme (P-02, jetzt umgesetzt): `CORE_DEBUG_LEVEL` von 3 auf 1. Das
entfernt die IDF-Meldungen, die im Protokoll die grosse Mehrheit ausmachen -
insbesondere die `getStreamPtr: not connected`-Salven mit etwa einer Zeile je
Sekunde waehrend jeder Netzstoerung. Die eigenen Meldungen des Projekts
bleiben unberuehrt.

Das ist keine Behebung des HWCDC-Fehlers, sondern eine Verkleinerung des
Zeitfensters. Bleibt der Absturz bestehen, waere der naechste Schritt, die
eigenen Ausgaben im Betrieb zu unterdruecken, sobald kein Monitor angeschlossen
ist (`if (Serial)`).

### 9.7 P-40 aufgeklaert: das TCP-Empfangsfenster (2026-09-20)

Die Suche nach dem Empfangs-Engpass endet in einer einzigen Zeile des
sdkconfig:

```
CONFIG_LWIP_TCP_WND_DEFAULT=5760     (= 4 x MSS von 1436)
CONFIG_LWIP_TCP_RECVMBOX_SIZE=6
```

Der Durchsatz einer TCP-Verbindung ist durch **Fenster / Umlaufzeit** gedeckelt.
Mit 5760 Bytes:

| Umlaufzeit | Deckel | entspricht |
| --- | --- | --- |
| 11 ms | 524 KB/s | unsere besten Messungen |
| 14 ms | 411 KB/s | Mittelfeld |
| 16 ms | 360 KB/s | die langsamen Laeufe |
| 19 ms | 303 KB/s | der schlechteste Messwert |

**Das erklaert das gesamte beobachtete Band von 296 bis 560 KB/s** - allein aus
schwankender Funklaufzeit, ohne jede weitere Annahme. Damit ist auch P-42
weitgehend erledigt: die "ungeklaerten Einbrueche" sind keine Stoerung, sondern
dieselbe Formel bei schlechterer Laufzeit.

Und es erklaert etwas Unangenehmeres: **keine Aenderung am Programm konnte je
etwas bewirken.** Der Deckel lag nie im Code.

#### Zwei Irrtuemer auf dem Weg dorthin, beide korrigiert

**Der 1436-Byte-Lesepuffer war es nicht.** `NetworkClientRxBuffer::read()`
schleift bei groesseren Anforderungen einfach mehrfach nach. Das sind rund 57
`recv()`-Aufrufe je Bild, zusammen etwa 1 ms - gegen 230 ms Messwert
bedeutungslos. Die Vermutung stand kurzzeitig im Code-Kommentar und ist dort
richtiggestellt.

**`SO_RCVBUF` ist nicht das Fenster.** lwIP setzt `pcb->rcv_wnd` beim Anlegen
der Verbindung aus `TCP_WND`; kein Socket-Aufruf kommt daran. `SO_RCVBUF`
steuert nur die Zwischenlagerung der netconn-Schicht. Der Aufruf bleibt
trotzdem stehen, damit diese Schicht nicht zum neuen Engpass wird, sobald das
Fenster groesser ist.

#### Massnahme

`custom_sdkconfig` in platformio.ini hebt das Fenster auf 28720 Bytes
(20 Segmente) und die Mailbox auf 24. Neuer Deckel bei 16 ms Umlaufzeit: rund
1,8 MB/s. Der Engpass wandert damit ins Funknetz, wo er hingehoert.

**Kosten:** Die Einstellung laesst die Bibliotheken aus der Quelle neu bauen -
der naechste Build dauert erheblich laenger als gewohnt. Ausserdem belegt ein
groesseres Fenster mehr internen RAM je Verbindung (Video, MQTT, HA-Websocket).
375 KB sind frei; P-23 (Heap-Verlauf mitschreiben) wird damit von "nuetzlich"
zu "noetig".

#### Der Build scheitert - an pioarduino, nicht an den Werten

Das Ersetzen funktioniert (alle fuenf Zeilen werden korrekt gemeldet), danach:

```
FileNotFoundError: ...\platforms\espressif32\builder\build_lib
  espidf.py:418  shutil.copytree(LIB_SOURCE, ...)
```

In `builder/frameworks/espidf.py` steht:

```python
LIB_SOURCE = os.path.join(platforms_dir, "espressif32", "builder", "build_lib")
```

Der Verzeichnisname ist fest verdrahtet. Dieses Projekt bindet die Plattform
aber als Git-URL, und PlatformIO legt sie dann unter
`espressif32@src-<hash>` ab - der erwartete Pfad existiert nicht. Mit der
Registry-Fassung traete der Fehler nicht auf.

Drei Wege, in der Reihenfolge des Risikos:

| Weg | Bewertung |
| --- | --- |
| `build_lib` einmalig an den erwarteten Pfad kopieren | Eingriff ausserhalb des Projekts, sofort rueckgaengig zu machen, ruehrt die Versionsbindung nicht an. Empfohlen. |
| `platform =` auf `pioarduino/espressif32@54.3.21` umstellen | Eine Zeile, aber es aendert die Herkunft der Plattform. Bei der Vorgeschichte mit Displayflackern nichts, was nebenbei passiert. |
| Beim heutigen Fenster bleiben | Kostet den Faktor 5 bei der Bildrate. |

`custom_sdkconfig` ist bis zur Entscheidung auskommentiert, damit das Projekt
baubar bleibt. Die Anleitung steht als Kommentar direkt daneben.

---

## 8. Aenderungsprotokoll

| Datum | ID | Aenderung | Wirkung |
| --- | --- | --- | --- |
| 2026-09-20 | - | Dokument angelegt, Messbasis und Massnahmen erfasst | - |
| 2026-09-20 | P-10 | PPA gemessen: Transfer 929 -> 46 ms, 1 -> 6 fps | Engpass ist jetzt das Netz |
| 2026-09-20 | P-15..17 | DMA2D-Haenger im PPA-Pfad belegt, Entschaerfung offen | siehe 9.2 |
| 2026-09-20 | P-15 | `--wrap` auf den DMA2D-Kanal-Reset, Umleitung am Abbild nachgewiesen | siehe 9.3 |
| 2026-09-20 | P-50 | Qualitaetsreihe gemessen: Download proportional zur Bildgroesse | Bandbreite begrenzt, nicht die Kamera |
| 2026-09-20 | P-40/P-51 | Warten vs. Empfang getrennt gemessen: Warten 0,3-1 ms | Bandbreite bestaetigt, Kamera entlastet |
| 2026-09-20 | P-02 | `CORE_DEBUG_LEVEL` 3 -> 1 nach Absturz im USB-CDC-Interrupt | siehe 9.6 |
| 2026-09-20 | P-40 | TCP-Fenster als Ursache identifiziert (5760 B / Umlaufzeit) | erklaert das gesamte Messband, siehe 9.7 |
