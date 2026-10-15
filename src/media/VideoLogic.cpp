#pragma GCC optimize ("O2")
// ==============================================================================
// VideoLogic - Steuerung der Videowiedergabe
// ==============================================================================
// Aufbau (siehe doc/video_refactoring.md, 2.18):
//
//   [VideoNet]  HTTP/MJPEG lesen  ->  Eingabeslot  ->  Warteschlange
//                                                          |
//   [VideoDec]  <---------------------------------------- Slot
//               dekodieren (VideoDecoder) -> anzeigen -> Slot zurueckgeben
//
// Die Trennung ist der Grund fuer die Bildrate: frueher lag der Decoder
// waehrend des Downloads still und der Socket waehrend der Dekodierung.
// Gemessen waren 167 ms Download und 12 ms Verarbeitung je Bild - sequenziell
// 179 ms (5,5 fps), ueberlappt nur noch das Maximum der beiden.
//
// Der Anzeigepfad kommt ohne PPA aus: das Bild wird in Originalgroesse mittig
// ausgegeben. Wird die Kamera erfolgreich auf die Panelaufloesung gezogen,
// fuellt es den Schirm ohnehin 1:1.
// ==============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <lwip/sockets.h>     // select() - siehe waitForData()
#include <ArduinoJson.h>
#include <M5Unified.hpp>
#include <lvgl.h>

// DebugFlags zuerst: VIDEO_USE_PPA entscheidet ueber den PPA-Header. Stuende
// die Bedingung davor, waere das Makro dort noch nicht definiert - der Header
// fiele aus, der spaetere Code waere aber trotzdem aktiv.
#include "DebugFlags.h"

#if VIDEO_USE_PPA
  #include <LGFX_PPA.hpp>
#endif

#include "VideoLogic.h"
#include "VideoDecoder.h"
#include "VideoOverlay.h"
#include "SharedData.h"
#include "Dma2dGuard.h"
#include "BabyCamApi.h"
#include "ViewBaby.h"

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

// ------------------------------------------------------------------------------
// Abstimmung
// ------------------------------------------------------------------------------
// Panelaufloesung - Ziel der Aushandlung mit der Kamera.
static const int PANEL_W = 1280;
static const int PANEL_H = 720;

// Die Kamera braucht nach dem GET bis zu 3 s, bis der Encoder laeuft.
// api_RE.md verlangt deshalb ausdruecklich mehr als 5000 ms.
static const uint16_t HTTP_STREAM_TIMEOUT_MS = 8000;
static const uint16_t HTTP_CTRL_TIMEOUT_MS   = 2500;

// ACHTUNG: WiFiClient::setTimeout() erwartet SEKUNDEN, nicht Millisekunden -
// es ueberschreibt Stream::setTimeout(ms) mit einer anderen Einheit. Ein hier
// gut gemeintes "50" haette den Socket 50 Sekunden blockieren lassen und jede
// Stall-Erkennung ausgehebelt.
static const uint32_t STREAM_READ_TIMEOUT_S = 1;


// Kein Bild mehr innerhalb dieser Zeit => Verbindung gilt als tot.
static const uint32_t STREAM_STALL_MS = 8000;

// Wartezeiten zwischen Verbindungsversuchen (Backoff).
static const uint32_t RECONNECT_MIN_MS = 1000;
static const uint32_t RECONNECT_MAX_MS = 8000;

// So viele Kopfzeilen ohne Content-Length gelten als Synchronisationsverlust.
static const int MAX_GARBAGE_LINES = 40;

// Obergrenze fuer eine einzelne Wartepause in select(). 20 ms ist der Wert,
// mit dem der Videopfad lange stabil gelaufen ist. select() kehrt ohnehin
// sofort zurueck, sobald ein Byte eintrifft.
static const uint32_t IDLE_WAIT_MS = 20;

// Normale Blockgroesse der Kamera. Groessere Bloecke werden angenommen, aber
// einmal gemeldet - sie erklaeren eine niedrige Bildrate.
static const int CAM_BLOCK_NORMAL = 81920;

// ------------------------------------------------------------------------------
// Fristen
// ------------------------------------------------------------------------------
// millis() laeuft nach 49,7 Tagen ueber. Ein direkter Vergleich
// `millis() > deadline` schlaegt dann dauerhaft an und reisst bei jedem Bild
// die Verbindung ab - bei einem Geraet, das monatelang durchlaeuft, ist das
// kein theoretischer Fall. Die Differenz in einem vorzeichenbehafteten Typ
// ueberlebt den Ueberlauf.
static inline bool deadlineReached(uint32_t deadlineMs) {
    return (int32_t)(millis() - deadlineMs) > 0;
}

// ------------------------------------------------------------------------------
// Slotverwaltung
// ------------------------------------------------------------------------------
struct FrameRef {
    uint8_t  slot;
    uint32_t size;
};

static QueueHandle_t s_freeQ = nullptr;   // Slots, die der Netzpfad fuellen darf
static QueueHandle_t s_fullQ = nullptr;   // gefuellte Slots fuer den Decoder

static TaskHandle_t s_netTask = nullptr;
static TaskHandle_t s_decTask = nullptr;

static volatile bool s_running = false;   // Laufzeitflag beider Tasks

// Ziel der dekodierten Bilder. Geschrieben vom BabyService (loopTask), gelesen
// vom Decoder-Task; ein einzelnes Byte, also ohne Sperre konsistent.
static volatile VideoOutput s_output = VideoOutput::None;

// Video-Overlay. Das Rechteck wird unter der LVGL-Sperre geschrieben und unter
// ihr gelesen (renderFrame) - so sieht der Decoder nie ein halb gesetztes.
struct PipRect { int16_t x, y, w, h; };
static PipRect s_pipRect = { 0, 0, 0, 0 };
static volatile uint16_t s_pipIntervalMs   = 200;   // 1000 / fps
static volatile uint32_t s_pipLastDecodeMs = 0;
static volatile uint32_t s_pipLastFrameMs  = 0;     // 0 = noch nie gezeichnet

// Von der Kamera tatsaechlich geliefert (nicht das, was wir angefordert haben).
static volatile int s_actualW = 0;
static volatile int s_actualH = 0;

// LVGL-Bilddeskriptoren, einer je Ausgabeslot.
static lv_img_dsc_t s_imgDsc[VIDEO_OUT_SLOTS] = {};

// ------------------------------------------------------------------------------
// Messwerte
// ------------------------------------------------------------------------------
static volatile uint32_t s_statDownloadUs = 0;
static volatile uint32_t s_statDecodeUs   = 0;
static volatile uint32_t s_statRenderUs   = 0;
static volatile uint32_t s_statBytes      = 0;
static volatile uint32_t s_statFrames     = 0;
static volatile uint32_t s_statDropped    = 0;
static volatile uint32_t s_statWaitUs     = 0;   // Warten auf den Kopf des naechsten Bildes
static volatile uint32_t s_statRecvUs     = 0;   // reines Empfangen der Bilddaten
static volatile uint32_t s_statIdleUs     = 0;   // davon reine Wartezeit ohne Daten
static volatile uint32_t s_statIdleMaxUs  = 0;   // laengste Einzelpause im Fenster
static volatile uint32_t s_statWaitCnt    = 0;   // wie oft je Bild auf Daten gewartet wurde
static volatile uint32_t s_statLockUs     = 0;
static volatile uint32_t s_statPushUs     = 0;

