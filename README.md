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
* **Webserver & Captive Portal:** Integriertes HTML-Setup-Portal zur Konfiguration von WLAN, MQTT, Home Assistant IP und Kamera-Streams.
* **Zeitsynchronisation:** Automatischer NTP-Abgleich (CET/CEST) für korrekte Graphen- und Systemzeiten.
* **Power-Management:** Live-Auslesen von Akkustand und Ladezustand über die M5Unified API.
* **Task-Management:** Parallele Verarbeitung von WebSockets, GUI-Rendering und Audio-Streaming via FreeRTOS.
* **Live-Screenshot Engine:** Web-Endpunkt zum Auslesen des RAM-Framebuffers und Download als unkomprimiertes `.BMP`-Bild.

### 1.2 Home Assistant Integration
* **WebSocket-Verbindung:** Persistenter, bidirektionaler Echtzeitkanal zu Home Assistant (ohne REST-Polling).
* **Speichereffizienz:** Das System abonniert und parst (via ArduinoJson) nur State-Updates von Entitäten, die aktiv auf dem Dashboard platziert sind.
* **Dienst-Steuerung:** Steuerung von Schaltern, Tasten, Lichtern (inkl. RGB/W-Farbwahl und Helligkeit), Mediaplayern sowie **Saugrobotern** (inkl. Saugstufen-Auswahl und Lokalisierung).
* **Lovelace-Parser:** Import bestehender Home Assistant Dashboards (Lovelace JSON) in native C++ LVGL-Widgets.

| HA Entitäts-Typ (Domain) | Status in unserer SW | Verwendete C++ Klasse | Kurze Beschreibung & UI-Verhalten |
| --- | --- | --- | --- |
| `light`, `switch`, `input_boolean` | ✅ **Fertig** | `HALightWidget` | **Der Standard-Schalter.** Ein einfaches 1x1 Widget. Klick schaltet den Zustand um (Toggle). Zeigt an, ob das Gerät ein- oder ausgeschaltet ist. |
| `sensor`, `binary_sensor`, `weather`, `person`, `device_tracker` | ✅ **Fertig** | `HASensorWidget` | **Die Informationsanzeige.** 1x1 Widget. Rein informativ (read-only). Kann bei numerischen Werten zusätzlich ein historisches Verlaufsdiagramm (`show_chart`) einblenden. |
| `button`, `input_button`, `script`, `scene`, `automation` | ✅ **Fertig** | `HAActionWidget` | **Die Aktionstaste.** 1x1 Widget. Hat keinen dauerhaften On/Off-Status, sondern feuert bei Klick sofort einen Befehl ab (z.B. Szene aktivieren). |
| `select`, `input_select` | ✅ **Fertig** | `HASelectWidget` | **Das Dropdown-Menü.** 1x1 Widget. Zeigt den aktuellen Text an. Bei Klick klappt ein LVGL-Menü mit den verfügbaren, eingebackenen Optionen auf. |
| `number`, `input_number` | ✅ **Fertig** | `HANumberWidget` | **Der Schieberegler.** 1x1 Widget. Beinhaltet einen Slider, um numerische Werte (z.B. 0-100%) inklusive Kommastellen sanft und stufenlos einzustellen. |
| `text`, `input_text` | ✅ **Fertig** | `HATextWidget` | **Die Texteingabe.** 2x1 Widget (doppelte Breite). Zeigt den Text an. Bei Klick öffnet sich ein Fullscreen-Overlay mit virtueller Bildschirmtastatur zur Bearbeitung. |
| `cover` | ✅ **Fertig** | `HACoverWidget` | **Die Rollladensteuerung.** 2x1 Widget. Bietet drei Schnell-Tasten (Auf, Stop, Ab) sowie einen Slider für die exakte Positionierung in Prozent. |
| `climate` | ✅ **Fertig** | `HAClimateWidget` | **Das Thermostat.** 2x1 Widget. Zeigt Ist- und Soll-Temperatur groß an. Bietet Plus/Minus-Tasten zur Einstellung und einen Button zum Durchschalten des Modus (Heizen, Kühlen, Auto). |
| `vacuum` | ✅ **Fertig** | `HAVacuumWidget` | **Die Staubsaugersteuerung.** 2x1 Widget. Extra Tasten für Start, Stop, Pause und Rückkehr zur Ladestation (Dock). |
| `media_player` | ✅ **Fertig** | `HAMediaWidget` | **Die Mediensteuerung.** 2x1 Widget. Play/Pause-Funktion, Lautstärke-Slider und ggf. Tasten für Vor/Zurück oder Quellenauswahl. |
| `fan` | ❌ *Offen* | *Fehlt noch* | **Ventilatoren.** Müsste idealerweise als 2x1 Widget gebaut werden (Kombination aus On/Off, Geschwindigkeits-Slider und Schwenk-Button). |
| `lock` | ❌ *Offen* | *Fehlt noch* | **Smarte Türschlösser.** Fällt aktuell in den Standard-Topf. Bräuchte ein eigenes Widget mit einer Sicherheitsabfrage ("Wirklich öffnen?"), um versehentliche Klicks zu vermeiden. |
| `alarm_control_panel` | ❌ *Offen* | *Fehlt noch* | **Alarmanlagen.** Würde ein Fullscreen-Overlay mit einem Ziffernblock (PIN-Pad) erfordern, um die Anlage (un)scharf zu schalten. |
| `input_datetime`, `time`, `date` | ❌ *Offen* | *Fehlt noch* | **Zeit & Datum.** Fällt aktuell durchs Raster. Erfordert ein Overlay mit LVGL-Rollern (wie bei einem Zahlenschloss), um Uhrzeiten einzustellen. |
| `camera`, `image` | ❌ *Offen* | *Zu hoher RAM-Bedarf* | **Kamera-Feeds.** Für einen ESP32 extrem schwer umzusetzen, da Live-JPEG-Dekodierung den Arbeitsspeicher massiv belastet. MJPEG Stream kann per Babycam eingebunden werden. |
| `update`, `water_heater`, `siren`, `valve`, `remote` | ❌ *Offen* | *Spezialfälle* | **Nischen-Entitäten.** Werden aktuell meist vom Fallback (`HALightWidget`) aufgefangen oder ignoriert, wenn keine `tap_action` definiert ist. |


