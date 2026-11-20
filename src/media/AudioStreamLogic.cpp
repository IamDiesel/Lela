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
//   [audioRxTask] --Quelle--> Jitterpuffer -+
//      Quelle: BabyCam (UDP) oder HTTP (PCM/WAV, AAC) - siehe AudioSourceTypes.h
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
#include "AudioSourceTypes.h"
#include "AudioSourceHttp.h"
#include "AudioCodecTab5.h"
#include "I2sRxStartGuard.h"
#include "DebugFlags.h"
#include "PttCapture.h"
#include "PcmResampler.h"
#include "VoiceProcessor.h"
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
// Die UDP-Ports sind einstellbar (SharedData.h, audioUdpPortIn/Out); meldet die
// BabyCam eigene, gelten diese (effectiveUdpPortIn/Out).

// Jitterpuffer. Muss die DMA-Kette (~43 ms) und das Underrun-Fenster (~24 ms)
// ueberbruecken; ueber SDIO-WLAN ist reichlich Reserve angebracht.
static const size_t   RING_BYTES      = 64 * 1024;          // ~740 ms
static const size_t   PREBUFFER_BYTES = 8 * 1024;           // ~93 ms Vorlauf
static const size_t   CHUNK_BYTES     = 1024;               // je playRaw-Haeppchen
static const uint8_t  CHUNK_SLOTS     = 3;                  // rotierende Puffer

static const size_t   MIC_SAMPLES     = 512;                // ~11,6 ms je Paket

// Startversuche des Mikrofons je Gegensprech-Anforderung. Der Start des
// I2S-Empfangs bleibt auf dem ESP32-P4 gelegentlich haengen (Befund
// 17.11.2026); I2sRxStartGuard erkennt das und lehnt den Start ab, statt das
// Geraet einzufrieren. Ein Neuaufbau setzt das I2S-Modul zurueck - deshalb
// genau eine Wiederholung. Scheitert auch sie, gilt das Gegensprechen als
// nicht startbar und der Lautsprecher kommt zurueck (CMD_START_PTT).
static const uint8_t  MIC_START_ATTEMPTS   = 2;
// So lange wartet micStartOnce() auf das Urteil des Schutzes. Der
// Mikrofon-Task hat die hoehere Prioritaet und ist in aller Regel schon durch,
// wenn M5.Mic.begin() zurueckkehrt.
static const uint32_t MIC_START_VERDICT_MS = 100;

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

// ------------------------------------------------------------------------------
// Konfiguration der Tonquelle
// ------------------------------------------------------------------------------
// s_cfgPending: zuletzt aus den Einstellungen gelesen (Hauptschleife, beim
//               Start-Wunsch und bei ApplyConfig).
// s_cfgActive:  gilt fuer die laufende Wiedergabe (Audio-Task setzt sie beim
//               Start, der RX-Task kopiert sie je Sitzung).
// Kopiert wird nur unter s_cfgMux - die Struktur ist zu gross fuer atomare
// Zugriffe.
static portMUX_TYPE       s_cfgMux     = portMUX_INITIALIZER_UNLOCKED;
static AudioSourceConfig  s_cfgPending = {};
static AudioSourceConfig  s_cfgActive  = {};
// Jede Wiedergabe bekommt eine neue Generation. Der RX-Task erkennt daran einen
// Neustart (z.B. neue Quelle), auch wenn isAudioStreaming dazwischen nur kurz
// false war.
static volatile uint32_t  s_streamGen  = 0;
// Abtastrate der Samples im Ring. BabyCam immer 44100; HTTP-Quellen melden
// ihre Rate ueber die Senke. M5Unified resampled je Kanal auf die Ausgaberate.
static volatile uint32_t  s_streamRate = AUDIO_RATE;
// Konfiguration beim Start des Gegensprechens (Ziel-Port). Nur Audio-Task.
static AudioSourceConfig  s_cfgPtt     = {};
// Sprachaufbereitung (VoiceProcessor): Einstellungen wie s_cfgPending von der
// Hauptschleife abgelegt (unter s_cfgMux), beim Start des Gegensprechens
// uebernommen. Der Arbeitspuffer haelt die aufbereitete Kopie eines
// Mikrofonpuffers. Prozessor und Puffer: nur Audio-Task.
static VoiceConfig        s_voicePending = {};
static VoiceProcessor     s_voice;
static int16_t            s_pttWork[MIC_SAMPLES];

static void captureConfig();   // Einstellungen -> s_cfgPending (Hauptschleife), siehe unten

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
    CMD_RESTART_BABY,   // neue Quellen-Einstellung: laufende Wiedergabe neu starten
    CMD_RESET_CODEC,    // Diagnose: Teil der Audio-Kette zuruecksetzen (AudioCodecTab5)
};

static QueueHandle_t s_cmdQ   = nullptr;
// Art des Resets fuer CMD_RESET_CODEC (AudioResetKind). Die Queue traegt nur
// das Kommando; geschrieben vom UI unmittelbar vor dem Senden.
static volatile uint8_t s_resetKind = 0;
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

