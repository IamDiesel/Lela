#pragma GCC optimize ("O2")
// ==============================================================================
// AudioStreamLogic - Half-Duplex-Audio fuer den Babyphone-Betrieb
// ==============================================================================
// Grundsatz: GENAU EIN Task konfiguriert die I2S-Hardware.
//
// Warum: Mic und Lautsprecher haengen auf dem Tab5 am selben I2S-Port und
// teilen sich sogar den MCLK-Pin (GPIO 30) - paralleler Betrieb ist hardware-
// seitig ausgeschlossen. M5Unified 0.2.13 serialisiert begin()/end() nicht;
// ruft ein zweiter Task playRaw() waehrend eines Teardowns, wird der I2S-Kanal
// unter der laufenden Task weggezogen (Core Panic). api_RE.md beschreibt genau
// diesen Fall.
//
// Loesung: UI und Netzwerk schicken nur noch Kommandos ueber eine Queue.
// Der Besitzer-Task fuehrt sie der Reihe nach aus.
//
//   [UI / SystemLogic] --Kommando--> [audioTask] --besitzt--> M5.Speaker / M5.Mic
//                                        ^
//   [audioRxTask] --UDP--> Jitterpuffer -+
//
// Der UI-Ton-Task (SharedData.cpp) bleibt ein zweiter Nutzer der Hardware -
// Tastenklicks und der Mattenalarm duerfen nicht vom Babyphone abhaengen. Er
// fasst sie aber nur unter AudioHw_Lock() an und nur, solange der Lautsprecher
// laut AudioHw_SpeakerUsable() ueberhaupt betriebsbereit ist.
//
// Der Jitterpuffer entkoppelt den Netzempfang von der Wiedergabe. Ohne ihn
// startet jeder Stream im Underrun: die DMA-Kette haelt nur ~43 ms Audio vor.
// Siehe src/doc/video_refactoring.md, 2.18.
// ==============================================================================

#include "AudioStreamLogic.h"
#include "SharedData.h"
#include "BabyCamApi.h"
#include <M5Unified.hpp>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_heap_caps.h"
#include <string.h>

volatile bool isAudioStreaming = false;
volatile bool isPTTActive      = false;

// ------------------------------------------------------------------------------
// Abstimmung
// ------------------------------------------------------------------------------
static const uint32_t AUDIO_RATE      = 44100;   // api_RE.md: 44100 Hz, 16 Bit, Mono
static const uint16_t UDP_PORT_IN     = 50001;   // Kamera -> Tablet
static const uint16_t UDP_PORT_OUT    = 50003;   // Tablet -> Kamera (PTT)

// Jitterpuffer. Muss die DMA-Kette (~43 ms) und das Underrun-Fenster (~24 ms)
// ueberbruecken; ueber SDIO-WLAN ist reichlich Reserve angebracht.
static const size_t   RING_BYTES      = 64 * 1024;          // ~740 ms
static const size_t   PREBUFFER_BYTES = 8 * 1024;           // ~93 ms Vorlauf
static const size_t   CHUNK_BYTES     = 1024;               // je playRaw-Haeppchen
static const uint8_t  CHUNK_SLOTS     = 3;                  // rotierende Puffer

static const size_t   MIC_SAMPLES     = 512;                // ~11,6 ms je Paket

// Kommt so lange kein Paket, wird der Wiedergabe-Befehl an die Kamera erneuert.
// Die Kamera sendet unkomprimiertes PCM ohne Pause - auch Stille sind Samples.
// Bleibt der Strom aus, hat sie den Befehl verloren (WLAN-Abbruch, Kamera-App
// neu gestartet). Frueher blieb das Babyphone dann stumm, bis jemand den Ton
// aus- und wieder einschaltete; im Hintergrundbetrieb faellt das niemandem auf.
static const uint32_t AUDIO_SILENCE_REASSERT_MS = 10000;

// M5Unified 0.2.13 kennt keinen Buffer-Release-Callback, deshalb drei Puffer:
// zwei koennen gleichzeitig in der Kanal-Warteschlange stehen.
static int16_t* s_chunk[CHUNK_SLOTS] = { nullptr };
static uint8_t  s_chunkIdx = 0;

// Mikrofon: zwei Puffer im Wechsel (siehe micRecord()). Bewusst statisch und im
// internen RAM - die I2S-DMA schreibt direkt hinein.
static int16_t  s_micBuf[2][MIC_SAMPLES];
static uint8_t  s_micIdx    = 0;
static bool     s_micPrimed = false;

