#include <Arduino.h>
#include <lvgl.h>
#include <M5Unified.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>


// ==============================================================
// FAIL-SAFE USB-HOST INTEGRATION (CDC-ACM VCP)
// ==============================================================
#if __has_include(<USBHostSerial.h>)
    #include <USBHostSerial.h>
    USBHostSerial dongleUSB;

    // Status-Tracker fuer den physischen Dongle (Heuristik)
    volatile bool dongleIsPhysicallyConnected = false;

    // Wir parsen die internen Bibliotheks-Logs, um den Status sicher zu ermitteln!
    // Wir parsen die internen Bibliotheks-Logs, um den Status sicher zu ermitteln!
    void usbHostLogger(const char* msg) {
        String s = String(msg);
        s.toLowerCase();
        
        // --- ANTI-SPAM FILTER ---
        // Unterdruecke die nervige Dauermeldung, wenn der Dongle fehlt
        if (s.indexOf("falling back to cdc") >= 0 || s.indexOf("vcp open returned null") >= 0) {
            dongleIsPhysicallyConnected = false;
            return; // Sofort abbrechen, NICHT printen!
        }
        
        // Die Bibliothek meldet typischerweise Verbindungsereignisse im Log
        if (s.indexOf("disconnect") >= 0 || s.indexOf("error") >= 0 || s.indexOf("fail") >= 0 || s.indexOf("unmount") >= 0) {
            dongleIsPhysicallyConnected = false;
        } else if (s.indexOf("connect") >= 0 || s.indexOf("ready") >= 0 || s.indexOf("open") >= 0 || s.indexOf("mount") >= 0) {
            dongleIsPhysicallyConnected = true;
        }
        
        Serial.print("[USBHostSerial] ");
        Serial.println(msg);
    }

    // Custom Stream-Adapter für die Bibliothek
    class UsbStreamWrapper : public Stream {
    private:
        USBHostSerial* _usb;
    public:
        UsbStreamWrapper(USBHostSerial* usb) : _usb(usb) {}
        int available() override { return _usb->available(); }
        int read() override { return _usb->read(); }
        int peek() override { return -1; } 
        void flush() override { }
        size_t write(uint8_t c) override { return _usb->write(c); }
        size_t write(const uint8_t *buffer, size_t size) override { return _usb->write(buffer, size); }
    };

    UsbStreamWrapper dongleStream(&dongleUSB);
#else
    HardwareSerial& dongleStream = Serial2; 
    volatile bool dongleIsPhysicallyConnected = true; // Fallback
#endif
// ==============================================================

#include "LVGL_Driver.h" 
#include "SharedData.h"
#include "SystemLogic.h"
#include "GuiManager.h"
#include "AudioStreamLogic.h"
#include "WebSetupLogic.h" 
#include "VideoLogic.h"    
#include "HaWebsocketLogic.h" 
#include "BleLogic.h"
#include "ViewBootScreen.h"


extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);
extern void ViewBaby_ExitFS(); 

extern lv_obj_t * splash_status_label; 

// ==============================================================
// NON-BLOCKING AUDIO STATE MACHINE (Ersetzt den fehlerhaften Task)
// ==============================================================
static const uint16_t boot_melody_freqs[] = {262, 330, 392, 523, 659, 784};
static const uint32_t boot_melody_durations[] = {100, 100, 100, 100, 100, 400};
static int current_note = 0;
static uint32_t note_start_time = 0;
static bool melody_playing = false;

void updateBootMelody() {
    if (!melody_playing) return;

    uint32_t now = millis();
    if (current_note >= 6) {
        melody_playing = false;
        return;
    }

    if (note_start_time == 0) {
        M5.Speaker.tone(boot_melody_freqs[current_note], boot_melody_durations[current_note], 1, true);
        note_start_time = now;
    } else {
        // 10ms Pause zwischen den Toenen fuer saubere Trennung
        if (now - note_start_time >= boot_melody_durations[current_note] + 10) {
            current_note++;
            if (current_note < 6) {
                M5.Speaker.tone(boot_melody_freqs[current_note], boot_melody_durations[current_note], 1, true);
                note_start_time = now;
            }
        }
    }
}

void updateStaticStatus(const char* text) {
    M5.Display.fillRect(0, 675, 1280, 45, TFT_BLACK);
    M5.Display.setTextColor(m5gfx::color565(150, 150, 150));
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(text, 640, 695);
}



