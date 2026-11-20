#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <M5Unified.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <WebServer.h>   
#include <DNSServer.h>   

#include "DebugFlags.h"   // Schalter fuer Diagnoselaeufe

#define HISTORY_SIZE 900 
#define WINDOW_SIZE 12
#define INTERVAL_HISTORY_SIZE 5
#define MAX_SCAN_DEVICES 40 
#define MAX_BT_MSGS 5

enum GraphMode { GRAPH_MODE_PRESSURE = 0, GRAPH_MODE_BLE_RSSI, GRAPH_MODE_WLAN_RSSI, GRAPH_MODE_BLE_INTERVAL, GRAPH_MODE_KIPPY_RSSI };

// Ton-Auftrag fuer audioTask(). volumeChannel: 0=UI, 1=Alarm, 2=Baby (Mixer-Kanal)
struct AudioMsg { uint16_t freq; uint32_t duration; uint8_t soundType; uint8_t volumeChannel; };

extern SemaphoreHandle_t bleMutex;

extern bool dongleAlarmEnabled; 

extern bool babyAlarmActive;

// Alarm-Quittierung - getrennt von der Stummschaltung im Mixer.
//
// Frueher stand hier muteAlarm/muteBaby in Doppelfunktion: Mixer-Stumm UND
// "Alarm quittiert". Folgen: Wer den Baby-Alarm quittierte, hoerte das Baby
// nicht mehr; wer den Katzenalarm quittierte, brachte auch einen gleichzeitigen
// Baby-Alarm zum Schweigen; und jeder neue Alarm hob eine bewusst gesetzte
// Mixer-Stummschaltung wieder auf.
//
// Jetzt gilt:
//   catAlarmAcked  - Katzen- und Verbindungsalarm quittiert (Ton und Popup aus,
//                    Alarm bleibt bis zum Zuruecksetzen bestehen)
//   babyAlarmAcked - Baby-Alarm quittiert
//   muteAlarm/muteBaby - ausschliesslich Lautstaerke im Mixer
// Die Quittierung wird nicht gespeichert; ein Neustart beginnt unquittiert.
extern bool catAlarmAcked;
extern bool babyAlarmAcked;

extern String wifiSsid;
extern String wifiPass;
// (Feste IP statt DHCP: je Netz in WifiNetworks, siehe WifiIpConfig.)
extern String mqttBroker;
extern int mqttPort;
extern String mqttUser;
extern String mqttPass;
extern String haIP;
extern int haPort;

extern String streamIp; 
extern bool useCustomUrls; 
extern String camEntity; 
extern String babyStreamUrl;   // URL der HTTP-Audioquelle (siehe audioSource)

extern String topbarStatusMsg;

extern String mqttBabyTopic;
extern String mqttCameraTriggerTopic;

extern bool useMqttForMat;
extern int matRssi;
extern int proxyWifiRssi;
extern uint32_t proxyLastUpdate;
extern String proxyStatusMsg;

extern int webSetupMode; 
extern volatile int pendingWebSetupMode; 
extern uint32_t webSetupStartTime;
extern String apPassword; 

extern volatile int pendingScreenshotMode; 
extern volatile bool screenshotModeActive; 
extern volatile bool isBabyArmed;

extern WebServer server;
extern DNSServer dnsServer;
extern WiFiClient espClient;
extern PubSubClient mqttClient;

extern String savedMatMac;
extern String savedKippyMac;

extern bool isSetupScanning;
extern int setupScanMode; 
extern uint32_t setupScanStartTime; 
extern bool scanJustFinished;
extern char scanOptionsStr[2048];

extern String scanResultMacs[MAX_SCAN_DEVICES];
extern String scanResultNames[MAX_SCAN_DEVICES];
extern int scanResultRssi[MAX_SCAN_DEVICES];
extern int scanResultCount;

extern volatile bool requestRollerUpdate; 
extern Preferences preferences;

extern uint32_t lastCatSeenTime;
extern int catRssi;

extern bool wifiEnabled;
extern bool matEnabled;
extern bool kippyEnabled;

extern bool isTrackerMode; 
extern bool isCatAtHome;        
extern bool isTrackerDataValid; 
extern int radarSetupPhase; 
extern bool pendingRadarTeardown;
extern bool pendingWifiDisconnect; 
extern bool isBooting; 

extern volatile bool requestWake;
extern volatile bool requestSleep;
extern volatile bool requestChartUpdate; 
extern volatile bool requestMainTab; 
extern bool force_auto_fit; 

extern bool connected;
extern uint32_t lastConnectTime; 
extern bool isArmed;                 
extern bool wasArmedBeforeDisconnect;
extern bool alarmActive;
extern bool wifiStarted;
extern bool timeSynced; 
extern uint32_t cooldownUntil;
extern uint32_t startTime;
extern uint32_t lastNotifyTime; 
extern int batteryPercent;
extern bool isBatteryCharging; 
extern int autoHealedDisconnectCount; 

extern int32_t rawPressure;
extern int32_t taraOffset;
extern int32_t currentPressure;
extern int32_t currentAvg; 
extern int32_t intervalMaxPressure; 
extern int32_t pressureHistory[HISTORY_SIZE];
extern int historyIdx;
extern int historyCount;
extern int32_t pressWindow[WINDOW_SIZE];
extern int pWinIdx;
extern int pWinCount;

// --- NEU: Getrenntes Lautstaerke-System ---
extern int volMaster;
extern int volUI;
extern int volAlarm;
extern int volBaby;

