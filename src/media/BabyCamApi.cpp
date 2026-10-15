// ==============================================================================
// BabyCamApi - Steuerung der Kamera-App
// ==============================================================================
// Alle Befehle laufen ueber eine Warteschlange und werden im Hintergrundtask
// ausgefuehrt. Frueher wurden sie direkt aus den LVGL-Callbacks gerufen - ein
// synchroner HTTP-Request mit 1,5 s Timeout, waehrend der Main-Loop den
// LVGL-Lock hielt. Die Oberflaeche stand dabei still.
//
// Ausnahme: die beiden Audio-Umschaltbefehle bleiben synchron. Sie werden aus
// dem Audio-Besitzer-Task gerufen, wo die Reihenfolge gegenueber der
// I2S-Umschaltung zaehlt.
//
// Endpunkte nach api_RE.md.
// ==============================================================================

#include "BabyCamApi.h"
#include "SharedData.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

volatile bool isBabyApiActive = false;

static TaskHandle_t  s_task   = nullptr;
static QueueHandle_t s_cmdQ   = nullptr;
static uint32_t s_lastHeartbeat  = 0;
static uint32_t s_lastStatusPoll = 0;

static const uint16_t HTTP_TIMEOUT_MS = 1500;

// ------------------------------------------------------------------------------
// V-05: Zwischenspeicher fuer die Statusantwort
// ------------------------------------------------------------------------------
// `camStatus`, `currentCamRes` und `camResolutions[]` sind Arduino-Strings und
// werden von der Oberflaeche gelesen. Wuerde dieser Task sie direkt
// beschreiben, gaebe eine Zuweisung den alten Puffer frei, waehrend ein
// GUI-Task noch darauf zeigt - ein Use-after-free, der sich als beliebiger
// Absturz zeigt und nie reproduzierbar ist.
//
// Deshalb schreibt der Task nur in feste Zeichenpuffer (keine Freigabe, keine
// Umlagerung) und setzt ein Flag. Die Strings selbst fuellt
// `BabyCamApi_Update()`, das aus der Hauptschleife gerufen wird - demselben
// Kontext, aus dem die Oberflaeche sie liest.
//
// Die Puffer sind so bemessen, dass das letzte Byte nie beschrieben wird und
// dauerhaft '\0' bleibt. Liest die Oberflaeche mitten in eine Aktualisierung
// hinein, bekommt sie dadurch schlimmstenfalls einen unvollstaendigen, aber
// immer terminierten Text - nie einen Ueberlauf.
#define CAM_TEXT_LEN 24
#define CAM_RES_MAX  10

static char s_stgStatus[CAM_TEXT_LEN]   = {0};
static char s_stgRes[CAM_TEXT_LEN]      = {0};
static char s_stgResList[CAM_RES_MAX][CAM_TEXT_LEN] = {{0}};
static volatile int  s_stgResCount = 0;
static volatile bool s_stgDirty    = false;

static void stageText(char* dst, const char* src) {
    snprintf(dst, CAM_TEXT_LEN - 1, "%s", src ? src : "");
    dst[CAM_TEXT_LEN - 1] = '\0';
}

// ------------------------------------------------------------------------------
// Kommandos
// ------------------------------------------------------------------------------
enum ApiOp : uint8_t {
    OP_QUALITY = 1, OP_SPEED, OP_ZOOM, OP_EXPOSURE, OP_RESOLUTION,
    OP_FLASH, OP_SCRFLASH, OP_LENSE, OP_SCREEN, OP_CAMERA_TOGGLE, OP_POWEROFF,
    OP_RECORD,
};

struct ApiCmd { uint8_t op; int32_t a; int32_t b; };

static void enqueue(ApiOp op, int32_t a = 0, int32_t b = 0) {
    if (!s_cmdQ) return;
    ApiCmd c = { (uint8_t)op, a, b };
    xQueueSend(s_cmdQ, &c, 0);
}