// Die Zaehler werden vom Netz- und vom Decoder-Task beschrieben. `volatile`
// haelt sie aus den Registern heraus, `++` ist darauf aber seit C++20
// abgekuendigt (der Standard laesst die Reihenfolge von Lesen und Schreiben
// offen). Ein ausgeschriebenes Lesen-Rechnen-Schreiben tut dasselbe und ist
// eindeutig. Genauigkeit spielt hier ohnehin keine Rolle: es sind reine
// Diagnosewerte, keine Steuergroessen.
static inline void statInc(volatile uint32_t& v) { v = v + 1; }

// Wartet, laesst sich aber vom Stopp-Wunsch unterbrechen.
//
// Die Backoff-Pausen sind bis zu 8 s lang. Wer in dieser Zeit auf Pause und
// sofort wieder auf Play tippt, wartet sonst, bis der alte Task seinen
// gesamten Verbindungsaufbau abgearbeitet hat - VideoLogic_Start() legt
// naemlich erst dann einen neuen Task an, wenn der alte verschwunden ist.
// Rueckgabe: false, wenn abgebrochen werden soll.
static bool waitInterruptible(uint32_t ms) {
    const uint32_t deadline = millis() + ms;
    while (s_running && !deadlineReached(deadline)) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return s_running;
}

// ------------------------------------------------------------------------------
// Statusanzeige - immer ueber den LVGL-Lock
// ------------------------------------------------------------------------------
static void setStatus(const char* msg) {
    if (lvgl_port_lock(100)) {
        ViewBaby_SetStatus(msg);
        lvgl_port_unlock();
    }
}

// ==============================================================================
// Kamera-Steuerung
// ==============================================================================

// Kleiner HTTP-GET, der den Rumpf als String zurueckgibt. Bewusst begrenzt.
static bool httpGetString(const String& url, String& out, uint16_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(timeoutMs);
    if (!http.begin(url)) return false;

    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");

    const int code = http.GET();
    bool ok = false;
    if (code == HTTP_CODE_OK) {
        out = http.getString();
        ok = true;
    }
    http.end();
    return ok;
}

static bool httpPostJson(const String& url, const String& body, uint16_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(timeoutMs);
    if (!http.begin(url)) return false;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("uuid", camUuid);
    http.addHeader("versionApp", "2.48");

    const int code = http.POST(body);
    http.end();
    return (code == HTTP_CODE_OK);
}

static String camBase() {
    return "http://" + streamIp + ":8080";
}

// Health-Check. Billig und schnell, verhindert lange Timeouts im Stream-GET.
static bool camPing() {
    String v;
    return httpGetString(camBase() + "/cgi/version", v, HTTP_CTRL_TIMEOUT_MS);
}

// Client anmelden. Muss laut api_RE.md alle 10 s wiederholt werden - das
// erledigt BabyCamApi::apiTask im Hintergrund.
static bool camLogin() {
    // Derselbe Geraetename wie im Heartbeat von BabyCamApi. Zwei Namen unter
    // derselben uuidClient sahen fuer die Kamera nach zwei Anmeldungen aus.
    String body = String("{\"deviceName\":\"Lela OS\",\"ip\":\"") +
                  WiFi.localIP().toString() +
                  "\",\"uuidClient\":\"" + camUuid + "\"}";
    return httpPostJson(camBase() + "/api/v1/client/online", body, HTTP_CTRL_TIMEOUT_MS);
}

// Liest die TATSAECHLICH eingestellte Aufloesung aus dem Statusobjekt.
// Frueher wurde nur gesendet und nie geprueft - die Kamera darf eine
// Anforderung aber ablehnen.
static bool camReadResolution(int* w, int* h) {
    String body;
    if (!httpGetString(camBase() + "/api/v1/status?fast=true", body, HTTP_CTRL_TIMEOUT_MS)) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return false;

    const char* res = doc["resolution"];
    if (!res) return false;

    const String r(res);
    const int x = r.indexOf('x');
    if (x <= 0) return false;

    *w = r.substring(0, x).toInt();
    *h = r.substring(x + 1).toInt();
    return (*w > 0 && *h > 0);
}

// Liest die eingestellte Aufloesung - und AENDERT SIE NICHT.
//
// Frueher hat diese Funktion bei jedem Verbindungsaufbau 1280x720 mit
// forceChangeResolution erzwungen. Das war falsch in zwei Richtungen: es hat
// die Auswahl des Benutzers in den Einstellungen bei jedem Reconnect wieder
// ueberschrieben (die Auflösung liess sich dadurch ueberhaupt nicht mehr
// umstellen), und es hat auch dann die volle Bandbreite gezogen, wenn im
// Fenstermodus eine kleinere Aufloesung voellig ausgereicht haette.
//
// Die Aufloesung gehoert dem Benutzer. Hier wird sie nur noch festgestellt,
// damit die Anzeige weiss, was sie bekommt.
static void camReadActualResolution() {
    int w = 0, h = 0;
    if (camReadResolution(&w, &h)) {
        s_actualW = w; s_actualH = h;
        Serial.printf("[Video] Kamera liefert %dx%d.\n", w, h);
    }
}

// Stream-URL. camEntity darf ein voller URL oder ein Pfad sein.
static String streamUrl() {
    // Die eigene URL IMMER frisch bauen, nie die zwischengespeicherte nehmen.
    //
    // `camEntity` wird beim Booten einmal aus camQuality zusammengesetzt und
    // danach nie wieder angefasst. Der Qualitaetswert darin war damit fuer den
    // Rest der Laufzeit eingefroren - und weil er als Startqualitaet in der
    // URL steht, hat JEDER Neuaufbau der Verbindung die Kamera wieder auf den
    // Bootwert zurueckgestellt. Was per POST /api/v1/camera/quality eingestellt
    // wurde, hielt also nur bis zum naechsten Verbindungsabbruch.
    //
    // Mit der neuen Selbstregelung (Qualitaet senken, wenn die Kamera auf
    // ihren 655360-Byte-Puffer umschaltet) waere daraus eine Endlosschleife
    // geworden: senken, neu verbinden, alter Wert aus der URL, wieder zu gross.
    if (!useCustomUrls && streamIp.length() > 0) {
        return "http://" + streamIp + ":8080/api-stream/v1/video?id=lela-os-stream&uuid=" +
               camUuid + "&quality=" + String(camQuality);
    }

    String url = camEntity;
    if (url.startsWith("http")) return url;
    if (url.length() == 0 || url.startsWith("/local/")) {
        // Vorgabe aus api_RE.md, wenn nichts Sinnvolles konfiguriert ist.
        return camBase() + "/api-stream/v1/video?id=lela-os-stream&uuid=" +
               camUuid + "&quality=" + String(camQuality);
    }
    return camBase() + url;
}

// ==============================================================================
// MJPEG-Parser
// ==============================================================================

// ------------------------------------------------------------------------------
// Eine Kopfzeile lesen - vollstaendig oder gar nicht
// ------------------------------------------------------------------------------
// Ersetzt Stream::readBytesUntil(). Der Unterschied ist kein Stilfrage, er ist
// ein Fehler gewesen:
//
// readBytesUntil() gibt bei Zeitablauf zurueck, was es bis dahin hat - eine
// halbe Zeile gilt ihm als Ergebnis. Stand "Content-Length: 81920" gerade erst
// als "Content-Length: 819" im Puffer, wurde daraus die Laenge 819. Der Rumpf
// war dann um 81101 Bytes zu kurz, das Bild unbrauchbar, der Bytestrom
// ausser Tritt - und die naechsten Kopfzeilen bestanden aus Bilddaten.
// Im Protokoll steht das als "Bild verworfen: 819 Bytes, Anfang FF D8".
//
// Diese Fassung liefert eine Zeile erst, wenn das '\n' wirklich da war, und
// gibt sonst -1 zurueck (Deadline oder Verbindung hin). Dazwischen schlaeft
// sie in select() statt in vTaskDelay() - der Task wacht auf, sobald ein Byte
// eintrifft, nicht erst zum naechsten Systemtick.
//
// Rueckgabe: Zeichen in der Zeile (ohne CR/LF), oder -1.
static void waitForData(WiFiClient* s, uint32_t maxMs);   // weiter unten definiert