// Ringpuffer
static uint8_t* s_ring      = nullptr;
static volatile size_t s_ringHead = 0;   // Schreibposition - gehoert dem RX-Task
static volatile size_t s_ringTail = 0;   // Leseposition   - gehoert dem Audio-Task
static volatile bool   s_ringOverrun = false;
static bool s_primed = false;            // Vorlauf erreicht? (nur Audio-Task)

// Zeitpunkt des letzten Lebenszeichens der Kamera: letztes UDP-Paket, Start
// der Wiedergabe oder letzte Erneuerung des Befehls. Beide Tasks schreiben nur
// millis() hinein - ein Wettlauf aendert den Wert also um Millisekunden.
static volatile uint32_t s_lastAliveMs = 0;

#if AUDIO_PROFILE
// Messwerte fuer [AudioProf]. Jeder Zaehler hat genau einen Schreiber (RX-Task
// bzw. Audio-Task); zurueckgesetzt wird nur vom Audio-Task. Ein dabei
// verlorenes Paket faellt bei einer Diagnosezeile nicht ins Gewicht.
static volatile uint32_t s_profPackets   = 0;   // RX-Task
static volatile uint32_t s_profBytes     = 0;   // RX-Task
static uint32_t s_profChunks    = 0;            // an den Lautsprecher gegeben
static uint32_t s_profEmpty     = 0;            // Wiedergabe wollte, Puffer leer
static uint32_t s_profQueueFull = 0;            // Kanal-Warteschlange voll
static int      s_profLastVol   = -1;           // zuletzt gesetzte Kanallautstaerke
static int32_t  s_profPeak      = 0;            // groesster Betrag eines abgespielten Samples
static uint64_t s_profAbsSum    = 0;            // Summe der Betraege (fuer den Mittelwert)
static uint32_t s_profSamples   = 0;            // Anzahl abgespielter Samples
// Kopf des zuletzt empfangenen Pakets: zeigt, ob vor den Samples noch ein
// Protokollkopf steht (z.B. RTP beginnt mit 0x80).
static volatile uint8_t  s_profHead[8] = {0};   // RX-Task
static volatile uint16_t s_profLastLen = 0;     // RX-Task
#endif

// ------------------------------------------------------------------------------
// Kommandos
// ------------------------------------------------------------------------------
enum AudioCmd : uint8_t {
    CMD_START_BABY = 1,
    CMD_STOP_BABY,
    CMD_START_PTT,
    CMD_STOP_PTT,
};

static QueueHandle_t s_cmdQ   = nullptr;
static TaskHandle_t  s_audio  = nullptr;
static TaskHandle_t  s_rx     = nullptr;

// ------------------------------------------------------------------------------
// Hardware-Schloss
// ------------------------------------------------------------------------------
static SemaphoreHandle_t s_hwMutex = nullptr;

