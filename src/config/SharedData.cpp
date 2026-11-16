#include "SharedData.h"
#include "secrets.h"
#include "SystemLogic.h" 
#include "GuiManager.h"  
#include "AudioStreamLogic.h" 
#include <WiFi.h>
#include <math.h>
#include "WifiNetworks.h"
#include "DebugFlags.h"
#include "ColdRestart.h"

SemaphoreHandle_t bleMutex = NULL;
volatile bool isStreamActive = false; 
volatile bool videoLowLatency = true;   // Standard: niedrige Latenz
uint8_t* download_buf = nullptr;

Preferences preferences;
QueueHandle_t audioQueue = NULL; 

WebServer server(80);
DNSServer dnsServer;
int webSetupMode = 0; 
volatile int pendingWebSetupMode = 0; 
volatile int pendingScreenshotMode = 0;
volatile bool screenshotModeActive = false;
uint32_t webSetupStartTime = 0;
String apPassword = ""; 

String wifiSsid = "";
String wifiPass = "";
String mqttBroker = ""; 
int mqttPort = SECRET_MQTT_PORT;
String mqttUser = "";
String mqttPass = "";

String haIP = SECRET_HA_IP;
int haPort = SECRET_HA_PORT;

String streamIp = SECRET_STREAM_IP;
bool useCustomUrls = false;
String camEntity = SECRET_CAM_ENTITY;
String babyStreamUrl = SECRET_BABY_STREAM_URL;

String topbarStatusMsg = ""; 

String mqttBabyTopic = SECRET_MQTT_BABY_TOPIC;
String mqttCameraTriggerTopic = SECRET_MQTT_CAM_TRIGGER;
volatile bool isBabyArmed = false;

volatile int babyStreamStatus = 0; 
volatile bool vidFSMode = false;
volatile bool showFps = false; 
volatile int currentFps = 0;

int camBatteryPercent = 0;
String camResolutions[10];
int camResCount = 0;
String currentCamRes = "";   // Ist-Wert laut Kamera; die Einstellung haelt der BabyService
bool camFlash = false;
bool camScrFlash = false;
int camZoom = 0;
int camQuality = 50;
int camSpeed = 0;
String camStatus = "PLAY";
bool camScreenOn = true;
const String camUuid = SECRET_CAM_UUID;
volatile bool pttSwipeLock = false;

bool mqttEnabled = true;

bool useMqttForMat = true;
int matRssi = -100;
int proxyWifiRssi = -100;
uint32_t proxyLastUpdate = 0;
String proxyStatusMsg = "Warte auf Proxy...";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String savedMatMac = SECRET_MAC_MAT;
String savedKippyMac = SECRET_MAC_KIPPY;

bool isSetupScanning = false;
int setupScanMode = 0; 
uint32_t setupScanStartTime = 0; 
bool scanJustFinished = false;
char scanOptionsStr[2048] = "Suche laeuft...";

String scanResultMacs[MAX_SCAN_DEVICES];
String scanResultNames[MAX_SCAN_DEVICES];
int scanResultRssi[MAX_SCAN_DEVICES];
int scanResultCount = 0;
volatile bool requestRollerUpdate = false; 

uint32_t lastCatSeenTime = 0;
int catRssi = -100;
bool wifiEnabled = true, matEnabled = true, kippyEnabled = true;
bool isTrackerMode = false, isCatAtHome = false, isTrackerDataValid = false; 
int radarSetupPhase = 0; 
bool pendingRadarTeardown = false, pendingWifiDisconnect = false, isBooting = true; 

volatile bool requestWake = false;
volatile bool requestSleep = false;
volatile bool requestChartUpdate = false; 
volatile bool requestMainTab = false; 
bool force_auto_fit = false;

bool connected = false;
uint32_t lastConnectTime = 0;
bool isArmed = false;                  
bool wasArmedBeforeDisconnect = false; 
bool alarmActive = false;
bool wifiStarted = false;
bool timeSynced = false;
uint32_t cooldownUntil = 0;
uint32_t startTime = 0;
uint32_t lastNotifyTime = 0;
int batteryPercent = 100;
bool isBatteryCharging = false;
int autoHealedDisconnectCount = 0; 