extern bool muteMaster;
extern bool muteUI;
extern bool muteAlarm;
extern bool muteBaby;

extern int thresholdVal; 
extern bool isDarkMode; 
extern int graphTimeSeconds; 
extern bool showTimeOnX; 
extern int currentGraphMode;
extern int storedGraphMode; 
extern bool autoBleInterval;
extern int manualBleIntervalMs;
extern bool pendingBleReconnect; 
extern bool intentionalDisconnect; 
extern bool disconnectAlarmActive; 
extern bool rssiIsZero; 
extern volatile bool isBleTabActive; 
extern uint32_t intervalHistory[INTERVAL_HISTORY_SIZE];
extern int intervalIdx;
extern int intervalCount;
extern float avgInterval;
extern float stdDevInterval;
extern bool displayIsOff;
extern int brightnessPercent;
extern bool mqttEnabled;
extern volatile int babyStreamStatus; 
extern volatile bool vidFSMode; 

// Latenzmodus: Der Decoder ueberspringt aufgestaute Bilder und nimmt immer das
// neueste. Kostet Bilder, haelt aber die Verzoegerung zwischen Realitaet und
// Anzeige klein - bei einem Babyphone meist die richtige Wahl.
extern volatile bool videoLowLatency;
extern volatile bool showFps;
extern volatile int currentFps;

extern volatile bool isStreamActive;
// VERALTET: wird nicht mehr benutzt. Die JPEG-Puffer verwaltet VideoDecoder
// ueber jpeg_alloc_decoder_mem() im PSRAM. Symbol bleibt aus
// Kompatibilitaetsgruenden erhalten.
extern uint8_t* download_buf;

// --- Babyphone-Ton: Quelle ------------------------------------------------
// BabyCam (UDP): rohes PCM 44,1 kHz/16 Bit/mono, die Kamera sendet nach einem
//   API-Befehl an unseren Empfangsport. Nur hier gibt es Gegensprechen (PTT).
// HTTP-Stream: beliebige Quelle unter `babyStreamUrl`, Format einstellbar.
// Wirksam beim naechsten Start des Tons (AudioStreamLogic_ApplyConfig()).
enum AudioSourceType : int { AUDIO_SRC_BABYCAM_UDP = 0, AUDIO_SRC_HTTP = 1 };
// Werte wie im frueheren Dropdown "Audio-Format" (NVS "audioFmt").
enum AudioHttpFormat : int { AUDIO_FMT_PCM16 = 0, AUDIO_FMT_AAC = 1 };

#define AUDIO_UDP_PORT_IN_DEFAULT   50001   // api_RE.md: audioBabyRecorderPort
#define AUDIO_UDP_PORT_OUT_DEFAULT  50003   // api_RE.md: audioParentPlayerPort
#define AUDIO_PCM_RATE_DEFAULT      44100

extern int audioSource;        // AudioSourceType            (NVS "audSrc")
extern int audioFormat;        // AudioHttpFormat            (NVS "audioFmt")
extern int audioPcmRate;       // Hz, nur HTTP-PCM ohne WAV-Kopf (NVS "audRate")
extern int audioPcmChannels;   // 1/2, nur HTTP-PCM ohne WAV-Kopf (NVS "audCh")
extern int audioUdpPortIn;     // BabyCam -> Tablet          (NVS "audPIn")
extern int audioUdpPortOut;    // Tablet -> BabyCam (PTT)    (NVS "audPOut")
// Sprachaufbereitung des Gegensprechens (VoiceProcessor.h: Bedeutung, Grenzen
// und Vorgaben). Einstellbar unter Babyphone > Gegensprechen; gilt ab dem
// naechsten Druck auf die Sprechtaste.
extern int  pttHighpassHz;     // Hochpass, 0 = aus                  (NVS "pttHp")
extern int  pttLowpassHz;      // Tiefpass, 0 = aus                  (NVS "pttLp")
extern bool pttAutoGain;       // Pegelautomatik                     (NVS "pttAgc")
extern int  pttGainDb;         // feste Verstaerkung / Obergrenze dB (NVS "pttGain")
// Von der BabyCam im Status gemeldete Ports, 0 = unbekannt. Geschrieben vom
// API-Task; gemeldete Werte haben Vorrang vor der Einstellung - die Kamera
// sendet dorthin, wohin SIE will.
extern volatile int camAudioPortIn;
extern volatile int camAudioPortOut;
extern volatile bool audioStandaloneMode;

extern uint8_t lastBtPackets[MAX_BT_MSGS][10];
extern int btPacketIdx;
extern int btPacketCount;

// --- NEUE BABYCAM API VARIABLEN ---
extern int camBatteryPercent;
extern String camResolutions[10];
extern int camResCount;
extern String currentCamRes;
extern bool camFlash;
extern bool camScrFlash;
extern int camZoom;
extern int camQuality;
extern int camSpeed;
extern String camStatus; 
extern bool camScreenOn;
extern const String camUuid;
extern volatile bool pttSwipeLock;

void Data_Init();
void Audio_Init();
void playToneI2S(uint16_t freq, uint32_t duration_ms, bool isUiSound = false);

// NEU: Sound fuer Slider Release
void playBingI2S(uint8_t volChannel); 

void playBabyAlarmI2S();
void playCatAlarmI2S();

void fullReset();
void wakeDisplay();
void sleepDisplay();
void factoryReset();

namespace SharedData {
    // Speichert die Mixer-Einstellungen (Lautstaerken und Stummschaltungen).
    void Save();
}