bool AudioHw_Lock(uint32_t timeoutMs) {
    // Vor dem Init gibt es genau einen Nutzer - dann ist jeder Zugriff sicher.
    if (!s_hwMutex) return true;
    return xSemaphoreTake(s_hwMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void AudioHw_Unlock() {
    if (s_hwMutex) xSemaphoreGive(s_hwMutex);
}

// ------------------------------------------------------------------------------
// Zustand des Lautsprechers
// ------------------------------------------------------------------------------
// SPK_UI     Grundzustand nach dem Boot: Default-Abtastrate, Kanal 1 spielt
//            Klicks und den Mattenalarm. main.cpp stellt ihn beim Start her.
// SPK_STREAM 44100 Hz fuer das Babyphone. Ohne die passende Rate resampled
//            M5Unified jedes einzelne Sample linear von 44100 auf 48000.
// SPK_OFF    abgeschaltet. Nur waehrend PTT - Halbduplex ist Hardware-Zwang.
enum SpkState : uint8_t { SPK_OFF = 0, SPK_UI, SPK_STREAM };
static volatile SpkState s_spkState = SPK_UI;
static volatile bool     s_spkBusy  = false;   // Umschaltvorgang laeuft
static bool s_micUp = false;
static bool s_resumeBabyAfterPTT = false;

// Absenkung waehrend eines UI-Tons. Frueher hat der Ton-Task die Kanallautstaerke
// selbst gesetzt - wirkungslos, weil feedSpeaker() sie alle 5 ms wieder auf den
// vollen Wert zog. Jetzt meldet er nur einen Zeitraum an, und die Absenkung
// passiert dort, wo die Lautstaerke ohnehin gesetzt wird.
static volatile uint32_t s_duckUntilMs = 0;

void AudioHw_DuckStream(uint32_t durationMs) {
    const uint32_t until = millis() + durationMs;
    if ((int32_t)(until - s_duckUntilMs) > 0) s_duckUntilMs = until;
}

// Boot-Konfiguration des Lautsprechers. decltype wertet nicht aus, deshalb ist
// die Deklaration hier oben unschaedlich - gefuellt wird sie erst im Init,
// nach M5.begin().
static decltype(M5.Speaker.config()) s_cfgUI;
static bool s_cfgUIValid = false;

bool AudioHw_SpeakerUsable() {
    return (s_spkState != SPK_OFF) && !s_spkBusy;
}

// ------------------------------------------------------------------------------
// Ringpuffer - Single Producer (RX-Task) / Single Consumer (Audio-Task)
// ------------------------------------------------------------------------------
// Regel: s_ringHead gehoert dem Schreiber, s_ringTail dem Leser. Kein Task
// fasst die Variable des anderen an. Sonst koennte ringRead() mitten im memcpy
// einen Tail sehen, den es nie gesetzt hat - und liest dann fremde Bytes.
static inline size_t ringUsed() {
    const size_t h = s_ringHead, t = s_ringTail;
    return (h >= t) ? (h - t) : (RING_BYTES - t + h);
}

static inline size_t ringFree() { return RING_BYTES - ringUsed() - 1; }

static void ringReset() {
    // Nur aufrufen, solange der RX-Task nachweislich nicht schreibt.
    s_ringHead = 0;
    s_ringTail = 0;
    s_ringOverrun = false;
    s_primed = false;
}

static void ringWrite(const uint8_t* src, size_t len) {
    if (!s_ring || len < 2) return;

    // 16-Bit-Samples: eine ungerade Byteanzahl wuerde alle folgenden Samples
    // um ein Byte verschieben - aus Audio wird Rauschen. Das einzelne Restbyte
    // ist die harmlosere Verluststelle.
    len &= ~(size_t)1;

    // freeNow bewusst nur EINMAL auswerten: der Leser laeuft parallel, ein
    // zweiter Aufruf liefert einen anderen Wert und die Differenz unterlaeuft
    // als size_t ins Riesenhafte.
    const size_t freeNow = ringFree();
    if (len > freeNow) {
        // Ueberlauf. Der Schreiber darf den Tail NICHT bewegen, also verwirft
        // er dieses Paket und meldet es. Das Nachziehen erledigt der Leser in
        // ringTrim() - dort gehoert der Tail hin.
        s_ringOverrun = true;
        return;
    }

    size_t h = s_ringHead;
    const size_t firstPart = (h + len <= RING_BYTES) ? len : (RING_BYTES - h);
    memcpy(s_ring + h, src, firstPart);
    if (firstPart < len) {
        memcpy(s_ring, src + firstPart, len - firstPart);
    }
    s_ringHead = (h + len) % RING_BYTES;
}

// Nur aus dem Leser-Task. Nach einem Ueberlauf haengt die Wiedergabe der
// Realzeit hinterher; bei einem Babyphone ist aktuelles Audio wichtiger als
// lueckenlose Historie, also werfen wir die aeltesten Daten weg.
static void ringTrim(size_t keepBytes) {
    const size_t used = ringUsed();
    if (used <= keepBytes) return;
    const size_t drop = (used - keepBytes) & ~(size_t)1;   // Samplegrenze halten
    if (drop == 0) return;
    s_ringTail = (s_ringTail + drop) % RING_BYTES;
}

static size_t ringRead(uint8_t* dst, size_t len) {
    const size_t avail = ringUsed();
    if (avail < len) len = avail;
    len &= ~(size_t)1;                    // nie ein halbes Sample herausgeben
    if (len == 0) return 0;

    size_t t = s_ringTail;
    const size_t firstPart = (t + len <= RING_BYTES) ? len : (RING_BYTES - t);
    memcpy(dst, s_ring + t, firstPart);
    if (firstPart < len) {
        memcpy(dst + firstPart, s_ring, len - firstPart);
    }
    s_ringTail = (t + len) % RING_BYTES;
    return len;
}

// ------------------------------------------------------------------------------
// Hardware - ausschliesslich aus dem Besitzer-Task
// ------------------------------------------------------------------------------
static int uiToneVolume() {
    const int v = (int)((muteMaster ? 0 : volMaster / 100.0f) *
                        (muteUI     ? 0 : volUI     / 100.0f) * 255.0f);
    return v > 255 ? 255 : (v < 0 ? 0 : v);
}

// Geordneter Teardown (api_RE.md): erst den Nachschub versiegen lassen, dann
// die Warteschlange leeren, dann die DMA-Kette ausspielen lassen, erst danach
// die Hardware abschalten.
static void speakerTeardown() {
    M5.Speaker.stop();

    const uint32_t guard = millis();
    while (M5.Speaker.isPlaying() && millis() - guard < 500) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    // Die DMA-Kette haelt bei den Tab5-Vorgaben noch rund 43 ms Audio.
    // isPlaying() == false heisst nur "Warteschlange leer", nicht "Ton ist raus".
    vTaskDelay(pdMS_TO_TICKS(60));

    M5.Speaker.end();
}

// Gibt true zurueck, wenn der gewuenschte Zustand erreicht wurde.
//
// Der Rueckgabewert ist wichtig: schlaegt begin() fehl, bliebe der
// Lautsprecher sonst stumm ZURUECK - und mit ihm Tastenklicks und der
// Mattenalarm, ohne dass es irgendwo auffaellt.
static bool speakerSet(SpkState want) {
    if (s_spkState == want) return true;

    // Ab hier ist der Lautsprecher fuer Aussenstehende tabu.
    s_spkBusy = true;
    AudioHw_Lock(portMAX_DELAY);

    if (s_spkState != SPK_OFF) {
        speakerTeardown();
        s_spkState = SPK_OFF;
    }

    if (want == SPK_UI && s_cfgUIValid) {
        M5.Speaker.config(s_cfgUI);
        if (M5.Speaker.begin()) {
            M5.Speaker.setVolume(255);
            M5.Speaker.setChannelVolume(1, uiToneVolume());
            s_spkState = SPK_UI;
        }
    } else if (want == SPK_STREAM) {
        auto cfg = M5.Speaker.config();
        cfg.sample_rate      = AUDIO_RATE;
        cfg.task_priority    = 13;   // ueber dem Zulieferer, klar unter lwIP (18)
        cfg.task_pinned_core = 1;
        M5.Speaker.config(cfg);

        if (M5.Speaker.begin()) {
            M5.Speaker.setVolume(255);
            M5.Speaker.setChannelVolume(1, uiToneVolume());
            s_spkState = SPK_STREAM;
        }
    }

    AudioHw_Unlock();
    s_spkBusy = false;

    if (s_spkState == want) return true;

    Serial.printf("[Audio] FEHLER: Lautsprecher liess sich nicht in Zustand %d "
                  "bringen (ist %d).\n", (int)want, (int)s_spkState);
    return false;
}

// Letzte Rettung: zurueck in den Grundzustand, damit wenigstens Klicks und
// der Mattenalarm wieder laufen.
static void speakerFallbackToUI() {
    if (s_spkState == SPK_UI) return;
    if (!speakerSet(SPK_UI)) {
        Serial.println("[Audio] FEHLER: Lautsprecher bleibt stumm - Toene fallen aus.");
    }
}

static void micUp() {
    if (s_micUp) return;

    AudioHw_Lock(portMAX_DELAY);
    auto cfg = M5.Mic.config();
    cfg.sample_rate      = AUDIO_RATE;
    cfg.stereo           = false;
    cfg.task_priority    = 13;
    cfg.task_pinned_core = 1;
    M5.Mic.config(cfg);

    s_micUp = M5.Mic.begin();
    AudioHw_Unlock();

    s_micIdx    = 0;
    s_micPrimed = false;

    if (!s_micUp) Serial.println("[Audio] FEHLER: Mikrofon laesst sich nicht starten.");
}

static void micDown() {
    if (!s_micUp) return;

    AudioHw_Lock(portMAX_DELAY);
    const uint32_t guard = millis();
    while (M5.Mic.isRecording() && millis() - guard < 500) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    M5.Mic.end();
    AudioHw_Unlock();

    s_micUp     = false;
    s_micPrimed = false;
}

// ------------------------------------------------------------------------------
// Aufnahme: liefert einen FERTIG aufgenommenen Puffer oder nullptr
// ------------------------------------------------------------------------------
// M5.Mic.record() reiht den Puffer nur in die DMA-Kette ein und kehrt sofort
// zurueck - der Inhalt ist beim Ruecksprung noch nicht gueltig. Wer ihn direkt
// verschickt, sendet beim ersten Mal uninitialisierten Speicher und danach
// dauerhaft veraltete Samples.
//
// Deshalb zwei Puffer im Wechsel: einer nimmt gerade auf, der andere ist fertig
// und geht raus. isRecording() liefert die Zahl belegter Slots.
static const int16_t* micRecord() {
    const uint8_t next = (uint8_t)(s_micIdx ^ 1);

    // Auch das Mikrofon gehoert unter das Hardware-Schloss: record() ruft
    // intern begin(), ist also der empfindlichste Aufruf ueberhaupt.
    if (!AudioHw_Lock(100)) return nullptr;

    if (!M5.Mic.record(s_micBuf[next], MIC_SAMPLES, AUDIO_RATE)) {
        AudioHw_Unlock();
        return nullptr;   // nicht rotieren: es wurde nichts eingereiht
    }

    // Warten, bis hoechstens noch der eben eingereihte Puffer laeuft. Erst dann
    // ist s_micBuf[s_micIdx] garantiert vollstaendig.
    bool ready = false;
    const uint32_t guard = millis();
    while (millis() - guard < 200) {
        if (M5.Mic.isRecording() < 2) { ready = true; break; }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    AudioHw_Unlock();

    // Rotieren MUSS hier passieren: s_micBuf[next] steht jetzt in der
    // DMA-Kette, der naechste Aufruf darf ihn nicht erneut einreihen.
    const uint8_t done = s_micIdx;
    s_micIdx = next;

    if (!s_micPrimed) { s_micPrimed = true; return nullptr; }   // erste Runde

    // Zeitueberschreitung: der Puffer wird noch beschrieben. Lieber ein Paket
    // auslassen als halb aufgenommene Samples verschicken.
    if (!ready) return nullptr;

    return s_micBuf[done];
}

// ------------------------------------------------------------------------------
// Wiedergabe: ein Haeppchen aus dem Ring an die Hardware
// ------------------------------------------------------------------------------
static void feedSpeaker() {
    if (s_spkState != SPK_STREAM || !s_ring) return;

    if (s_ringOverrun) {
        s_ringOverrun = false;
        ringTrim(PREBUFFER_BYTES);
    }

    // Vorlauf abwarten, sonst startet die Wiedergabe sofort im Underrun.
    if (!s_primed) {
        if (ringUsed() < PREBUFFER_BYTES) {
#if AUDIO_PROFILE
            s_profEmpty++;
#endif
            return;
        }
        s_primed = true;
    }
    if (ringUsed() == 0) {
#if AUDIO_PROFILE
        s_profEmpty++;
#endif
        s_primed = false;
        return;
    }

    int16_t* buf = s_chunk[s_chunkIdx];
    if (!buf) return;

    if (!AudioHw_Lock(20)) return;

    // Warteschlange des Kanals voll? Dann NICHT einreihen - playRaw() wuerde
    // sonst blockieren und den Zulieferer ausbremsen.
    if (M5.Speaker.isPlaying(0) >= 2) {
#if AUDIO_PROFILE
        s_profQueueFull++;
#endif
        AudioHw_Unlock();
        return;
    }

    const size_t got = ringRead((uint8_t*)buf, CHUNK_BYTES);
    if (got < 2) { AudioHw_Unlock(); return; }

    int vol = (int)((muteMaster ? 0 : volMaster / 100.0f) *
                    (muteBaby   ? 0 : volBaby   / 100.0f) * 255.0f);
    if (vol > 255) vol = 255;
    if (vol < 0)   vol = 0;

    // Laeuft gerade ein UI- oder Alarmton, tritt der Stream zurueck.
    if ((int32_t)(s_duckUntilMs - millis()) > 0) vol /= 3;

    M5.Speaker.setChannelVolume(0, vol);

    // Fester Kanal 0: mit -1 sucht M5Unified rueckwaerts einen freien Kanal,
    // wodurch ein Dauerstrom zwischen Kanaelen springen kann.
    M5.Speaker.playRaw(buf, got / 2, AUDIO_RATE, false, 1, 0);

    AudioHw_Unlock();

#if AUDIO_PROFILE
    s_profChunks++;
    s_profLastVol = vol;
    // Pegel der tatsaechlich abgespielten Samples. Liegt er bei 0, kommt von
    // der Kamera Stille (z.B. Mikrofon der Kamera-App aus) - dann ist am
    // Tablet nichts kaputt.
    for (size_t i = 0; i < got / 2; i++) {
        const int32_t a = (buf[i] < 0) ? -(int32_t)buf[i] : (int32_t)buf[i];
        if (a > s_profPeak) s_profPeak = a;
        s_profAbsSum += (uint32_t)a;
    }
    s_profSamples += (uint32_t)(got / 2);
#endif

    s_chunkIdx = (uint8_t)((s_chunkIdx + 1) % CHUNK_SLOTS);
}

// ------------------------------------------------------------------------------
// UDP-Empfang -> Ringpuffer
// ------------------------------------------------------------------------------
static void audioRxTask(void*) {
    WiFiUDP udp;
    bool bound = false;
    static uint8_t packet[1500];

    while (true) {
        const bool want = isAudioStreaming && !isPTTActive;

        if (!want) {
            if (bound) { udp.stop(); bound = false; }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (!bound) {
            if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
            bound = udp.begin(UDP_PORT_IN);
            if (!bound) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        }

        const int size = udp.parsePacket();
        if (size > 0) {
            const int n = udp.read(packet, sizeof(packet));
            if (n > 0) {
                ringWrite(packet, (size_t)n);
                s_lastAliveMs = millis();
#if AUDIO_PROFILE
                s_profPackets = s_profPackets + 1;
                s_profBytes   = s_profBytes + (uint32_t)n;
                s_profLastLen = (uint16_t)n;
                for (int i = 0; i < 8 && i < n; i++) s_profHead[i] = packet[i];
#endif
            }
            // Auch im Erfolgsfall abgeben: bei dichtem Paketstrom wuerde diese
            // Schleife sonst nie den Scheduler erreichen und gleich priorisierte
            // Tasks verhungern lassen.
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}

// ------------------------------------------------------------------------------
// Ueberwachung des Babyphone-Stroms
// ------------------------------------------------------------------------------
// Nur aus dem Besitzer-Task: der Befehl an die Kamera ist synchron und gehoert
// in die Reihenfolge der I2S-Umschaltungen (siehe BabyCamApi.h).
static void reassertIfSilent() {
    const uint32_t now = millis();
    if (now - s_lastAliveMs < AUDIO_SILENCE_REASSERT_MS) return;
    s_lastAliveMs = now;   // auch ohne WLAN: nicht in jedem Durchlauf erneut pruefen

    if (WiFi.status() != WL_CONNECTED) return;
    Serial.println("[Audio] Kein Babyphone-Ton seit 10 s - Wiedergabe-Befehl wird erneuert.");
    BabyCamApi_SetBabyAudio(true);
}

#if AUDIO_PROFILE
// Eine Zeile alle 5 s, solange das Babyphone laeuft. Liest sich von links nach
// rechts entlang des Weges: Netz -> Jitterpuffer -> Lautsprecher.
static void logAudioProfile() {
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if (lastMs == 0) { lastMs = now; return; }
    const uint32_t elapsed = now - lastMs;
    if (elapsed < 5000) return;
    lastMs = now;

    Serial.printf("[AudioProf] Netz: %lu Pakete, %.1f KB/s | Puffer %u B | "
                  "Lautsprecher: %lu Haeppchen, leer %lu, voll %lu | "
                  "Lautstaerke %d/255 (Master %d%s, Baby %d%s) | Zustand %d\n",
                  (unsigned long)s_profPackets,
                  s_profBytes / 1.024f / elapsed,
                  (unsigned)ringUsed(),
                  (unsigned long)s_profChunks,
                  (unsigned long)s_profEmpty,
                  (unsigned long)s_profQueueFull,
                  s_profLastVol,
                  volMaster, muteMaster ? " stumm" : "",
                  volBaby,   muteBaby   ? " stumm" : "",
                  (int)s_spkState);

    // Zweite Zeile: Inhalt statt Menge. Spitzenpegel und mittlerer Betrag
    // der Samples (Vollausschlag 32767), Paketgroesse und Paketkopf.
    Serial.printf("[AudioProf] Signal: Spitze %ld, Mittel %lu | Paket %u B, Kopf %02X %02X %02X %02X %02X %02X %02X %02X | Kanal 0 spielt %d\n",
                  (long)s_profPeak,
                  (unsigned long)(s_profSamples ? (s_profAbsSum / s_profSamples) : 0),
                  (unsigned)s_profLastLen,
                  s_profHead[0], s_profHead[1], s_profHead[2], s_profHead[3],
                  s_profHead[4], s_profHead[5], s_profHead[6], s_profHead[7],
                  (int)M5.Speaker.isPlaying(0));

    s_profPackets = 0; s_profBytes = 0;
    s_profChunks = 0; s_profEmpty = 0; s_profQueueFull = 0;
    s_profPeak = 0; s_profAbsSum = 0; s_profSamples = 0;
}
#endif

// ------------------------------------------------------------------------------
// Besitzer-Task
// ------------------------------------------------------------------------------
static void audioTask(void*) {
    WiFiUDP udpOut;

    while (true) {
        // --- Kommandos abarbeiten --------------------------------------------
        AudioCmd cmd;
        while (xQueueReceive(s_cmdQ, &cmd, 0) == pdTRUE) {
            switch (cmd) {

            case CMD_START_BABY:
                if (isPTTActive || isAudioStreaming) break;
                micDown();
                if (!speakerSet(SPK_STREAM)) {
                    // Ohne Lautsprecher keine Wiedergabe - und vor allem nicht
                    // der Kamera signalisieren, dass wir zuhoeren.
                    speakerFallbackToUI();
                    break;
                }
                ringReset();                   // RX-Task schreibt hier noch nicht
                BabyCamApi_SetBabyAudio(true);
                s_lastAliveMs = millis();
                isAudioStreaming = true;
                break;

            case CMD_STOP_BABY:
                // Waehrend PTT ist isAudioStreaming bereits false - das
                // Kommando wuerde sonst wirkungslos verpuffen und der Stream
                // nach dem Loslassen gegen den Benutzerwillen wieder anlaufen.
                if (isPTTActive) { s_resumeBabyAfterPTT = false; break; }
                if (!isAudioStreaming) break;
                isAudioStreaming = false;      // Zulieferer zuerst stoppen
                vTaskDelay(pdMS_TO_TICKS(10)); // RX-Task die Runde beenden lassen
                speakerSet(SPK_UI);            // Klicks und Alarm bleiben hoerbar
                BabyCamApi_SetBabyAudio(false);
                break;

            case CMD_START_PTT: {
                if (isPTTActive) break;
                s_resumeBabyAfterPTT = isAudioStreaming;

                if (isAudioStreaming) {
                    isAudioStreaming = false;
                    vTaskDelay(pdMS_TO_TICKS(10));
                    BabyCamApi_SetBabyAudio(false);
                }
                // Halbduplex: der Lautsprecher muss komplett weg, bevor das
                // Mikrofon denselben I2S-Port belegen kann.
                speakerSet(SPK_OFF);
                micUp();

                if (!s_micUp) {
                    // Sonst staende das Geraet mit totem Mikrofon UND totem
                    // Lautsprecher da - auch der Mattenalarm waere stumm.
                    speakerFallbackToUI();
                    if (s_resumeBabyAfterPTT) {
                        s_resumeBabyAfterPTT = false;
                        if (speakerSet(SPK_STREAM)) {
                            ringReset();
                            BabyCamApi_SetBabyAudio(true);
                            s_lastAliveMs = millis();
                            isAudioStreaming = true;
                        }
                    }
                    break;
                }

                BabyCamApi_SetParentAudio(true);
                isPTTActive = true;
                break;
            }

            case CMD_STOP_PTT:
                if (!isPTTActive) break;
                isPTTActive = false;
                micDown();
                BabyCamApi_SetParentAudio(false);

                if (s_resumeBabyAfterPTT) {
                    s_resumeBabyAfterPTT = false;
                    if (speakerSet(SPK_STREAM)) {
                        ringReset();
                        BabyCamApi_SetBabyAudio(true);
                        s_lastAliveMs = millis();
                        isAudioStreaming = true;
                    } else {
                        speakerFallbackToUI();
                    }
                } else {
                    speakerFallbackToUI();
                }
                break;
            }
        }

        // --- Laufender Betrieb ------------------------------------------------
        if (isPTTActive && s_micUp) {
            const int16_t* ready = micRecord();
            if (ready) {
                if (WiFi.status() == WL_CONNECTED && streamIp.length() > 0) {
                    udpOut.beginPacket(streamIp.c_str(), UDP_PORT_OUT);
                    udpOut.write((const uint8_t*)ready, MIC_SAMPLES * sizeof(int16_t));
                    udpOut.endPacket();
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            continue;
        }

        if (isAudioStreaming && s_spkState == SPK_STREAM) {
            reassertIfSilent();
            feedSpeaker();
#if AUDIO_PROFILE
            logAudioProfile();
#endif
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle - schickt nur Kommandos, fasst keine Hardware an
// ------------------------------------------------------------------------------
static void post(AudioCmd c) {
    if (s_cmdQ) xQueueSend(s_cmdQ, &c, 0);
}

// Gibt alles frei, was bis zum Fehlerpunkt schon belegt wurde. Ein halb
// aufgebautes Audio-System waere schlimmer als gar keines: es haette Puffer
// belegt, die niemand mehr nutzt.
static void initCleanup() {
    if (s_ring) { heap_caps_free(s_ring); s_ring = nullptr; }
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        if (s_chunk[i]) { heap_caps_free(s_chunk[i]); s_chunk[i] = nullptr; }
    }
    if (s_cmdQ)    { vQueueDelete(s_cmdQ);            s_cmdQ    = nullptr; }
    if (s_hwMutex) { vSemaphoreDelete(s_hwMutex);     s_hwMutex = nullptr; }
}

void AudioStreamLogic_Init() {
    isAudioStreaming = false;
    isPTTActive      = false;

    // Der Lautsprecher laeuft zu diesem Zeitpunkt bereits: main.cpp hat ihn im
    // Setup gestartet, damit Bootmelodie und UI-Toene funktionieren. Diese
    // Konfiguration ist unser Rueckkehrpunkt nach jedem Babyphone-Betrieb.
    s_cfgUI      = M5.Speaker.config();
    s_cfgUIValid = true;
    s_spkState   = SPK_UI;
    s_spkBusy    = false;

    s_hwMutex = xSemaphoreCreateMutex();
    if (!s_hwMutex) {
        Serial.println("[Audio] FEHLER: Hardware-Mutex fehlt.");
        return;
    }

    // Der Jitterpuffer (64 KB) bleibt im PSRAM - er wird selten und in grossen
    // Bloecken angefasst, da faellt der Unterschied nicht ins Gewicht.
    s_ring = (uint8_t*)heap_caps_malloc(RING_BYTES, MALLOC_CAP_SPIRAM);

    // Die Haeppchen dagegen ins interne SRAM: aus ihnen liest die I2S-DMA
    // direkt. 3 KB kosten nichts, und DMA aus internem Speicher spart der
    // Hardware den Umweg ueber den PSRAM-Cache.
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        s_chunk[i] = (int16_t*)heap_caps_malloc(CHUNK_BYTES,
                                                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s_chunk[i]) {
            s_chunk[i] = (int16_t*)heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_SPIRAM);
        }
    }

    bool chunksOk = true;
    for (int i = 0; i < CHUNK_SLOTS; i++) if (!s_chunk[i]) chunksOk = false;

    if (!s_ring || !chunksOk) {
        Serial.println("[Audio] FEHLER: Puffer konnten nicht angelegt werden.");
        initCleanup();
        return;
    }
    ringReset();

    s_cmdQ = xQueueCreate(8, sizeof(AudioCmd));
    if (!s_cmdQ) {
        Serial.println("[Audio] FEHLER: Kommando-Queue fehlt.");
        initCleanup();
        return;
    }

    // Prioritaeten: beide klar unter der lwIP-Task (18), sonst blockierten sie
    // ihren eigenen Netzwerkstack. Der Empfang liegt knapp unter der Ausgabe.
    if (xTaskCreatePinnedToCore(audioTask, "Audio", 8192, NULL, 12, &s_audio, 1) != pdPASS) {
        Serial.println("[Audio] FEHLER: Besitzer-Task laesst sich nicht anlegen.");
        initCleanup();
        return;
    }
    if (xTaskCreatePinnedToCore(audioRxTask, "Audio_RX", 6144, NULL, 11, &s_rx, 1) != pdPASS) {
        Serial.println("[Audio] FEHLER: Empfangs-Task laesst sich nicht anlegen.");
        vTaskDelete(s_audio);
        s_audio = nullptr;
        initCleanup();
        return;
    }

    Serial.printf("[Audio] Bereit. Jitterpuffer %u KB (~%u ms).\n",
                  (unsigned)(RING_BYTES / 1024),
                  (unsigned)((RING_BYTES * 1000UL) / (AUDIO_RATE * 2)));
}

void AudioStreamLogic_StartBaby() { post(CMD_START_BABY); }
void AudioStreamLogic_StopBaby()  { post(CMD_STOP_BABY);  }
void AudioStreamLogic_StartPTT()  { post(CMD_START_PTT);  }
void AudioStreamLogic_StopPTT()   { post(CMD_STOP_PTT);   }

void AudioStreamLogic_Stop() {
    post(CMD_STOP_PTT);
    post(CMD_STOP_BABY);
}