static int readHeaderLine(WiFiClient* s, char* out, int maxLen, uint32_t deadlineMs) {
    int n = 0;
    while (s_running && s->connected()) {
        if (deadlineReached(deadlineMs)) return -1;

        const int c = s->read();
        if (c < 0) { waitForData(s, IDLE_WAIT_MS); continue; }
        if (c == '\n') { out[n] = '\0'; return n; }
        if (c == '\r') continue;
        if (n < maxLen - 1) out[n++] = (char)c;
        // Laengere Zeilen werden abgeschnitten, aber weiter bis zum '\n'
        // gelesen. So bleibt der Bytestrom in jedem Fall synchron.
    }
    return -1;
}

// Kopfzeilen-Vergleich ohne strncasecmp().
//
// Bei ASCII-Buchstaben unterscheidet sich Gross- von Kleinschreibung in genau
// einem Bit (0x20). Ein ODER darauf macht aus jedem Buchstaben den
// Kleinbuchstaben - ein Takt je Zeichen, ohne Funktionsaufruf, ohne
// locale-Behandlung. `key` muss deshalb kleingeschrieben uebergeben werden.
static inline bool headerIs(const char* line, int lineLen, const char* key, int keyLen) {
    if (lineLen < keyLen) return false;
    for (int i = 0; i < keyLen; i++) {
        if ((line[i] | 0x20) != key[i]) return false;
    }
    return true;
}

// Vorzeichenlose Dezimalzahl ohne atoi().
//
// Die Multiplikation mit 10 steht als (v<<3)+(v<<1) da: zwei Schiebe- und eine
// Addierbefehl statt eines Multiplizierers. Der Vergleich auf Ziffer ist als
// einzelne vorzeichenlose Subtraktion formuliert - ein Vergleich statt zwei.
static inline int parseUInt(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    int v = 0;
    while ((uint8_t)(*p - '0') <= 9u) {
        v = (v << 3) + (v << 1) + (*p - '0');
        p++;
    }
    return v;
}

// Liest die Multipart-Kopfzeilen und liefert die Laenge des naechsten Bildes.
// Rueckgabe: >0 = Laenge, 0 = noch nichts, -1 = Verbindung unbrauchbar.
static int readFrameHeader(WiFiClient* s, uint32_t deadlineMs) {
    static const char KEY_LENGTH[] = "content-length:";   // 15 Zeichen
    static const char KEY_TYPE[]   = "content-type:";     // 13 Zeichen

    char line[128];
    int contentLength = 0;
    int garbage = 0;

    while (s_running) {
        const int n = readHeaderLine(s, line, sizeof(line), deadlineMs);
        if (n < 0) return -1;

        // Leerzeile nach den Kopfzeilen => Bilddaten folgen.
        if (n == 0) {
            if (contentLength > 0) return contentLength;
            continue;
        }

        if (headerIs(line, n, KEY_LENGTH, 15)) {
            contentLength = parseUInt(line + 15);
            garbage = 0;
        } else if (line[0] == '-' || headerIs(line, n, KEY_TYPE, 13)) {
            garbage = 0;   // Boundary und Content-Type sind erwartbar
        } else if (++garbage > MAX_GARBAGE_LINES) {
            return -1;     // Synchronisation verloren
        }
    }
    return -1;
}

// Wartet, bis am Socket Daten anliegen - und wacht sofort auf, wenn sie kommen.
//
// Das ersetzt ein `vTaskDelay(1)`. Der Unterschied ist gemessen und gross:
// `vTaskDelay(1)` schlaeft eine ganze Systemtick-Periode, unabhaengig davon,
// ob die Daten schon nach 50 Mikrosekunden eintreffen. Bei rund hundert
// Leerlaeufen je Bild waren das 120 der 160 Millisekunden, die als "Empfang"
// gemessen wurden - also mehr als drei Viertel der vermeintlichen Netzzeit.
//
// `select()` blockiert den Task ebenfalls (kein Leerlaufkreisen, der
// Watchdog bleibt zufrieden), gibt ihn aber frei, sobald das erste Byte da
// ist. Die Obergrenze ist nur eine Notbremse.
static void waitForData(WiFiClient* s, uint32_t maxMs) {
    const int fd = s->fd();
    if (fd < 0) { vTaskDelay(1); return; }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = (suseconds_t)(maxMs * 1000);

    select(fd + 1, &rfds, NULL, NULL, &tv);
}

// Liest genau len Bytes. Rueckgabe: true bei Erfolg.
//
// Die Form dieser Schleife ist wichtiger, als sie aussieht. Erst `available()`
// fragen und nur das Vorhandene holen - nicht das ganze Bild anfordern und bei
// Rueckgabe 0 schlafen legen.
//
// Der Unterschied: Wer 80 KB am Stueck anfordert, bekommt zurueck, was gerade
// da ist, und landet danach im Schlaf. Bei Dutzenden Leerlaeufen je Bild summiert
// sich das auf hundert Millisekunden - die in der Messung als "Empfang"
// erscheinen und damit wie ein Netzproblem aussehen, obwohl es Wartezeit in
// dieser Schleife ist. Die Vorgaengerfassung des Projekts hat es richtig gemacht;
// beim Umbau ist diese Eigenschaft verloren gegangen.
//
// s_statIdleUs misst genau diesen Anteil mit, damit die Frage nie wieder
// geraten werden muss.
static bool readFrameBody(WiFiClient* s, uint8_t* dst, size_t len, uint32_t deadlineMs) {
    size_t got = 0;
    while (got < len && s_running && s->connected()) {
        if (deadlineReached(deadlineMs)) return false;

        const int avail = s->available();
        if (avail > 0) {
            size_t want = len - got;
            if ((size_t)avail < want) want = (size_t)avail;

            const int n = s->read(dst + got, want);
            if (n > 0) got += (size_t)n;
        } else {
            // Auf Daten warten statt blind zu schlafen.
            const uint32_t tIdle = micros();
            waitForData(s, IDLE_WAIT_MS);
            const uint32_t dIdle = micros() - tIdle;
            s_statIdleUs += dIdle;
            // Laengste Einzelpause mitschreiben. Der Mittelwert allein sagt
            // nicht, ob wir auf ein Fensterupdate warten (viele gleich lange
            // Pausen) oder auf die Kamera (eine lange, viele kurze).
            if (dIdle > s_statIdleMaxUs) s_statIdleMaxUs = dIdle;
            statInc(s_statWaitCnt);
        }
    }
    return (got == len);
}