### 1.3 On-Device Dashboard-Editor
* **Layout-Engine:** Freies Platzieren von Widgets per Drag & Drop mit 10px-Snap-to-Grid.
* **Stabile Rendering-Pipeline:** Sicheres Z-Order-Management beim Widget-Editing.
* **Widget-Konfiguration:** Einstellungen für Margin, Alignment, Farben und Icons direkt über Touch-Eingaben und On-Screen-Keyboard. Optionale Skalierung für dynamische Sub-Elemente (z. B. Vacuum-Buttons).
* **Backup/Restore (PSRAM optimiert):** JSON-basierter Export und Import des Layouts über das lokale Web-Portal. Das Speichern großer Konfigurationen wird via Custom Allocator in den 32MB PSRAM ausgelagert, um den internen RAM zu schonen (Out-Of-Memory Schutz).

### 1.4 Datenvisualisierung (Sensor-Charts)
* **Dynamische Graphen:** Rendering von Sensordaten als Area-Charts über LVGL.
* **Smartes UI-Design:** Dynamische LVGL-`recolor`-Einheiten (dezent und kontrastarm) sowie semi-transparente Wasserzeichen-Icons für maximale Lesbarkeit der Sensorwerte in Kachel-Ansichten.
* **Historisierung:** Lokales Array für 50 Messwerte inkl. NTP-Zeitstempel (HH:MM) für die X-Achse.
* **Auto-Scaling:** Skalierung der Y-Achse auf Basis von Min/Max-Werten mit dynamischem Padding. Manuelle Fixierung von Wertebereichen möglich.
* **Hold-Logik:** Interpolation fehlender Datenpunkte in festen Zeitintervallen (10s) zur Wahrung der Zeitachsen-Integrität.

