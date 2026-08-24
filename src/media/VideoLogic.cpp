#pragma GCC optimize ("O3") 

#include "GuiManager.h"   
#include <M5Unified.hpp>
#include <LGFX_PPA.hpp> 
#include "VideoLogic.h"
#include "SharedData.h"
#include <WiFi.h>
#include <HTTPClient.h>      
#include <ArduinoJson.h>
#include "ViewBaby.h"   
#include "BabyCamApi.h"     
#include <driver/jpeg_decode.h>

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

static lv_img_dsc_t cam_img_dsc[3] = {{0}, {0}, {0}};
static uint8_t* jpg_bufs[3] = {nullptr, nullptr, nullptr}; 
static uint8_t write_idx = 0;
static uint8_t read_idx = 1;

static TaskHandle_t videoTaskHandle = NULL;
static lgfx::PPASrm* ppa_srm = nullptr;
static jpeg_decoder_handle_t jpeg_engine = NULL;

#define MAX_JPEG_DOWNLOAD_SIZE 150000  
#define MAX_PIXEL_BUF_SIZE (1280 * 960 * 2) 

static void setUiStatus(const char* msg) {
    if (lvgl_port_lock(portMAX_DELAY)) { ViewBaby_SetStatus(msg); lvgl_port_unlock(); }
}

static void reset_jpeg_engine() {
    if (jpeg_engine) {
        jpeg_del_decoder_engine(jpeg_engine);
        jpeg_engine = NULL;
    }
    jpeg_decode_engine_cfg_t eng_cfg = { .timeout_ms = 1000 };
    jpeg_new_decoder_engine(&eng_cfg, &jpeg_engine);
}

