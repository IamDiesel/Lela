# Lela OS 2

Lela OS ist eine auf FreeRTOS und LVGL basierende Smart-Home-Betriebssystemumgebung für ESP32-Mikrocontroller (spezifisch optimiert für das M5Stack Tab5 / ESP32-P4). Es vereint eine direkte Home Assistant WebSocket-Integration, ein dynamisches On-Device Dashboard-Editing, Multimedia-Streaming (Babyphone-Modus) und spezialisierte BLE-Sensor-Auswertungen.

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/01794d18-cf56-4e82-b485-18b429af5852" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/0744ee97-ae6c-481b-a7cc-810118674257" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/248af16d-70d8-421c-b933-fef771af4c68" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/1e9dc37d-7b92-448b-aa02-4b974506b825" />




## Inhaltsverzeichnis
1. [Systemarchitektur & Features](#1-systemarchitektur--features)
2. [Hardware-Voraussetzungen](#2-hardware-voraussetzungen)
3. [Ordnerstruktur](#3-ordnerstruktur)
4. [Software & Abhängigkeiten](#4-software--abhängigkeiten)
5. [Installation & Build-Prozess](#5-installation--build-prozess)
6. [Ersteinrichtung (Web-Setup)](#6-ersteinrichtung-web-setup)
7. [Lizenz](#7-lizenz)

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
* **Dienst-Steuerung:** Steuerung von Schaltern, Tasten, Lichtern (inkl. RGB/W-Farbwahl und Helligkeit) sowie Mediaplayern.
* **Lovelace-Parser:** Import bestehender Home Assistant Dashboards (Lovelace JSON) in native C++ LVGL-Widgets.

### 1.3 On-Device Dashboard-Editor
* **Layout-Engine:** Freies Platzieren von Widgets per Drag & Drop mit 10px-Snap-to-Grid.
* **Z-Order Management:** Ebenensteuerung (Vordergrund/Hintergrund) für sich überlappende Elemente.
* **Widget-Konfiguration:** Einstellungen für Margin, Alignment, Farben und Icons direkt über Touch-Eingaben und On-Screen-Keyboard.
* **Backup/Restore:** JSON-basierter Export und Import des Layouts über das lokale Web-Portal.

### 1.4 Datenvisualisierung (Sensor-Charts)
* **Dynamische Graphen:** Rendering von Sensordaten als Area-Charts über LVGL.
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

---

## 3. Ordnerstruktur

Der Quellcode ist modular in folgende Hauptbereiche unterteilt:

* `src/config/`: Systemweite Konstanten, globale Variablen und Zugangsdaten (`secrets.h`).
* `src/doc/`: Zusätzliche Dokumentationen und Setup-Hinweise.
* `src/gui/`: Die vollständige LVGL-Benutzeroberfläche.
  * `core/`: Haupt-Views (Dashboard, Settings, Topbar).
  * `ha/`: Die C++ Widget-Klassen für Home Assistant und die zuhörigen Edit-Dialoge.
  * `baby/` & `cat/`: Spezialisierte Ansichten für die Video/Audio- und Sensor-Überwachung.
* `src/ha/`: Logik für die Home Assistant WebSocket-Kommunikation, JSON-Parsing und Config-Management.
* `src/media/`: Audio- und Video-Streaming-Dienste sowie Puffer-Verwaltung.
* `src/system/`: Hardwarenahe Logik (BLE-Scanning, MQTT, WLAN-Handling, lokaler Webserver und LVGL-Treiber-Initialisierung).

---

## 4. Software & Abhängigkeiten

Das Projekt wird mit **PlatformIO** (Framework: Arduino) kompiliert. Alle erforderlichen Bibliotheken werden automatisch über die `platformio.ini` geladen.

* **Plattform:** `pioarduino/platform-espressif32` (Branch/Version `#54.03.21`)
* **Board:** `esp32p4` (`esp32-p4-evboard`)

**Verwendete Haupt-Bibliotheken:**
* **`m5stack/M5Unified`** (`^0.2.2`): Hardware-Abstraktion für Display, Touch und I2S-Audio des M5Stack.
* **`bblanchon/ArduinoJson`** (`^7.3.0`): Für performantes, speichersicheres JSON-Parsing.
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

## 7. Lizenz

Dieses Projekt ist unter der **MIT-Lizenz** lizenziert. Es darf für private und kommerzielle Zwecke frei verwendet, modifiziert und weitergegeben werden, sofern der ursprüngliche Copyright-Hinweis beibehalten wird. Die Nutzung erfolgt ohne Gewährleistung.