void setup() {
    Serial.begin(115200);
    delay(10); // ANTI-BLOCK: Massiv gekuerzt von 1000ms
    Serial.println("\n--- SYSTEM BOOT ---");
    
    auto cfg = M5.config();
    M5.begin(cfg);
    delay(10); // ANTI-BLOCK: Massiv gekuerzt von 500ms

    Data_Init(); // Laedt sofort die wifiSsid aus dem Speicher
    
    // ==============================================================
    // EARLY-RADIO-INIT: WLAN sofort im Hintergrund starten!
    // ==============================================================
    Serial.println("[SYSTEM] Konfiguriere WLAN-Pins fuer M5Tab5...");
    WiFi.setPins(12, 13, 11, 10, 9, 8, 15);
    
    if (wifiSsid != "") {
        Serial.println("[SYSTEM] Starte WLAN asynchron im Hintergrund...");
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        WiFi.setSleep(false);
        wifiStarted = true; // Verhindert, dass SystemLogic es doppelt startet
    }

    // Saubere Helligkeitsberechnung direkt beim Start
    M5.Display.setBrightness((brightnessPercent * 255) / 100);

    BleLogic_Init();

    #if __has_include(<USBHostSerial.h>)
        Serial.println("[USB] Starte USBHostSerial Treiber...");
        dongleUSB.setLogger(usbHostLogger);
        dongleUSB.begin(115200, 0, 0, 8); 
        BleLogic_SetDongleStream(&dongleStream);
    #else
        Serial.println("[USB] WARNUNG: Library fehlt. Nutze Dummy-Serial.");
        dongleStream.begin(115200);
        BleLogic_SetDongleStream(&dongleStream);
    #endif

Serial.println("[AUDIO-DIAG] Erzwinge I2S/I2C Hardware-Reset...");
    M5.Speaker.end();
    delay(100); // ANTI-CRASH FIX: Muss 100ms bleiben fuer echten Hardware-Reset!
    bool speaker_ok = M5.Speaker.begin();
    
    if (speaker_ok) {
        Serial.println("[AUDIO-DIAG] ERFOLG: Audio-Hardware ist online!");
        M5.Speaker.setVolume(255);
        int uiVol = (int)( (muteMaster ? 0 : volMaster/100.0f) * (muteUI ? 0 : volUI/100.0f) * 255.0f );
        M5.Speaker.setChannelVolume(1, uiVol);
        
        // Starte die State-Machine anstelle des abstuerzenden FreeRTOS Tasks
        melody_playing = true;
        current_note = 0;
        note_start_time = 0;
    }

    M5.Display.setRotation(1); 
    M5.Display.setSwapBytes(true);
    M5.Display.fillScreen(TFT_BLACK);
    
    M5.Display.setTextColor(m5gfx::color565(0, 160, 255));
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSansBold24pt7b);
    M5.Display.setTextSize(3);
    M5.Display.drawString("Lela OS 2", 640, 150);

    uint16_t c_white = m5gfx::color565(255, 255, 255);
    uint16_t c_blue = m5gfx::color565(79, 165, 214);
    uint16_t c_orange = m5gfx::color565(230, 126, 34);

    int cx = 340;
    int cy = 400;
    int cat_pts[][2] = {{0,0}, {12,36}, {42,36}, {66,24}, {72,9}, {84,9}, {96,27}, {84,42}, {84,72}, {72,72}, {72,54}, {42,54}, {42,72}, {30,72}, {30,54}, {12,36}};
    for(int i=0; i<15; i++) M5.Display.drawWideLine(cx+cat_pts[i][0]*1.5, cy+cat_pts[i][1]*1.5, cx+cat_pts[i+1][0]*1.5, cy+cat_pts[i+1][1]*1.5, 6, c_white);
    M5.Display.drawWideLine(cx+cat_pts[15][0]*1.5, cy+cat_pts[15][1]*1.5, cx+cat_pts[0][0]*1.5, cy+cat_pts[0][1]*1.5, 6, c_white);
    
    int bx = 790; int by = 400;
    int b_pts[][2] = {{45, 90}, {135, 90}, {150, 135}, {135, 180}, {45, 180}, {30, 135}, {45, 90}};
    for(int i=0; i<6; i++) M5.Display.drawWideLine(bx+b_pts[i][0]*1.5, by+b_pts[i][1]*1.5, bx+b_pts[i+1][0]*1.5, by+b_pts[i+1][1]*1.5, 9, c_blue);
    int h_pts[][2] = {{60, 45}, {120, 45}, {135, 66}, {120, 90}, {60, 90}, {45, 66}, {60, 45}};
    for(int i=0; i<6; i++) M5.Display.drawWideLine(bx+h_pts[i][0]*1.5, by+h_pts[i][1]*1.5, bx+h_pts[i+1][0]*1.5, by+h_pts[i+1][1]*1.5, 6, c_white);
    M5.Display.drawWideLine(bx+84*1.5, by+78*1.5, bx+96*1.5, by+78*1.5, 6, c_orange);
    M5.Display.drawWideLine(bx+96*1.5, by+78*1.5, bx+96*1.5, by+84*1.5, 6, c_orange);
    M5.Display.drawWideLine(bx+96*1.5, by+84*1.5, bx+84*1.5, by+84*1.5, 6, c_orange);
    M5.Display.drawWideLine(bx+84*1.5, by+84*1.5, bx+84*1.5, by+78*1.5, 6, c_orange);

    M5.Display.setTextColor(c_white);
    M5.Display.setTextSize(1);
    M5.Display.drawString("zzzZZZ", bx+120, by+40);

    startTime = millis();

    updateStaticStatus("Lade Grafik-Engine und reserviere Speicher...");
    Lvgl_Init();
    AudioStreamLogic_Init(); 

    int batLevel = M5.Power.getBatteryLevel();
    if (batLevel >= 0 && batLevel <= 100) {
        batteryPercent = batLevel;
    }

    updateStaticStatus("Lese Flash-Speicher & Konfigurationen...");
    SystemLogic_Init(); 

    updateStaticStatus("Bereite Benutzeroberflaeche vor...");
    gui.init();

    updateStaticStatus("Starte Hintergrunddienste...");
    SystemLogic_Update(); 
    
    // HIER STAND DAS 2,5s DELAY. Es wurde restlos entfernt! Das System springt SOFORT in die loop().
}