void VideoLogic_Init() {
    Serial.printf("[VideoLogic] Initialisiere RAM...\n");
    Serial.printf("[VideoLogic] Freies RAM vorab: %d KB | Freies PSRAM: %d KB\n", ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
    
    if (jpg_bufs[0] == nullptr) {
        for(int i=0; i<3; i++) {
            jpg_bufs[i] = (uint8_t*)heap_caps_aligned_alloc(64, MAX_PIXEL_BUF_SIZE, MALLOC_CAP_SPIRAM);
            if (jpg_bufs[i]) {
                Serial.printf("[VideoLogic] Buffer %d im PSRAM angelegt: %p\n", i, jpg_bufs[i]);
            } else {
                Serial.printf("[VideoLogic] FEHLER: Konnte Buffer %d nicht im PSRAM anlegen!\n", i);
            }
        }
        
        if (download_buf && jpg_bufs[0] && jpg_bufs[1] && jpg_bufs[2]) {
            Serial.println("[VideoLogic] RAM erfolgreich vorab reserviert.");
        } else {
            Serial.println("[VideoLogic] KRITISCHER FEHLER beim Reservieren des RAMs!");
        }
    }
}


template <bool FAST_MODE>
void processStream(WiFiClient* stream, uint8_t* d_buf) {
    static jpeg_decode_cfg_t dec_cfg = { .output_format = JPEG_DECODE_OUT_FORMAT_RGB565 };
    static jpeg_decode_picture_info_t pic_info = {};
    uint32_t out_size_actual = 0;

    char header_buf[256]; 
    static bool lastFSMode = false;

    while (isStreamActive && stream->connected()) {
        int frameSize = 0;
        uint32_t headerStartMs = millis();
        int garbage_lines = 0; 
        
        stream->setTimeout(10); 
        
        while (isStreamActive && stream->connected()) {
            vTaskDelay(pdMS_TO_TICKS(1)); 
            
            size_t len = stream->readBytesUntil('\n', header_buf, sizeof(header_buf) - 1);
            
            if (len == 0) {
                if (millis() - headerStartMs > 5000) { 
                    if (isStreamActive) {
                        Serial.println("[Video-Diag] Timeout beim Warten auf Header!");
                        setUiStatus("Stream stockt (Header Timeout)...");
                        playToneI2S(800, 100, false);
                        playToneI2S(600, 150, false);
                    }
                    return; 
                }
                continue; 
            }
            
            header_buf[len] = '\0';
            
            if (len <= 2 && frameSize > 0) break; 
            
            if (strncasecmp(header_buf, "Content-Length:", 15) == 0) {
                frameSize = atoi(header_buf + 15);
                garbage_lines = 0; 
            } else {
                garbage_lines++;
                if (garbage_lines > 25) {
                    Serial.println("[Video-Diag] Zu viele unbekannte Header! Sync verloren. Reconnect...");
                    return; 
                }
            }
        }

        if (!isStreamActive || !stream->connected()) return;

        if (unlikely(frameSize <= 0 || frameSize > MAX_JPEG_DOWNLOAD_SIZE)) {
            Serial.printf("[Video-Diag] Ungueltige Framegroesse: %d Bytes. Abbruch.\n", frameSize);
            return; 
        }

        size_t bytesRead = 0;
        uint32_t startMs = millis();
        uint32_t lastYieldMs = millis(); 
        
        while (bytesRead < frameSize && isStreamActive && stream->connected()) {
            int avail = stream->available();
            if (avail > 0) {
                size_t toRead = (frameSize - bytesRead > avail) ? avail : (frameSize - bytesRead);
                int readNow = stream->read(d_buf + bytesRead, toRead);
                
                if (readNow > 0) {
                    bytesRead += readNow;
                    startMs = millis(); 
                } else {
                    Serial.printf("[Video-Diag] Stream-Read Abbruch! readNow=%d, avail=%d\n", readNow, avail);
                    break; 
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(1)); 
                if (millis() - startMs > 3000) {
                    if (isStreamActive) {
                        Serial.println("[Video-Diag] TCP Timeout beim Bild-Download!");
                        setUiStatus("Download blockiert! (Timeout)...");
                        playToneI2S(800, 100, false);
                        playToneI2S(600, 150, false);
                    }
                    return; 
                }
            }
            
            if (millis() - lastYieldMs > 5) {
                vTaskDelay(pdMS_TO_TICKS(1));
                lastYieldMs = millis();
            }
        }

        if (!isStreamActive || !stream->connected()) return;

        bool is_valid_jpeg = false;
        int actual_jpeg_size = 0;

        if (bytesRead > 512 && d_buf[0] == 0xFF && d_buf[1] == 0xD8) {
            for (int i = bytesRead - 2; i >= 0; i--) {
                if (d_buf[i] == 0xFF && d_buf[i+1] == 0xD9) {
                    actual_jpeg_size = i + 2;
                    is_valid_jpeg = true;
                    break;
                }
            }
        }

        if (unlikely(!is_valid_jpeg)) {
            Serial.println("==================================================");
            Serial.printf("[Video-Diag] SYNC LOST ODER BILD KORRUPT!\n");
            Serial.printf("[Video-Diag] Erwartet: %d Bytes | Empfangen: %d Bytes\n", frameSize, (int)bytesRead);
            Serial.println("==================================================");
            
            setUiStatus("Bild-Sync verloren! (Reset)...");
            playToneI2S(800, 100, false);
            playToneI2S(600, 150, false);
            return; 
        }

        if (likely(jpeg_decoder_get_info(d_buf, actual_jpeg_size, &pic_info) == ESP_OK)) {
            
            if (pic_info.width * pic_info.height * 2 > MAX_PIXEL_BUF_SIZE) {
                setUiStatus("Fehler: Bildaufloesung zu gross!");
                return; 
            }

            uint8_t next_write = (write_idx + 1) % 3;
            if (unlikely(next_write == read_idx)) next_write = (next_write + 1) % 3;
            
            uint8_t* out_buf = jpg_bufs[next_write];
            dec_cfg.rgb_order = vidFSMode ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;

            esp_err_t dec_res = jpeg_decoder_process(jpeg_engine, &dec_cfg, d_buf, actual_jpeg_size, out_buf, MAX_PIXEL_BUF_SIZE, &out_size_actual);

            if (likely(dec_res == ESP_OK)) {
                write_idx = next_write;
                read_idx = next_write; 

                if (vidFSMode) {
                    if (!lastFSMode) {
                        M5.Display.clear(TFT_BLACK);
                        lastFSMode = true;
                    }
                    
                    if (unlikely(!ppa_srm)) ppa_srm = new lgfx::PPASrm(&M5.Display, false); 
                    float zoom_x = 1280.0f / pic_info.width;
                    float zoom_y = 720.0f / pic_info.height;
                    float zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y; 
                    
                    M5.Display.startWrite();  
                    
                    ppa_srm->pushImageSRM((1280 - (int)(pic_info.width*zoom)) / 2, 
                                         (720 - (int)(pic_info.height*zoom)) / 2, 
                                         0, 0, 0, zoom, zoom, pic_info.width, pic_info.height, (uint16_t*)out_buf);
                    
                    M5.Display.waitDisplay(); 
                    M5.Display.setCursor(20, 80); 
                    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
                    
                    if (showFps) {
                        M5.Display.printf("FPS: %d  |  Akku: %d%%", currentFps, camBatteryPercent);
                    } else {
                        M5.Display.printf("Akku: %d%%", camBatteryPercent);
                    }
                    
                    M5.Display.endWrite();
                    vTaskDelay(pdMS_TO_TICKS(5));
                    
                } else {
                    if (lastFSMode) {
                        lastFSMode = false;
                        if (lvgl_port_lock(50)) {
                            lv_obj_invalidate(lv_scr_act()); 
                            lvgl_port_unlock();
                        }
                    }

                    if (lvgl_port_lock(15)) { 
                        cam_img_dsc[read_idx].header.w = pic_info.width; 
                        cam_img_dsc[read_idx].header.h = pic_info.height; 
                        cam_img_dsc[read_idx].header.cf = LV_IMG_CF_TRUE_COLOR;
                        cam_img_dsc[read_idx].data = out_buf; 
                        ViewBaby_SetImage(&cam_img_dsc[read_idx]); 
                        lvgl_port_unlock();
                    }
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
                
                static uint32_t fCount = 0; static uint32_t lFps = 0;
                fCount++;
                if (unlikely(millis() - lFps >= 1000)) { 
                    currentFps = fCount; 
                    fCount = 0; 
                    lFps = millis(); 
                }
            } else {
                Serial.println("[Video-Diag] Hardware JPEG Decoder Fehler! Auto-Reset...");
                reset_jpeg_engine();
            }
        } else {
            Serial.println("[Video-Diag] jpeg_decoder_get_info() fehlgeschlagen. Verwerfe Frame.");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}


static void videoTask(void * pvParameters) {
    if (download_buf && jpg_bufs[0] && jpg_bufs[1] && jpg_bufs[2]) {
        
        reset_jpeg_engine(); 

        while(isStreamActive) {
            if (WiFi.status() != WL_CONNECTED) { 
                vTaskDelay(pdMS_TO_TICKS(1000)); 
                continue; 
            }
            
            String url = camEntity;
            if (!url.startsWith("http")) url = "http://" + haIP + ":" + String(haPort) + url;

            // ==============================================================
            // SCHRITT 1 & 2: BOOT-SEQUENZ (HEALTH CHECK & SESSION START)
            // ==============================================================
            setUiStatus("Ping Kamera...");
            HTTPClient httpInit;
            httpInit.setTimeout(2000);
            httpInit.begin("http://" + streamIp + ":8080/cgi/version");
            int pingCode = httpInit.GET();
            httpInit.end();

            if (pingCode != HTTP_CODE_OK) {
                Serial.printf("[VideoLogic] Ping fehlgeschlagen: %d\n", pingCode);
                setUiStatus("Kamera nicht erreichbar");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            setUiStatus("Client Login...");
            httpInit.begin("http://" + streamIp + ":8080/api/v1/client/online");
            httpInit.addHeader("Content-Type", "application/json");
            httpInit.addHeader("uuid", camUuid);
            httpInit.addHeader("versionApp", "2.48");
            String loginPayload = "{\"deviceName\":\"Lela OS Monitor\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"uuidClient\":\"" + camUuid + "\"}";
            int loginCode = httpInit.POST(loginPayload);
            httpInit.end();

            if (loginCode != HTTP_CODE_OK) {
                Serial.printf("[VideoLogic] Login fehlgeschlagen: %d\n", loginCode);
                setUiStatus("Login fehlgeschlagen");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            // ==============================================================
            // SCHRITT 3 & 4: WUNSCH-AUFLOESUNG SENDEN & WARTEN
            // ==============================================================
            setUiStatus("Sende Aufloesung...");
            int reqW = 1280, reqH = 720;
            int xIdx = currentCamRes.indexOf('x');
            if(xIdx != -1) {
                reqW = currentCamRes.substring(0, xIdx).toInt();
                reqH = currentCamRes.substring(xIdx+1).toInt();
            }
            
            // Setzt die Aufloesung via REST API auf der Kamera
            BabyCamApi_SetResolution(reqW, reqH);
            
            // ATEMPAUSE: Der Kamera 1,5 Sekunden geben, um den Encoder mit der neuen Auflösung neuzustarten!
            vTaskDelay(pdMS_TO_TICKS(1500)); 

            // ==============================================================
            // SCHRITT 5: VIDEO STREAM ABGREIFEN
            // ==============================================================
            HTTPClient http;
            http.setReuse(false);
            http.setTimeout(5000); // Dem Stream beim Start bis zu 5 Sekunden Wakeup-Zeit geben
            
            String connMsg = "Verbinde: " + url;
            setUiStatus(connMsg.c_str());
            
            http.begin(url);
            int httpCode = http.GET();
            
            if (httpCode == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    stream->setNoDelay(true);
                    stream->setTimeout(10); 
                    
                    // Startet den Download und das Zeichnen der Frames
                    processStream<true>(stream, download_buf);
                } else {
                    if (isStreamActive) setUiStatus("Stream nicht verfuegbar");
                }
            } else {
                if (isStreamActive) {
                    String errMsg = "HTTP Fehler: " + String(httpCode);
                    setUiStatus(errMsg.c_str());
                }
            }
            
            http.end();
            
            if (isStreamActive) {
                Serial.println("[VideoLogic] Stream unerwartet beendet. Reconnect in 2s...");
                setUiStatus("Verbindung weg. Reconnect in 2s...");
                vTaskDelay(pdMS_TO_TICKS(2000)); 
            }
        }
    } else {
        setUiStatus("Fehler: RAM voll!");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    if (jpeg_engine) {
        jpeg_del_decoder_engine(jpeg_engine);
        jpeg_engine = NULL;
    }
    
    videoTaskHandle = NULL;
    vTaskDelete(NULL); 
}

void VideoLogic_Start() {
    if (videoTaskHandle == NULL && isStreamActive) {
        xTaskCreatePinnedToCore(videoTask, "VideoTask", 8192, NULL, 5, &videoTaskHandle, 1); 
    }
}

void VideoLogic_Stop() { 
    isStreamActive = false; 
    requestBabyStream = false; 
}