bool babyAlarmActive = false;
bool catAlarmAcked = false;
bool babyAlarmAcked = false;

int32_t rawPressure = 0;
int32_t taraOffset = 0;
int32_t currentPressure = 0;
int32_t currentAvg = 0;
int32_t intervalMaxPressure = -32000;
int32_t pressureHistory[HISTORY_SIZE];
int historyIdx = 0;
int historyCount = 0;
int32_t pressWindow[WINDOW_SIZE];
int pWinIdx = 0;
int pWinCount = 0;

int volMaster = 80;
int volUI = 100;
int volAlarm = 100;
int volBaby = 100;

bool muteMaster = false;
bool muteUI = false;
bool muteAlarm = false;
bool muteBaby = false;

int thresholdVal = 150;
bool isDarkMode = true;
int graphTimeSeconds = 150;
bool showTimeOnX = true;
int currentGraphMode = GRAPH_MODE_PRESSURE;
int storedGraphMode = GRAPH_MODE_PRESSURE;
bool autoBleInterval = true;
int manualBleIntervalMs = 1000;
bool pendingBleReconnect = false;
bool intentionalDisconnect = false;
bool disconnectAlarmActive = false;
bool rssiIsZero = false;
volatile bool isBleTabActive = false;
uint32_t intervalHistory[INTERVAL_HISTORY_SIZE];
int intervalIdx = 0;
int intervalCount = 0;
float avgInterval = 0.0;
float stdDevInterval = 0.0;
bool displayIsOff = false;
int brightnessPercent = 80;

int audioSource      = AUDIO_SRC_BABYCAM_UDP;
int audioFormat      = AUDIO_FMT_PCM16;
int audioPcmRate     = AUDIO_PCM_RATE_DEFAULT;
int audioPcmChannels = 1;
int audioUdpPortIn   = AUDIO_UDP_PORT_IN_DEFAULT;
int audioUdpPortOut  = AUDIO_UDP_PORT_OUT_DEFAULT;
volatile int camAudioPortIn  = 0;
volatile int camAudioPortOut = 0;
volatile bool audioStandaloneMode = false;

uint8_t lastBtPackets[MAX_BT_MSGS][10];
int btPacketIdx = 0;
int btPacketCount = 0;

bool dongleAlarmEnabled = false;

// ------------------------------------------------------------------------------
// Einstellungen mit Rueckfall auf secrets.h
// ------------------------------------------------------------------------------
// Regel: Was im Geraet eingestellt wurde (Web-Setup, Einstellungen), gilt.
// secrets.h liefert nur den Rueckfall, solange nichts eingestellt ist - fehlt
// der Schluessel oder ist er leer, greift der Wert aus secrets.h. Ein geleertes
// Feld im Web-Setup fuehrt damit wieder auf secrets.h zurueck.
static String prefOrSecret(const char* key, const char* fallback) {
    String v = preferences.isKey(key) ? preferences.getString(key, "") : String();
    v.trim();
    return (v.length() > 0) ? v : String(fallback);
}

static int prefOrSecretInt(const char* key, int fallback) {
    const int v = preferences.isKey(key) ? preferences.getInt(key, 0) : 0;
    return (v > 0) ? v : fallback;
}

// Schluessel entfallener Einstellungen. Werden einmalig entfernt, damit sie
// keinen Platz im NVS belegen und nicht wieder auftauchen, falls ein Name
// spaeter neu vergeben wird.
static void removeObsoleteKeys() {
    static const char* const OBSOLETE[] = {
        "camRef",    // Update-Intervall (Snapshot-Abruf, durch MJPEG ersetzt)
        "mjDrop",    // Latenz-Drop (nie ausgewertet)
        "audDbg",    // Audio-Debug-Overlay (Anzeige existierte nicht mehr)
        "prioM",     // Funk-Multiplex Matte (Altlast Einzel-Funkchip)
        "prioS",     // Funk-Multiplex Kippy/WLAN
        "camHackM",  // Aufloesungs-Hack (durch BabyCamApi ersetzt)
    };
    for (const char* key : OBSOLETE) {
        if (preferences.isKey(key)) preferences.remove(key);
    }
}