// Plausibilitaet UND wahre Bildlaenge.
//
// Die Content-Length der Kamera ist NICHT die Laenge des JPEG: hinter dem
// EOI-Marker (FF D9) folgen noch Zeilenumbrueche und der Anfang der naechsten
// Multipart-Grenze. Wer ein Bild deshalb daran erkennen will, dass die letzten
// beiden Bytes FF D9 sind, verwirft jedes einzelne Bild - lautlos, ohne
// Logzeile. Sichtbar wird das nur als "Keine Bilder mehr", also als
// Netzwerkproblem, das keines ist. Genau dieser Fehler hat den ersten Anlauf
// des Umbaus lahmgelegt; der Vorgaengercode hat den Marker immer rueckwaerts
// gesucht.
//
// Rueckgabe: Laenge bis einschliesslich EOI, oder 0 wenn es kein JPEG ist.
static size_t jpegLength(const uint8_t* b, size_t len) {
    if (len < 512 || b[0] != 0xFF || b[1] != 0xD8) return 0;

    // Rueckwaerts: im Normalfall steht der Marker in den letzten Bytes, die
    // Schleife bricht also sofort ab.
    for (size_t i = len - 1; i >= 1; i--) {
        if (b[i] == 0xD9 && b[i - 1] == 0xFF) return i + 1;
    }
    return 0;
}

// ------------------------------------------------------------------------------
// Diagnose: was steckt wirklich in einem Block?
// ------------------------------------------------------------------------------
// Die Profilzeile meldet in JEDER Auflaesung 80 KB je Bild - bei 320x240
// genauso wie bei 1280x720. Das kann nicht beides stimmen: ein JPEG mit
// 320x240 bei Qualitaet 50 wiegt rund 10 KB. Der Wert stammt aus der
// Content-Length der Kamera, und fuer die gibt es genau zwei Erklaerungen:
//
//   (a) Die Kamera sendet wirklich 81920 Bytes je Bild und fuellt den Rest
//       hinter dem EOI-Marker mit Nullen auf. Dann kostet jedes angezeigte
//       Bild 80 KB Leitung, unabhaengig von Aufloesung und Qualitaet - und
//       die Bildrate haengt einzig am Durchsatz.
//
//   (b) Die Content-Length luegt und die Kamera schickt kurze Bilder dicht
//       hintereinander. Dann verschlucken wir mit einem 81920-Byte-Lesen
//       mehrere Bilder auf einmal, zeigen davon nur das letzte an und
//       verschenken den Rest. Die Bildrate waere dann sofort um den Faktor
//       der verschluckten Bilder hoeher zu haben.
//
// Zwischen (a) und (b) entscheidet genau eine Beobachtung: wie viele
// SOI-Marker (FF D8) stehen in dem Block? Einer bedeutet (a), mehrere
// bedeuten (b). Dazu der Fuellanteil hinter dem EOI.
//
// Kostet einen Durchlauf ueber 80 KB - deshalb hoechstens alle 5 Sekunden.
#if VIDEO_PROFILE
static void diagFrame(const uint8_t* b, size_t len, size_t jlen) {
    static uint32_t lastLog = 0;
    const uint32_t now = millis();
    if (now - lastLog < 5000) return;
    lastLog = now;

    // SOI-Marker zaehlen. Wortweise waere schneller, aber der Marker darf an
    // ungerader Adresse stehen - und einmal alle 5 s ist das gleichgueltig.
    uint32_t soi = 0;
    for (size_t i = 0; i + 1 < len; i++) {
        if (b[i] == 0xFF && b[i + 1] == 0xD8) soi++;
    }

    // Wie viel vom Schwanz hinter dem EOI ist Fuellmaterial (Null)?
    uint32_t zeros = 0;
    for (size_t i = jlen; i < len; i++) {
        if (b[i] == 0x00) zeros++;
    }

    Serial.printf("[VideoDiag] Content-Length %u | echtes JPEG %u | SOI im Block %lu | "
                  "Schwanz %u Bytes, davon %lu Null\n",
                  (unsigned)len, (unsigned)jlen, (unsigned long)soi,
                  (unsigned)(len - jlen), (unsigned long)zeros);
}
#endif

// Ein verworfenes Bild darf nie stumm bleiben - siehe oben, das kostet sonst
// einen Abend Fehlersuche. Hoechstens alle 5 s, damit die Konsole nutzbar
// bleibt.
static void logBadFrame(const uint8_t* b, size_t len) {
    static uint32_t lastLog = 0;
    const uint32_t now = millis();
    if (now - lastLog < 5000) return;
    lastLog = now;

    Serial.printf("[Video] Bild verworfen: %u Bytes, Anfang %02X %02X, Ende %02X %02X\n",
                  (unsigned)len, b[0], b[1],
                  (len >= 2) ? b[len - 2] : 0, (len >= 1) ? b[len - 1] : 0);
}

