# 📚 Babycam API – Inoffizielle Spezifikation & Client-Guidelines

**Fokus:** Rein lokaler Betrieb (WLAN / WiFi Direct) ohne Cloud-Zwang.
**Basis-URL:** `http://<Kamera-IP>:8080` (Die IP stammt typischerweise aus einem mDNS-Broadcast oder dem QR-Code-Scan).
**Authentifizierung:** Lela OS identifiziert sich gegenüber der Kamera mit einer statischen UUIDv4.
**Standard-Header:** Alle POST/GET Requests benötigen zwingend folgende Header-Struktur:

* `uuid`: `<CLIENT_UUID>` (Format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`, z.B. `550e8400-e29b-41d4-a716-446655440000`)
* `versionApp`: `<APP_VERSION>` (z.B. `"2.48"`)
* `Content-Type`: `application/json` (nur bei POST-Requests)

---

## 0. Zwei Zugänge: App und Browser

Die Kamera-App (BabyCam von Arjona Software, hier Version 2.48) betreibt auf Port 8080 **einen** Webserver mit **einer** Routentabelle (vollständige Liste in Abschnitt 10). Eltern-App und Browser benutzen dieselben Endpunkte. Sie unterscheiden sich in den Parametern und darin, wie der Ton transportiert wird:

| | App-Zugang (Eltern-App, Lela OS) | Web-Zugang (Browser, Dateien der Kamera unter `/`) |
| --- | --- | --- |
| Anmeldung | `POST /api/v1/client/online` mit eigener `uuidClient`, alle 10 s (Abschnitt 1) | ebenfalls `client/online`, aber fest mit `{"ip":"1.1.1.1","deviceName":"BabyCam Web"}` |
| Video | `GET /api-stream/v1/video?id=<Zufalls-UUID>&uuid=<Client-UUID>&quality=<n>` | `GET /api-stream/v1/video?random=<Zufallszahl>` (bei Passwortschutz zusätzlich `password=`) |
| Ton von der Kamera | UDP, rohes PCM, Pakete zu 500 Bytes an Port 50001 | HTTP: `GET /api-stream/v1/audio`, WAV-Strom (`audio/wav`) |
| Gegensprechen | UDP, rohes PCM, Pakete zu 500 Bytes an Port 50003 | in der Web-Oberfläche nicht vorhanden |
| Standbild | nicht untersucht (Lela OS benutzt es nicht) | `GET /api-stream/v1/photo?quality=25&id=<Zähler>` (Bild-im-Bild), `…/photo?quality=90&download=true` |

**Der Video-Endpunkt ist für beide derselbe, das Format auch** (MJPEG, Abschnitt 2). Der Server wertet nur `uuid`, `quality` und `chromecast` aus; `id` und `random` ignoriert er (sie verhindern nur, dass ein Zwischenspeicher eine alte Antwort liefert). Daraus folgt:

* **Ohne `uuid` gilt der Abrufer als Browser:** Die Kamera trägt ihn als „BabyCam Web“ mit der IP `1.1.1.1` ein.
* **`quality` wirkt nur zusammen mit `uuid`** – die Qualität wird je Client-UUID gemerkt.
* Alte Namen derselben Ströme: `/stream/video` und `/stream/audioHttp`.

Quelle für diesen Abschnitt: der entpackte Code der App (Routentabelle des Servers, Bearbeitung der Ströme) und die mitgelieferte Web-Oberfläche (`index.js`), ausgewertet am 17.11.2026.

---

## 1. Verbindungsaufbau & Keep-Alive (Boot-Sequenz)

Da die Cloud-Infrastruktur umgangen wird, muss der Client die TCP-Session lokal aufrechterhalten. Ohne regelmäßigen Keep-Alive schließt der Kamera-Server aus Sicherheitsgründen die aktiven Video- und Audio-Sockets.

### Schritt 1: Lokaler Health-Check (Ping-Ersatz)

Prüft latenzfrei, ob der Webserver der Kamera hochgefahren und erreichbar ist.

* **Methode:** `GET /cgi/version`
* **Response:** Plaintext-String der Firmware-Version (z.B. `2.47`)

### Schritt 2: Client-Anmeldung (Session Start / Keep-Alive)

Registriert den Monitor-Client bei der Kamera. **Dieser Aufruf muss zwingend alle 10 Sekunden als zyklischer Hintergrund-Task wiederholt werden!**

* **Methode:** `POST /api/v1/client/online`
* **Body:**
```json
{
  "deviceName": "Lela OS Monitor",
  "ip": "192.168.178.50",
  "uuidClient": "550e8400-e29b-41d4-a716-446655440000"
}

```

*(Hinweis: `ip` ist die IP-Adresse des zugreifenden ESP32/Lela OS Clients, nicht die der Kamera).*

* **Response:** `{"status":"OK"}`

### Schritt 3: Session beenden (Graceful Disconnect)

Meldet den Client sauber ab, wenn Lela OS in den Standby geht.

* **Methode:** `POST /api/v1/client/offline`

### Schritt 4: Passwort-Prüfung (Optional)

Falls die Kamera mit einem Passwort geschützt ist (kann via `/api/v1/password/enabled` geprüft werden).

* **Methode:** `POST /api/v1/password/check`

---

## 2. Video-Stream (HTTP Downlink) & Parsing-Guidelines

Das Video wird als kontinuierlicher MJPEG-Stream (Motion JPEG) über `multipart/x-mixed-replace` gepusht.

* **URL:** `GET /api-stream/v1/video?id=lela-os-stream&uuid=<CLIENT_UUID>&quality=50`
* **Parameter:**
* `id`: Statischer Bezeichner (meist `lela-os-stream`).
* `uuid`: Die Client-UUID.
* `quality`: Startqualität `0` (schlecht) bis `100` (beste).



**⚠️ Kritische Embedded-Client Guidelines:**

1. **Wake-Up Latenz:** Nach dem Absetzen des `GET`-Requests benötigt die Kamera (Hardware-Encoder & Sensor) ca. 2500 ms bis 3000 ms zum Initialisieren. Der Timeout des HTTP-Clients muss zwingend auf `> 5000 ms` konfiguriert werden, um Verbindungsabbrüche (Code `-11`) abzufangen. Sobald der Stream steht, kann der Timeout auf `10 ms` gesenkt werden.
2. **RTOS WDT-Schutz:** Bei Netzwerk-Latenzen schließt die Kamera den TCP-Socket oft stumm ("Graceful Close") oder verliert den Header-Sync. `Content-Length`-Parser müssen zwingend mit echten OS-Yields (`vTaskDelay(1)`) ausgestattet sein und ungültige Binär-Zeilen zählen (> 25 Zeilen Müll = Hard Reset der TCP-Connection), um Watchdog-Panics zu vermeiden.

---

## 3. Audio-Streams (UDP) & Hardware-Routing

Audio wird zur Reduktion von Latenzen verbindungslos über UDP übertragen. Das Format ist beidseitig:

* **Format:** Unkomprimiertes Raw PCM (LPCM)
* **Abtastrate:** 44100 Hz
* **Kanäle & Tiefe:** 16-Bit Mono, Little Endian

### 👶 Baby-Ton hören (Audio Downlink)

Weist die Kamera an, einen UDP-Stream an die definierte IP des Clients zu senden.

* **Befehl:** `POST /api/v1/audio/baby/start-stop`
* **Body:**

```json
{
  "audioBabyCompressed": false,
  "ip": "192.168.178.50",
  "status": "PLAY" 
}

```

*(Zum Stoppen: `"status": "STOP"`).*

* **Netzwerk-Routing:** Die Kamera feuert UDP-Pakete an den lokalen Client-Port **50001** (`audioBabyRecorderPort`), in der Regel 500 Bytes je Paket (das Kleinere aus 500 und der Mindestpuffergröße des Mikrofons der Kamera).

### 🎤 Mit dem Baby sprechen (Push-to-Talk / Audio Uplink)

Versetzt die Kamera in den Zuhör-Modus.

* **Befehl:** `POST /api/v1/audio/parents/start-stop`
* **Body:**

```json
{
  "ip": "192.168.178.50",
  "status": "PLAY"
}

```

* **Netzwerk-Routing:** Der Client sendet UDP-Pakete mit rohem PCM an die IP der Kamera auf Ziel-Port **50003** (`audioParentPlayerPort`).
* **Paketgröße: höchstens 500 Bytes (250 Samples).** Die Kamera liest jedes Paket in einen Puffer von genau 500 Bytes und spielt nur das ab; der Rest eines größeren Pakets ist verloren. Die Eltern-App sendet selbst 500 Bytes je Paket. Pakete mit genau 1 Byte verwirft die Kamera (Lebenszeichen).
  * Erfahrung mit Lela OS (17.11.2026): 1024 Bytes je Paket → mehr als die Hälfte fehlt, die Stimme klingt massiv roboterhaft. 512 Bytes → klingt gut, es fehlen aber noch 6 Samples je Paket. Seitdem 500 Bytes.

**⚠️ Hardware Race Conditions (I2S):**
Auf Embedded-Systemen mit geteiltem I2S-Bus für Mikrofon und Lautsprecher (z.B. ESP32-P4 / M5Stack Tab5) muss die Audio-Steuerung zwingend **Half-Duplex** erfolgen. Die Umschaltung muss über OS-Flags isoliert werden (erst Stream-Task logisch stoppen -> RTOS Yield ausführen -> I2S-Hardware deaktivieren -> Gegenseite hochfahren). Ein hartes Trennen der I2S-DMA-Hardware während eines aktiven Schreibzugriffs führt sofort zu einem Fatal Exception Core Panic.

---

## 4. Kamera-Status & Sensoren (Automatisches Polling)

Dieser Endpunkt sollte zyklisch (alle 3 bis 5 Sekunden) abgerufen werden, um das UI aktuell zu halten.

* **Methode:** `GET /api/v1/status?fast=true`
* **Response Payload:**

```json
{
  "batteryLevel": 85,
  "batteryIsCharging": false,
  "wifiQuality": 100,
  "cameraHasFlash": true,
  "flash": false,
  "cameraStatus": "PLAY",
  "screenOn": true,
  "audioBabyRecorderPort": 50001,
  "audioParentPlayerPort": 50003,
  "babyPlayer": false,
  "resolution": "1280x720",
  "zoom": 0,
  "version": "2.47",
  "resolutions": [
    "1280x720",
    "800x450",
    "640x480",
    "320x240"
  ]
}

```

---

## 5. Manuelle Hardware-Steuerung & Einstellungen

### ⚙️ Video- & Performance-Einstellungen

* **Kompression/Qualität:**
* `POST /api/v1/camera/quality`
* Body: `{"level": 80, "uuid": "<CLIENT_UUID>"}` *(0 bis 100)*


* **Framerate / Energiesparmodus:**
* `POST /api/v1/camera/speed`
* Body: `{"level": 30, "uuid": "<CLIENT_UUID>"}` *(0 = Maximale FPS, 100 = Maximaler Eco-Modus)*


* **Auflösung ändern:**
* `POST /api/v1/camera/change-resolution`
* Body: `{"forceChangeResolution": true, "width": 1280, "height": 720}` *(Führt serverseitig zu einem Stream-Neustart; mit `false` darf die Kamera ablehnen, siehe Abschnitt 9)*


* **Belichtung (Exposure) anpassen:**
* `POST /api/v1/camera/exposure`
* Body: `{"percentage": 50, "uuid": "<CLIENT_UUID>"}` *(0 bis 100)*



### 📷 Kamera- & Licht-Toggles

* **Kamera-Linse wechseln (Front/Back):**
* `GET /api/v1/camera/switch`
* Response: `{"resolutions": ["1280x720", "800x450", "640x480"]}` *(Liefert direkt die nativen Auflösungen der neuen Linse)*


* **Digitaler Zoom / PTZ:**
* `POST /api/v1/camera/zoom`
* Body: `{"percentage": 25}` *(0 bis 100)*


* **Nachtlicht (Kamera-LED):**
* `GET /api/v1/camera/flash`
* Response: `{"status": "ON"}` oder `{"status": "OFF"}`


* **Displaylicht (Weißer Bildschirm):**
* `GET /api/v1/camera/flash-screen`
* Response: `{"status": "ON"}` oder `{"status": "OFF"}`


* **Lokale Videoaufzeichnung:**
* `POST /api/v1/camera/record-video`



### 🔋 Power Management & Standby

* **Stream pausieren (Standby):** `GET /api/v1/camera/off` -> `{"status": "OFF"}`
* **Stream fortsetzen:** `GET /api/v1/camera/on` -> `{"status": "ON"}`
* **Physisches Geräte-Display umschalten:**
* `GET /api/v1/screen/on-off`
* Response: `{"screenOffTimeout": 30, "status": "ON"}` oder `{"screenOffTimeout": 0, "status": "OFF"}`


* **Bildschirm temporär einschalten:**
* `GET /api/v1/screen/on-temporal`
* Response: `OK`


* **Remote Power-Off (Kompletter Shutdown):**
* `GET /api/v1/camera/power-off` -> `{"status": "OK"}`
* *Achtung:* Fährt die Smartphone/Kamera-App vollständig herunter. Das Gerät ist danach nicht mehr per Netzwerk erreichbar.



---

## 6. Geräteinformationen & Lightweight-Polling

Ressourcenschonende Endpunkte für Microcontroller (liefern kurze Plaintext- oder JSON-Antworten ohne den Overhead des globalen Status-Objekts).

* **Akkustand abfragen:**
* `GET /api/v1/battery`
* Response (Plaintext): `46% Not charging`


* **Gerätename abfragen:**
* `GET /api/v1/name-device`
* Response (Plaintext): `Google Pixel 9 Pro`


* **Passwort-Schutz prüfen:**
* `GET /api/v1/password/enabled`
* Response (JSON): `{"status": "NO"}`


* **Verfügbare Auflösungen auflisten (ohne Umschaltung):**
* `GET /api/v1/camera/resolutions`
* Response (JSON): `{"resolutions": ["1280x720", "1024x768", "800x600", "640x480", "320x240"]}`



---

## 7. Erweiterte Audio-Funktionen (Schlaflieder & Alarme)

* **Schlaflied-Steuerung (Media Player):**
* Wiedergabe starten/stoppen: `POST /api/v1/lullaby/start-stop`
* Nächstes Lied: `POST /api/v1/lullaby/next`
* Vorheriges Lied: `POST /api/v1/lullaby/previous`
* Lautstärke ändern: `POST /api/v1/lullaby/change-media-volume`


* **Geräuscherkennung / Baby-Alarm:**
* Alarm umschalten: `POST /api/v1/audio-alert/baby/start-stop`

---

## 8. Web-Zugang (Browser)

Die Kamera liefert unter `http://<Kamera-IP>:8080/` eine Web-Oberfläche aus (`index.html`, `index.js`). Was sie aufruft, steht im mitgelieferten `index.js` der App-Version 2.48; **von Lela OS wird dieser Zugang nicht benutzt.**

* **Anmeldung:** `POST /api/v1/client/online` mit `{"ip": "1.1.1.1", "deviceName": "BabyCam Web"}`.
* **Video:** `<img src="/api-stream/v1/video?random=<Zufallszahl>">` – derselbe MJPEG-Strom wie in Abschnitt 2, ohne `uuid` und `quality`.
  * Die Kamera nimmt denselben Endpunkt auch mit `uuid` und `quality` an (z. B. `…/video?uuid=<UUID>&quality=50&random=0.72…`); dann zählt der Abruf zu dieser UUID und nicht als „BabyCam Web“.
* **Ton:** `<audio src="/api-stream/v1/audio">` – ein fortlaufender WAV-Strom (`audio/wav`) aus demselben Mikrofon wie der UDP-Ton: 44100 Hz, 16 Bit, Mono. Mit dem Parameter `notAddWavHeader` entfällt der WAV-Kopf (rohes PCM).
  * Lela OS könnte diesen Strom über die Tonquelle „HTTP-Stream“ (PCM/WAV) abspielen – ungetestet.
* **Standbild:** `/api-stream/v1/photo?quality=25&id=<Zähler>` (Bild-im-Bild), `/api-stream/v1/photo?quality=90&download=true`.
* **Aufgenommenes Video laden:** `/api-stream/v1/download-video`.
* **Steuerung:** dieselben `/api/v1/…`-Endpunkte wie die App (Zoom, Licht, Linse, Auflösung, Aufnahme, Schlaflied, Kamera an/aus, Power-Off, Akku, Passwort).
* **Nur im Browser benutzt:** `GET /cgi/getUpdatingImage`, `GET /cgi/sensors`.
* **Passwortschutz:** als Kopfzeile `Password: <Wert>` bei den Steuer-Aufrufen, als Parameter `password=<Wert>` bei den Strömen.
* **Gegensprechen:** in der Web-Oberfläche nicht vorhanden.
* Die Web-Oberfläche spricht außerdem einen zweiten Server auf **Port 8081** an (`/cgi/initServer`), der die Kamera aus der Ferne startet – nicht untersucht.

---

## 9. Erkenntnisse aus dem Betrieb (Lela OS, Stand 17.11.2026)

Was die Beobachtung der Schnittstelle nicht gezeigt hat und erst im Betrieb aufgefallen ist. Belege stehen im Quelltext von Lela OS (`src/media/BabyCamApi.cpp`, `VideoLogic.cpp`, `AudioStreamLogic.cpp`) und in `src/doc/video_refactoring.md`.

### Anmeldung
* **Erst anmelden, dann alles andere.** Eine Statusabfrage vor dem ersten `client/online` zeigt die Kamera-App als „Babycam Web“ an.
* **Ein Gerätename je `uuidClient`.** Zwei verschiedene `deviceName` unter derselben UUID sehen für die Kamera wie zwei Anmeldungen aus.
* Lela OS wiederholt die Anmeldung alle 8 s (verlangt: 10 s) und fragt den Status alle 5 s ab.
* `POST /api/v1/client/offline` beim Beenden wirklich senden – sonst bleibt die Sitzung auf der Kamera stehen.

### Video
* **`Content-Length` ist nicht die Länge des JPEG.** Hinter dem Bildende (`FF D9`) folgt Füllmaterial; die Kamera liefert Blöcke fester Größe (beobachtet: 81920 Bytes je Bild in jeder Auflösung, bei großen Bildern 655360 Bytes). Das Bildende rückwärts suchen, nur das JPEG an den Decoder geben.
* **`quality` in der Stream-URL ist die Startqualität und gilt bei jedem Verbindungsaufbau neu.** Was per `POST /api/v1/camera/quality` eingestellt wurde, hält nur bis zur nächsten Verbindung, wenn die URL einen anderen Wert trägt.
* **`forceChangeResolution` auf `true` setzen.** Mit `false` darf die Kamera die Auflösung ablehnen – das hat sie nachweislich getan. Die tatsächliche Auflösung danach aus dem Status lesen.
* `/api/v1/camera/speed`: 0 = höchste Bildrate, 100 = stärkster Sparmodus (leicht zu verwechseln).

### Ton
* **Gegensprechen: höchstens 500 Bytes je UDP-Paket** (Empfangspuffer der Kamera, Abschnitt 3). Größere Pakete werden abgeschnitten; das Gesendete selbst war dabei in Ordnung – der Fehler ist nur an der Kamera zu hören.
* **44100 Hz in beide Richtungen bestätigt:** Der Babyton spielt seit September sauber mit 44100 Hz; beim Gegensprechen klingt 48000 Hz hörbar tiefer.
* Die Ports aus dem Status (`audioBabyRecorderPort`, `audioParentPlayerPort`) haben Vorrang vor festen Werten – die Kamera sendet dorthin, wohin sie will.

### Stand der Prüfung

| Abschnitt | Stand |
| --- | --- |
| 1 Anmeldung, Keep-Alive, Abmeldung | im Betrieb bestätigt |
| 2 Videostrom (App-Zugang) | im Betrieb bestätigt, mit den Ergänzungen oben |
| 3 Ton über UDP, Gegensprechen | im Betrieb bestätigt; Paketgröße, Rate und Ports zusätzlich am Code der App geprüft |
| 4 Status | benutzte Felder bestätigt (`batteryLevel`, `screenOn`, `flash`, `zoom`, `cameraStatus`, `resolution`, `resolutions`, Audio-Ports); übrige nur beobachtet |
| 5 Steuerung | von Lela OS benutzt: Qualität, Bildrate, Zoom, Belichtung, Auflösung, Aufnahme, Licht, Linse, Display, Ein/Aus, Power-Off; Antworten nicht einzeln ausgewertet |
| 6 Geräteinformationen | nur beobachtet, von Lela OS nicht benutzt (außer `/cgi/version`) |
| 7 Schlaflieder, Geräuschalarm | nur beobachtet, nicht benutzt |
| 0, 8 Web-Zugang | am Code der App und an der mitgelieferten Web-Oberfläche geprüft (Version 2.48); von Lela OS nicht benutzt |
| 10 Routenliste | aus dem Code der App (Version 2.48) |
| 11 WiFi Direct | aus dem Code und den Texten der App (Version 2.48); mit Lela OS nicht getestet |
| 12 Verschlüsselung | aus dem Code der App (Version 2.48); am Gerät nicht geprüft |

---

## 10. Alle Routen des Kamera-Servers (Port 8080)

Aus der Routentabelle der App, Version 2.48. Was nicht hier steht, beantwortet der Server als Datei der Web-Oberfläche oder mit 404.

**Ströme** (`/api-stream/…`, lange laufende Antworten)

* `/api-stream/v1/video` (alt: `/stream/video`) – MJPEG, `multipart/x-mixed-replace; boundary=Ba4oTvQMY8ew04N8dcnM`
* `/api-stream/v1/audio` (alt: `/stream/audioHttp`) – WAV-Strom
* `/api-stream/v1/photo` – einzelnes JPEG (`quality`, `download`)
* `/api-stream/v1/download-video` – aufgenommenes Video (MP4)

**Sitzung und Auskunft**

* `/api/v1/client/online`, `/api/v1/client/offline`
* `/api/v1/status`, `/api/v1/battery`, `/api/v1/version`, `/api/v1/name-device`
* `/api/v1/password/check`, `/api/v1/password/enabled`, `/api/v1/premium`, `/api/v1/update-app`

**Kamera und Bildschirm**

* `/api/v1/camera/on`, `/off`, `/power-off`, `/switch`
* `/api/v1/camera/quality`, `/speed`, `/zoom`, `/exposure`
* `/api/v1/camera/resolutions`, `/change-resolution`
* `/api/v1/camera/flash`, `/flash-screen`, `/record-video`
* `/api/v1/screen/on-off`, `/api/v1/screen/on-temporal`

**Ton**

* `/api/v1/audio/baby/start-stop`, `/api/v1/audio/parents/start-stop`
* `/api/v1/lullaby/start-stop`, `/previous`, `/next`, `/change-media-volume`

**Ältere Namen** (`/cgi/…`)

* `/cgi/version`, `/cgi/nameDevice`, `/cgi/batteryQuery`, `/cgi/offline`, `/cgi/getUpdatingImage`, `/cgi/sensors`

Die Eltern-App ruft zusätzlich `/api/v1/audio-alert/baby/start-stop` auf; in dieser Routentabelle steht der Pfad nicht – wo er bedient wird, ist nicht untersucht.

---

## 11. WiFi Direct (Betrieb ohne Router)

Aus dem Code und den Hilfetexten der App, Version 2.48. **Mit Lela OS nicht getestet.**

**Kurz:** WiFi Direct ändert nur das Netz, nicht die Schnittstelle. Die Kamera spannt ein eigenes Funknetz auf und ist darin immer unter **`192.168.49.1`** erreichbar. Server, Port, Anmeldung, Video- und Tonpfade sind dieselben wie in den Abschnitten 1 bis 3 und 10 – im Code gibt es keinen eigenen Pfad für WiFi Direct.

### Was die Kamera tut

* Sie eröffnet eine WiFi-Direct-Gruppe und ist deren Besitzer (Group Owner). Android vergibt dem Besitzer fest die Adresse `192.168.49.1` und verteilt den Gegenstellen Adressen aus `192.168.49.x`.
* Ab Android 10 legt die App Namen und Passwort selbst fest:
  * Netzname: Vorgabe `DIRECT-WIFI-BABYCAM`, in der App änderbar.
  * Passwort: 8 zufällige Zeichen aus `0-9a-z`, einmal erzeugt und dann gespeichert; in der App änderbar.
  * Band: Vorgabe 2,4 GHz. Mit der Einstellung „Start WiFi Direct on the 5GHz WiFi band“ wählt das Handy das Band selbst – dann kommen Geräte, die nur 2,4 GHz können, nicht mehr hinein.
* Vor Android 10 vergibt das System Namen (`DIRECT-xy-…`) und Passwort; die App liest sie aus und zeigt sie an.
* Danach läuft derselbe Kamera-Bildschirm mit demselben Server auf Port 8080. Wo der Code den Modus abfragt, geht es um Anzeige, Statistik, die eigene Adresse (die der WiFi-Direct-Schnittstelle) und den Mobilfunk-Zugang – nicht um die lokalen Pfade.

### Zwei Wege hinein

| | „WiFi Direct“ | „WiFi Direct Hotspot“ |
| --- | --- | --- |
| Verbindungsaufbau | echtes WiFi Direct: Eltern-App sucht Geräte in der Nähe und fragt die Verbindung an | die Gegenstelle tritt dem Netz der Kamera wie einem gewöhnlichen WLAN bei (Name + Passwort, WPA2) |
| Bestätigung an der Kamera | ja – die Anfrage muss am entsperrten Kamera-Handy angenommen werden | nein, das Passwort genügt |
| Zugangsdaten | keine Eingabe | QR-Code auf dem Kamera-Handy: `WIFI:T:WPA;S:<Netzname>;P:<Passwort>;H:;` oder von Hand |
| Adresse der Kamera | vom System gemeldete Adresse des Gruppenbesitzers, ersatzweise `192.168.49.1` | `192.168.49.1` |
| Geeignet für | Android-Handys mit der Eltern-App | alles mit WLAN: Browser (`http://192.168.49.1:8080`), iPhone, PC – und Lela OS |

### Ablauf nach dem Verbinden (beide Wege gleich)

1. Erreichbarkeit prüfen: `GET http://192.168.49.1:8080/cgi/version` – die Antwort muss mit `1.`, `2.` oder `3.` beginnen. (Die App probiert daneben die Ports 6060 und 8443, siehe Abschnitt 12; Vorgabe ist 8080.)
2. Anmelden: `POST /api/v1/client/online` wie in Abschnitt 1. Als `ip` gehört die **eigene Adresse im Netz der Kamera** hinein (`192.168.49.x`), denn dorthin schickt die Kamera den Ton.
3. Video: `GET /api-stream/v1/video?...` wie in Abschnitt 2.
4. Ton: `audio/baby/start-stop` und `audio/parents/start-stop` wie in Abschnitt 3, UDP-Ports 50001 und 50003, Pakete zu 500 Bytes.

### Für Lela OS

* Der Weg „WiFi Direct Hotspot“ passt ohne neue Schnittstelle: den QR-Code der Kamera mit der vorhandenen WLAN-Einrichtung einlesen (das Format wird gelesen), als Kamera-Adresse `192.168.49.1` eintragen.
* Echtes WiFi Direct (Suchen und Aushandeln) kann das Tab5 nicht.
* Das Tab5 funkt nur auf 2,4 GHz – die 5-GHz-Einstellung der Kamera-App muss aus bleiben (Vorgabe).
* Solange das Tab5 im Netz der Kamera hängt, ist das Heim-WLAN weg: kein Home Assistant, keine Uhrzeit aus dem Netz.

---

## 12. Verschlüsselung (HTTPS)

Aus dem Code der App, Version 2.48. **Am Gerät nicht geprüft** – erste Probe: `https://<Kamera-IP>:8443/cgi/version` im Browser (Zertifikatswarnung ist zu erwarten).

### Was die Kamera anbietet

Beim Start öffnet die Kamera-App drei Zugänge mit **derselben Routentabelle** (Abschnitt 10):

| Port | Transport | Bemerkung |
| --- | --- | --- |
| 8080 | HTTP | Vorgabe; von Lela OS benutzt |
| 6060 | HTTP | zweiter Zugang, auch Ausweichport, falls 8080 belegt ist |
| 8443 | **HTTPS (TLS)** | wird zusätzlich gestartet; schlägt der Start fehl, bleibt es still bei den anderen beiden |

Die Eltern-App nimmt `https://` genau dann, wenn ihr Port 8443 ist. Ob sie 8443 als Vorgabe benutzt, bestimmt ein Schalter des Herstellers (`useHttpsByDefault`, Fernkonfiguration über Firebase; Vorgabe im Code, soweit ablesbar: aus).

### Was sich damit verschlüsseln lässt – und was nicht

| Strecke | über HTTPS möglich? |
| --- | --- |
| Anmeldung, Status, Steuerung (`/api/v1/…`) | ja |
| Video (`/api-stream/v1/video`) | ja |
| Babyton als WAV-Strom (`/api-stream/v1/audio`, Abschnitt 8) | ja |
| Babyton über UDP (Port 50001) | **nein** – rohes PCM, im Code keine Verschlüsselung |
| Gegensprechen über UDP (Port 50003) | **nein** – und einen anderen Weg zur Kamera gibt es dafür nicht |
| Passwort der Kamera (`Password`-Kopfzeile / `password=`) | nur über HTTPS geschützt; über HTTP geht es im Klartext |

### Das Zertifikat

* Selbst ausgestellt, `CN=BabyCam`, RSA 2048, gültig 12.07.2023 bis 3022, SHA-256-Fingerabdruck `0A:CE:8C:B2:52:AC:08:3A:78:FF:4E:05:1E:2E:E5:86:53:71:D9:63:0F:56:70:69:25:22:F9:ED:FC:3A:5F:AE`.
* **Es liegt samt privatem Schlüssel in der App (`assets/keystore.bks`) und ist in jeder Installation dasselbe.** Die Eltern-App prüft den Namen im Zertifikat nicht.
* Folge: Die Verschlüsselung schützt vor bloßem Mitlesen (sofern ein Schlüsseltausch mit ECDHE ausgehandelt wird), **nicht** vor jemandem, der sich aktiv als Kamera ausgibt – den Schlüssel dazu hat jeder, der die App hat. Den Fingerabdruck zu prüfen stellt nur sicher, dass die Gegenstelle eine BabyCam-App ist, nicht welche.

### Einordnung für Lela OS

* Im Heim-WLAN und im WiFi-Direct-Netz ist die Funkstrecke bereits durch WPA2/WPA3 verschlüsselt. HTTPS schützt zusätzlich nur vor anderen Geräten **im selben Netz**.
* Preis auf dem Tab5 (nicht gemessen): TLS-Entschlüsselung des Videostroms (Blöcke zu 81920 Bytes je Bild, Abschnitt 9), ein TLS-Verbindungsaufbau je Steuer-Aufruf, solange die Verbindungen nicht wiederverwendet werden, zusätzlicher Speicher je Verbindung.
* Der Babyton ließe sich nur über den WAV-Strom verschlüsseln (TCP: keine Lücken, dafür mehr Verzögerung); das Gegensprechen bliebe in jedem Fall unverschlüsselt.

### Passwortschutz der Kamera

Der Passwortschutz wird in der Kamera-App eingeschaltet. Er gilt auf allen drei Ports gleich (dieselben Bearbeiter).

* **Übergabe:** Kopfzeile `Password: <Wert>` oder Parameter `password=<Wert>` (auch klein geschrieben als Kopfzeile, auch als Parameter mit großem `P`). Die Kopfzeile wird zuerst gelesen.
* **Wert:** das Passwort als Base64 (URL-Alphabet, ohne Füllzeichen, ohne Zeilenumbruch) – so legt die Kamera es ab und so schickt es die Eltern-App. Der Wert ist also dem Passwort gleichwertig und bei jedem Aufruf derselbe.
* **Ohne oder mit falschem Wert:** Der Aufruf scheitert („Invalid password“).
* Abfragen, ob der Schutz an ist: `GET /api/v1/password/enabled`. Prüfen: `POST /api/v1/password/check`.

| Mit Passwort geschützt | Auch ohne Passwort erreichbar |
| --- | --- |
| Video, Standbild, Ton-Strom, Video-Download (`/api-stream/…`, `/stream/…`) | `/cgi/version`, `/api/v1/version` |
| Anmeldung und Abmeldung (`client/online`, `client/offline`) | `/cgi/nameDevice`, `/api/v1/name-device` |
| Ton starten/stoppen (`audio/baby/…`, `audio/parents/…`) | `/api/v1/status` (Akku, Auflösung, Ports …) |
| alle Kamera-, Bildschirm- und Schlaflied-Befehle | `/api/v1/password/enabled`, `/api/v1/password/check` |
| `/cgi/batteryQuery`, `/api/v1/battery`, `/cgi/sensors`, `/cgi/getUpdatingImage`, `/cgi/offline` | `/api/v1/premium`, `/api/v1/update-app` |

### HTTPS und Passwort zusammen – was es gegen jemanden im WLAN bringt

* **Ohne Passwort nützt die Verschlüsselung nichts:** Wer im WLAN ist, holt sich Bild und Ton einfach selbst von der Kamera (Port 8080).
* **Mit Passwort, aber über HTTP:** Das Passwort geht im Klartext über die Leitung und ist mitlesbar.
* **Mit Passwort über HTTPS (Port 8443):** Passwort, Bild und Ton-Strom sind vor Mitlesen geschützt, und ohne Passwort gibt die Kamera weder Bild noch Ton heraus.

Was offen bleibt:

* Die Ports 8080 und 6060 lassen sich nicht abschalten. Jede Gegenstelle, die dort mit Passwort anfragt (die Eltern-App auf dem Handy, solange der Hersteller sie nicht auf HTTPS umstellt; ein Browser mit `http://`), verrät es einem Mitleser.
* Das Zertifikat ist allgemein bekannt (siehe oben): Wer den Verkehr aktiv über sich umleitet, kann sich als Kamera ausgeben und das Passwort abgreifen. Gegen bloßes Mitlesen hilft HTTPS, gegen diesen Angriff nicht.
* Der Ton über UDP (Babyton an Port 50001, Gegensprechen an Port 50003) bleibt unverschlüsselt. Der Babyton lässt sich auf den verschlüsselten Ton-Strom verlegen, das Gegensprechen nicht.
* `status`, Version und Gerätename bleiben ohne Passwort lesbar.

Lela OS kann derzeit weder HTTPS noch das Passwort (feste Adresse `http://<IP>:8080`, keine `Password`-Kopfzeile).