void loop() {
    M5.update(); 
    updateBootMelody();
    
    // BOOTSCREEN-DAUER: Auf 1,2 Sekunden verkuerzt (passend zur Startmelodie)
    bool nativeSplashActive = (millis() - startTime < 1200);

    // ANTI-SKIP FIX: LVGL darf keine Zeit berechnen, waehrend der Bootscreen sichtbar ist.
    // So startet die Animation danach exakt bei Frame 0 und nicht in der Mitte!
    if (!nativeSplashActive) {
        static uint32_t last_tick = 0;
        if (last_tick == 0) last_tick = millis(); // Setzt den Startpunkt exakt auf das Ende des Splashs
        uint32_t now = millis();
        uint32_t dt = now - last_tick;
        last_tick = now;
        if (dt > 50) dt = 50;
        lv_tick_inc(dt); 
    }
    
    if (lvgl_port_lock(0)) {
        
        // LVGL darf erst zeichnen, wenn der native Splash beendet ist!
        if (!vidFSMode && !nativeSplashActive) {
            lv_timer_handler();
        }
        
// ==============================================================
        // DIE NEUE STATE-MACHINE FUER DIE BOOT-ANIMATION
        // ==============================================================
        if (!nativeSplashActive && gui.getCurrentScreen() == (ScreenID)99) {
            static BootState current_boot_state = BOOT_STATE_INIT;
            static uint32_t lvgl_anim_start_time = 0;
            static uint32_t wifi_ready_time = 0; // NEU: Timer fuer die Toleranzzeit
            
            if (lvgl_anim_start_time == 0) lvgl_anim_start_time = millis();
            
            BootState new_state = current_boot_state;

            // Logik-Kaskade: Smartes Warten (verhindert den 15s Timeout-Trap!)
            if (WiFi.status() == WL_CONNECTED) {
                if (wifi_ready_time == 0) wifi_ready_time = millis();
                
                // Wir beenden die Animation, sobald HA verbunden ist,
                // ODER spaetestens 2,5 Sekunden nachdem das WLAN steht!
                // So friert das Tablet niemals ein, wenn ein Server nicht antwortet.
                if (HaWebsocketLogic_IsConnected() || (millis() - wifi_ready_time > 2500)) {
                    new_state = BOOT_STATE_DONE;
                } else {
                    new_state = BOOT_STATE_HA;
                }
            } else {
                new_state = BOOT_STATE_WIFI;
            }

            // Hard-Timeout-Schutz (Greift nur noch, wenn der Router gar nicht antwortet)
            if (new_state != BOOT_STATE_DONE && millis() - lvgl_anim_start_time > 15000) {
                new_state = BOOT_STATE_TIMEOUT;
            }

            // Bei Zustandswechsel: Gib das Kommando an den Bildschirm!
            if (new_state != current_boot_state) {
                current_boot_state = new_state;
                ViewBootScreen::setBootState(new_state, wifiSsid.c_str());
            }
        }
        
        static uint32_t fs_start_time = 0;
        if (vidFSMode) {
            if (fs_start_time == 0) fs_start_time = millis();
            if (millis() - fs_start_time > 500 && M5.Touch.getCount() > 0) {
                M5.Display.waitDisplay(); 
                vidFSMode = false;
                fs_start_time = 0;
                ViewBaby_ExitFS(); 
            }
        } else {
            fs_start_time = 0;
        }

        lvgl_port_unlock();
    }
    
    BleLogic_SetDongleReady(dongleIsPhysicallyConnected);

    // ==========================================================
    // PARALLELER BOOT:
    // Diese Updates rennen ungebremst WAEHREND der Boot-Animation
    // im Hintergrund und verbinden WLAN, MQTT und Bluetooth!
    // ==========================================================
    SystemLogic_Update(); 
    WebSetupLogic_Update();

    if (M5.Display.getBrightness() == 0) {
        delay(100); 
    } else {
        delay(5);
    }
}