// ==============================================================================
// Netzwerk-Task
// ==============================================================================
static void videoNetTask(void*) {
    uint32_t backoff = RECONNECT_MIN_MS;

    while (s_running) {
        if (WiFi.status() != WL_CONNECTED) {
            setStatus("Warte auf WLAN...");
            if (!waitInterruptible(500)) break;
            continue;
        }

        // --- Verbindungsaufbau ------------------------------------------------
        setStatus("Suche Kamera...");
        if (!camPing()) {
            setStatus("Kamera nicht erreichbar");
            if (!waitInterruptible(backoff)) break;
            backoff = (backoff * 2 > RECONNECT_MAX_MS) ? RECONNECT_MAX_MS : backoff * 2;
            continue;
        }

        if (!s_running) break;

        setStatus("Melde an...");
        if (!camLogin()) {
            setStatus("Anmeldung abgelehnt");
            if (!waitInterruptible(backoff)) break;
            backoff = (backoff * 2 > RECONNECT_MAX_MS) ? RECONNECT_MAX_MS : backoff * 2;
            continue;
        }

        camReadActualResolution();
        if (!s_running) break;

        // --- Kameraseitige Drosseln zuruecksetzen -----------------------------
        // api_RE.md, Abschnitt 5: /api/v1/camera/speed steuert die Bildrate,
        // 0 = maximale fps, 100 = maximaler Eco-Modus. Die Kamera merkt sich
        // diesen Wert selbst - er ueberlebt also unsere Verbindung, unseren
        // Neustart und sogar ein Neuflashen des Tablets.
        //
        // Bisher wurde er nur beim Schieben des Reglers gesendet. Stand die
        // Kamera aus einer frueheren Sitzung im Eco-Modus, drosselte sie
        // seither jeden Stream - unabhaengig von Aufloesung und Qualitaet.
        // Genau das passt zum Messbild: gleiche Bildrate bei 320x240 wie bei
        // 1280x720. Deshalb wird der gewuenschte Wert ab jetzt bei jedem
        // Streamstart durchgesetzt.
        BabyCamApi_SetSpeed(camSpeed);
        BabyCamApi_SetQuality(camQuality);

        // --- Stream oeffnen ---------------------------------------------------
        setStatus("Verbinde...");

        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(HTTP_STREAM_TIMEOUT_MS);
        http.setConnectTimeout(HTTP_STREAM_TIMEOUT_MS);

        const String url = streamUrl();
        if (!http.begin(url)) {
            http.end();
            if (!waitInterruptible(backoff)) break;
            continue;
        }
        http.addHeader("uuid", camUuid);
        http.addHeader("versionApp", "2.48");

        const int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[Video] Stream-GET fehlgeschlagen: %d\n", code);
            setStatus("Stream nicht verfuegbar");
            http.end();
            if (!waitInterruptible(backoff)) break;
            backoff = (backoff * 2 > RECONNECT_MAX_MS) ? RECONNECT_MAX_MS : backoff * 2;
            continue;
        }

        WiFiClient* stream = http.getStreamPtr();
        if (!stream) {
            http.end();
            if (!waitInterruptible(backoff)) break;
            continue;
        }

        stream->setNoDelay(true);
        stream->setTimeout(STREAM_READ_TIMEOUT_S);   // Sekunden!

        // --- Stay-alive auf Socketebene --------------------------------------
        // api_RE.md verlangt einen Anwendungs-Heartbeat alle 10 s (den sendet
        // BabyCamApi). Der haelt die Kamera bei Laune, sagt aber nichts
        // darueber, ob DIESE TCP-Verbindung noch lebt: stirbt die Gegenstelle
        // still (Funkloch, Akku leer, App im Hintergrund weggeraeumt), bleibt
        // der Socket "connected" und wir warten bis zur Stall-Frist.
        //
        // Mit Keepalive fragt der Stack nach 5 s Stille selbst nach und gibt
        // nach drei erfolglosen Versuchen im 2-s-Abstand auf - die Verbindung
        // wird also nach rund 11 s erkennbar tot statt irgendwann.
        {
            const int fd = stream->fd();
            if (fd >= 0) {
                const int on = 1, idle = 5, intvl = 2, cnt = 3;
                setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &on,    sizeof(on));
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
                setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
            }
        }

        // Hier stand ein Versuch mit SO_RCVBUF. Er ist wieder draussen, aus
        // zwei Gruenden - beide erst nach dem Einbau geklaert:
        //
        // 1. `setOption()` setzt die Ebene IPPROTO_TCP, SO_RCVBUF gehoert aber
        //    zu SOL_SOCKET. Der Aufruf schlug deshalb immer fehl (errno 109,
        //    "Protocol not available") und protokollierte das je Verbindung.
        // 2. Selbst richtig aufgerufen haette er nichts gebracht: SO_RCVBUF
        //    steuert die Zwischenlagerung der netconn-Schicht, nicht das
        //    angekuendigte TCP-Fenster. Das setzt lwIP beim Anlegen der
        //    Verbindung aus CONFIG_LWIP_TCP_WND_DEFAULT - eine Bauzeit-
        //    Einstellung, siehe P-40 in performance.md.
        //
        // Der Engpass ist damit zur Laufzeit nicht erreichbar.
        backoff = RECONNECT_MIN_MS;   // Verbindung steht
        Serial.println("[Video] Stream laeuft.");

        // --- Bilder lesen -----------------------------------------------------
        uint32_t lastFrameMs = millis();

        while (s_running && stream->connected()) {
            if (millis() - lastFrameMs > STREAM_STALL_MS) {
                Serial.println("[Video] Keine Bilder mehr - Verbindung wird erneuert.");
                break;
            }

            const uint32_t t0 = micros();
            const uint32_t deadline = millis() + STREAM_STALL_MS;

            // Die beiden Anteile getrennt messen. `Download` allein beantwortet
            // die entscheidende Frage nicht: Warten wir auf die Leitung oder
            // auf die Kamera? Steckt die Zeit im Kopf-Lesen, hat die Kamera
            // noch nichts geschickt (sie ist die Grenze). Steckt sie im
            // Rumpf-Lesen, ist es die Bandbreite.
            const uint32_t tWait = micros();
            const int len = readFrameHeader(stream, deadline);
            s_statWaitUs += (micros() - tWait);
            if (len < 0) break;
            if (len == 0) continue;

            if (len > (int)VIDEO_MAX_JPEG_BYTES) {
                // Groesser als jede beobachtete Blockstufe: das ist keine
                // Bildgroesse mehr, sondern ein zerschossener Kopf. Der
                // Bytestrom ist dann ausser Tritt - neu verbinden.
                Serial.printf("[Video] Unplausible Blocklaenge %d - Verbindung wird erneuert.\n", len);
                statInc(s_statDropped);
                break;
            }

            // Grosse Blockstufe einmal je Minute melden. Kein Fehler, aber
            // die Erklaerung, wenn die Bildrate ploetzlich einbricht.
            if (len > CAM_BLOCK_NORMAL) {
                static uint32_t lastBigLog = 0;
                const uint32_t nowMs = millis();
                if (lastBigLog == 0 || nowMs - lastBigLog > 60000) {
                    lastBigLog = nowMs;
                    Serial.printf("[Video] Kamera sendet %d-Byte-Bloecke statt %d - "
                                  "Bildrate sinkt entsprechend.\n", len, CAM_BLOCK_NORMAL);
                }
            }

            // Freien Slot holen. Wartet der Decoder, blockiert das hier kurz -
            // das ist gewollte Rueckstaufuehrung statt wachsender Latenz.
            uint8_t slot;
            if (xQueueReceive(s_freeQ, &slot, pdMS_TO_TICKS(1000)) != pdTRUE) {
                statInc(s_statDropped);
                continue;
            }

            size_t cap = 0;
            uint8_t* buf = VideoDecoder_GetInputBuffer(slot, &cap);
            if (!buf || (size_t)len > cap) {
                xQueueSend(s_freeQ, &slot, 0);
                statInc(s_statDropped);
                continue;
            }

            const uint32_t tRecv = micros();
            const bool bodyOk = readFrameBody(stream, buf, (size_t)len, deadline);
            s_statRecvUs += (micros() - tRecv);

            if (!bodyOk) {
                xQueueSend(s_freeQ, &slot, 0);
                break;   // Verbindung unbrauchbar
            }

            // Auf die wahre Bildlaenge kuerzen: hinter dem EOI-Marker steht
            // Fuellmaterial, das der Hardware-Decoder nicht sehen soll.
            const size_t jlen = jpegLength(buf, (size_t)len);
            if (jlen == 0) {
                // Einzelnes defektes Bild ueberspringen, Verbindung behalten.
                logBadFrame(buf, (size_t)len);
                xQueueSend(s_freeQ, &slot, 0);
                statInc(s_statDropped);
                continue;
            }

#if VIDEO_PROFILE
            diagFrame(buf, (size_t)len, jlen);
#endif

            s_statDownloadUs += (micros() - t0);
            s_statBytes      += (uint32_t)len;
            lastFrameMs = millis();

            FrameRef ref = { slot, (uint32_t)jlen };
            if (xQueueSend(s_fullQ, &ref, pdMS_TO_TICKS(200)) != pdTRUE) {
                xQueueSend(s_freeQ, &slot, 0);
                statInc(s_statDropped);
            }
        }

        http.end();

        if (s_running) {
            setStatus("Verbindung verloren - neuer Versuch...");
            waitInterruptible(backoff);
        }
    }

    // Die Abmeldung (api_RE.md Schritt 3) erledigt BabyCamApi, sobald die
    // Kamerasitzung insgesamt endet. Hier waere sie falsch: der Heartbeat des
    // API-Tasks meldet uns 8 s spaeter ohnehin wieder an - und vor allem darf
    // das Beenden des Videos das noch laufende Babyphone-Audio nicht kappen.
    Serial.println("[Video] Netzwerk-Task beendet.");
    s_netTask = nullptr;
    vTaskDelete(NULL);
}