// Babyphone-Ton (siehe SharedData.h). Unplausible Werte fallen auf den
// Standard zurueck - ein falscher Port oder eine krumme Rate wuerden sonst
// still zu "kein Ton" fuehren.
static bool validPort(int p) { return p > 0 && p <= 65535; }

static void loadAudioSourceSettings() {
    audioSource      = preferences.getInt("audSrc", AUDIO_SRC_BABYCAM_UDP);
    audioFormat      = preferences.getInt("audioFmt", AUDIO_FMT_PCM16);
    audioPcmRate     = preferences.getInt("audRate", AUDIO_PCM_RATE_DEFAULT);
    audioPcmChannels = preferences.getInt("audCh", 1);
    audioUdpPortIn   = preferences.getInt("audPIn", AUDIO_UDP_PORT_IN_DEFAULT);
    audioUdpPortOut  = preferences.getInt("audPOut", AUDIO_UDP_PORT_OUT_DEFAULT);

    if (audioSource != AUDIO_SRC_HTTP)                  audioSource = AUDIO_SRC_BABYCAM_UDP;
    if (audioFormat != AUDIO_FMT_AAC)                   audioFormat = AUDIO_FMT_PCM16;
    if (audioPcmRate < 8000 || audioPcmRate > 48000)    audioPcmRate = AUDIO_PCM_RATE_DEFAULT;
    if (audioPcmChannels != 2)                          audioPcmChannels = 1;
    if (!validPort(audioUdpPortIn))                     audioUdpPortIn  = AUDIO_UDP_PORT_IN_DEFAULT;
    if (!validPort(audioUdpPortOut))                    audioUdpPortOut = AUDIO_UDP_PORT_OUT_DEFAULT;
}