### 1.5 Audio & Video (Babyphone-Modus)
* **MJPEG-Video-Stream:** Direkter Abgriff, Dekodierung und Anzeige von HTTP-Kamerastreams. Als Quelle kann hierfür beispielsweise ein altes Smartphone mit der App [BabyCam](https://play.google.com/store/apps/details?id=com.arjonasoftware.babycam) genutzt werden.
* **Hardware-Audio (I2S):** Soundausgabe und Dekodierung von unkomprimierten (PCM/WAV) und komprimierten (AAC) Audio-Streams.
* **Latenz-Management:** Konfigurierbarer Frame-Drop-Schwellenwert bei schwacher WLAN-Verbindung.
* **Alarmierung:** Automatischer Screen-Wakeup via MQTT-Trigger.

### 1.6 BLE Sensorik (Katzen-Matte & Tracker)
* **BLE-Scanner:** Integriertes GUI zum Suchen und Binden von Bluetooth-MAC-Adressen.
* **Druck-Auswertung:** Gleitende Durchschnittsberechnung (Sliding Window) mit Tara-Funktion zur Drift-Kompensation des Matten-Sensors.
* **Lokaler Alarm:** Optische und akustische Warnungen (Custom I2S Töne) bei Limit-Überschreitungen oder Verbindungsabbruch.

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
  * `core/`: Haupt-Views (Dashboard, Settings, Topbar) und `GuiManager`.
  * `ha/`: C++ LVGL-Integration für Home Assistant.
    * `HaWidgets/`: Modulare Widget-Klassen (Base, Light, Sensor, Vacuum, Media).
    * `HaDialogs/`: Gekapselte UI-Popups (Edit, Add, Light, Vacuum, ColorPicker) und die `UIHelper`-Factory für sauberen LVGL-Code.
  * `baby/` & `cat/`: Spezialisierte Ansichten für die Video/Audio- und Sensor-Überwachung.
* `src/ha/`: Logik für die Home Assistant WebSocket-Kommunikation, JSON-Parsing und Config-Management (`HaConfigLogic`).
* `src/media/`: Audio- und Video-Streaming-Dienste sowie Puffer-Verwaltung.
* `src/system/`: Hardwarenahe Logik (BLE-Scanning, MQTT, WLAN-Handling, lokaler Webserver und LVGL-Treiber-Initialisierung).

---

## 4. Software & Abhängigkeiten

Das Projekt wird mit **PlatformIO** (Framework: Arduino) kompiliert. Alle erforderlichen Bibliotheken werden automatisch über die `platformio.ini` geladen.

* **Plattform:** `pioarduino/platform-espressif32` (Branch/Version `#54.03.21`)
* **Board:** `esp32p4` (`esp32-p4-evboard`)

**Verwendete Haupt-Bibliotheken:**
* **`m5stack/M5Unified`** (`^0.2.2`): Hardware-Abstraktion für Display, Touch und I2S-Audio des M5Stack.
* **`bblanchon/ArduinoJson`** (`^7.3.0`): Für performantes, PSRAM-optimiertes JSON-Parsing.
* **`gilmaimon/ArduinoWebsockets`** (`^0.5.4`): Für den bidirektionalen Home Assistant Echtzeit-Feed.
* **`earlephilhower/ESP8266Audio`** (`^1.9.9`): Für die I2S-Audiosignalausgabe (MP3/AAC/WAV).
* **`lvgl/lvgl`** (`~8.4.0`): Die Basis der Grafik-Engine, konfiguriert über die lokale `lv_conf.h`.

---

## 5. Installation & Build-Prozess

### 5.1 Projekt vorbereiten
1. Repository klonen: `git clone <repo-url>`
2. Das Projekt in VSCode mit installierter **PlatformIO**-Erweiterung öffnen.
3. Die Datei `src/config/secrets_dummy.h` duplizieren und in `src/config/secrets.h` umbenennen.
4. In der neuen `secrets.h` den **Home Assistant Long-Lived Access Token** eintragen. *(Diesen erhält man in Home Assistant unter Profil -> Sicherheit -> Langlebige Zugangs-Token).*

### 5.2 Workarounds & Bugfixes anwenden
Aufgrund spezifischer Abhängigkeiten im Espressif/PlatformIO Ökosystem für den ESP32-P4 müssen vor dem ersten Kompilieren zwei manuelle Anpassungen vorgenommen werden:

1. **Python Click-Modul Downgrade (PlatformIO Bug):**
   Öffne das VS Code Terminal und führe folgenden Befehl aus, um das Modul auf eine kompatible Version zu setzen:
   
       C:\Users\<DeinNutzername>\.platformio\penv\Scripts\pip.exe install "click==8.1.7"
   
2. **ESP8266Audio PDM-Kompilierungsfehler:**
   Damit die Audio-Bibliothek auf dem P4-Chip fehlerfrei kompiliert, muss nach dem ersten Laden der Bibliotheken eine fehlerhafte PDM-Datei gelöscht werden. Navigiere im Dateibaum zu:
   `.pio/libdeps/esp32p4_m5stack/ESP8266Audio/src/AudioOutputPDM.cpp` und lösche diese Datei.

### 5.3 Kompilieren
Nach den Vorbereitungen kann das Projekt regulär über PlatformIO gebaut (`Build`) und per USB geflasht (`Upload`) werden. Das PSRAM und die Custom-Partition (`default_16MB.csv`) sind bereits in der `.ini` vorkonfiguriert.

---

## 6. Ersteinrichtung (Web-Setup)

Wenn das Gerät zum ersten Mal startet (oder sein konfiguriertes WLAN nicht findet), wechselt es in den Setup-Modus.

1. Das Display zeigt ein WLAN-Netzwerk namens `Lela-Setup` an, sowie ein automatisch generiertes Passwort.
2. Verbinde ein Smartphone oder einen PC mit diesem Netzwerk.
3. Rufe im Browser `http://192.168.4.1` auf.
4. Über die Weboberfläche können nun alle Zugangsdaten eingetragen werden:
   * Lokales WLAN
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
