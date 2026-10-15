// ==============================================================================
// BabyService - Implementierung (siehe Header)
// ==============================================================================

#include "BabyService.h"
#include "SharedData.h"
#include "VideoLogic.h"
#include "AudioStreamLogic.h"
#include "BabyCamApi.h"
#include "HeapMonitor.h"
#include <Preferences.h>
#include <esp_heap_caps.h>

// ------------------------------------------------------------------------------
// Abstimmung
// ------------------------------------------------------------------------------
// Weicht der Audiozustand vom Wunsch ab (Start fehlgeschlagen, Lautsprecher
// nicht verfuegbar), wird das Kommando in diesem Abstand wiederholt. Die
// Kommandos sind im Audio-Task idempotent; ein doppeltes schadet also nicht.
static const uint32_t AUDIO_RETRY_MS = 3000;

static const int PIP_FPS_DEFAULT = 5;
static const int PIP_FPS_MIN     = 1;
static const int PIP_FPS_MAX     = 15;

// Flash: eigener Namensraum fuer das Overlay, die Baby-Aufloesung liegt aus
// historischen Gruenden unter "catmat"/"camRes".
static const char* NVS_PIP       = "pip";
static const char* KEY_PIP_FPS   = "fps";
static const char* KEY_PIP_RES   = "res";
static const char* KEY_RES_LIST  = "resList";
static const char* NVS_MAIN      = "catmat";
static const char* KEY_BABY_RES  = "camRes";

// ------------------------------------------------------------------------------
// Zustand - nur aus der Hauptschleife
// ------------------------------------------------------------------------------
static BabyMode s_mode            = BabyMode::Off;
static bool     s_babyViewVisible = false;
static bool     s_pipVisible      = false;
static bool     s_displayOn       = true;

static bool     s_audioWantSent   = false;   // zuletzt an AudioStreamLogic gemeldeter Wunsch
static uint32_t s_audioCmdMs      = 0;       // Zeitpunkt dieses Kommandos

static int      s_pipFps          = PIP_FPS_DEFAULT;
static String   s_pipRes;                    // Einstellung, "" = automatisch
static String   s_babyRes;                   // Einstellung der Baby-Ansicht
static String   s_appliedRes;                // zuletzt an die Kamera gesendet
static String   s_knownRes;                  // letzte bekannte Kameraliste ('\n')

// Sicherheitsventil (siehe "Sicherheitsventil" weiter unten)
static const uint32_t MEM_CHECK_MS        = 250;
static const size_t   MEM_PAUSE_BELOW     = 12 * 1024;
static const size_t   MEM_RESUME_ABOVE    = 32 * 1024;
static const uint32_t MEM_HOLD_MIN_MS     = 10000;
static const uint32_t MEM_HOLD_MAX_MS     = 300000;
static const uint32_t MEM_CALM_MS         = 600000;
static bool     s_memPaused       = false;
static uint32_t s_memLastCheck    = 0;
static uint32_t s_memOkSince      = 0;
static uint32_t s_memHoldMs       = MEM_HOLD_MIN_MS;
static uint32_t s_memLastResume   = 0;

// ------------------------------------------------------------------------------
// Hilfen
// ------------------------------------------------------------------------------
BabyMode BabyMode_From(bool video, bool audio) {
    if (video && audio) return BabyMode::VideoAudio;
    if (video)          return BabyMode::Video;
    if (audio)          return BabyMode::AudioOnly;
    return BabyMode::Off;
}

const char* BabyMode_Name(BabyMode m) {
    switch (m) {
        case BabyMode::Off:        return "Aus";
        case BabyMode::AudioOnly:  return "Audio";
        case BabyMode::Video:      return "Video";
        case BabyMode::VideoAudio: return "Video+Audio";
    }
    return "?";
}

// Waehrend des Web-Setups ruht der Dienst vollstaendig: das Setup-Portal baut
// einen eigenen Netzbetrieb auf (Access Point), in dem die Kamera ohnehin nicht
// erreichbar ist. Der gewaehlte Modus bleibt erhalten und gilt danach wieder.
static bool suspended() {
    return webSetupMode > 0 || pendingWebSetupMode > 0;
}

static BabyMode effectiveMode() {
    return suspended() ? BabyMode::Off : s_mode;
}

// Zerlegt "1280x720". false, wenn das Format nicht stimmt.
static bool parseRes(const String& res, int* w, int* h) {
    const int x = res.indexOf('x');
    if (x <= 0) return false;
    *w = res.substring(0, x).toInt();
    *h = res.substring(x + 1).toInt();
    return (*w > 0 && *h > 0);
}

