#include <Arduino.h>
#include <lvgl.h>
#include <M5Unified.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>

#include "LVGL_Driver.h" 
#include "SharedData.h"
#include "SystemLogic.h"
#include "GuiManager.h"
#include "AudioStreamLogic.h"
#include "WebSetupLogic.h" 
#include "VideoLogic.h"    
#include "HaWebsocketLogic.h" 

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);
extern void ViewBaby_ExitFS(); 

extern lv_obj_t * splash_status_label; 

void updateStaticStatus(const char* text) {
    M5.Display.fillRect(0, 675, 1280, 45, TFT_BLACK);
    M5.Display.setTextColor(m5gfx::color565(150, 150, 150));
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(text, 640, 695);
}

void setup() {
    Serial.begin(115200);
    delay(1000); 
    Serial.println("\n--- SYSTEM BOOT ---");
    
    auto cfg = M5.config();
    M5.begin(cfg);
    delay(500); 
    //M5.Display.setRefreshRate(60);
    Data_Init();
    
    // FIX: Saubere Helligkeitsberechnung direkt beim Start
    M5.Display.setBrightness((brightnessPercent * 255) / 100);

    /*WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD, BOARD_SDIO_ESP_HOSTED_D0, 
                 BOARD_SDIO_ESP_HOSTED_D1, BOARD_SDIO_ESP_HOSTED_D2, BOARD_SDIO_ESP_HOSTED_D3, 
                 BOARD_SDIO_ESP_HOSTED_RESET);*/
    // ==============================================================
    // FIX: Die M5Tab5 WLAN-Pins (aus deinem Snippet)
    // ==============================================================
    Serial.println("[SYSTEM] Konfiguriere WLAN-Pins fuer M5Tab5...");
    WiFi.setPins(12, 13, 11, 10, 9, 8, 15);
    // ==============================================================



    Serial.println("[AUDIO-DIAG] Erzwinge I2S/I2C Hardware-Reset...");
    M5.Speaker.end();
    delay(100);
    bool speaker_ok = M5.Speaker.begin();
    
    if (speaker_ok) {
        Serial.println("[AUDIO-DIAG] ERFOLG: Audio-Hardware ist online!");
        M5.Speaker.setVolume(255);
        int uiVol = (volumePercent * 255) / 100;
        M5.Speaker.setChannelVolume(1, uiVol);
        
        M5.Speaker.tone(262, 100, 1, true); delay(100);
        M5.Speaker.tone(330, 100, 1, true); delay(100); 
        M5.Speaker.tone(392, 100, 1, true); delay(100); 
        M5.Speaker.tone(523, 100, 1, true); delay(100); 
        M5.Speaker.tone(659, 100, 1, true); delay(100);
        M5.Speaker.tone(784, 400, 1, true); delay(200); 
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

    updateStaticStatus("Starte WLAN & Hintergrunddienste...");
    SystemLogic_Update(); 
    
    while (millis() - startTime < 2500) {
        delay(10);
    }
}

void loop() {
    M5.update(); 
    
    static uint32_t last_tick = 0;
    if (last_tick == 0) last_tick = millis();
    uint32_t now = millis();
    uint32_t dt = now - last_tick;
    last_tick = now;
    if (dt > 50) dt = 50;
    lv_tick_inc(dt); 
    
    if (lvgl_port_lock(0)) {
        if (!vidFSMode) {
            lv_timer_handler();
        }
        
        if (gui.getCurrentScreen() == (ScreenID)99 && splash_status_label != nullptr) {
            static uint32_t last_splash_upd = 0;
            if (millis() - last_splash_upd > 150) {
                last_splash_upd = millis();
                if (HaWebsocketLogic_IsConnected()) {
                    lv_label_set_text(splash_status_label, "Home Assistant verbunden! Starte Dashboard...");
                } else if (WiFi.status() == WL_CONNECTED) {
                    lv_label_set_text(splash_status_label, "WLAN verbunden. Verbinde Home Assistant...");
                } else {
                    String s = String("Verbinde mit WLAN '") + wifiSsid + "'...";
                    lv_label_set_text(splash_status_label, s.c_str());
                }
            }
        }
        
        static uint32_t fs_start_time = 0;
        if (vidFSMode) {
            if (fs_start_time == 0) fs_start_time = millis();
            if (millis() - fs_start_time > 500 && M5.Touch.getCount() > 0) {
                vidFSMode = false;
                fs_start_time = 0;
                ViewBaby_ExitFS(); 
            }
        } else {
            fs_start_time = 0;
        }

        lvgl_port_unlock();
    }
    
    SystemLogic_Update(); 
    WebSetupLogic_Update();
    delay(5);
}