// ==============================================================================
// Dekodier- und Anzeige-Task
// ==============================================================================
// ------------------------------------------------------------------------------
// Schneller Bildtransfer ins Panel
// ------------------------------------------------------------------------------
// M5GFX kennt in Panel_FrameBufferBase::writeImage() genau einen schnellen Weg:
// ist `no_convert` gesetzt, kopiert es zeilenweise per memcpy. Andernfalls geht
// JEDES Pixel einzeln ueber einen Funktionszeiger - bei 1280x720 sind das
// 921600 Aufrufe je Bild. Genau das sieht man auf dem Schirm als Zeilenlauf von
// links nach rechts.
//
// Der uebliche Aufruf `pushImage(x, y, w, h, (uint16_t*)daten)` kommt auf
// diesen Weg NIE:
//   * `create_pc(const uint16_t*)` deutet die Daten ohne `setSwapBytes(true)`
//     als `swap565_t` - das Panel will aber `rgb565_nonswapped`,
//   * und `create_pc<T>()` setzt `no_convert` fuer 16-Bit-Typen zusaetzlich
//     ausdruecklich auf false, weil `pushImage` auch skalieren koennen muss.
//
// Deshalb bauen wir den Kopierauftrag selbst: Quelle und Ziel sind beide
// natives RGB565, also darf memcpy laufen.
#if VIDEO_USE_PPA
// Der PPA dreht und skaliert per DMA direkt in den DSI-Framebuffer und ist
// damit der einzige Weg, der die Panel-Rotation nicht Pixel fuer Pixel
// bezahlen muss.
//
// Bewusst OHNE startWrite()/endWrite() darum herum (Hebel 1, siehe 2.8): der
// PPA braucht M5GFX' Schreibtransaktion nicht, und die Klammer machte das
// Display frueher zu einem zweiten DMA2D-Nutzer in derselben Runde.
static lgfx::PPASrm* s_ppa = nullptr;

static void pushFramePPA(const DecodedFrame& f) {
    if (!s_ppa) s_ppa = new lgfx::PPASrm(&M5.Display, false);
    if (!s_ppa) return;

    const float zx = (float)PANEL_W / (float)f.width;
    const float zy = (float)PANEL_H / (float)f.height;
    const float z  = (zx < zy) ? zx : zy;    // Seitenverhaeltnis erhalten

    s_ppa->pushImageSRM((PANEL_W - (int)(f.width  * z)) / 2,
                        (PANEL_H - (int)(f.height * z)) / 2,
                        0, 0, 0, z, z,
                        f.width, f.height, (uint16_t*)f.pixels);
}

// Video-Overlay: das Bild in das Zielrechteck einpassen.
//
// Der PPA skaliert in Schritten von 1/16 (Festkomma mit vier Nachkommabits).
// Der Faktor wird deshalb auf 1/16 ABGERUNDET - so wird das Bild nie groesser
// als das Rechteck. Was es kleiner bleibt, wird schwarz gefuellt: LVGL zeichnet
// in dieser Flaeche nicht (Lvgl_SetReservedArea), dort stuende sonst Altes.
static void pushFramePip(const DecodedFrame& f, const PipRect& r) {
    if (!s_ppa) s_ppa = new lgfx::PPASrm(&M5.Display, false);
    if (!s_ppa) return;

    const float zx = (float)r.w / (float)f.width;
    const float zy = (float)r.h / (float)f.height;
    float z = (zx < zy) ? zx : zy;
    z = floorf(z * 16.0f) / 16.0f;
    if (z <= 0.0f) return;   // Rechteck kleiner als 1/16 des Bildes - unsinnig

    const int dw = (int)(f.width  * z);
    const int dh = (int)(f.height * z);
    const int dx = r.x + (r.w - dw) / 2;
    const int dy = r.y + (r.h - dh) / 2;

    // Raender (nur wenn das Seitenverhaeltnis nicht genau passt)
    if (dy > r.y)              M5.Display.fillRect(r.x, r.y, r.w, dy - r.y, TFT_BLACK);
    if (dy + dh < r.y + r.h)   M5.Display.fillRect(r.x, dy + dh, r.w, r.y + r.h - (dy + dh), TFT_BLACK);
    if (dx > r.x)              M5.Display.fillRect(r.x, dy, dx - r.x, dh, TFT_BLACK);
    if (dx + dw < r.x + r.w)   M5.Display.fillRect(dx + dw, dy, r.x + r.w - (dx + dw), dh, TFT_BLACK);

    // Zielposition fuer den PPA. LGFX_PPA rechnet die Querformat-Koordinaten
    // bei gedrehtem Panel nur durch VERTAUSCHEN von x und y in den hochkant
    // liegenden Bildspeicher um - die Spiegelung der y-Achse fehlt. Ohne
    // Ausgleich landete ein Bild unten rechts oben rechts (gemessen am Geraet).
    // Im Vollbild faellt das nicht auf: dort ist dy = 0 und dh = 720, beide
    // Formeln liefern 0. Deshalb hier die gespiegelte Zeile uebergeben.
    const int ppaY = PANEL_H - dy - dh;
    if (ppaY < 0) return;
    s_ppa->pushImageSRM(dx, ppaY, 0, 0, 0, z, z, f.width, f.height, (uint16_t*)f.pixels);
}
#endif

static void pushFrameFast(int x, int y, int w, int h, const uint8_t* pixels) {
    lgfx::pixelcopy_t pc(pixels,
                         lgfx::color_depth_t::rgb565_nonswapped,   // Ziel: DSI-Framebuffer
                         lgfx::color_depth_t::rgb565_nonswapped,   // Quelle: JPEG-Decoder
                         false);
    pc.no_convert = true;
    M5.Display.pushImage(x, y, w, h, &pc);
}

// V-02: M5.Display wird von zwei Tasks bespielt - vom LVGL-Flush und von hier.
// Beide gehen ueber denselben MIPI-DSI-Bus. Ohne Serialisierung koennen sich
// zwei Transfers ueberlappen; sichtbar wird das als Flackern oder versetzte
// Bildhaelften. Der LVGL-Lock ist bereits die Klammer um jeden Displayzugriff
// der Oberflaeche, also nutzen wir ihn auch hier.
//
// Bewusst NICHT unter dem Lock: die Dekodierung. Sie dauert ~12 ms und wuerde
// die Oberflaeche in genau dem Mass ausbremsen (siehe 2.18, Hebel 2).
// Overlay-Zweig von renderFrame(). Rechteck und Ziel werden unter der Sperre
// gelesen: blendet die Oberflaeche das Overlay gerade aus (Dialog, Alarm),
// darf kein Bild mehr ueber sie gemalt werden.
static void renderPip(const DecodedFrame& f) {
#if VIDEO_USE_PPA
    const uint32_t tLock = micros();
    if (!lvgl_port_lock(100)) {
        s_statLockUs += (micros() - tLock);
        return;
    }
    s_statLockUs += (micros() - tLock);

    const PipRect r = s_pipRect;
    if (s_output == VideoOutput::Pip && r.w > 0 && r.h > 0) {
        const uint32_t tPush = micros();
        pushFramePip(f, r);
        s_statPushUs += (micros() - tPush);
        const uint32_t now = millis();
        s_pipLastFrameMs = now ? now : 1;   // 0 bedeutet "noch nie"
    }
    lvgl_port_unlock();
#else
    // Ohne PPA gibt es kein Overlay: der Weg ueber M5GFX ist auf dem
    // gedrehten Panel zu langsam (siehe VIDEO_USE_PPA in DebugFlags.h).
    (void)f;
#endif
}