void Data_Init() {
    for(int i=0; i<HISTORY_SIZE; i++) pressureHistory[i] = -32000;
    preferences.begin("catmat", false);
    wifiSsid = preferences.getString("wifiSsid", "");
    wifiPass = preferences.getString("wifiPass", "");

    // Die feste IP gehoert seit 16.11.2026 zum einzelnen Netz (WifiNetworks);
    // eine fruehere Einstellung von hier uebernimmt WifiNetworks beim ersten Zugriff.
    
    removeObsoleteKeys();

    // Verbindungsdaten: Geraete-Einstellung vor secrets.h (siehe prefOrSecret).
    // Der Broker liegt ohne eigene Angabe auf dem HA-Rechner.
    mqttBroker    = prefOrSecret("mqIP", SECRET_HA_IP);
    mqttPort      = prefOrSecretInt("mqPort", SECRET_MQTT_PORT);
    mqttUser      = prefOrSecret("mqUser", SECRET_MQTT_USER);
    mqttPass      = prefOrSecret("mqPass", SECRET_MQTT_PASS);
    haIP          = prefOrSecret("haIP", SECRET_HA_IP);
    haPort        = prefOrSecretInt("haPort", SECRET_HA_PORT);
    savedMatMac   = prefOrSecret("macM", SECRET_MAC_MAT);
    savedKippyMac = prefOrSecret("macK", SECRET_MAC_KIPPY);
    streamIp      = prefOrSecret("strIp", SECRET_STREAM_IP);

    mqttEnabled = preferences.getBool("mqttEn", true); 
    showFps = preferences.getBool("showFps", false); 

    // Kamera. `camEntity` ist ausschliesslich die Video-URL des Expertenmodus;
    // die BabyCam-URL baut VideoLogic bei jedem Verbindungsaufbau selbst aus
    // streamIp und aktueller Qualitaet. Rueckfall wie bisher wirksam: der
    // Snapshot-/Stream-Pfad aus secrets.h (SECRET_CAM_ENTITY ist ein alter
    // HA-Entitaetsname und als URL unbrauchbar).
    useCustomUrls = preferences.getBool("useCstUrl", false);
    camEntity     = prefOrSecret("camEntity", SECRET_CAM_SNAPSHOT_PATH);
    babyStreamUrl = prefOrSecret("babyUrl", SECRET_BABY_STREAM_URL);

    // Kameraeinstellungen. Sie wurden bisher zwar GESCHRIEBEN (in
    // ViewBabySettings), aber nie zurueckgelesen - nach jedem Neustart standen
    // die Regler wieder auf den Anfangswerten, waehrend die Kamera noch den
    // zuletzt gesendeten Wert hielt. Anzeige und Wirklichkeit liefen damit
    // auseinander, und beim Speed-Regler (0 = maximale fps, 100 = Eco) fiel
    // das erst an der Bildrate auf.
    //
    // Vorgaben bewusst so: Tempo auf Anschlag, Qualitaet in der Mitte. Beides
    // wird jetzt bei jedem Streamstart an die Kamera durchgereicht.
    camQuality = preferences.getInt("camQuality", 50);
    camSpeed   = preferences.getInt("camSpeed", 0);

    loadAudioSourceSettings();

    volMaster = preferences.getInt("volM", 80);
    volUI = preferences.getInt("volU", 100);
    volAlarm = preferences.getInt("volA", 100);
    volBaby = preferences.getInt("volB", 100);
    muteMaster = preferences.getBool("mutM", false);
    muteUI = preferences.getBool("mutU", false);
    muteAlarm = preferences.getBool("mutA", false);
    muteBaby = preferences.getBool("mutB", false);

    // Einmalige Bereinigung nach der Trennung von Quittierung und Mixer (siehe
    // SharedData.h): Unter der alten Doppelfunktion konnte eine Quittierung
    // als Mixer-Stumm gespeichert worden sein - dann blieben Alarmton oder
    // Babyphone dauerhaft still, ohne erkennbaren Grund. Im Zweifel hoerbar.
    if (!preferences.getBool("muteSplit", false)) {
        if (muteAlarm || muteBaby) {
            Serial.println("[Audio] Mixer: Stummschaltung Alarm/Baby einmalig zurueckgesetzt (Umstellung Quittierung).");
        }
        muteAlarm = false;
        muteBaby  = false;
        preferences.putBool("mutA", false);
        preferences.putBool("mutB", false);
        preferences.putBool("muteSplit", true);
    }
    
    thresholdVal = preferences.getInt("thr", 150);
    taraOffset = preferences.getUInt("off", 0);
    // isDarkMode setzt UiTheme::init() (Modus Dunkel/Hell/Automatisch).
    graphTimeSeconds = preferences.getInt("gtime", 150);
    showTimeOnX = preferences.getBool("timeX", true);
    currentGraphMode = preferences.getInt("gMode", GRAPH_MODE_PRESSURE);
    autoBleInterval = preferences.getBool("bleAuto", true);
    manualBleIntervalMs = preferences.getInt("bleManMs", 1000);
    brightnessPercent = preferences.getInt("bright", 80);
    wifiEnabled = preferences.getBool("wifiEn", true);
    matEnabled = preferences.getBool("matEn", true);
    kippyEnabled = preferences.getBool("kipEn", true);
    useMqttForMat = preferences.getBool("useMqtt", true);
    dongleAlarmEnabled = preferences.getBool("dongleAl", false);
    preferences.end();
}