// Aktuelle Kameraliste, sonst die zuletzt gespeicherte ('\n'-getrennt).
static String resolutionList() {
    if (camResCount <= 0) return s_knownRes;
    String list;
    for (int i = 0; i < camResCount; i++) {
        if (i > 0) list += '\n';
        list += camResolutions[i];
    }
    return list;
}

// Merkt sich die Liste der Kamera im Flash, sobald sie sich aendert. So kann
// die Einstellung auch angeboten werden, wenn die Kamera gerade aus ist.
static void rememberResolutionList() {
    if (camResCount <= 0) return;
    const String list = resolutionList();
    if (list == s_knownRes) return;
    s_knownRes = list;
    Preferences p;
    p.begin(NVS_PIP, false);
    p.putString(KEY_RES_LIST, s_knownRes);
    p.end();
}

// Waehlt die Overlay-Aufloesung: die eingestellte, solange die Kamera sie
// anbietet - sonst die kleinste (Flaeche) der Liste. Nach einem Linsenwechsel
// kann die Liste eine andere sein.
static String effectivePipRes() {
    const String list = resolutionList();
    if (list.length() == 0) return s_pipRes;

    String smallest;
    long smallestArea = 0;
    int start = 0;
    while (start <= (int)list.length()) {
        int end = list.indexOf('\n', start);
        if (end < 0) end = list.length();
        const String entry = list.substring(start, end);
        start = end + 1;

        if (s_pipRes.length() > 0 && entry == s_pipRes) return s_pipRes;

        int w = 0, h = 0;
        if (!parseRes(entry, &w, &h)) continue;
        const long area = (long)w * h;
        if (smallestArea == 0 || area < smallestArea) { smallestArea = area; smallest = entry; }
    }
    return smallest;
}

// Wer bekommt das Videobild? Die Baby-Ansicht hat Vorrang (dort gibt es kein
// Overlay); ohne sichtbaren Abnehmer wird nicht dekodiert.
static VideoOutput outputFor(bool wantVideo) {
    if (!wantVideo || !s_displayOn) return VideoOutput::None;
    if (s_babyViewVisible)          return VideoOutput::BabyView;
    if (s_pipVisible)               return VideoOutput::Pip;
    return VideoOutput::None;
}

static const char* outputName(VideoOutput out) {
    switch (out) {
        case VideoOutput::None:     return "kein Bild";
        case VideoOutput::BabyView: return "Baby-Ansicht";
        case VideoOutput::Pip:      return "Overlay";
    }
    return "?";
}

// Setzt das Ziel des Videos und meldet jeden Wechsel einmal im Log.
static void setOutput(VideoOutput out) {
    static VideoOutput last = VideoOutput::None;
    if (out != last) {
        Serial.printf("[Baby] Videoausgabe: %s -> %s\n", outputName(last), outputName(out));
        last = out;
    }
    VideoLogic_SetOutput(out);
}

static void applyOutput() {
    setOutput(outputFor(BabyMode_HasVideo(effectiveMode()) && !s_memPaused));
}

// Setzt die Kameraaufloesung passend zum Abnehmer. Erst, wenn die Sitzung
// steht - vorher verwirft der API-Task Kommandos. Die Kamera startet ihren
// Stream daraufhin selbst neu; der Netz-Task verbindet sich dann neu.
static void reconcileResolution() {
    String target;
    if (s_babyViewVisible)      target = s_babyRes;
    else if (s_pipVisible)      target = effectivePipRes();
    else                        return;   // kein Abnehmer: nichts umstellen

    if (target.length() == 0 || target == s_appliedRes) return;
    if (!BabyCamApi_IsSessionReady()) return;

    int w = 0, h = 0;
    if (!parseRes(target, &w, &h)) return;

    Serial.printf("[Baby] Kamera-Aufloesung -> %s (fuer %s)\n", target.c_str(),
                  s_babyViewVisible ? "Baby-Ansicht" : "Overlay");
    BabyCamApi_SetResolution(w, h);
    s_appliedRes = target;
}