// ------------------------------------------------------------------------------
// HTTP-Bausteine
// ------------------------------------------------------------------------------
// Rueckgabe: HTTP-Statuscode, negativ bei Verbindungsfehler, -1000 ohne WLAN.
static int sendPost(const char* endpoint, const char* payload) {
    if (WiFi.status() != WL_CONNECTED) return -1000;

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);

    char url[160];
    snprintf(url, sizeof(url), "http://%s:8080%s", streamIp.c_str(), endpoint);
    if (!http.begin(url)) return -1001;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    const int code = http.POST(payload);
    http.end();
    return code;
}

static void sendGet(const char* endpoint) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);

    char url[160];
    snprintf(url, sizeof(url), "http://%s:8080%s", streamIp.c_str(), endpoint);
    if (!http.begin(url)) return;

    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");
    http.GET();
    http.end();
}

// ------------------------------------------------------------------------------
// Ausfuehrung eines Kommandos
// ------------------------------------------------------------------------------
static void execute(const ApiCmd& c) {
    char payload[144];

    switch ((ApiOp)c.op) {

    case OP_QUALITY:
        snprintf(payload, sizeof(payload), "{\"level\":%ld,\"uuid\":\"%s\"}",
                 (long)c.a, camUuid.c_str());
        sendPost("/api/v1/camera/quality", payload);
        break;

    case OP_SPEED:
        snprintf(payload, sizeof(payload), "{\"level\":%ld,\"uuid\":\"%s\"}",
                 (long)c.a, camUuid.c_str());
        sendPost("/api/v1/camera/speed", payload);
        break;

    case OP_ZOOM:
        snprintf(payload, sizeof(payload), "{\"percentage\":%ld}", (long)c.a);
        sendPost("/api/v1/camera/zoom", payload);
        break;

    case OP_EXPOSURE:
        snprintf(payload, sizeof(payload), "{\"percentage\":%ld,\"uuid\":\"%s\"}",
                 (long)c.a, camUuid.c_str());
        sendPost("/api/v1/camera/exposure", payload);
        break;

    case OP_RESOLUTION:
        // forceChangeResolution bewusst true: mit false darf die Kamera
        // ablehnen, was sie nachweislich getan hat.
        snprintf(payload, sizeof(payload),
                 "{\"forceChangeResolution\":true,\"width\":%ld,\"height\":%ld}",
                 (long)c.a, (long)c.b);
        sendPost("/api/v1/camera/change-resolution", payload);
        break;

    case OP_RECORD:      sendPost("/api/v1/camera/record-video", "{}");   break;
    case OP_FLASH:       sendGet("/api/v1/camera/flash");                 break;
    case OP_SCRFLASH:    sendGet("/api/v1/camera/flash-screen");
                         camScrFlash = !camScrFlash;                      break;
    case OP_LENSE:       sendGet("/api/v1/camera/switch");                break;
    case OP_SCREEN:      sendGet("/api/v1/screen/on-off");                break;
    case OP_POWEROFF:    sendGet("/api/v1/camera/power-off");             break;

    case OP_CAMERA_TOGGLE:
        // Bewusst der Zwischenspeicher und nicht `camStatus`: dieser Task darf
        // die String-Objekte der Oberflaeche nicht einmal lesen.
        if (strcmp(s_stgStatus, "PLAY") == 0) sendGet("/api/v1/camera/off");
        else                                  sendGet("/api/v1/camera/on");
        break;
    }
}