void audioTask(void *pvParameters) {
    AudioMsg msg;
    while(1) {
        if (xQueueReceive(audioQueue, &msg, portMAX_DELAY) == pdTRUE) {
            
            float effMaster = muteMaster ? 0.0f : (volMaster / 100.0f);
            float effUI = muteUI ? 0.0f : (volUI / 100.0f);
            float effAlarm = muteAlarm ? 0.0f : (volAlarm / 100.0f);
            float effBaby = muteBaby ? 0.0f : (volBaby / 100.0f); 

            float volFactor = 0.0f;
            if (msg.volumeChannel == 0) volFactor = effMaster * effUI;
            else if (msg.volumeChannel == 1) volFactor = effMaster * effAlarm;
            else if (msg.volumeChannel == 2) volFactor = effMaster * effBaby;

            if (volFactor <= 0.01f) {
                // Kanal stumm (Mixer) - einmal je 10 s melden, sonst stumm verworfen.
                static uint32_t s_lastMuteLog = 0;
                if (millis() - s_lastMuteLog > 10000) {
                    s_lastMuteLog = millis();
                    Serial.printf("[Ton] Verworfen: Kanal %d stumm (Master %d%%%s, UI %d%%%s, Alarm %d%%%s).\n",
                                  msg.volumeChannel, volMaster, muteMaster ? " stumm" : "",
                                  volUI, muteUI ? " stumm" : "", volAlarm, muteAlarm ? " stumm" : "");
                }
                continue;
            }

            // Der Lautsprecher gehoert dem Audio-Besitzer-Task
            // (AudioStreamLogic). Waehrend PTT ist er abgeschaltet - das
            // Mikrofon belegt denselben I2S-Port - und waehrend eines
            // Umschaltvorgangs ist er nicht ansprechbar.
            //
            // Hier wird gewartet statt verworfen: ein Alarmton wird genau
            // einmal abgeschickt. Faellt er in ein Umschaltfenster (bis zu
            // ~600 ms), waere er sonst stillschweigend weg - bei einer
            // Katzenmatte und einem Babyphone die falsche Sparsamkeit.
            // Die Grenze verhindert, dass ein dauerhaft defekter Lautsprecher
            // die Warteschlange aufstaut.
            // Siehe src/doc/video_refactoring.md, 2.18.
            bool haveHw = false;
            for (int tries = 0; tries < 60; tries++) {          // max. ~1,5 s
                if (AudioHw_SpeakerUsable() && AudioHw_Lock(25)) { haveHw = true; break; }
                vTaskDelay(pdMS_TO_TICKS(25));
            }
            if (!haveHw) {
                static uint32_t s_lastHwLog = 0;
                if (millis() - s_lastHwLog > 10000) {
                    s_lastHwLog = millis();
                    Serial.println("[Ton] Verworfen: Lautsprecher 1,5 s nicht verfuegbar (abgeschaltet/belegt).");
                }
                continue;
            }

            // Nach dem Schloss erneut pruefen: zwischen Pruefung und Schloss
            // kann der Besitzer-Task den Lautsprecher abgeschaltet haben.
            if (!AudioHw_SpeakerUsable()) { AudioHw_Unlock(); continue; }

            // Den Babystream fuer die Dauer des Tons absenken. Bewusst NICHT
            // ueber setChannelVolume(0, ...): die Wiedergabe setzt die
            // Kanallautstaerke alle ~5 ms selbst und wuerde das sofort
            // ueberschreiben.
            if (isAudioStreaming) AudioHw_DuckStream(msg.duration + 50);

            int toneVol = (int)(volFactor * 255.0f);
            if (toneVol > 255) toneVol = 255;
#if DIAG_TEST_TOOLS
            // Fehlersuche Tastentoene: jeder gespielte Ton mit Art, Kanal und
            // wirksamer Lautstaerke (0-255). Hoechstens 5 Zeilen je Sekunde.
            {
                static uint32_t s_logWindow = 0;
                static uint8_t  s_logCount  = 0;
                if (millis() - s_logWindow > 1000) { s_logWindow = millis(); s_logCount = 0; }
                if (s_logCount++ < 5) {
                    Serial.printf("[Ton] Art %d, Kanal %d, Lautstaerke %d/255\n",
                                  msg.soundType, msg.volumeChannel, toneVol);
                }
            }
#endif
            M5.Speaker.setChannelVolume(1, toneVol);

            if (msg.soundType == 0) M5.Speaker.tone(msg.freq, msg.duration, 1, true); 
            else if (msg.soundType == 1) M5.Speaker.tone(600, 200, 1, true); 
            else if (msg.soundType == 2) M5.Speaker.tone(1100, 100, 1, true); 
            else if (msg.soundType == 3) M5.Speaker.tone(800, 100, 1, true); 
            else if (msg.soundType == 4) M5.Speaker.tone(150, 15, 1, true); 

            // Das Schloss NICHT ueber die Wartezeit halten: der Ton laeuft
            // asynchron weiter, der Besitzer-Task braucht die Hardware
            // zwischendurch fuer den Stream.
            AudioHw_Unlock();

            vTaskDelay(pdMS_TO_TICKS(msg.duration + 50));
        }
    }
}