static void renderFrame(const DecodedFrame& f) {
    static bool lastFS = false;

    if (s_output == VideoOutput::Pip) {
        renderPip(f);
        return;
    }

    if (vidFSMode) {
        // Das Zeichnen ins Bild selbst braucht kein Display - es geht in den
        // PSRAM-Puffer. Also vor dem Lock, damit der kurz bleibt.
        if (showFps) {
            VideoOverlay_Draw((uint16_t*)f.pixels, f.width, f.height,
                              currentFps, camBatteryPercent, batteryPercent);
        }

        const uint32_t tLock = micros();
        if (!lvgl_port_lock(100)) {         // Bild verwerfen statt kollidieren
            s_statLockUs += (micros() - tLock);
            return;
        }
        s_statLockUs += (micros() - tLock);

        // Unter der Sperre erneut pruefen: zwischen der Abfrage oben und dem
        // Lock kann die Baby-Ansicht verlassen worden sein (z.B. Alarm). Ein
        // Vollbild-Transfer wuerde dann direkt ueber die neue Ansicht malen.
        if (!vidFSMode || s_output != VideoOutput::BabyView) {
            lvgl_port_unlock();
            return;
        }

        if (!lastFS) {
            M5.Display.clear(TFT_BLACK);
            lastFS = true;
        }

        // Ohne PPA: Originalgroesse, mittig. Liefert die Kamera die
        // Panelaufloesung, fuellt das Bild den Schirm ohnehin genau aus.
        const int x = (PANEL_W - (int)f.width) / 2;
        const int y = (PANEL_H - (int)f.height) / 2;

        const uint32_t tPush = micros();
#if VIDEO_USE_PPA
        (void)x; (void)y;
        pushFramePPA(f);
#else
        pushFrameFast(x < 0 ? 0 : x, y < 0 ? 0 : y, f.width, f.height, f.pixels);
#endif
        s_statPushUs += (micros() - tPush);

        lvgl_port_unlock();
        return;
    }

    if (lastFS) {
        lastFS = false;
        if (lvgl_port_lock(100)) {
            lv_obj_invalidate(lv_scr_act());
            lvgl_port_unlock();
        }
    }

    const uint32_t tLockW = micros();
    const bool gotLock = lvgl_port_lock(50);
    s_statLockUs += (micros() - tLockW);

    if (gotLock) {
        const uint32_t tPushW = micros();
        lv_img_dsc_t* d = &s_imgDsc[f.slot];
        d->header.w  = f.width;
        d->header.h  = f.height;
        d->header.cf = LV_IMG_CF_TRUE_COLOR;
        d->data      = f.pixels;
        d->data_size = (uint32_t)f.width * f.height * 2;
        ViewBaby_SetImage(d);
        lvgl_port_unlock();
        s_statPushUs += (micros() - tPushW);
    }
}

static void videoDecodeTask(void*) {
    // Zwei Fenster: die Bildrate fuer das Overlay soll mitlaufen, die
    // Profilzeile aber nicht die Konsole fluten.
    uint32_t fpsStart      = millis();
    uint32_t fpsFrames     = 0;
    uint32_t profStart     = millis();
    uint32_t profFrames    = 0;

    while (s_running) {
        FrameRef ref;
        if (xQueueReceive(s_fullQ, &ref, pdMS_TO_TICKS(200)) != pdTRUE) {
            continue;
        }

        // Niemand will ein Bild: Slot zurueck, nicht dekodieren. Der Empfang
        // laeuft weiter, damit die Anzeige beim Zurueckkehren sofort ein Bild
        // hat. Die Bildrate gilt dann als 0 - sonst stuende im Overlay oder
        // in der Baby-Ansicht ein veralteter Wert.
        if (s_output == VideoOutput::None) {
            xQueueSend(s_freeQ, &ref.slot, 0);
            currentFps = 0;
            fpsStart   = millis();
            fpsFrames  = 0;
            continue;
        }

        // Latenzmodus: Steht mehr als ein Bild an, ist das aelteste schon
        // ueberholt. Wir geben seinen Slot zurueck und nehmen das neueste -
        // das kostet Bilder, aber die Anzeige haengt der Realitaet nicht
        // hinterher. Ohne das waechst der Rueckstand, solange der Zulieferer
        // schneller ist als die Anzeige.
        if (videoLowLatency) {
            FrameRef newer;
            while (xQueueReceive(s_fullQ, &newer, 0) == pdTRUE) {
                xQueueSend(s_freeQ, &ref.slot, 0);
                statInc(s_statDropped);
                ref = newer;
            }
        }

        // Overlay: gedrosselt dekodieren. Erst NACH dem Latenzmodus, damit
        // das dekodierte Bild das neueste ist.
        if (s_output == VideoOutput::Pip) {
            const uint32_t nowMs = millis();
            if (nowMs - s_pipLastDecodeMs < s_pipIntervalMs) {
                xQueueSend(s_freeQ, &ref.slot, 0);
                continue;
            }
            s_pipLastDecodeMs = nowMs;
        }

        // Byte-Reihenfolge zum Ausgabeweg passend waehlen: der PPA schreibt
        // unveraendert in den Framebuffer (RGB), der Weg ueber M5GFX/LVGL
        // erwartet BGR. Muss VOR dem Dekodieren stehen.
#if VIDEO_USE_PPA
        // BGR nur fuer den LVGL-Weg (Baby-Ansicht im Fenster). Vollbild und
        // Overlay laufen ueber den PPA.
        VideoDecoder_SetBgrOrder(s_output == VideoOutput::BabyView && !vidFSMode);
#else
        VideoDecoder_SetBgrOrder(true);
#endif

        size_t cap = 0;
        const uint8_t* jpeg = VideoDecoder_GetInputBuffer(ref.slot, &cap);

        DecodedFrame frame;
        const uint32_t t0 = micros();
        const bool ok = jpeg && VideoDecoder_Decode(jpeg, ref.size, &frame);
        const uint32_t t1 = micros();

        // Slot sofort zurueckgeben: der Netzpfad soll nicht auf die Anzeige warten.
        xQueueSend(s_freeQ, &ref.slot, 0);

        if (!ok) {
            statInc(s_statDropped);
            continue;
        }

        renderFrame(frame);
        const uint32_t t2 = micros();

        s_statDecodeUs += (t1 - t0);
        s_statRenderUs += (t2 - t1);
        statInc(s_statFrames);
        fpsFrames++;
        profFrames++;

        // --- Bildrate: kurzes Fenster, damit das Overlay nicht nachhinkt ------
        const uint32_t now = millis();
        const uint32_t fpsElapsed = now - fpsStart;
        if (fpsElapsed >= 1000) {
            currentFps = (int)((fpsFrames * 1000UL) / fpsElapsed);
            fpsStart  = now;
            fpsFrames = 0;
        }

        // --- Profilfenster ----------------------------------------------------
        const uint32_t elapsed = now - profStart;
        if (elapsed >= 5000) {
            const uint32_t n = s_statFrames ? s_statFrames : 1;

#if VIDEO_PROFILE
            // Nur ausgeben, wenn wirklich ein USB-Host lauscht. Die HWCDC des
            // P4 loest beim Abziehen des Kabels mitten in einem Schreibvorgang
            // einen Store-Fehler in ihrem eigenen ISR aus (MTVAL 0x500D2000,
            // hw_cdc_isr_handler) - genau das hat den letzten Absturz erzeugt,
            // und zwar mitten in dieser Zeile ("[VideoPr" im Protokoll).
            if (Serial)
            Serial.printf("[VideoProf] %ux%u | %lu Bilder in %lu ms = %d fps | je Bild: "
                          "Download %.1f ms, Decode %.1f ms, Anzeige %.1f ms | "
                          "(Warten %.1f ms, Empfang %.1f ms, davon Leerlauf %.1f ms in %.0f Pausen "
                          "= %.1f KB je Schub, laengste %.1f ms) | "
                          "(Anzeige: Sperre %.1f ms, Transfer %.1f ms) | "
                          "%.0f KB/Bild = %.1f Mbit/s | verworfen %lu | Fehler %lu/%lu"
                          " | DMA2D-Rettungen %lu (max %lu Zyklen) | RSSI %d dBm\n",
                          (unsigned)frame.width, (unsigned)frame.height,
                          (unsigned long)profFrames, (unsigned long)elapsed, currentFps,
                          s_statDownloadUs / 1000.0f / n,
                          s_statDecodeUs   / 1000.0f / n,
                          s_statRenderUs   / 1000.0f / n,
                          s_statWaitUs     / 1000.0f / n,
                          s_statRecvUs     / 1000.0f / n,
                          s_statIdleUs     / 1000.0f / n,
                          (float)s_statWaitCnt / n,
                          (s_statWaitCnt ? (s_statBytes / 1024.0f / s_statWaitCnt) : 0.0f),
                          s_statIdleMaxUs  / 1000.0f,
                          s_statLockUs     / 1000.0f / n,
                          s_statPushUs     / 1000.0f / n,
                          s_statBytes / 1024.0f / n,
                          (s_statBytes * 8.0f) / (elapsed * 1000.0f),
                          (unsigned long)s_statDropped,
                          (unsigned long)VideoDecoder_GetErrorCount(),
                          (unsigned long)VideoDecoder_GetRejectCount(),
                          (unsigned long)(g_dma2dGuardTxTimeouts + g_dma2dGuardRxTimeouts),
                          (unsigned long)g_dma2dGuardMaxCycles,
                          // Funkstaerke mitschreiben. Die Einbrueche auf
                          // 0,5 KB je Schub und 3 s je Bild sehen nach einer
                          // zusammengebrochenen Funkstrecke aus, nicht nach
                          // einem Fensterproblem - ohne diesen Wert laesst
                          // sich das eine nicht vom anderen trennen.
                          (int)WiFi.RSSI());
#endif

            profStart        = now;
            profFrames       = 0;
            s_statDownloadUs = 0;
            s_statDecodeUs   = 0;
            s_statRenderUs   = 0;
            s_statWaitUs     = 0;
            s_statRecvUs     = 0;
            s_statIdleUs     = 0;
            s_statIdleMaxUs  = 0;
            s_statWaitCnt    = 0;
            s_statLockUs     = 0;
            s_statPushUs     = 0;
            s_statBytes      = 0;
            s_statFrames     = 0;
            s_statDropped    = 0;
        }
    }

    Serial.println("[Video] Decoder-Task beendet.");
    s_decTask = nullptr;
    vTaskDelete(NULL);
}

