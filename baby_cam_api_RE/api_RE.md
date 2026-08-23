# 📚 Babycam API – Offizielle Spezifikation & Client-Guidelines

**Fokus:** Rein lokaler Betrieb (WLAN / WiFi Direct) ohne Cloud-Zwang.
**Basis-URL:** `http://<Kamera-IP>:8080` (Die IP stammt typischerweise aus einem mDNS-Broadcast oder dem QR-Code-Scan).
**Authentifizierung:** Lela OS identifiziert sich gegenüber der Kamera mit einer statischen UUIDv4.
**Standard-Header:** Alle POST/GET Requests benötigen zwingend folgende Header-Struktur:

* `uuid`: `<CLIENT_UUID>` (Format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`, z.B. `550e8400-e29b-41d4-a716-446655440000`)
* `versionApp`: `<APP_VERSION>` (z.B. `"2.48"`)
* `Content-Type`: `application/json` (nur bei POST-Requests)

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
* **Netzwerk-Routing:** Die Kamera feuert UDP-Pakete an den lokalen Client-Port **50001** (`audioBabyRecorderPort`).

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


* **Netzwerk-Routing:** Der Client muss nun UDP-Audio-Chunks (Empfehlung: 512 bis 1024 Bytes) an die IP der Kamera auf Ziel-Port **50003** (`audioParentPlayerPort`) senden.

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
* Body: `{"forceChangeResolution": false, "width": 1280, "height": 720}` *(Führt serverseitig zu einem Stream-Neustart)*



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



### 🔋 Power Management & Standby

* **Stream pausieren (Standby):** `GET /api/v1/camera/off` -> `{"status": "OFF"}`
* **Stream fortsetzen:** `GET /api/v1/camera/on` -> `{"status": "ON"}`
* **Physisches Geräte-Display umschalten:**
* `GET /api/v1/screen/on-off`
* Response: `{"screenOffTimeout": 30, "status": "ON"}` oder `{"screenOffTimeout": 0, "status": "OFF"}`


* **Remote Power-Off (Kompletter Shutdown):**
* `GET /api/v1/camera/power-off` -> `{"status": "OK"}`
* *Achtung:* Fährt die Smartphone/Kamera-App vollständig herunter. Das Gerät ist danach nicht mehr per Netzwerk erreichbar.