// ------------------------------------------------------------------------------
// Statusabfrage
// ------------------------------------------------------------------------------
static void pollStatus() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(2000);
    http.setConnectTimeout(2000);

    char url[160];
    snprintf(url, sizeof(url), "http://%s:8080/api/v1/status?fast=true", streamIp.c_str());
    if (!http.begin(url)) return;

    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");

    if (http.GET() == HTTP_CODE_OK) {
        // ArduinoJson 7: JsonDocument waechst bei Bedarf, ein zu kleiner
        // fester Puffer verschluckte frueher die ganze Antwort.
        JsonDocument doc;
        if (deserializeJson(doc, http.getStream()) == DeserializationError::Ok) {
            camBatteryPercent = doc["batteryLevel"] | camBatteryPercent;
            camScreenOn       = doc["screenOn"]     | camScreenOn;
            camFlash          = doc["flash"]        | camFlash;
            camZoom           = doc["zoom"]         | camZoom;

            if (doc["cameraStatus"].is<const char*>()) {
                stageText(s_stgStatus, (const char*)doc["cameraStatus"]);
            }
            // Tatsaechlich eingestellte Aufloesung mitfuehren - die Kamera darf
            // von der angeforderten abweichen.
            if (doc["resolution"].is<const char*>()) {
                stageText(s_stgRes, (const char*)doc["resolution"]);
            }
            if (doc["resolutions"].is<JsonArray>()) {
                JsonArray arr = doc["resolutions"];
                int n = 0;
                for (JsonVariant v : arr) {
                    if (n < CAM_RES_MAX) stageText(s_stgResList[n++], v.as<const char*>());
                }
                s_stgResCount = n;
            }

            // Erst ganz zum Schluss freigeben: die Hauptschleife soll nie eine
            // halb gefuellte Liste uebernehmen.
            s_stgDirty = true;
        }
    }
    http.end();
}

// ------------------------------------------------------------------------------
// Abmeldung (api_RE.md Schritt 3)
// ------------------------------------------------------------------------------
// Bewusst hier und nicht im Video-Task: dieser Task haelt die Sitzung mit
// seinem Heartbeat am Leben, also muss er sie auch beenden. Eine Abmeldung aus
// dem Video-Task waere 8 s spaeter durch den naechsten Heartbeat widerrufen -
// und wuerde nebenbei das noch laufende Babyphone-Audio kappen.
// volatile: geschrieben vom API-Task, gelesen ueber BabyCamApi_IsSessionReady()
// aus der Hauptschleife.
static volatile bool s_registered = false;

static void sendOffline() {
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"uuidClient\":\"%s\"}", camUuid.c_str());
    sendPost("/api/v1/client/offline", payload);
}

