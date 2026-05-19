#include "SharedData.h"
#include "secrets.h"
#include "SystemLogic.h" 
#include "GuiManager.h"  
#include "AudioStreamLogic.h" 
#include <WiFi.h>
#include <math.h>

SemaphoreHandle_t bleMutex = NULL;
volatile bool isStreamActive = false; 

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

volatile bool requestBabyStream = false;
volatile int babyStreamStatus = 0; 
volatile bool vidFSMode = false;
volatile bool showFps = false; 
volatile int currentFps = 0;

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
int prioMaster = 60, prioSlave = 50;
float effPrioMat = 0, effPrioKippy = 0, effPrioWifi = 0;
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

// --- NEU: Standardwerte Volumes ---
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

int mjpegDropThreshold = 0; 
int camHackMode = 0; 
bool audioDebugEnabled = false;
String audioLogs[10];
int audioLogIdx = 0;

int audioFormat = 0; 
volatile bool audioStandaloneMode = false;

uint8_t lastBtPackets[MAX_BT_MSGS][10];
int btPacketIdx = 0;
int btPacketCount = 0;

bool dongleAlarmEnabled = false;

void addAudioLog(String msg) {
    if(!audioDebugEnabled) return;
    msg.trim();
    if(msg.length() == 0) return; 
    audioLogs[audioLogIdx] = msg;
    audioLogIdx = (audioLogIdx + 1) % 10;
}

void Data_Init() {
    for(int i=0; i<HISTORY_SIZE; i++) pressureHistory[i] = -32000;
    preferences.begin("catmat", false);
    wifiSsid = preferences.getString("wifiSsid", "");
    wifiPass = preferences.getString("wifiPass", "");
    
    mqttBroker = preferences.getString("mqIP", SECRET_HA_IP); 
    mqttBroker.trim();
    if (mqttBroker.length() == 0) mqttBroker = SECRET_HA_IP;

    mqttPort = preferences.getInt("mqPort", SECRET_MQTT_PORT);
    if (mqttPort == 0) mqttPort = SECRET_MQTT_PORT;

    mqttUser = preferences.getString("mqUser", SECRET_MQTT_USER);
    mqttUser.trim();
    if (mqttUser.length() == 0) mqttUser = SECRET_MQTT_USER;

    mqttPass = preferences.getString("mqPass", SECRET_MQTT_PASS);
    mqttPass.trim();
    if (mqttPass.length() == 0) mqttPass = SECRET_MQTT_PASS;
    
    mqttEnabled = preferences.getBool("mqttEn", true); 
    savedMatMac = preferences.getString("macM", SECRET_MAC_MAT);
    savedKippyMac = preferences.getString("macK", SECRET_MAC_KIPPY);
    showFps = preferences.getBool("showFps", false); 
    haIP = preferences.getString("haIP", SECRET_HA_IP);
    haPort = preferences.getInt("haPort", SECRET_HA_PORT);
    
    streamIp = preferences.getString("strIp", SECRET_STREAM_IP);
    useCustomUrls = preferences.getBool("useCstUrl", false);
    camEntity = preferences.getString("camEntity", SECRET_CAM_ENTITY);
    babyStreamUrl = preferences.getString("babyUrl", SECRET_BABY_STREAM_URL);

    if (!useCustomUrls) {
        camEntity = "http://" + streamIp + ":8080/api-stream/v1/video?random=0";
        babyStreamUrl = "http://" + streamIp + ":8080/api-stream/v1/audio";
    }
    
    audioFormat = preferences.getInt("audioFmt", 0); 
    camHackMode = preferences.getInt("camHackM", 0); 

    // --- NEU: Volume Laden ---
    volMaster = preferences.getInt("volM", 80);
    volUI = preferences.getInt("volU", 100);
    volAlarm = preferences.getInt("volA", 100);
    volBaby = preferences.getInt("volB", 100);
    muteMaster = preferences.getBool("mutM", false);
    muteUI = preferences.getBool("mutU", false);
    muteAlarm = preferences.getBool("mutA", false);
    muteBaby = preferences.getBool("mutB", false);
    
    thresholdVal = preferences.getInt("thr", 150);
    taraOffset = preferences.getUInt("off", 0);
    isDarkMode = preferences.getBool("dark", true);
    graphTimeSeconds = preferences.getInt("gtime", 150);
    showTimeOnX = preferences.getBool("timeX", true);
    currentGraphMode = preferences.getInt("gMode", GRAPH_MODE_PRESSURE);
    autoBleInterval = preferences.getBool("bleAuto", true);
    manualBleIntervalMs = preferences.getInt("bleManMs", 1000);
    brightnessPercent = preferences.getInt("bright", 80);
    audioDebugEnabled = preferences.getBool("audDbg", false);
    mjpegDropThreshold = preferences.getInt("mjDrop", 0);
    wifiEnabled = preferences.getBool("wifiEn", true);
    matEnabled = preferences.getBool("matEn", true);
    kippyEnabled = preferences.getBool("kipEn", true);
    prioMaster = preferences.getInt("prioM", 60);
    prioSlave = preferences.getInt("prioS", 50);
    useMqttForMat = preferences.getBool("useMqtt", true);
    dongleAlarmEnabled = preferences.getBool("dongleAl", false);
    preferences.end();
}

