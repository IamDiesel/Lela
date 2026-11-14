# Lela OS 2

Lela OS ist eine auf FreeRTOS und LVGL basierende Smart-Home-Betriebssystemumgebung für ESP32-Mikrocontroller (spezifisch optimiert für das M5Stack Tab5 / ESP32-P4). Es vereint eine direkte Home Assistant WebSocket-Integration, ein dynamisches On-Device Dashboard-Editing, Multimedia-Streaming (Babyphone-Modus) und spezialisierte BLE-Sensor-Auswertungen.

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/01794d18-cf56-4e82-b485-18b429af5852" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/0744ee97-ae6c-481b-a7cc-810118674257" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/1e9dc37d-7b92-448b-aa02-4b974506b825" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/76344272-6600-479e-a90e-29b8496e24bb" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/351dd6f1-f55c-44d4-82c0-ea6ff94155e8" />

## Inhaltsverzeichnis
1. [Systemarchitektur & Features](#1-systemarchitektur--features)
2. [Hardware-Voraussetzungen](#2-hardware-voraussetzungen)
3. [Ordnerstruktur](#3-ordnerstruktur)
4. [Software & Abhängigkeiten](#4-software--abhängigkeiten)
5. [Installation & Build-Prozess](#5-installation--build-prozess)
6. [Ersteinrichtung (Web-Setup)](#6-ersteinrichtung-web-setup)
7. [Eigene Icons generieren (Icon Studio)](#7-eigene-icons-generieren-icon-studio)
8. [Lizenz](#8-lizenz)

---

## 1. Systemarchitektur & Features

Das System ist in dedizierte Logik- und View-Klassen unterteilt, die asynchron auf verschiedenen CPU-Kernen ausgeführt werden. 

### 1.1 Netzwerk & System-Infrastruktur
* **WLAN-Management:** Automatische Verbindungs- und Reconnect-Logik. Fallback in den Access-Point-Modus (`Lela-Setup`), falls keine Zugangsdaten vorliegen.
* **WLAN-Start (`WifiConnect`):**
  * **Ablauf (gemessen):** ~2,9 s Hochfahren des WLAN-Chips (ESP32-C6 über ESP-Hosted), ~3 s Suchen und Anmelden, 1,5–2,5 s DHCP.
  * **Beschleunigung:** Optional feste IP – DHCP entfällt, gemessen 7,9 s statt 9–10 s bis zum Dashboard. Ein Kanal-Hinweis (mit und ohne BSSID) brachte nichts: Das Anmelden dauert so oder so 3,0 s; er wurde wieder entfernt.
  * **Protokoll:** Jedes WLAN-Ereignis mit Zeitstempel und Anmeldeverfahren (`[WLAN] +x ms …`).
* **Bootscreen:** Wartet nur auf das WLAN (Home Assistant verbindet sich erst beim Öffnen der HA-App). Die Katze folgt dem Ladebalken. Sobald das WLAN steht, läuft sie im gleichen Tempo die Reststrecke bis zum Ende und setzt sich, auch wenn der Start sehr schnell ist.
* **Webserver & Captive Portal:** Integriertes HTML-Setup-Portal zur Konfiguration von WLAN, MQTT, Home Assistant IP und Kamera-Streams.
* **Zeitsynchronisation:** Automatischer NTP-Abgleich (CET/CEST) für korrekte Graphen- und Systemzeiten.
* **Power-Management:** Live-Auslesen von Akkustand und Ladezustand über die M5Unified API.
* **Task-Management:** Parallele Verarbeitung von WebSockets, GUI-Rendering und Audio-Streaming via FreeRTOS.
* **Live-Screenshot Engine:** Web-Endpunkt zum Auslesen des RAM-Framebuffers und Download als unkomprimiertes `.BMP`-Bild.
* **TLS im PSRAM (`TlsMemory`):** mbedTLS legt seine Puffer (je Verbindung mehrere 10 KB) im PSRAM statt im knappen internen RAM an. Das interne RAM bleibt für den SDIO-WLAN-Treiber frei; HTTPS (Kartenkacheln) und Video laufen so parallel.
* **Absturz-Diagnose (`CrashDiag`):** Unterscheidet Kalt- und Warmstart, zählt Abstürze seit dem letzten Stromlos-Machen und legt einen Bericht auf MQTT (`lela/diag/boot`) ab. Nach einem Absturz kann die Audio-Ausgabe stumm bleiben, bis das Gerät einmal stromlos war – die Kopfleiste zeigt dann einen Hinweis.
* **Speicher-Diagnose (`HeapMonitor`):** `[Heap]`-Zeile im Log mit Bildschirm und Babyphone-Zustand (alle 60 s, mit `HEAP_PROFILE` in `DebugFlags.h` alle 10 s). Entscheidend ist der größte DMA-fähige Block (WLAN-Treiber).

### 1.2 Home Assistant Integration
* **WebSocket-Verbindung:** Persistenter, bidirektionaler Echtzeitkanal zu Home Assistant (ohne REST-Polling). Ziel ist die HA-IP/Port aus dem Web-Setup; `secrets.h` dient als Rückfall (leeres Setup oder wiederholt nicht erreichbar). Lebenszeichen per Ping, automatischer Neuaufbau mit wachsender Wartezeit, lange Pausen bei ungültigem Token.
* **Live-Zustände (`HaEntitySync`):** `subscribe_entities` mit Filter auf die platzierten Entitäten (Widgets, Ordnerkinder, Bedingungen) – in Blöcken zu 16, beim Start vollständiger Zustand inkl. Attribute, danach nur Änderungen. Nur die von der Oberfläche benötigten Attribute werden geparst. Vor jedem Neuaufbau werden die alten Abos abgemeldet.
* **Speichereffizienz (`HaEntityCache`):** Je Entität ein JSON-Abbild im PSRAM; das knappe interne RAM bleibt für WLAN (ESP-Hosted/SDIO) frei.
* **Schneller Start der HA-App:**
  * **Letzter Stand:** Beim Verlassen der App bleiben die Zustände erhalten und werden im Flash gesichert (`/ha_states.json`, nur bei Änderung). Beim Booten werden sie geladen. Die App zeichnet so sofort in den richtigen Farben; die Live-Daten übermalen danach nur, was sich geändert hat.
  * **Schutz vor veralteten Werten:** Bedienungen, die auf einem Wert aufbauen (Thermostat +/− und Modus), warten, bis die Entität wieder live ist.
  * **Aufbau:** Nur der aktive Tab wird sofort angelegt, die übrigen stückweise im Hintergrund (20 ms je Durchlauf), Nachbartabs zuerst. Bei Tabwechsel oder Bearbeiten wird ein Tab sofort vollständig angelegt. Der Positionsverlauf der Karte lädt erst danach.
  * **Nur Geändertes zeichnen:** Jede Entität hat einen Änderungszähler; ein Widget wird nur aktualisiert, wenn sich seine Entitäten geändert haben (Ordner: ihre Kinder, Karte: ihre Tracker). Ordner setzen ihre Sichtbarkeit nur bei Änderung. So bleiben die Bilder klein und Eingaben gehen nicht verloren.
  * **Messzeilen:** `[Boot]`, `[HA-Start]` und mit `PERF_PROFILE` in `DebugFlags.h` auch `[Perf]` (Zeichenzeit großer Bildaufbauten).
* **Entitätsliste (`HaEntityCatalog`):** Für Suche und Hinzufügen wird bei Bedarf eine kompakte Liste aller wählbaren Entitäten per einmaligem Template geladen (nicht beim Start).
* **Dienst-Steuerung:** Steuerung von Schaltern, Tasten, Lichtern (inkl. RGB/W-Farbwahl und Helligkeit), Mediaplayern sowie **Saugrobotern** (inkl. Saugstufen-Auswahl und Lokalisierung).
* **Rückmeldung beim Antippen:** Schalter und Lichter zeigen nach dem Antippen einen Zwischenstand (Symbol halbdurchsichtig), bis HA den neuen Zustand meldet. Andere Meldungen in der Zwischenzeit ändern nichts. Kommt binnen 5 s keine Änderung (Misserfolg), erscheint wieder der tatsächliche Zustand. Aktionstasten (Skripte, Szenen, Buttons) quittieren 1,5 s bzw. bis zur Zustandsänderung.
* **Lovelace-Parser:** Import bestehender Home Assistant Dashboards (Lovelace JSON) in native C++ LVGL-Widgets. Karten (`type: map`) werden mit `entities`, `default_zoom` und `hours_to_show` übernommen (siehe 1.7).

| HA Entitäts-Typ (Domain) | Status | Verwendete C++ Klasse | Kurze Beschreibung & UI-Verhalten |
| --- | --- | --- | --- |
| `light`, `switch`, **`input_boolean`** | ✅ **Fertig** | `HALightWidget` | **Der Standard-Schalter (Stateful).** Ein einfaches 1x1 Widget. Zeigt einen dauerhaften Status an. **Besonders `input_boolean` dient oft als virtueller Schalter, dessen Statuswechsel im Hintergrund komplexe HA-Automationen auslöst.** Kann zudem via `tap_action` überschrieben werden. |
| `button`, `input_button`, `script`, `scene`, `automation` | ✅ **Fertig** | `HAActionWidget` | **Die Aktionstaste (Stateless).** 1x1 Widget. Hat keinen dauerhaften On/Off-Status. Feuert bei Klick sofort einen Service-Call ab, **um Skripte, Szenen oder Automationen direkt einmalig zu triggern.** |
| `sensor`, `binary_sensor`, `weather`, `person`, `device_tracker` | ✅ **Fertig** | `HASensorWidget` | **Die Informationsanzeige.** 1x1 Widget. Rein informativ (read-only). Kann bei numerischen Werten zusätzlich ein historisches Verlaufsdiagramm (`show_chart`) einblenden. |
| `select`, `input_select` | ✅ **Fertig** | `HASelectWidget` | **Das Dropdown-Menü.** 1x1 Widget. Zeigt den aktuellen Text an. Bei Klick klappt ein LVGL-Menü mit den verfügbaren Optionen auf. |
| `number`, `input_number` | ✅ **Fertig** | `HANumberWidget` | **Der Schieberegler.** 1x1 Widget. Beinhaltet einen Slider, um numerische Werte (z.B. 0-100%) inklusive Kommastellen sanft und stufenlos einzustellen. |
| `text`, `input_text` | ✅ **Fertig** | `HATextWidget` | **Die Texteingabe.** 2x1 Widget (doppelte Breite). Bei Klick öffnet sich ein Fullscreen-Overlay mit virtueller Bildschirmtastatur zur Eingabe von Zeichenketten. |
| `cover` | ✅ **Fertig** | `HACoverWidget` | **Die Rollladensteuerung.** 2x1 Widget. Bietet drei Schnell-Tasten (Auf, Stop, Ab) sowie einen Slider für die exakte Positionierung in Prozent. |
| `climate` | ✅ **Fertig** | `HAClimateWidget` | **Das Thermostat.** 2x1 Widget. Zeigt Ist- und Soll-Temperatur groß an. Bietet Plus/Minus-Tasten zur Einstellung und einen Button zum Durchschalten des Modus (Heizen, Kühlen, Auto). |
| `vacuum` | ✅ **Fertig** | `HAVacuumWidget` | **Die Staubsaugersteuerung.** 2x1 Widget. Extra Tasten für Start, Stop, Pause und Rückkehr zur Ladestation (Dock). |
| `media_player` | ✅ **Fertig** | `HAMediaWidget` | **Die Mediensteuerung.** 2x1 Widget. Play/Pause-Funktion, Lautstärke-Slider und ggf. Tasten für Vor/Zurück oder Quellenauswahl. |
| `device_tracker`, `person`, `zone` (Karte) | ✅ **Fertig** | `HAMapWidget` | **Die Karte.** Frei skalierbar (Standard 520x340). OpenStreetMap-Karte mit Trackern, Zonen und Wegverlauf; Antippen öffnet das Vollbild. Details siehe 1.7. |
| `fan` | ❌ *Offen* | *Fehlt noch* | **Ventilatoren.** Müsste idealerweise als 2x1 Widget gebaut werden (Kombination aus On/Off, Geschwindigkeits-Slider und Schwenk-Button). |
| `lock` | ❌ *Offen* | *Fehlt noch* | **Smarte Türschlösser.** Fällt aktuell in den Standard-Topf. Bräuchte ein eigenes Widget mit einer Sicherheitsabfrage ("Wirklich öffnen?"), um versehentliche Klicks zu vermeiden. |
| `alarm_control_panel` | ❌ *Offen* | *Fehlt noch* | **Alarmanlagen.** Würde ein Fullscreen-Overlay mit einem Ziffernblock (PIN-Pad) erfordern, um die Anlage (un)scharf zu schalten. |
| `input_datetime`, `time`, `date` | ❌ *Offen* | *Fehlt noch* | **Zeit & Datum.** Erfordert ein Overlay mit LVGL-Rollern (wie bei einem Zahlenschloss), um Uhrzeiten oder Daten exakt einzustellen. |
| `camera`, `image` | ❌ *Offen* | *Zu hoher RAM-Bedarf* | **Kamera-Feeds.** Für einen ESP32 extrem schwer umzusetzen, da Live-JPEG-Dekodierung den Arbeitsspeicher massiv belastet. MJPEG Stream inkl. Audio kann über Babymonitor App eingebunden werden |
| `update`, `water_heater`, `siren`, `valve`, `remote` | ❌ *Offen* | *Spezialfälle* | **Nischen-Entitäten.** Werden aktuell meist vom Fallback (`HALightWidget`) aufgefangen oder ignoriert, wenn keine `tap_action` definiert ist. |

---



### 1.3 On-Device Dashboard-Editor
* **Layout-Engine:** Freies Platzieren von Widgets per Drag & Drop mit 10px-Snap-to-Grid (begrenzt auf die Tab-Breite).
* **Ordner und Bedingungen:** Ordner-Widgets zeigen das erste Kind, dessen Bedingungen erfüllt sind; Sichtbarkeitsbedingungen (AND/OR, `==` `!=` `<` `>` `<=` `>=`) wirken auf jedes Widget.
* **Datensicherheit:** Konfiguration wird atomar gespeichert (temporäre Datei + Umbenennen); eine defekte Datei wird als `/ha_config.bad` gesichert statt überschrieben. Der Web-Import prüft die Datei vollständig, bevor er das Layout ersetzt.
* **Stabile Rendering-Pipeline:** Sicheres Z-Order-Management beim Widget-Editing.
* **Widget-Konfiguration:** Einstellungen für Margin, Alignment, Farben und Icons direkt über Touch-Eingaben und On-Screen-Keyboard. Optionale Skalierung für dynamische Sub-Elemente (z. B. Vacuum-Buttons).
* **Backup/Restore (PSRAM optimiert):** JSON-basierter Export und Import des Layouts über das lokale Web-Portal. Das Speichern großer Konfigurationen wird via Custom Allocator in den 32MB PSRAM ausgelagert, um den internen RAM zu schonen (Out-Of-Memory Schutz).

### 1.4 Datenvisualisierung (Sensor-Charts)
* **Dynamische Graphen:** Rendering von Sensordaten als Area-Charts über LVGL.
* **Smartes UI-Design:** Dynamische LVGL-`recolor`-Einheiten (dezent und kontrastarm) sowie semi-transparente Wasserzeichen-Icons für maximale Lesbarkeit der Sensorwerte in Kachel-Ansichten.
* **Historisierung:** Lokales Array für 50 Messwerte inkl. NTP-Zeitstempel (HH:MM) für die X-Achse.
* **Auto-Scaling:** Skalierung der Y-Achse aus dem sichtbaren Verlauf (wird auch wieder kleiner) mit dynamischem Padding; Werteskalierung passt sich an, sodass auch große Werte (W, lx, ppm) ohne Überlauf dargestellt werden. Manuelle Fixierung von Wertebereichen möglich.
* **Hold-Logik:** Interpolation fehlender Datenpunkte in festen Zeitintervallen (10s) zur Wahrung der Zeitachsen-Integrität.

### 1.5 Audio & Video (Babyphone-Modus)
* **Kamera-Quelle:** MJPEG-Video über HTTP und unkomprimiertes PCM-Audio (44,1 kHz, 16 Bit, mono) über UDP. Als Quelle kann beispielsweise ein altes Smartphone mit der App [BabyCam](https://play.google.com/store/apps/details?id=com.arjonasoftware.babycam) dienen.
* **Babyphone-Dienst (`BabyService`):** Video, Ton und Kamerasitzung hängen nicht am Bildschirm, sondern an einem Modus: **Aus / Audio / Video / Video+Audio**. Ein Bildschirmwechsel ändert den Modus nicht. Nach einem Neustart ist der Modus „Aus“.
  * Umschalten über die QuickSettings (Zeile „Babyphone“), in der Baby-App (Tippen aufs Bild = Video+Audio an/aus, Audio-Knopf = nur Ton) oder per MQTT.
  * Die Kopfleiste zeigt den laufenden Modus als Symbol rechts neben dem Akku.
  * Push-to-Talk (Gegensprechen) gibt es nur in der Baby-App; danach läuft der Babyton von selbst weiter.
  * Bleibt der Babyton 10 s aus, wird der Wiedergabe-Befehl an die Kamera erneuert (z. B. nach einem WLAN-Abbruch).
* **Video-Overlay (Bild im Bild):** Läuft Video, erscheint es auf Dashboard, Home Assistant und CatMat als verschiebbares Fenster.
  * **Kurz tippen** öffnet die Baby-App, **halten oder ziehen** verschiebt das Fenster, der **Griff unten rechts** ändert die Größe (Breite 200–800 px, Seitenverhältnis bleibt). Lage und Größe werden gespeichert.
  * Es blendet sich automatisch aus bei HA-Dialogen, Tastatur, Bearbeiten-Modus, aufgeklappten Auswahllisten, QuickSettings, Alarm-Popups, abgedunkeltem Bildschirm und Screenshot-Modus. In den Einstellungen wird nicht dekodiert, die Verbindung zur Kamera bleibt aber stehen.
  * Technik: LVGL zeichnet nur Rahmen und Statuszeile; das Bild skaliert der Hardware-Beschleuniger (PPA) direkt in den Bildspeicher, LVGL spart diese Fläche beim Zeichnen aus (`Lvgl_SetReservedArea`).
  * Einstellungen (Baby-App → Zahnrad): Overlay-Bildrate (1–15 fps, Standard 5), Overlay-Auflösung (Standard: kleinste, die die Kamera meldet) und „Overlay-Lage zurücksetzen“.
* **Auflösung je Abnehmer:** Die Baby-App nutzt die eingestellte Kamera-Auflösung, das Overlay seine eigene. Ein Wechsel zwischen beiden startet den Kamerastream neu (~3 s).
* **Latenz-Modus:** Stauen sich Bilder, wird nur das neueste dekodiert – die Anzeige hängt der Realität nicht hinterher.
* **Sicherheitsventil:** Wird der interne DMA-fähige Speicher knapp (größter Block < 12 KB), pausiert nur das Video („Video pausiert – Speicher knapp“); Ton und Alarme laufen weiter. Hintergrund: Der WLAN-Treiber braucht dort bis zu 7,5 KB am Stück, sonst startet das Gerät neu.
* **Alarmierung:** Baby-Alarm per MQTT weckt das Display, öffnet die Baby-App und startet Video+Audio.

### 1.6 BLE Sensorik (Katzen-Matte & Tracker)
* **BLE-Scanner:** Integriertes GUI zum Suchen und Binden von Bluetooth-MAC-Adressen.
* **Druck-Auswertung:** Gleitende Durchschnittsberechnung (Sliding Window) mit Tara-Funktion zur Drift-Kompensation des Matten-Sensors.
* **Lokaler Alarm:** Optische und akustische Warnungen (Custom I2S Töne) bei Limit-Überschreitungen oder Verbindungsabbruch.
* **Alarme und Mixer:** Lautstärken (Master, UI, Alarm, Baby) im Mixer der QuickSettings. Das Quittieren eines Alarms (erster Druck) ist davon getrennt: Es schaltet Ton und Popup des Alarms ab, nicht den Mixer-Kanal; der zweite Druck setzt den Alarm zurück. Läuft ein Alarm, der auf dem aktuellen Bildschirm keine eigene Anzeige hat, erscheint ein bildschirmfüllendes Popup (auch auf dem HA-Bildschirm).

### 1.7 Karten-Widget (Tracker, Zonen, Wegverlauf)
Zeigt Entitäten mit Koordinaten (`device_tracker`, `person`, Sensoren mit `latitude`/`longitude`) als farbige Marker mit Initialen und `zone.*` als Zonenkreis auf einer OpenStreetMap-Karte – wie die HA-Kartenkarte.

* **Hinzufügen und Import:**
  * Im Bearbeiten-Modus über „Hinzufügen“ → Typ „Karte“ (Vorschläge nur für Entitäten mit Koordinaten).
  * Der HA-Import übernimmt `type: map` mit `entities`, `default_zoom` (bis 20) und `hours_to_show`.
  * Karten sind in Ordnern nicht möglich.
* **Reiter „Karte“ im Bearbeiten-Dialog:**
  * Tracker hinzufügen oder entfernen.
  * Weglänge (Aus, 5 min bis 24 h) und max. Zoomstufe beim Einpassen (12–20).
  * „Veraltet nach Funkstille“ (Aus, 5–60 min, Standard 10).
  * Festen Ausschnitt zurücksetzen.
* **Vollbild (Karte antippen):**
  * **Bedienung:** +/− zoomen (Stufe 20 ist zweifach vergrößert). Ziehen verschiebt die Karte (neu gezeichnet beim Loslassen). „Alle“ passt wieder automatisch ein.
  * **Springe zu …** zentriert auf einen Tracker oder eine Zone.
  * **Antippen eines Markers** zeigt Name, Zustand, Genauigkeit und „Aktualisiert vor …“.
  * **Wischgesten** sind abgefangen: Kein versehentlicher App-Wechsel und keine QuickSettings.
  * **Beim Schließen mit X** übernimmt das Widget den Ausschnitt (Zoom und Mitte) und speichert ihn. Nach „Alle“ passt es wieder automatisch ein.
* **Wegverlauf:**
  * **Darstellung:** Linie in der Trackerfarbe, ältere Teile blasser und schmaler. Im Vollbild zusätzlich die Messpunkte.
  * **Laden (`HaTrackHistory`):** Beim Öffnen des HA-Bildschirms je Tracker in Zeitabschnitten über `history/history_during_period`. Die Länge eines Abschnitts richtet sich nach der gemessenen Datenrate (~256 KB je Antwort, höchstens 5 min, 4 MB je Tracker).
  * **Lücken nachladen:** Nach einem Wiederverbinden oder beim Zurückkehren wird nur die Zeit seit dem Verbindungsende geladen.
  * **Live-Punkte** kommen aus dem laufenden Abo, ohne zusätzlichen Abruf.
  * **Reduktion:** Ein neuer Punkt erst ab halber GPS-Genauigkeit Abstand.
* **Funkstille:** Meldet ein Tracker länger nichts oder liefert er keine Position (z. B. außer Reichweite), erscheint er **halbdurchsichtig** an der letzten bekannten Position. Sein Weg wird dort unterbrochen statt quer verbunden. Die Infobox zeigt „Zuletzt gesehen vor …“.
* **Tipp:** Für die Karte den `device_tracker` einer Integration verwenden, nicht Diagnose-Sensoren mit großen Attributen. Das Log meldet Entitäten mit mehr als 4 KB je Zustand (`[HA-Verlauf] Tipp: …`).
* **Kacheln (`MapTileService`):**
  * Über HTTPS von `tile.openstreetmap.org`, mit eigenem User-Agent und Quellenhinweis gemäß OSM Tile Usage Policy.
  * Abruf nur für sichtbare Kacheln, in einer Hintergrund-Task mit niedriger Priorität, die sich im Leerlauf beendet.
  * **Zwischenspeicher:** 56 dekodierte Kacheln im PSRAM; dazu im Flash (`/tiles`, max. 2 MB, mindestens 7 Tage gültig), sodass sie nach einem Neustart sofort da sind.
  * Bei knappem DMA-Speicher pausiert der Abruf („Karte pausiert – Speicher knapp“). Video und WLAN haben Vorrang.

---

## 2. Hardware-Voraussetzungen

* **Basis-Gerät:** M5Stack Tab5 (ESP32-P4)
* *(Optional)* BLE-Tracker ("Kippy")
* *(Optional)* IP-Kamera oder ein ausgemustertes Smartphone mit der App [BabyCam](https://play.google.com/store/apps/details?id=com.arjonasoftware.babycam) für den MJPEG- und Audio-Stream.
* *(Optional)* BLE-fähige Drucksensormatte
  * *Hinweis zur Sensormatte:* Details zur Hardware, Funktionsweise und dem dazugehörigen ESP32-Repeater finden sich [hier im CatMat-Repository](https://github.com/IamDiesel/CatMat/tree/main/bt_repeater).
* *(Optional)* T-Dongle S3 [GIT](https://github.com/Xinyuan-LilyGO/T-Dongle-S3) von Lilygo als Bluetooth Coprozessor. Co-Prozessor SW siehe [GIT](https://github.com/IamDiesel/T-Dongle-S3-bt-coprocessor)

---

## 3. Ordnerstruktur

Der Quellcode ist modular in folgende Hauptbereiche unterteilt:

* `src/config/`: Systemweite Konstanten, globale Variablen und Zugangsdaten (`secrets.h`).
* `src/doc/`: Zusätzliche Dokumentationen und Setup-Hinweise.
* `src/gui/`: Die vollständige LVGL-Benutzeroberfläche.
  * `core/`: Haupt-Views (Dashboard, Settings, Topbar, QuickSettings, Alarm-Popups), Video-Overlay (`ViewVideoPip`) und `GuiManager`.
  * `map/`: Wiederverwendbare Kartenansicht `MapView` (Kacheln, Marker, Wege, Ausschnitt) ohne HA-Bezug.
  * `ha/`: C++ LVGL-Integration für Home Assistant.
    * `HaWidgets/`: Modulare Widget-Klassen (Base, Light, Sensor, Vacuum, Media, Map).
    * `HaDialogs/`: Gekapselte UI-Popups (Edit, Add, Light, Vacuum, ColorPicker) und die `UIHelper`-Factory für sauberen LVGL-Code.
  * `baby/` & `cat/`: Spezialisierte Ansichten für die Video/Audio- und Sensor-Überwachung.
* `src/ha/`: Logik für die Home Assistant WebSocket-Kommunikation, JSON-Parsing und Config-Management (`HaConfigLogic`), Positionsverlauf der Karte (`HaTrackHistory`).
* `src/map/`: Kartenkacheln: Projektion (`MapProjection`), PNG-Dekoder (`MapPngDecoder`), Flash-Speicher (`MapTileStore`) und Abruf-Dienst (`MapTileService`, Konstanten in `MapTileConfig.h`).
* `src/media/`: Babyphone-Dienst (`BabyService`, Modus und Zustand), Video (`VideoLogic`, `VideoDecoder`), Audio (`AudioStreamLogic`) und Kamera-Steuerung (`BabyCamApi`).
* `src/system/`: Hardwarenahe Logik (BLE-Scanning, MQTT, WLAN-Handling, lokaler Webserver und LVGL-Treiber-Initialisierung).

---

## 4. Software & Abhängigkeiten

Das Projekt wird mit **PlatformIO** (Framework: Arduino) kompiliert. Alle erforderlichen Bibliotheken werden automatisch über die `platformio.ini` geladen.

* **Plattform:** `pioarduino/platform-espressif32` (Branch/Version `#54.03.21`)
* **Board:** `esp32p4` (`esp32-p4-evboard`)

**Verwendete Haupt-Bibliotheken:**
Die Versionen von LVGL, M5GFX und M5Unified sind **bewusst exakt festgelegt**: andere Kombinationen führten zu Displayflackern. Nicht ohne Test aktualisieren.

* **`lvgl/lvgl`** (`8.3.11`): Grafik-Engine, konfiguriert über die lokale `lv_conf.h` (Speicher im PSRAM).
* **`m5stack/M5GFX`** (`0.2.19`) und **`m5stack/M5Unified`** (`0.2.13`): Hardware-Abstraktion für Display, Touch, Akku und I2S-Audio.
* **`tobozo/LGFX_PPA`** (`0.2.2`): Hardware-Bildverarbeitung (PPA) des ESP32-P4 für das Video.
* **`bblanchon/ArduinoJson`** (`7.4.3`): JSON-Parsing, große Dokumente mit PSRAM-Allocator (`HaPsramAllocator`).
* **`gilmaimon/ArduinoWebsockets`** (`0.5.4`): Home Assistant WebSocket.
* **`knolleary/PubSubClient`** (`2.8.0`): MQTT.
* **`bertmelis/USBHostSerial`** (Git, Commit `3608e9a`, Version 0.2.0): USB-CDC zum BLE-Coprozessor (T-Dongle S3).

Seit v2.0.0 sind **alle** Bibliotheken exakt gepinnt (Git-Bibliotheken auf einen Commit), damit ein Neubau auch später dieselben Stände zieht. Die Babyphone-Audioausgabe läuft direkt über M5Unified; die frühere Abhängigkeit `ESP8266Audio` war ungenutzt und ist entfernt.

---

## 5. Installation & Build-Prozess

### 5.1 Projekt vorbereiten
1. Repository klonen: `git clone <repo-url>`
2. Das Projekt in VSCode mit installierter **PlatformIO**-Erweiterung öffnen.
3. Die Datei `src/config/secrets_dummy.h` duplizieren und in `src/config/secrets.h` umbenennen.
4. In der neuen `secrets.h` den **Home Assistant Long-Lived Access Token** eintragen. *(Diesen erhält man in Home Assistant unter Profil -> Sicherheit -> Langlebige Zugangs-Token).*

### 5.2 Workarounds & Bugfixes anwenden
Aufgrund spezifischer Abhängigkeiten im Espressif/PlatformIO Ökosystem für den ESP32-P4 ist vor dem ersten Kompilieren **eine** manuelle Anpassung nötig (einmal je Rechner):

1. **Python Click-Modul Downgrade (PlatformIO Bug):**
   Das `esptool` dieser Plattform verträgt sich nicht mit `click` ab Version 8.2 – der Upload bricht dann mit `TypeError: ... get_metavar() missing 1 required positional argument: 'ctx'` ab. Im VS Code Terminal:
   
       C:\Users\<DeinNutzername>\.platformio\penv\Scripts\pip.exe install "click==8.1.8"
   
   (Jede 8.1.x-Version funktioniert.)

Automatisch beim Build und **ohne** Handarbeit:
* `tools/patch_m5gfx.py` (in `extra_scripts`): schaltet in M5GFX die DMA2D-Nutzung des DSI-Panels ab (`use_dma2d = false`), da sich der 2D-DMA-Controller auf Silizium v1.3 sonst aufhängt. Legt beim ersten Lauf eine `.orig`-Sicherung an.
* `src/system/Dma2dGuard.c` mit `-Wl,--wrap=dma2d_hal_…_reset_channel`: ersetzt das unbegrenzte Warten im Kanal-Reset des Treibers durch eines mit Zeitlimit.

Die frühere Anpassung „`AudioOutputPDM.cpp` löschen“ entfällt seit v2.0.0 (Bibliothek `ESP8266Audio` entfernt).

### 5.3 Kompilieren
Nach den Vorbereitungen kann das Projekt regulär über PlatformIO gebaut (`Build`) und per USB geflasht (`Upload`) werden. Das PSRAM und die Custom-Partition (`default_16MB.csv`) sind bereits in der `.ini` vorkonfiguriert.

---

## 6. Ersteinrichtung (Web-Setup)

Wenn das Gerät zum ersten Mal startet (oder sein konfiguriertes WLAN nicht findet), wechselt es in den Setup-Modus.

1. Das Display zeigt ein WLAN-Netzwerk namens `Lela-Setup` an, sowie ein automatisch generiertes Passwort.
2. Verbinde ein Smartphone oder einen PC mit diesem Netzwerk.
3. Rufe im Browser `http://192.168.4.1` auf.
4. Über die Weboberfläche können nun alle Zugangsdaten eingetragen werden:
   * Lokales WLAN – mit „Passwort anzeigen“ und einer Auswahlliste **gespeicherter Netze**. Jedes Netz, mit dem sich das Gerät erfolgreich verbunden hat, wird samt Passwort gemerkt (bis zu 8, NVS-Bereich `wifinets`). Die Auswahl füllt SSID und Passwort; „Ausgewähltes Netz vergessen“ entfernt einen Eintrag. Die Werkseinstellung löscht alle.
   * Optional eine **feste IP-Adresse** (IP, Gateway, Subnetzmaske, DNS) statt DHCP. Der Start ist damit ca. 2 s schneller. Die Adresse vorher im Router fest für das Gerät reservieren. Ungültige Angaben werden nicht übernommen (es bleibt bei DHCP).
   * IP und Port von Home Assistant
   * MQTT-Broker Credentials
   * Stream-URLs für Video & Audio (z. B. aus der BabyCam-App)
   * MAC-Adressen der Bluetooth-Peripherie
5. Nach dem Speichern startet das Display neu und verbindet sich automatisch mit der Infrastruktur.

---

## 7. Eigene Icons generieren (Icon Studio)
Im Verzeichnis `python_lvgl_mdi_icon_studio` liegt ein Python-Tool (`iconConverter.py`), mit dem die LVGL-Icon-Datei (`lela_icons.c`) komfortabel um eigene Material Design Icons erweitert werden kann.

<img width="1144" height="827" alt="image" src="https://github.com/user-attachments/assets/6187b960-2d38-4d36-ad63-866d4d1e6975" />

### Vorbereitungen
Damit das Skript funktioniert, müssen folgende Abhängigkeiten auf dem PC installiert sein:

* **Node.js** (wird für das LVGL-Font-Tool `npx` benötigt).
* **Python 3** inkl. der Bibliothek `PyQt5`.
  * Installation via Terminal: `pip install PyQt5`

**Lade die Font- und CSS-Dateien herunter:**
1. Gehe auf das Repo [Templarian/MaterialDesign-Webfont (master)](https://github.com/Templarian/MaterialDesign-Webfont).
2. Lade die Datei `materialdesignicons-webfont.ttf` (aus dem Ordner `fonts`) herunter.
3. Lade die Datei `materialdesignicons.css` (aus dem Ordner `css`) herunter.
4. Lege beide Dateien direkt in denselben Ordner wie das `iconConverter.py` Skript.

### Icons kompilieren
1. Führe das Skript aus: `python iconConverter.py`
2. Es öffnet sich eine Benutzeroberfläche (Lela OS - Icon Studio). Wähle dort die gewünschten Icons aus.
3. Nach dem Bestätigen generiert das Skript über `npx lv_font_conv` vollautomatisch eine neue C-Datei (`lela_icons.c`).
4. Ersetze die bestehende `lela_icons.c` im Projekt (`src/gui/fonts/`) durch die neu generierte Datei.

---

## 8. Lizenz

Dieses Projekt ist unter der **MIT-Lizenz** lizenziert. Es darf für private und kommerzielle Zwecke frei verwendet, modifiziert und weitergegeben werden, sofern der ursprüngliche Copyright-Hinweis beibehalten wird. Die Nutzung erfolgt ohne Gewährleistung.