// Messwerte einer Gegensprech-Sitzung, ausgegeben beim Loslassen
// (logPttStats). Sie trennen die moeglichen Ursachen einer zerhackten Stimme:
//   Pakete je Sekunde  Soll 44100 / 512 = 86,1 (gemessen 85,6 bis 86,2).
//                      Abweichung: das Mikrofon laeuft mit falscher Rate.
//   Luecken            Der Mikrofon-Task stand ohne Puffer da und hat Samples
//                      verworfen - der Audio-Task kam mit dem Nachlegen zu spaet.
//   Sendefehler        Das Paket hat das Geraet nicht verlassen.
//   laengstes Senden   Ab etwa 10 ms entsteht daraus die naechste Luecke.
//   Pegel              Spitze in Prozent der Vollaussteuerung und Zahl der
//                      Samples am Anschlag - Uebersteuerung verzerrt die Stimme.
// Nur Audio-Task.
struct PttStats {
    uint32_t startMs;
    uint32_t packets;
    uint32_t gaps;
    uint32_t sendFailed;
    uint32_t maxSendMs;
    int32_t  peak;          // groesster Betrag eines Samples
    uint32_t clipped;       // Samples am Anschlag
};
static PttStats s_ptt = {};

// M5Unified begrenzt Aufnahmewerte auf +/-(32767 - 16); alles ab hier gilt als
// "am Anschlag".
static const int32_t PTT_CLIP_LEVEL = 32700;

// Abtastrate der Sprachpakete an die Kamera. Vorgabe ist die Rate des Mikrofons
// (AUDIO_RATE, keine Umrechnung). Weicht sie ab, rechnet PcmResampler die
// Aufnahme um - der Takt des Mikrofons bleibt unberuehrt. Damit laesst sich
// pruefen, mit welcher Rate die Kamera die Pakete wirklich abspielt
// (AudioStreamLogic_SetPttSendRate).
static const uint32_t PTT_SEND_RATE_MIN = 8000;
static const uint32_t PTT_SEND_RATE_MAX = 2 * AUDIO_RATE;
static volatile uint32_t s_pttSendRateWanted = AUDIO_RATE;   // von der Oberflaeche gesetzt
static uint32_t          s_pttSendRate       = AUDIO_RATE;   // gilt fuer die laufende Sitzung

// Groesse der Sprachpakete in Samples: 250, also 500 Bytes je Paket.
//
// Die Kamera-App liest die Sprachpakete in einen Puffer von genau 500 Bytes
// (BabyCam, Empfaenger des Eltern-Tons: `new byte[500]`); was ein Paket darueber
// hinaus enthaelt, verwirft UDP. Die Eltern-App sendet deshalb selbst 500 Bytes
// je Paket. Quelle: entpackter Code der App, siehe api_RE.md Abschnitt 3.
//
// Verlauf (17.11.2026): Seit dem Umbau vom 21.09.2026 gingen 1024 Bytes je
// Paket hinaus - die Kamera spielte davon nicht einmal die Haelfte, die Stimme
// klang massiv nach Roboter. Mit 512 Bytes war es gut, es fehlten aber noch
// 12 Bytes (6 Samples) je Paket.
//
// Die Aufnahme wird deshalb unabhaengig von der Groesse des Mikrofonpuffers
// in Pakete zerlegt (forwardPttAudio). Zum Gegenpruefen umstellbar ueber
// AudioStreamLogic_SetPttPacketBytes.
static const size_t PTT_PACKET_SAMPLES_DEFAULT = 250;
static const size_t PTT_PACKET_SAMPLES_MIN     = 64;
static const size_t PTT_PACKET_SAMPLES_MAX     = MIC_SAMPLES;
static volatile size_t s_pttPacketSamplesWanted = PTT_PACKET_SAMPLES_DEFAULT;   // von der Oberflaeche gesetzt
static size_t          s_pttPacketSamples       = PTT_PACKET_SAMPLES_DEFAULT;   // gilt fuer die laufende Sitzung

// Samples, bis ein volles Paket beisammen ist. Platz: der Rest eines Pakets
// plus alles, was aus einem Mikrofonpuffer bei der hoechsten Senderate
// entstehen kann. Nur Audio-Task.
static const size_t PTT_OUT_CAPACITY = 3 * MIC_SAMPLES + 8;
static PcmResampler s_pttResampler;
static int16_t      s_pttOut[PTT_OUT_CAPACITY];
static size_t       s_pttOutCount = 0;

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

