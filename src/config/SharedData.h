#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <M5Unified.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <WebServer.h>   
#include <DNSServer.h>   

#define HISTORY_SIZE 900 
#define WINDOW_SIZE 12
#define INTERVAL_HISTORY_SIZE 5
#define MAX_SCAN_DEVICES 40 
#define MAX_BT_MSGS 5

enum GraphMode { GRAPH_MODE_PRESSURE = 0, GRAPH_MODE_BLE_RSSI, GRAPH_MODE_WLAN_RSSI, GRAPH_MODE_BLE_INTERVAL, GRAPH_MODE_KIPPY_RSSI };

struct AudioMsg { uint16_t freq; uint32_t duration; bool isUiSound; uint8_t soundType; };

extern SemaphoreHandle_t bleMutex;

extern bool audioDebugEnabled;
extern String audioLogs[10];
extern int audioLogIdx;
extern void addAudioLog(String msg);

extern bool babyAlarmActive;
extern bool babyMuted;

extern String wifiSsid;
extern String wifiPass;
extern String mqttBroker;
extern int mqttPort;
extern String mqttUser;
extern String mqttPass;
extern String haIP;
extern int haPort;

// --- NEU: Globale Stream Variablen ---
extern String streamIp; 
extern bool useCustomUrls; 
extern String camEntity; // Wird als Video-URL verwendet
extern String babyStreamUrl; // Wird als Audio-URL verwendet

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
extern int prioMaster; 
extern int prioSlave;  
extern float effPrioMat;
extern float effPrioKippy;
extern float effPrioWifi;

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
extern bool muted;
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

extern int volumePercent;
extern int streamVolumePercent;
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
extern volatile bool requestBabyStream;
extern volatile int babyStreamStatus; 
extern volatile bool vidFSMode; 
extern volatile bool showFps;
extern volatile int currentFps;

extern int mjpegDropThreshold; 
extern int camHackMode; 
extern volatile bool isStreamActive;

extern int audioFormat; 
extern volatile bool audioStandaloneMode;

extern uint8_t lastBtPackets[MAX_BT_MSGS][10];
extern int btPacketIdx;
extern int btPacketCount;

void Data_Init();
void Audio_Init();
void calcMultiplex();
void playToneI2S(uint16_t freq, uint32_t duration_ms, bool isUiSound = false);

void playBabyAlarmI2S();
void playCatAlarmI2S();

void fullReset();
void wakeDisplay();
void sleepDisplay();
void factoryReset();