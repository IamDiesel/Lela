
---

# 📚 Babycam API – Inoffizielle Dokumentation

**Fokus:** Rein lokaler Betrieb (WLAN / WiFi Direct) ohne Cloud-Zwang.
**Basis-URL:** `http://<Kamera-IP>:8080` (Die IP stammt z. B. aus dem QR-Code-Scan).
**Authentifizierung:** Übermittlung einer statischen `uuidClient` bei Steuerungsbefehlen.

---

## 1. Lokaler Verbindungsaufbau & Keep-Alive (Die Boot-Sequenz)

Da die Cloud deaktiviert ist, verbindet sich Lela OS direkt mit der Kamera-IP. Der Ablauf ist extrem schlank:

### Schritt 1: Lokaler Health-Check (Ping-Ersatz)

Prüft, ob der Webserver der Kamera hochgefahren und erreichbar ist.

* **Taktung:** Einmalig beim Start, danach **alle 10 Sekunden** als Keep-Alive im Hintergrund-Task!
* **Methode:** `GET /cgi/version`
* **Response:** `2.47` (Plaintext Firmware-Version)

### Schritt 2: Client-Anmeldung (Session Start)

Registriert Lela OS bei der Kamera, damit diese den Stream nicht wegen Inaktivität schließt.

* **Methode:** `POST /api/v1/client/online`
* **Body:** `{"deviceName":"Lela OS Monitor", "ip":"<IP_DES_ESP32>", "uuidClient":"<LELA_OS_UUID>"}`
* **Response:** `{"status":"OK"}`

*(Nach diesem Schritt kann Lela OS direkt den Video- und Audio-Stream öffnen).*

---

## 2. Audio- und Videostreams (Format & Routing)

Die eigentlichen Mediendaten laufen über eigene Ports und Sockets.

### 🎥 Video-Stream (Downlink)

* **Methode:** `GET`
* **URL:** `/api-stream/v1/video?id=<stream-id>&uuid=<lela-os-uuid>&quality=50`
* **Format:** MJPEG-Stream (Motion JPEG) über HTTP `multipart/x-mixed-replace`.

### 👶 Baby-Ton hören (Audio Downlink)

* **Start-Befehl:** `POST /api/v1/audio/baby/start-stop`
* **Body:** `{"audioBabyCompressed":false, "ip":"<IP_DES_ESP32>", "status":"PLAY"}`
* **Response:** `{"audioBabyCompressed":false, "buffer":3528, "status":"PLAY"}`


* **Format:** Unkomprimiertes Raw PCM (LPCM). Chunk-Größe: 3528 Bytes (entspricht ca. 40ms Audio bei 44.1kHz, 16-Bit Mono).
* **Port:** Die Kamera streamt das Audio über eine TCP-Verbindung an den `audioParentPlayerPort` (meist **Port 50003**).

### 🎤 Mit dem Baby sprechen (Audio Uplink / Intercom)

* **Start-Befehl (Push-To-Talk):** `POST /api/v1/audio/parents/start-stop`
* **Body:** `{"ip":"<IP_DES_ESP32>", "status":"PLAY"}` (Beim Loslassen `"STOP"`)


* **Port:** Lela OS schiebt die aufgenommenen Mikrofon-Audiodaten (Raw PCM) an die Kamera-IP auf dem `audioBabyRecorderPort` (meist **Port 50001**).

---

## 3. Kamera-Status & Sensoren (Automatisches Polling)

Dieser Endpunkt wird periodisch von der Lela OS GUI abgefragt, um die Top-Bar (Batterie, WLAN) zu aktualisieren.

* **Taktung:** Alle paar Sekunden (z. B. alle 5s).
* **Methode:** `GET /api/v1/status?fast=true`
* **Response (Wichtige Felder für Lela OS):**
```json
{
  "batteryLevel": 100,
  "batteryIsCharging": false,
  "wifiQuality": 100,
  "cameraHasFlash": true,
  "flash": false,
  "cameraStatus": "PLAY",
  "audioBabyRecorderPort": 50001,
  "audioParentPlayerPort": 50003,
  "resolution": "1280x720",
  "version": "2.47"
}

```



---

## 4. Manuelle Hardware-Steuerung & Einstellungen

Diese Befehle werden *nur* gesendet, wenn der Nutzer aktiv etwas in der GUI verstellt.

### ⚙️ Video-Einstellungen (Manuell via UI)

* **Framerate drosseln/erhöhen:** `POST /api/v1/camera/speed`
* Body: `{"level": 100, "uuid": "<LELA_OS_UUID>"}` *(Werte: 100 für Echtzeit, 30/20 für Drosselung, 0 für Pause)*


* **Kompression/Qualität:** `POST /api/v1/camera/quality`
* Body: `{"level": 50, "uuid": "<LELA_OS_UUID>"}`


* **Auflösung ändern:** `POST /api/v1/camera/change-resolution`
* Body: `{"forceChangeResolution":false, "height":720, "width":1280}`


* **Auflösungen abfragen:** `GET /api/v1/camera/switch`
* Response: `{"resolutions":["1280x720", "800x450", "640x480", "320x240"]}`



### 🔍 PTZ / Digitaler Zoom

* **Methode:** `POST /api/v1/camera/zoom`
* **Body:** `{"percentage":42}` (0 bis 100)

### 🔆 Nachtlicht (Taschenlampe)

* **Blitz einschalten:** `GET /api/v1/camera/flash` (Response: `{"status":"ON"}`)
* **Blitz ausschalten:** `GET /api/v1/camera/flash` (Response: `{"status":"OFF"}`)
* **Bildschirmlicht:** `GET /api/v1/camera/flash-screen` (Toggelt Display-Helligkeit als Licht)

### 🚫 Kamera ausschalten (Remote Shutdown)

* **Methode:** `GET /api/v1/camera/off`
* **Response:** `{"status":"OFF"}`

---