// Gleicht den Babyphone-Ton ab. Pegel- statt flankengesteuert: ein
// fehlgeschlagener Start wird nach AUDIO_RETRY_MS wiederholt, statt dass der
// Ton stumm bleibt, bis jemand den Modus erneut waehlt.
static void reconcileAudio(bool want) {
    // Waehrend Push-to-Talk gehoert die Audio-Hardware dem Gegensprechen. Der
    // Audio-Task setzt die Wiedergabe danach selbst fort; eine Aenderung des
    // Wunsches wird erst nach dem Loslassen weitergereicht.
    if (isPTTActive) return;

    const uint32_t now = millis();

    if (want != s_audioWantSent) {
        s_audioWantSent = want;
        s_audioCmdMs    = now;
        if (want) AudioStreamLogic_StartBaby();
        else      AudioStreamLogic_StopBaby();
        return;
    }

    if (want == (bool)isAudioStreaming) return;
    if (now - s_audioCmdMs < AUDIO_RETRY_MS) return;

    s_audioCmdMs = now;
    if (want) AudioStreamLogic_StartBaby();
    else      AudioStreamLogic_StopBaby();
}

// ------------------------------------------------------------------------------
// Sicherheitsventil: Video bei knappem DMA-Speicher pausieren
// ------------------------------------------------------------------------------
// Der WLAN-Treiber (ESP-Hosted ueber SDIO) holt sich fuer jeden Empfangsblock
// bis zu 7,5 KB zusammenhaengenden, DMA-faehigen internen Speicher. Bekommt er
// ihn nicht, bricht er ab ("assert failed: sdio_rx_get_buffer") - das ganze
// Geraet startet neu, Katzenalarm und Babyphone inklusive.
//
// Der Videostrom ist der groesste Netzverbraucher, auf den wir verzichten
// koennen. Wird der Speicher knapp, pausiert deshalb NUR das Video; Ton und
// Alarme laufen weiter. Ein beherrschter Verzicht statt eines Absturzes.
//
// Gegen Pendeln: Fortgesetzt wird erst, wenn der Speicher eine Weile deutlich
// ueber der Schwelle lag. Folgt kurz darauf die naechste Pause, verdoppelt
// sich diese Wartezeit (bis 5 min); nach 10 min Ruhe beginnt sie von vorn.