void calcMultiplex() {
    if (isStreamActive && !isArmed) { effPrioMat = 0; effPrioKippy = 0; effPrioWifi = 100.0; return; }
    if (!wifiEnabled && !kippyEnabled && !matEnabled) { effPrioMat = 0; effPrioKippy = 0; effPrioWifi = 0; return; }
    if (!matEnabled && (!kippyEnabled || !wifiEnabled)) { effPrioMat = 0; effPrioWifi = wifiEnabled ? 100.0 : 0.0; effPrioKippy = kippyEnabled ? 100.0 : 0.0; return; }
    if (matEnabled) {
        effPrioMat = (float)prioMaster;
        float remaining = 100.0 - effPrioMat;
        if (wifiEnabled && kippyEnabled) { effPrioKippy = remaining * ((float)prioSlave / 100.0); effPrioWifi = remaining - effPrioKippy; } 
        else if (wifiEnabled) { effPrioWifi = remaining; effPrioKippy = 0; } 
        else if (kippyEnabled) { effPrioKippy = remaining; effPrioWifi = 0; } 
        else { effPrioMat = 100.0; effPrioWifi = 0; effPrioKippy = 0; }
    } else {
        effPrioMat = 0;
        effPrioKippy = (float)prioSlave;
        effPrioWifi = 100.0 - effPrioKippy;
    }
}

// --- NEU: Master/Slave Audio Engine ---
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

            if (volFactor <= 0.01f) continue; // Muted -> Direkt abbrechen, spielt keinen Ton

            if (isAudioStreaming) {
                int streamVol = (int)(effMaster * effBaby * 255.0f);
                if (streamVol > 255) streamVol = 255;
                M5.Speaker.setChannelVolume(0, streamVol / 3); // Ducking waehrend Ton spielt
            }

            int toneVol = (int)(volFactor * 255.0f);
            if (toneVol > 255) toneVol = 255;
            M5.Speaker.setChannelVolume(1, toneVol);

            if (msg.soundType == 0) M5.Speaker.tone(msg.freq, msg.duration, 1, true); 
            else if (msg.soundType == 1) M5.Speaker.tone(600, 200, 1, true); // Baby Alarm
            else if (msg.soundType == 2) M5.Speaker.tone(1100, 100, 1, true); // Cat Alarm
            else if (msg.soundType == 3) M5.Speaker.tone(800, 100, 1, true); // Slider Bing
            else if (msg.soundType == 4) M5.Speaker.tone(150, 15, 1, true); // Moderner Klick!
            
            vTaskDelay(pdMS_TO_TICKS(msg.duration + 50));
            
            if (isAudioStreaming) {
                int streamVol = (int)(effMaster * effBaby * 255.0f);
                if (streamVol > 255) streamVol = 255;
                M5.Speaker.setChannelVolume(0, streamVol);
            }
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
        msg.soundType = isUiSound ? 4 : 0; // 4 = Moderner Klick
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

void playBabyAlarmI2S() { if(audioQueue != NULL && !muteAlarm) { AudioMsg msg; msg.soundType = 1; msg.volumeChannel = 1; msg.duration=200; xQueueSend(audioQueue, &msg, 0); } }
void playCatAlarmI2S() { if(audioQueue != NULL && !muteAlarm) { AudioMsg msg; msg.soundType = 2; msg.volumeChannel = 1; msg.duration=100; xQueueSend(audioQueue, &msg, 0); } }

// --- FIX: Ersetzt die veralteten muted und babyMuted ---
void fullReset() { alarmActive = false; disconnectAlarmActive = false; muteAlarm = false; isArmed = false; cooldownUntil = millis() + 5000; topbarStatusMsg = ""; }
void wakeDisplay() { requestWake = true; M5.Display.wakeup(); }
void sleepDisplay() { requestSleep = true; M5.Display.sleep(); }
void factoryReset() { preferences.begin("catmat", false); preferences.clear(); preferences.end(); delay(1000); ESP.restart(); }

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
        preferences.end();
    }
}