// ==============================================================================
// Oeffentliche Schnittstelle
// ==============================================================================
void VideoLogic_Init() {
    if (!VideoDecoder_Init()) {
        Serial.println("[Video] Decoder nicht verfuegbar - Video bleibt aus.");
        return;
    }

    s_freeQ = xQueueCreate(VideoDecoder_InputSlotCount(), sizeof(uint8_t));
    s_fullQ = xQueueCreate(VideoDecoder_InputSlotCount(), sizeof(FrameRef));

    if (!s_freeQ || !s_fullQ) {
        Serial.println("[Video] FEHLER: Warteschlangen konnten nicht angelegt werden.");
        return;
    }

    Serial.println("[Video] Bereit.");
}

// Idempotent: legt nur dann Tasks an, wenn keine laufen. Darf in jedem
// Durchlauf gerufen werden - so holt sich der Start automatisch nach, sobald
// ein auslaufender Vorgaenger weg ist.
void VideoLogic_Start() {
    if (!VideoDecoder_IsReady() || !s_freeQ || !s_fullQ) return;
    if (s_netTask || s_decTask) return;

    // Slots zuruecksetzen: alle frei, keine gefuellt.
    xQueueReset(s_freeQ);
    xQueueReset(s_fullQ);
    for (uint8_t i = 0; i < VideoDecoder_InputSlotCount(); i++) {
        xQueueSend(s_freeQ, &i, 0);
    }

    s_running = true;

    // Der Decoder laeuft auf Core 1 knapp unter dem Netzpfad; beide deutlich
    // unter der lwIP-Task (Prioritaet 18), sonst blockierten sie ihren eigenen
    // Netzwerkstack.
    if (xTaskCreatePinnedToCore(videoDecodeTask, "VideoDec", 8192, NULL, 4, &s_decTask, 1) != pdPASS) {
        Serial.println("[Video] FEHLER: Decoder-Task laesst sich nicht anlegen.");
        s_decTask = nullptr;
        s_running = false;
        return;
    }

    if (xTaskCreatePinnedToCore(videoNetTask, "VideoNet", 8192, NULL, 5, &s_netTask, 0) != pdPASS) {
        Serial.println("[Video] FEHLER: Netzwerk-Task laesst sich nicht anlegen.");
        s_netTask = nullptr;
        // Der Decoder-Task beendet sich selbst, sobald er s_running == false sieht.
        s_running = false;
        return;
    }
}

// Setzt nur das Laufzeitflag. Die Zustandsflags (isStreamActive) gehoeren dem
// BabyService - frueher setzte diese Funktion sie als Nebenwirkung zurueck,
// und wer Video stoppte, aenderte damit unbemerkt den Babyphone-Modus.
void VideoLogic_Stop() {
    if (!s_running) return;
    s_running = false;

    // Die Tasks beenden sich selbst; sie pruefen s_running in jeder Schleife.
    // Die Abmeldung bei der Kamera erledigt BabyCamApi, sobald die Sitzung
    // endet - hier waere sie ein synchroner HTTP-POST in der Loop-Task und
    // wuerde die Oberflaeche bis zu 2,5 s blockieren.
}

bool VideoLogic_IsRunning() {
    return s_netTask != nullptr || s_decTask != nullptr;
}

void VideoLogic_SetOutput(VideoOutput out) {
    if (out == VideoOutput::Pip && s_output != VideoOutput::Pip) {
        // Beim Einblenden sofort ein Bild zeigen, nicht erst nach dem
        // Drosselintervall - und die Anzeige "Verbindung ..." neu beginnen.
        s_pipLastDecodeMs = millis() - s_pipIntervalMs;
        s_pipLastFrameMs  = 0;
    }
    s_output = out;
}

VideoOutput VideoLogic_GetOutput() {
    return s_output;
}

void VideoLogic_SetPipRect(int x, int y, int w, int h) {
    // Sperre: der Decoder liest das Rechteck unter ihr (renderPip). Aus der
    // Hauptschleife gerufen, die die rekursive Sperre ggf. schon haelt.
    if (!lvgl_port_lock(portMAX_DELAY)) return;
    const PipRect r = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h };
    if (r.x != s_pipRect.x || r.y != s_pipRect.y || r.w != s_pipRect.w || r.h != s_pipRect.h) {
        // Neues Ziel: "noch kein Bild dort". Sonst gaebe die Oberflaeche die
        // neue Flaeche frei (Aussparung), bevor der PPA sie bemalt hat.
        s_pipLastFrameMs = 0;
    }
    s_pipRect = r;
    lvgl_port_unlock();
}

void VideoLogic_SetPipFps(int fps) {
    if (fps < 1)  fps = 1;
    if (fps > 30) fps = 30;
    s_pipIntervalMs = (uint16_t)(1000 / fps);
}

uint32_t VideoLogic_MsSinceLastPipFrame() {
    const uint32_t last = s_pipLastFrameMs;
    if (last == 0) return UINT32_MAX;
    return millis() - last;
}