static void updateMemoryValve(bool videoWanted) {
    // Ohne Videowunsch gibt es nichts zu pausieren - und beim naechsten
    // Einschalten soll nicht eine alte Pause nachwirken.
    if (!videoWanted) { s_memPaused = false; return; }

    const uint32_t now = millis();
    if (now - s_memLastCheck < MEM_CHECK_MS) return;
    s_memLastCheck = now;

    const size_t dmaBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

    if (!s_memPaused) {
        if (dmaBlock >= MEM_PAUSE_BELOW) return;
        s_memPaused  = true;
        s_memOkSince = 0;
        const bool soonAgain = (s_memLastResume != 0) && (now - s_memLastResume < MEM_CALM_MS);
        s_memHoldMs = soonAgain ? ((s_memHoldMs * 2 > MEM_HOLD_MAX_MS) ? MEM_HOLD_MAX_MS : s_memHoldMs * 2)
                                : MEM_HOLD_MIN_MS;
        Serial.printf("[Baby] Speicher knapp (DMA-Block %u KB) - Video pausiert, Ton laeuft weiter.\n",
                      (unsigned)(dmaBlock / 1024));
        HeapMonitor_Log("Video pausiert");
        return;
    }

    if (dmaBlock < MEM_RESUME_ABOVE) { s_memOkSince = 0; return; }
    if (s_memOkSince == 0) s_memOkSince = now ? now : 1;
    if (now - s_memOkSince < s_memHoldMs) return;

    s_memPaused     = false;
    s_memLastResume = now;
    Serial.printf("[Baby] Speicher wieder ausreichend (DMA-Block %u KB) - Video laeuft weiter.\n",
                  (unsigned)(dmaBlock / 1024));
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void BabyService_Init() {
    s_mode            = BabyMode::Off;   // Entscheidung: nach Neustart immer "Aus"
    s_babyViewVisible = false;
    s_pipVisible      = false;
    s_displayOn       = true;
    s_audioWantSent   = false;
    s_audioCmdMs      = 0;
    s_appliedRes      = "";

    // Schreibbar oeffnen: ein lesendes begin() auf einen noch nie benutzten
    // Namensraum schlaegt fehl und meldet "nvs_open failed: NOT_FOUND".
    Preferences p;
    p.begin(NVS_PIP, false);
    s_pipFps   = p.getInt(KEY_PIP_FPS, PIP_FPS_DEFAULT);
    // isKey() vorab: getString() auf einen fehlenden Schluessel meldet sonst
    // bei jedem Start "nvs_get_str len fail".
    s_pipRes   = p.isKey(KEY_PIP_RES)  ? p.getString(KEY_PIP_RES, "")  : String();
    s_knownRes = p.isKey(KEY_RES_LIST) ? p.getString(KEY_RES_LIST, "") : String();
    p.end();
    if (s_pipFps < PIP_FPS_MIN || s_pipFps > PIP_FPS_MAX) s_pipFps = PIP_FPS_DEFAULT;

    p.begin(NVS_MAIN, true);
    s_babyRes = p.getString(KEY_BABY_RES, "1280x720");
    p.end();

    isStreamActive  = false;
    isBabyApiActive = false;
    VideoLogic_SetOutput(VideoOutput::None);
    VideoLogic_SetPipFps(s_pipFps);
}

void BabyService_Update() {
    const BabyMode eff = effectiveMode();
    updateMemoryValve(BabyMode_HasVideo(eff));
    const bool wantVideo = BabyMode_HasVideo(eff) && !s_memPaused;
    const bool wantAudio = BabyMode_HasAudio(eff);

    // 1. Kamerasitzung (Heartbeat und Statusabfrage in BabyCamApi). Ohne sie
    //    schliesst die Kamera Video- und Audio-Sockets. Die offene Baby-Ansicht
    //    braucht sie auch im Modus "Aus" fuer Akkustand und Einstellungen.
    isBabyApiActive = !suspended() && (eff != BabyMode::Off || s_babyViewVisible);
    rememberResolutionList();

    // 2. Videoempfang. Start/Stop sind idempotent und billig, siehe V-03 in
    //    VideoLogic: der Start holt sich so automatisch nach, sobald ein
    //    auslaufender Vorgaenger-Task weg ist.
    isStreamActive = wantVideo;
    if (wantVideo) VideoLogic_Start();
    else           VideoLogic_Stop();

    // 3. Videoausgabe und passende Kameraaufloesung
    const VideoOutput out = outputFor(wantVideo);
    setOutput(out);
    if (wantVideo) reconcileResolution();

    // 4. Babyphone-Ton
    reconcileAudio(wantAudio);
}

void BabyService_SetMode(BabyMode m) {
    if (m == s_mode) return;
    Serial.printf("[Baby] Modus: %s -> %s\n", BabyMode_Name(s_mode), BabyMode_Name(m));
    s_mode = m;
}

BabyMode BabyService_GetMode() {
    return s_mode;
}

const char* BabyService_Describe() {
    static char buf[40];
    snprintf(buf, sizeof(buf), "%s | %s",
             BabyMode_Name(effectiveMode()), outputName(VideoLogic_GetOutput()));
    return buf;
}

bool BabyService_IsVideoMemoryPaused() {
    return s_memPaused;
}

bool BabyService_IsVideoActive() {
    return BabyMode_HasVideo(effectiveMode());
}

void BabyService_SetVideo(bool on) {
    BabyService_SetMode(BabyMode_From(on, BabyMode_HasAudio(s_mode)));
}

void BabyService_SetAudio(bool on) {
    BabyService_SetMode(BabyMode_From(BabyMode_HasVideo(s_mode), on));
}

void BabyService_SetBabyViewVisible(bool visible) {
    s_babyViewVisible = visible;
    applyOutput();
}

void BabyService_SetPipVisible(bool visible) {
    s_pipVisible = visible;
    applyOutput();
}

void BabyService_SetDisplayOn(bool on) {
    s_displayOn = on;
    applyOutput();
}

void BabyService_SetBabyResolution(const String& res) {
    s_babyRes = res;
    currentCamRes = res;   // Anzeige sofort; die Statusabfrage bestaetigt spaeter
    Preferences p;
    p.begin(NVS_MAIN, false);
    p.putString(KEY_BABY_RES, res);
    p.end();
}

void BabyService_SetPipResolution(const String& res) {
    s_pipRes = res;
    Preferences p;
    p.begin(NVS_PIP, false);
    p.putString(KEY_PIP_RES, res);
    p.end();
}

String BabyService_GetPipResolutionSetting() {
    return s_pipRes;
}

String BabyService_GetPipResolution() {
    return effectivePipRes();
}

String BabyService_GetResolutionOptions() {
    return resolutionList();
}

void BabyService_SetPipFps(int fps) {
    if (fps < PIP_FPS_MIN) fps = PIP_FPS_MIN;
    if (fps > PIP_FPS_MAX) fps = PIP_FPS_MAX;
    s_pipFps = fps;
    VideoLogic_SetPipFps(fps);
    Preferences p;
    p.begin(NVS_PIP, false);
    p.putInt(KEY_PIP_FPS, fps);
    p.end();
}

int BabyService_GetPipFps() {
    return s_pipFps;
}