// Ein Startversuch. true = das Mikrofon laeuft.
//
// M5.Mic.begin() meldet nur, ob Kanal und Task angelegt wurden. Den
// eigentlichen Start (i2s_channel_enable) fuehrt erst der Mikrofon-Task aus,
// und M5Unified verwirft dessen Ergebnis. Ob der Start gelang, sagt deshalb
// der Schutz I2sRxStartGuard: seine Zaehler werden vor und nach begin()
// verglichen.
static bool micStartOnce(uint8_t attempt) {
    I2sRxGuardStats before;
    I2sRxStartGuard_GetStats(&before);

    AudioHw_Lock(portMAX_DELAY);
    auto cfg = M5.Mic.config();
    cfg.sample_rate      = AUDIO_RATE;
    cfg.stereo           = false;
    // over_sampling bleibt auf der M5Unified-Vorgabe (2), wie in v2.0.0.
    // Gemessen am 17.11.2026 (Sitzungsbilanz, logPttStats): 86 Pakete/s, das
    // sind die verlangten 44100 Hz. Eine Rechnung aus dem Quelltext von
    // M5Unified hatte 39062 Hz ergeben und zur Umstellung auf 1 gefuehrt -
    // die Rechnung war falsch, die Stimme wurde davon tiefer. Zurueckgenommen;
    // nicht ohne Messung aendern.
    cfg.task_priority    = 13;
    cfg.task_pinned_core = 1;
    M5.Mic.config(cfg);

    bool up = M5.Mic.begin();

    // M5Unified startet das Mikrofon beim ersten record() nach dem Einschalten
    // von sich aus neu (Mic_Class::begin: _rec_sample_rate wird erst dort
    // gesetzt). Dieser zweite Start soll HIER passieren, wo sein Ergebnis
    // geprueft wird - nicht spaeter mitten in der Aufnahme. Eine Aufnahme der
    // Laenge 0 loest genau das aus und reiht nichts ein.
    if (up) up = M5.Mic.record(s_micBuf[0], 0, AUDIO_RATE);

    if (up) {
        // Auf das Urteil des Schutzes warten (begrenzt).
        I2sRxGuardStats now = before;
        const uint32_t waitStart = millis();
        for (;;) {
            I2sRxStartGuard_GetStats(&now);
            if (now.starts != before.starts) break;
            if (millis() - waitStart >= MIC_START_VERDICT_MS) break;
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (now.starts == before.starts) {
            // Kein Urteil: Der Schutz ist nicht eingebunden. Dann verhaelt sich
            // der Start wie frueher - also weitermachen, aber nicht still.
            Serial.println("[Audio] Hinweis: Startschutz des Mikrofons meldet sich nicht "
                           "(platformio.ini: -Wl,--wrap=i2s_channel_enable).");
        } else {
            if (now.refused != before.refused) {
                Serial.printf("[Audio] Mikrofonstart abgefangen (Versuch %u/%u, %lu von %lu Starts, %s): "
                              "rx_conf %08lX, Takt %08lX %08lX %08lX %08lX.\n",
                              (unsigned)attempt, (unsigned)MIC_START_ATTEMPTS,
                              (unsigned long)(now.refused - before.refused),
                              (unsigned long)(now.starts - before.starts),
                              I2sRxStartGuard_StageText(now.lastStage),
                              (unsigned long)now.lastRxConf,
                              (unsigned long)now.lastClk[0], (unsigned long)now.lastClk[1],
                              (unsigned long)now.lastClk[2], (unsigned long)now.lastClk[3]);
            }
            // Es zaehlt der juengste Start: Wurde er abgelehnt, laeuft der
            // Kanal nicht. Task beenden und Kanal abbauen - das ist auch die
            // Voraussetzung fuer den naechsten Versuch.
            if (now.lastResult != I2S_RX_GUARD_OK) {
                M5.Mic.end();
                up = false;
            }
        }
    }
    AudioHw_Unlock();

    return up;
}

static void micUp() {
    if (s_micUp) return;

    for (uint8_t attempt = 1; attempt <= MIC_START_ATTEMPTS && !s_micUp; attempt++) {
        s_micUp = micStartOnce(attempt);
        if (s_micUp && attempt > 1) Serial.println("[Audio] Mikrofon laeuft nach Neuaufbau.");
    }

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

    // Luecke: Nach dem Anlauf muss beim Nachlegen noch ein Puffer in Arbeit
    // sein. Steht der Mikrofon-Task schon im Leerlauf, hat er die Samples
    // seither verworfen - dieser Umlauf kam zu spaet.
    if (s_micPrimed && M5.Mic.isRecording() == 0) s_ptt.gaps++;

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
    M5.Speaker.playRaw(buf, got / 2, s_streamRate, false, 1, 0);

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
// Senke fuer die Quellen (AudioSourceTypes.h) - nur aus dem Empfangs-Task
// ------------------------------------------------------------------------------
// TCP-Quellen (HTTP) liefern so schnell, wie der Server sendet. Statt bei
// vollem Puffer zu verwerfen, wartet die Senke kurz: Der Gegendruck bremst
// ueber das TCP-Fenster den Server. UDP kennt das nicht - dort gilt weiter
// "verwerfen und nachziehen" (ringWrite/ringTrim).
static const uint32_t SINK_WAIT_MAX_MS = 200;

static uint32_t s_rxGen = 0;   // Generation der laufenden Empfangssitzung (RX-Task)

static bool sinkRunning() {
    return isAudioStreaming && !isPTTActive && s_rxGen == s_streamGen;
}

static void sinkWrite(const int16_t* mono, size_t samples) {
    // Nach dem Ende der Wiedergabe nichts mehr schreiben: der Audio-Task setzt
    // den Ring beim naechsten Start zurueck (ringReset), waehrend eine HTTP-
    // Quelle noch in einem blockierenden Aufruf stecken kann.
    if (!sinkRunning()) return;
    size_t bytes = samples * sizeof(int16_t);
    const uint32_t start = millis();
    while (ringFree() < bytes && sinkRunning() && millis() - start < SINK_WAIT_MAX_MS) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (!sinkRunning()) return;
    ringWrite((const uint8_t*)mono, bytes);
    s_lastAliveMs = millis();
#if AUDIO_PROFILE
    s_profPackets = s_profPackets + 1;
    s_profBytes   = s_profBytes + (uint32_t)bytes;
    s_profLastLen = (uint16_t)bytes;
#endif
}

static void sinkSetRate(uint32_t hz) {
    if (hz < 8000 || hz > 96000 || hz == s_streamRate) return;
    s_streamRate = hz;
}

static const AudioSink s_sink = { sinkWrite, sinkSetRate, sinkRunning };

// Empfangsport der BabyCam: was die Kamera meldet, sonst die Einstellung.
static uint16_t effectiveUdpPortIn(const AudioSourceConfig& cfg) {
    const int cam = camAudioPortIn;
    return (cam > 0 && cam <= 65535) ? (uint16_t)cam : cfg.udpPortIn;
}

static uint16_t effectiveUdpPortOut(const AudioSourceConfig& cfg) {
    const int cam = camAudioPortOut;
    return (cam > 0 && cam <= 65535) ? (uint16_t)cam : cfg.udpPortOut;
}

// ------------------------------------------------------------------------------
// Empfangs-Task: Quelle -> Ringpuffer
// ------------------------------------------------------------------------------
// Je Wiedergabe (Generation) eine Sitzung mit der beim Start eingefrorenen
// Konfiguration. BabyCam: UDP-Pakete sind schon Mono-PCM und gehen direkt in
// den Ring. HTTP: AudioSourceHttp_Run() blockiert, bis die Wiedergabe endet
// oder die Verbindung abreisst; danach folgt ein Neuversuch mit Wartezeit.
static void audioRxTask(void*) {
    WiFiUDP udp;
    uint16_t boundPort = 0;
    static uint8_t packet[1500];
    AudioSourceConfig cfg;
    uint32_t httpRetryMs = 0;      // naechster HTTP-Versuch frueheste Zeit
    uint32_t httpBackoff = 2000;

    // Messung je Wiedergabe: mit welcher Rate und Paketgroesse die BabyCam
    // wirklich sendet (eine Logzeile nach RATE_CHECK_MS). Das Format ist nur
    // aus Beobachtung bekannt (api_RE.md) und kann sich mit der Kamera-App
    // aendern - dann stimmt auch die Annahme fuer das Gegensprechen nicht mehr.
    static const uint32_t RATE_CHECK_MS = 10000;
    uint32_t rateStartMs = 0;      // Zeitpunkt des ersten Pakets, 0 = noch keines
    uint32_t rateBytes   = 0;      // Bytes NACH dem ersten Paket
    int      rateFirstLen = 0;     // Groesse des ersten Pakets
    bool     rateLogged  = false;

    while (true) {
        const bool want = isAudioStreaming && !isPTTActive;

        if (!want) {
            if (boundPort) { udp.stop(); boundPort = 0; }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Neue Wiedergabe: Konfiguration uebernehmen.
        if (s_rxGen != s_streamGen) {
            if (boundPort) { udp.stop(); boundPort = 0; }
            taskENTER_CRITICAL(&s_cfgMux);
            cfg = s_cfgActive;
            s_rxGen = s_streamGen;
            taskEXIT_CRITICAL(&s_cfgMux);
            httpRetryMs = 0;
            httpBackoff = 2000;
            rateStartMs = 0;
            rateBytes   = 0;
            rateLogged  = false;
        }

        if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }

        // --- HTTP-Strom ---------------------------------------------------------
        if (cfg.source == AUDIO_SRC_HTTP) {
            if ((int32_t)(millis() - httpRetryMs) < 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
            const bool gotAudio = AudioSourceHttp_Run(cfg, s_sink);
            // Ton kam an: rasch wieder verbinden. Sonst wachsend warten,
            // damit eine falsche URL das Log nicht im Sekundentakt fuellt.
            httpBackoff = gotAudio ? 2000 : ((httpBackoff * 2 > 30000) ? 30000 : httpBackoff * 2);
            httpRetryMs = millis() + httpBackoff;
            continue;
        }

        // --- BabyCam (UDP) ------------------------------------------------------
        const uint16_t port = effectiveUdpPortIn(cfg);
        if (boundPort && boundPort != port) {
            // Die Kamera hat im Status einen anderen Port gemeldet.
            udp.stop();
            boundPort = 0;
        }
        if (!boundPort) {
            if (!udp.begin(port)) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
            boundPort = port;
            if (port != cfg.udpPortIn) {
                Serial.printf("[Audio] BabyCam meldet Empfangsport %u (eingestellt %u) - Kamerawert gilt.\n",
                              port, cfg.udpPortIn);
            }
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
                if (rateStartMs == 0) {
                    rateStartMs  = millis();
                    rateFirstLen = n;
                } else if (!rateLogged) {
                    rateBytes += (uint32_t)n;
                    const uint32_t elapsed = millis() - rateStartMs;
                    if (elapsed >= RATE_CHECK_MS) {
                        rateLogged = true;
                        Serial.printf("[Audio] Babyphone-Ton kommt mit %lu Samples/s in Paketen zu %d Bytes "
                                      "(Wiedergabe mit %lu Hz).\n",
                                      (unsigned long)((uint64_t)rateBytes * 1000u / sizeof(int16_t) / elapsed),
                                      rateFirstLen, (unsigned long)s_streamRate);
                    }
                }
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
    // Nur die BabyCam braucht den Befehl; HTTP-Quellen verbindet der RX-Task neu.
    if (s_cfgActive.source != AUDIO_SRC_BABYCAM_UDP) return;
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
// Babyphone-Wiedergabe starten/stoppen - nur aus dem Besitzer-Task
// ------------------------------------------------------------------------------
static const char* sourceName(const AudioSourceConfig& c) {
    if (c.source == AUDIO_SRC_HTTP) return (c.format == AUDIO_FMT_AAC) ? "HTTP AAC" : "HTTP PCM";
    return "BabyCam UDP";
}

// Lautsprecher auf Strom, Ring leeren, Konfiguration einfrieren, Quelle starten.
// false: Lautsprecher nicht verfuegbar (dann ist der Grundzustand wiederhergestellt).
static bool startBabyStream() {
    if (!speakerSet(SPK_STREAM)) {
        // Ohne Lautsprecher keine Wiedergabe - und vor allem nicht der Kamera
        // signalisieren, dass wir zuhoeren.
        speakerFallbackToUI();
        return false;
    }
    ringReset();                       // RX-Task schreibt hier nicht (isAudioStreaming false)

    taskENTER_CRITICAL(&s_cfgMux);
    s_cfgActive = s_cfgPending;
    taskEXIT_CRITICAL(&s_cfgMux);

    // BabyCam: feste 44,1 kHz. HTTP-PCM: Einstellung, bis ein WAV-Kopf etwas
    // anderes meldet. AAC: Platzhalter, bis der erste Rahmen dekodiert ist.
    s_streamRate = (s_cfgActive.source == AUDIO_SRC_HTTP && s_cfgActive.format == AUDIO_FMT_PCM16)
                       ? s_cfgActive.pcmRate : AUDIO_RATE;

    if (s_cfgActive.source == AUDIO_SRC_BABYCAM_UDP) {
        BabyCamApi_SetBabyAudio(true);
        Serial.printf("[Audio] Babyphone-Ton: %s, Empfangsport %u\n",
                      sourceName(s_cfgActive), effectiveUdpPortIn(s_cfgActive));
    } else {
        Serial.printf("[Audio] Babyphone-Ton: %s, %s\n", sourceName(s_cfgActive),
                      AudioSourceHttp_LogUrl(s_cfgActive.url).c_str());
    }

    s_lastAliveMs = millis();
    s_streamGen = s_streamGen + 1;     // vor isAudioStreaming: RX-Task sieht die neue Sitzung
    isAudioStreaming = true;
    return true;
}

// Zulieferer anhalten, Kamera abmelden. Den Lautsprecher stellt der Aufrufer
// ein (UI-Zustand, aus fuer PTT oder gleich wieder Strom).
static void stopBabyStream() {
    isAudioStreaming = false;          // Zulieferer zuerst stoppen
    vTaskDelay(pdMS_TO_TICKS(10));     // RX-Task die Runde beenden lassen
    if (s_cfgActive.source == AUDIO_SRC_BABYCAM_UDP) BabyCamApi_SetBabyAudio(false);
}

// Diagnose-Hoertest (AudioResetKind::SoundTest): feste Abstaende, damit sich
// jedes Geraeusch einem Schritt zuordnen laesst. Blockiert den Besitzer-Task
// fuer ~8 s - nur zur Fehlersuche.
static void runSoundTest(SpkState before) {
    static constexpr uint32_t STEP_MS = 1500;
    const uint32_t t0 = millis();
    auto step = [t0](const char* what) {
        Serial.printf("[Hoertest] +%4lu ms: %s\n", (unsigned long)(millis() - t0), what);
    };
    vTaskDelay(pdMS_TO_TICKS(STEP_MS));            // Abstand zum Tastenklick
    step("1 Lautsprecher AUS (Verstaerker aus)");
    speakerSet(SPK_OFF);
    vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    step("2 Lautsprecher AN (Codec-Init, Verstaerker an)");
    if (!speakerSet(before == SPK_STREAM ? SPK_STREAM : SPK_UI)) speakerFallbackToUI();
    vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    step("3 Klick (wie Tastenton)");
    playToneI2S(800, 15, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    step("4 Ton 1 kHz, 300 ms");
    playToneI2S(1000, 300, false);
    vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    step("5 Klick (wie Tastenton)");
    playToneI2S(800, 15, true);
}

// Diagnose "Erster Ton" (AudioResetKind::FirstTone): Lautsprecher neu starten
// (M5Unified initialisiert dabei den Codec) und danach sechs kurze Pieptoene
// zu festen Zeiten nach dem Neustart. Die Zahl der hoerbaren Pieptoene zeigt,
// wie lange der Codec im stummen Zustand nach der Initialisierung noch Ton
// ausgibt.
static void runFirstTone(SpkState before) {
    static constexpr uint32_t AT_MS[] = { 0, 150, 400, 800, 1500, 3000 };
    vTaskDelay(pdMS_TO_TICKS(1500));               // Abstand zum Tastenklick
    speakerSet(SPK_OFF);
    if (!speakerSet(before == SPK_STREAM ? SPK_STREAM : SPK_UI)) speakerFallbackToUI();
    const uint32_t t0 = millis();                  // Zeitpunkt der Codec-Initialisierung
    Serial.println("[ErsterTon] Lautsprecher neu gestartet (Codec initialisiert).");
    int n = 1;
    for (uint32_t at : AT_MS) {
        const uint32_t now = millis() - t0;
        if (at > now) vTaskDelay(pdMS_TO_TICKS(at - now));
        Serial.printf("[ErsterTon] Piep %d bei +%lu ms (1 kHz, 60 ms)\n", n++, (unsigned long)(millis() - t0));
        playToneI2S(1000, 60, false);
    }
}

// Diagnose-Frequenztest (AudioResetKind::ToneSweep): gleiche Tonausgabe wie
// alle Toene (playToneI2S, Kanal Alarm), nur Frequenz und Dauer variieren.
// Der Lautsprecher wird NICHT umgeschaltet.
static void runToneSweep() {
    struct ToneStep { uint16_t hz; uint16_t ms; };
    static constexpr ToneStep STEPS[] = {
        { 150, 300 }, { 400, 300 }, { 800, 300 }, { 1600, 300 }, { 3200, 300 }, { 6400, 300 },
        { 800, 15 }, { 150, 15 },
    };
    const uint32_t t0 = millis();
    vTaskDelay(pdMS_TO_TICKS(1500));               // Abstand zum Tastenklick
    for (const ToneStep& s : STEPS) {
        Serial.printf("[Frequenztest] +%5lu ms: %u Hz, %u ms\n",
                      (unsigned long)(millis() - t0), (unsigned)s.hz, (unsigned)s.ms);
        playToneI2S(s.hz, s.ms, false);
        vTaskDelay(pdMS_TO_TICKS(1000 + s.ms));
    }
}

// Pegel eines aufgenommenen Pakets in die Sitzungsbilanz uebernehmen.
static void notePttLevel(const int16_t* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const int32_t level = samples[i] < 0 ? -(int32_t)samples[i] : (int32_t)samples[i];
        if (level > s_ptt.peak)      s_ptt.peak = level;
        if (level >= PTT_CLIP_LEVEL) s_ptt.clipped++;
    }
}

// Bilanz der Gegensprech-Sitzung, eine Zeile beim Loslassen (siehe PttStats).
static void logPttStats() {
    const uint32_t ms       = millis() - s_ptt.startMs;
    const uint32_t rate10   = ms ? (uint32_t)((uint64_t)s_ptt.packets * 10000u / ms) : 0;   // Pakete/s x 10
    const uint32_t target10 = s_pttSendRate * 10u / s_pttPacketSamples;
    const int gainTenth     = VoiceProcessor_GainTenthDb(&s_voice);
    Serial.printf("[Audio] Gegensprechen: %lu.%lu s, %lu Pakete (%lu.%lu/s, Soll %lu.%lu bei %lu Hz), "
                  "%lu Luecken, %lu Sendefehler, laengstes Senden %lu ms, "
                  "Pegelspitze %ld %%, %lu Samples am Anschlag, Verstaerkung zuletzt %s%d.%d dB.\n",
                  (unsigned long)(ms / 1000), (unsigned long)((ms % 1000) / 100),
                  (unsigned long)s_ptt.packets,
                  (unsigned long)(rate10 / 10), (unsigned long)(rate10 % 10),
                  (unsigned long)(target10 / 10), (unsigned long)(target10 % 10),
                  (unsigned long)s_pttSendRate,
                  (unsigned long)s_ptt.gaps, (unsigned long)s_ptt.sendFailed,
                  (unsigned long)s_ptt.maxSendMs,
                  (long)(s_ptt.peak * 100 / 32767), (unsigned long)s_ptt.clipped,
                  gainTenth < 0 ? "-" : "+", abs(gainTenth) / 10, abs(gainTenth) % 10);
}

// Ein Sprachpaket (count Werte) an die Kamera schicken und in die
// Sitzungsbilanz aufnehmen.
static void sendPttPacket(WiFiUDP& udp, const int16_t* samples, size_t count) {
    const uint32_t sendStart = millis();
    udp.beginPacket(streamIp.c_str(), effectiveUdpPortOut(s_cfgPtt));
    udp.write((const uint8_t*)samples, count * sizeof(int16_t));
    const bool sent = (udp.endPacket() == 1);
    const uint32_t sendMs = millis() - sendStart;

    s_ptt.packets++;
    if (!sent) s_ptt.sendFailed++;
    if (sendMs > s_ptt.maxSendMs) s_ptt.maxSendMs = sendMs;
    notePttLevel(samples, count);
#if DIAG_TEST_TOOLS
    PttCapture_Add(samples, count);
#endif
}

// Einen fertigen Mikrofonpuffer weitergeben: aufbereiten, bei abweichender
// Senderate umrechnen, sammeln und in Paketen der gewaehlten Groesse senden.
// Paket- und Puffergroesse muessen nicht zueinander passen - was kein volles
// Paket ergibt, bleibt fuer den naechsten Puffer liegen (hoechstens ein Paket,
// also wenige Millisekunden). Mehrere Pakete aus einem Puffer gehen unmittelbar
// nacheinander hinaus.
static void forwardPttAudio(WiFiUDP& udp, const int16_t* mic) {
    const size_t packet = s_pttPacketSamples;

    // Sprachaufbereitung auf einer Kopie: der Mikrofonpuffer gehoert M5.Mic.
    if (!VoiceProcessor_IsBypass(&s_voice)) {
        memcpy(s_pttWork, mic, sizeof(s_pttWork));
        VoiceProcessor_Process(&s_voice, s_pttWork, MIC_SAMPLES);
        mic = s_pttWork;
    }

    // Anhaengen: unveraendert oder auf die Senderate umgerechnet.
    if (s_pttSendRate == AUDIO_RATE) {
        memcpy(s_pttOut + s_pttOutCount, mic, MIC_SAMPLES * sizeof(int16_t));
        s_pttOutCount += MIC_SAMPLES;
    } else {
        s_pttOutCount += PcmResampler_Process(&s_pttResampler, mic, MIC_SAMPLES,
                                              s_pttOut + s_pttOutCount,
                                              PTT_OUT_CAPACITY - s_pttOutCount);
    }

    size_t sent = 0;
    while (s_pttOutCount - sent >= packet) {
        sendPttPacket(udp, s_pttOut + sent, packet);
        sent += packet;
    }
    if (sent > 0) {
        s_pttOutCount -= sent;
        memmove(s_pttOut, s_pttOut + sent, s_pttOutCount * sizeof(int16_t));
    }
}

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
                startBabyStream();
                break;

            case CMD_STOP_BABY:
                // Waehrend PTT ist isAudioStreaming bereits false - das
                // Kommando wuerde sonst wirkungslos verpuffen und der Stream
                // nach dem Loslassen gegen den Benutzerwillen wieder anlaufen.
                if (isPTTActive) { s_resumeBabyAfterPTT = false; break; }
                if (!isAudioStreaming) break;
                stopBabyStream();
                speakerSet(SPK_UI);            // Klicks und Alarm bleiben hoerbar
                break;

            case CMD_RESTART_BABY:
                // Neue Quellen-Einstellung. Laeuft gerade nichts, gilt sie
                // beim naechsten Start von selbst.
                if (isPTTActive || !isAudioStreaming) break;
                Serial.println("[Audio] Tonquelle geaendert - Wiedergabe wird neu gestartet.");
                stopBabyStream();
                startBabyStream();             // Lautsprecher bleibt im Strom-Zustand
                break;

            case CMD_RESET_CODEC: {
                // Halbduplex: waehrend des Gegensprechens gehoert der I2S-Port
                // dem Mikrofon - dann nicht eingreifen.
                if (isPTTActive) {
                    Serial.println("[Audio] Codec-Reset waehrend Gegensprechen ignoriert.");
                    break;
                }
                const SpkState before = s_spkState;
                const AudioResetKind kind = (AudioResetKind)s_resetKind;
                if (kind == AudioResetKind::SoundTest) {   // kein Reset, nur Hoertest
                    runSoundTest(before);
                    break;
                }
                if (kind == AudioResetKind::ToneSweep) {   // kein Reset, nur Frequenztest
                    runToneSweep();
                    break;
                }
                if (kind == AudioResetKind::FirstTone) {   // kein Reset, nur Hoertest
                    runFirstTone(before);
                    break;
                }
                AudioCodec_Dump("vor Reset");
                if (AudioCodec_NeedsClock(kind)) {
                    // Reset bei laufendem Takt: Lautsprecher muss an sein.
                    if (s_spkState == SPK_OFF) speakerSet(SPK_UI);
                    AudioHw_Lock(portMAX_DELAY);
                    AudioCodec_Reset(kind);
                    AudioHw_Unlock();
                    speakerSet(SPK_OFF);           // danach frisch initialisieren
                } else {
                    speakerSet(SPK_OFF);           // geordneter Teardown
                    AudioHw_Lock(portMAX_DELAY);
                    AudioCodec_Reset(kind);
                    AudioHw_Unlock();
                }
                // begin() schreibt die Betriebswerte und gibt den Codec frei.
                if (!speakerSet(before == SPK_STREAM ? SPK_STREAM : SPK_UI)) speakerFallbackToUI();
                AudioCodec_Dump("nach Reset");
                playToneI2S(1000, 400, false);     // Testton: hoerbar = Codec wieder da
                break;
            }

            case CMD_START_PTT: {
                if (isPTTActive) break;
                taskENTER_CRITICAL(&s_cfgMux);
                s_cfgPtt = s_cfgPending;
                const VoiceConfig voiceCfg = s_voicePending;
                taskEXIT_CRITICAL(&s_cfgMux);
                // Gegensprechen gibt es nur mit der BabyCam: nur sie hat einen
                // Rueckkanal (UDP an die Kamera).
                if (s_cfgPtt.source != AUDIO_SRC_BABYCAM_UDP) {
                    Serial.println("[Audio] Gegensprechen nur mit Tonquelle BabyCam (UDP) - ignoriert.");
                    break;
                }
                s_resumeBabyAfterPTT = isAudioStreaming;

                if (isAudioStreaming) stopBabyStream();
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
                        startBabyStream();
                    }
                    break;
                }

                BabyCamApi_SetParentAudio(true);

                // Senderate und Paketgroesse fuer diese Sitzung festlegen
                // (Wechsel wirkt erst beim naechsten Druck) und die Umrechnung
                // von vorn beginnen.
                s_pttPacketSamples = s_pttPacketSamplesWanted;
                s_pttSendRate = s_pttSendRateWanted;
                VoiceProcessor_Reset(&s_voice, voiceCfg, AUDIO_RATE);
                Serial.printf("[Audio] Sprachaufbereitung: Hochpass %u Hz, Tiefpass %u Hz, %s %u dB (0 = aus).\n",
                              (unsigned)s_voice.cfg.highpassHz, (unsigned)s_voice.cfg.lowpassHz,
                              s_voice.cfg.autoGain ? "Pegelautomatik bis" : "feste Verstaerkung",
                              (unsigned)s_voice.cfg.gainDb);
                PcmResampler_Reset(&s_pttResampler, AUDIO_RATE, s_pttSendRate);
                s_pttOutCount = 0;

                // Ziel der Sprachpakete. Der Port kommt aus dem Kamera-Status,
                // wenn sie einen meldet (effectiveUdpPortOut) - seit v2.0.0 die
                // einzige Aenderung am Sendeweg, deshalb steht er im Log.
                Serial.printf("[Audio] Gegensprechen sendet an %s:%u (%u Bytes je Paket, %lu Hz).\n",
                              streamIp.c_str(), (unsigned)effectiveUdpPortOut(s_cfgPtt),
                              (unsigned)(s_pttPacketSamples * sizeof(int16_t)), (unsigned long)s_pttSendRate);
                s_ptt = {};
                s_ptt.startMs = millis();
#if DIAG_TEST_TOOLS
                PttCapture_Begin(s_pttSendRate);   // Mitschnitt fuer /ptt_capture
#endif
                isPTTActive = true;
                break;
            }

            case CMD_STOP_PTT:
                if (!isPTTActive) break;
                isPTTActive = false;
                logPttStats();
#if DIAG_TEST_TOOLS
                PttCapture_End();
                // Zustand des Empfangs, solange das Mikrofon noch laeuft. Der
                // volle Auszug (liest ES7210/ES8388 ueber I2C) nur einmal je
                // Einschalten, die Registerzeile jedes Mal.
                {
                    static bool s_dumpedOnce = false;
                    if (!s_dumpedOnce) { s_dumpedOnce = true; AudioCodec_Dump("Gegensprechen"); }
                    AudioCodec_DumpI2sRx("Gegensprechen");
                }
#endif
                micDown();
                BabyCamApi_SetParentAudio(false);

                if (s_resumeBabyAfterPTT) {
                    s_resumeBabyAfterPTT = false;
                    startBabyStream();         // faellt bei Fehler selbst auf UI zurueck
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
                    forwardPttAudio(udpOut, ready);
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
    captureConfig();

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
    // 8 KB Stack: Der Empfang laeuft je nach Quelle ueber HTTPClient und den
    // AAC-Decoder (Helix rechnet mit lokalen Feldern); 6 KB reichten nur fuer UDP.
    if (xTaskCreatePinnedToCore(audioRxTask, "Audio_RX", 8192, NULL, 11, &s_rx, 1) != pdPASS) {
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

// Liest die Einstellungen (SharedData.h) in s_cfgPending. Nur aus der
// Hauptschleife - dort werden die Einstellungen auch geaendert.
static void captureConfig() {
    AudioSourceConfig c = {};
    c.source      = (uint8_t)audioSource;
    c.format      = (uint8_t)audioFormat;
    c.pcmChannels = (uint8_t)audioPcmChannels;
    c.pcmRate     = (uint32_t)audioPcmRate;
    c.udpPortIn   = (uint16_t)audioUdpPortIn;
    c.udpPortOut  = (uint16_t)audioUdpPortOut;
    strlcpy(c.url, babyStreamUrl.c_str(), sizeof(c.url));

    VoiceConfig v = {};
    v.highpassHz = (uint16_t)pttHighpassHz;
    v.lowpassHz  = (uint16_t)pttLowpassHz;
    v.autoGain   = pttAutoGain;
    v.gainDb     = (uint8_t)pttGainDb;

    taskENTER_CRITICAL(&s_cfgMux);
    s_cfgPending   = c;
    s_voicePending = v;
    taskEXIT_CRITICAL(&s_cfgMux);
}

void AudioStreamLogic_StartBaby() { captureConfig(); post(CMD_START_BABY); }
void AudioStreamLogic_StopBaby()  { post(CMD_STOP_BABY);  }
void AudioStreamLogic_StartPTT()  { captureConfig(); post(CMD_START_PTT);  }
void AudioStreamLogic_StopPTT()   { post(CMD_STOP_PTT);   }

void AudioStreamLogic_ApplyConfig() {
    captureConfig();
    post(CMD_RESTART_BABY);
}

void AudioStreamLogic_ResetAudio(uint8_t kind) {
    s_resetKind = kind;
    post(CMD_RESET_CODEC);
}

void AudioStreamLogic_SetPttSendRate(uint32_t hz) {
    if (hz < PTT_SEND_RATE_MIN) hz = PTT_SEND_RATE_MIN;
    if (hz > PTT_SEND_RATE_MAX) hz = PTT_SEND_RATE_MAX;
    s_pttSendRateWanted = hz;
}

uint32_t AudioStreamLogic_GetPttSendRate() {
    return s_pttSendRateWanted;
}

void AudioStreamLogic_SetPttPacketBytes(size_t bytes) {
    size_t samples = bytes / sizeof(int16_t);   // nur ganze Samples
    if (samples < PTT_PACKET_SAMPLES_MIN) samples = PTT_PACKET_SAMPLES_MIN;
    if (samples > PTT_PACKET_SAMPLES_MAX) samples = PTT_PACKET_SAMPLES_MAX;
    s_pttPacketSamplesWanted = samples;
}

size_t AudioStreamLogic_GetPttPacketBytes() {
    return s_pttPacketSamplesWanted * sizeof(int16_t);
}

bool AudioStreamLogic_SupportsPTT() {
    return audioSource == AUDIO_SRC_BABYCAM_UDP;
}

void AudioStreamLogic_Stop() {
    post(CMD_STOP_PTT);
    post(CMD_STOP_BABY);
}