void Audio_Init() {
    if (audioQueue == NULL) {
        audioQueue = xQueueCreate(15, sizeof(AudioMsg));
        xTaskCreatePinnedToCore(audioTask, "AudioTask_UI", 8192, NULL, 1, NULL, 1); 
    }
}

void playToneI2S(uint16_t freq, uint32_t duration_ms, bool isUiSound) {
    static uint32_t last_tone_time = 0;
    if (isUiSound) { if (millis() - last_tone_time < 150) return; last_tone_time = millis(); }
    if(audioQueue != NULL) { 
        AudioMsg msg; 
        msg.freq = freq; 
        msg.duration = isUiSound ? 15 : duration_ms; 
        msg.soundType = isUiSound ? 4 : 0; 
        msg.volumeChannel = isUiSound ? 0 : 1; 
        xQueueSend(audioQueue, &msg, 0); 
    }
}

void playBingI2S(uint8_t volChannel) {
    if(audioQueue != NULL) { 
        AudioMsg msg; msg.freq = 800; msg.duration = 100; 
        msg.soundType = 3; msg.volumeChannel = volChannel; 
        xQueueSend(audioQueue, &msg, 0); 
    }
}

// Die Lautstaerke (inkl. Mixer-Stumm) wendet audioTask() an - hier wird nur
// eingereiht. Ob der Alarm ueberhaupt toent (Quittierung), entscheidet der
// Aufrufer in SystemLogic.
void playBabyAlarmI2S() { if(audioQueue != NULL) { AudioMsg msg; msg.soundType = 1; msg.volumeChannel = 1; msg.duration=200; xQueueSend(audioQueue, &msg, 0); } }
void playCatAlarmI2S() { if(audioQueue != NULL) { AudioMsg msg; msg.soundType = 2; msg.volumeChannel = 1; msg.duration=100; xQueueSend(audioQueue, &msg, 0); } }

void fullReset() { alarmActive = false; disconnectAlarmActive = false; catAlarmAcked = false; isArmed = false; cooldownUntil = millis() + 5000; topbarStatusMsg = ""; }
void wakeDisplay() { requestWake = true; M5.Display.wakeup(); }
void sleepDisplay() { requestSleep = true; M5.Display.sleep(); }
void factoryReset() {
    preferences.begin("catmat", false);
    preferences.clear();
    preferences.end();
    WifiNetworks_Clear();   // gemerkte WLAN-Netze samt Passwoertern
    delay(1000);
    ColdRestart_WarmRestart("Werkseinstellungen");
}

namespace SharedData {
    void Save() {
        preferences.begin("catmat", false);
        preferences.putInt("volM", volMaster);
        preferences.putInt("volU", volUI);
        preferences.putInt("volA", volAlarm);
        preferences.putInt("volB", volBaby);
        preferences.putBool("mutM", muteMaster);
        preferences.putBool("mutU", muteUI);
        preferences.putBool("mutA", muteAlarm);
        preferences.putBool("mutB", muteBaby);
        // Bewusst NUR der Mixer: Hier standen frueher auch WLAN-, Kamera- und
        // Stream-Werte. Jede Lautstaerkeaenderung schrieb sie mit - darunter
        // die gerade von der Kamera gemeldete Aufloesung in die Einstellung
        // "camRes" und die Rueckfallwerte aus secrets.h ins NVS. Diese Werte
        // speichert jeweils die Stelle, an der sie eingestellt werden.
        preferences.end();
    }
}