// ------------------------------------------------------------------------------
// Hintergrundtask
// ------------------------------------------------------------------------------
static void apiTask(void*) {
    while (true) {
        const bool online = isBabyApiActive && wifiStarted &&
                            WiFi.status() == WL_CONNECTED;

        if (!online) {
            // Waren wir angemeldet, sauber abmelden - aber nur, wenn das
            // Netz noch steht. Ohne WLAN ist ohnehin nichts zuzustellen, und
            // die Kamera raeumt die Sitzung dann selbst ab.
            if (s_registered) {
                if (WiFi.status() == WL_CONNECTED) sendOffline();
                s_registered = false;
            }

            s_lastHeartbeat = 0;
            s_lastStatusPoll = 0;
            // Anstehende Kommandos verwerfen, sie waeren ohnehin nicht zustellbar.
            ApiCmd drop;
            while (xQueueReceive(s_cmdQ, &drop, 0) == pdTRUE) {}
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        const uint32_t now = millis();

        // Heartbeat ZUERST. api_RE.md: zwingend alle 10 s, sonst schliesst die
        // Kamera Video- und Audio-Sockets.
        //
        // Er stand frueher hinter der Kommandoabarbeitung. Ein Schieberegler
        // fuellt die Warteschlange aber im Handumdrehen mit einem Dutzend
        // Befehlen, und jeder darf bis zu 1,5 s brauchen - der Heartbeat waere
        // dann bis zu 18 s ausgeblieben und die Kamera haette mitten in der
        // Bedienung den Stream gekappt. Ein einzelner POST vor den Kommandos
        // kostet dagegen nichts Spuerbares.
        // `s_lastHeartbeat == 0` heisst: noch nie angemeldet. Dann SOFORT.
        //
        // Ohne diese Bedingung schlug erst die Differenz `now - 0 > 8000` zu,
        // die Anmeldung kam also fruehestens 8 s nach dem Start. Die
        // Statusabfrage darunter feuerte aber schon nach 5 s - die Kamera sah
        // damit eine unangemeldete HTTP-Anfrage und zeigte sie als
        // "Babycam Web" an, so wie einen Browser. Erst mit dem ersten
        // Heartbeat wurde daraus "Lela OS".
        if (s_lastHeartbeat == 0 || now - s_lastHeartbeat > 8000) {
            s_lastHeartbeat = now;
            char payload[144];
            snprintf(payload, sizeof(payload),
                     "{\"deviceName\":\"Lela OS\",\"ip\":\"%s\",\"uuidClient\":\"%s\"}",
                     WiFi.localIP().toString().c_str(), camUuid.c_str());
            const int code = sendPost("/api/v1/client/online", payload);
            // Nur die erste Anmeldung einer Sitzung melden, nicht jeden Heartbeat.
            if (!s_registered) {
                Serial.printf("[BabyCamApi] Sitzung bei der Kamera angemeldet: HTTP %d\n", code);
            }
            s_registered = true;
            continue;
        }

        // Danach die Bedienung: sie kommt vom Benutzer und soll sich sofort
        // auswirken.
        ApiCmd c;
        if (xQueueReceive(s_cmdQ, &c, pdMS_TO_TICKS(100)) == pdTRUE) {
            execute(c);
            continue;
        }

#if NET_QUIET_DURING_VIDEO
        if (isStreamActive) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
#endif

        // api_RE.md nennt 3 bis 5 Sekunden. Bewusst das obere Ende: jede
        // Abfrage ist eine eigene TCP-Verbindung zur selben Kamera und
        // konkurriert waehrend der Wiedergabe mit dem Videostrom um dieselbe
        // Leitung. Fuer Akkustand und Aufloesung reichen 5 s vollkommen.
        // Erst abfragen, wenn die Sitzung steht. Eine Abfrage vor der
        // Anmeldung ist fuer die Kamera ein fremder Web-Client.
        if (s_registered && (now - s_lastStatusPoll > 5000)) {
            s_lastStatusPoll = now;
            pollStatus();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void BabyCamApi_Init() {
    if (s_task) return;

    s_cmdQ = xQueueCreate(12, sizeof(ApiCmd));
    if (!s_cmdQ) {
        Serial.println("[BabyCamApi] FEHLER: Kommando-Queue fehlt, Kamerasteuerung bleibt aus.");
        return;
    }

    if (xTaskCreatePinnedToCore(apiTask, "ApiTask", 6144, NULL, 2, &s_task, 0) != pdPASS) {
        Serial.println("[BabyCamApi] FEHLER: Task laesst sich nicht anlegen.");
        vQueueDelete(s_cmdQ);
        s_cmdQ = nullptr;
        s_task = nullptr;
    }
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle - kehrt sofort zurueck
// ------------------------------------------------------------------------------
void BabyCamApi_SetQuality(int qual) { camQuality = qual; enqueue(OP_QUALITY, qual); }
void BabyCamApi_SetSpeed(int speed)  { camSpeed   = speed; enqueue(OP_SPEED, speed); }
void BabyCamApi_SetZoom(int zoom)    { camZoom    = zoom;  enqueue(OP_ZOOM, zoom); }
void BabyCamApi_SetExposure(int pct) { enqueue(OP_EXPOSURE, pct); }
void BabyCamApi_SetResolution(int w, int h) { enqueue(OP_RESOLUTION, w, h); }
void BabyCamApi_RecordVideo()        { enqueue(OP_RECORD); }
void BabyCamApi_ToggleFlash()        { enqueue(OP_FLASH); }
void BabyCamApi_ToggleScrFlash()     { enqueue(OP_SCRFLASH); }
void BabyCamApi_ToggleLense()        { enqueue(OP_LENSE); }
void BabyCamApi_ToggleScreen()       { enqueue(OP_SCREEN); }
void BabyCamApi_ToggleCamera()       { enqueue(OP_CAMERA_TOGGLE); }
void BabyCamApi_PowerOff()           { enqueue(OP_POWEROFF); }

// Zerlegt "1280x720". Gibt false zurueck, wenn das Format nicht stimmt.
static bool parseRes(const char* text, int* w, int* h) {
    if (!text || !*text) return false;
    const char* x = strchr(text, 'x');
    if (!x || x == text) return false;

    *w = atoi(text);
    *h = atoi(x + 1);
    return (*w > 0 && *h > 0);
}

void BabyCamApi_ApplyBestResolution(int reqW, int reqH) {
    const int count = s_stgResCount;
    if (count <= 0) return;

    int bestW = 0, bestH = 0, bestArea = 0;
    const int reqArea = reqW * reqH;

    for (int i = 0; i < count && i < CAM_RES_MAX; i++) {
        int w = 0, h = 0;
        if (!parseRes(s_stgResList[i], &w, &h)) continue;

        // Genau die gewuenschte Aufloesung schlaegt jede Annaeherung.
        if (w == reqW && h == reqH) { bestW = w; bestH = h; break; }

        const int area = w * h;
        if (area <= reqArea && area > bestArea) { bestArea = area; bestW = w; bestH = h; }
    }

    // Nichts Passendes gefunden: die erste gemeldete Aufloesung nehmen.
    if (bestW == 0 && !parseRes(s_stgResList[0], &bestW, &bestH)) return;

    if (bestW > 0 && bestH > 0) BabyCamApi_SetResolution(bestW, bestH);
}

// ------------------------------------------------------------------------------
// Uebernahme in die Oberflaeche - MUSS aus der Hauptschleife gerufen werden
// ------------------------------------------------------------------------------
bool BabyCamApi_IsSessionReady() {
    return isBabyApiActive && s_registered;
}

void BabyCamApi_Update() {
    if (!s_stgDirty) return;
    s_stgDirty = false;

    camStatus     = s_stgStatus;
    currentCamRes = s_stgRes;

    const int n = s_stgResCount;
    for (int i = 0; i < n && i < CAM_RES_MAX; i++) {
        camResolutions[i] = s_stgResList[i];
    }
    camResCount = (n > CAM_RES_MAX) ? CAM_RES_MAX : n;
}

// ------------------------------------------------------------------------------
// Audio-Umschaltung: bewusst synchron, siehe Kopf der Datei
// ------------------------------------------------------------------------------
void BabyCamApi_SetBabyAudio(bool play) {
    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"audioBabyCompressed\":false,\"ip\":\"%s\",\"status\":\"%s\"}",
             WiFi.localIP().toString().c_str(), play ? "PLAY" : "STOP");
    const int code = sendPost("/api/v1/audio/baby/start-stop", payload);

    // Eine Zeile je Umschaltung. Ohne sie ist "kein Ton" nicht von "Kamera
    // hat den Befehl abgelehnt" zu unterscheiden.
    Serial.printf("[BabyCamApi] Babyphone-Ton %s an %s: HTTP %d\n",
                  play ? "PLAY" : "STOP", WiFi.localIP().toString().c_str(), code);
}

void BabyCamApi_SetParentAudio(bool play) {
    char payload[160];
    snprintf(payload, sizeof(payload), "{\"ip\":\"%s\",\"status\":\"%s\"}",
             WiFi.localIP().toString().c_str(), play ? "PLAY" : "STOP");
    const int code = sendPost("/api/v1/audio/parents/start-stop", payload);
    Serial.printf("[BabyCamApi] Gegensprechen %s: HTTP %d\n", play ? "PLAY" : "STOP", code);
}
