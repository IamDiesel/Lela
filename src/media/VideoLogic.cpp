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
static uint8_t* download_buf = nullptr; 
static uint8_t write_idx = 0;
static uint8_t read_idx = 1;

static TaskHandle_t videoTaskHandle = NULL;
static lgfx::PPASrm* ppa_srm = nullptr;
static jpeg_decoder_handle_t jpeg_engine = NULL;

// Temporaerer Diagnose-Test:
#define MAX_JPEG_DOWNLOAD_SIZE 150000  
#define MAX_PIXEL_BUF_SIZE (1280 * 960 * 2) 

static void setUiStatus(const char* msg) {
    if (lvgl_port_lock(portMAX_DELAY)) { ViewBaby_SetStatus(msg); lvgl_port_unlock(); }
}

void VideoLogic_Init() {
    // Dauerhafte Reservierung beim Booten! Keine Fragmentierung mehr moeglich.
    if (download_buf == nullptr) {
        download_buf = (uint8_t*)heap_caps_aligned_alloc(64, MAX_JPEG_DOWNLOAD_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        for(int i=0; i<3; i++) {
            jpg_bufs[i] = (uint8_t*)heap_caps_aligned_alloc(64, MAX_PIXEL_BUF_SIZE, MALLOC_CAP_SPIRAM);
        }
        if (download_buf) {
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

    while (likely(isStreamActive && stream->connected())) {
        int frameSize = 0;
        
        stream->setTimeout(50);
        uint32_t headerStartMs = millis();
        
        while (likely(stream->connected() && isStreamActive)) {
            size_t len = stream->readBytesUntil('\n', header_buf, sizeof(header_buf) - 1);
            
            if (len == 0) {
                if (unlikely(millis() - headerStartMs > 2000)) { frameSize = -1; break; }
                continue;
            }
            
            if (unlikely(len < 2 && frameSize > 0)) break; 
            
            header_buf[len] = '\0';
            if (strncasecmp(header_buf, "Content-Length:", 15) == 0) {
                frameSize = atoi(header_buf + 15);
            }
        }

        if (unlikely(frameSize <= 0 || frameSize > MAX_JPEG_DOWNLOAD_SIZE)) {
            continue;
        }

        size_t bytesRead = 0;
        uint32_t startMs = millis();
        
        while (likely(bytesRead < frameSize && stream->connected())) {
            int avail = stream->available();
            if (likely(avail > 0)) {
                size_t toRead = (frameSize - bytesRead > avail) ? avail : (frameSize - bytesRead);
                bytesRead += stream->read((uint8_t*)d_buf + bytesRead, toRead);
            } else {
                vTaskDelay(1); 
                if (unlikely(millis() - startMs > 1500)) break; 
            }
        }

        if (likely(bytesRead == frameSize && d_buf[0] == 0xFF && d_buf[1] == 0xD8)) {
            if (likely(jpeg_decoder_get_info(d_buf, frameSize, &pic_info) == ESP_OK)) {
                
                uint8_t next_write = (write_idx + 1) % 3;
                if (unlikely(next_write == read_idx)) next_write = (next_write + 1) % 3;
                
                uint8_t* out_buf = jpg_bufs[next_write];

                dec_cfg.rgb_order = vidFSMode ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;

                if (likely(jpeg_decoder_process(jpeg_engine, &dec_cfg, d_buf, frameSize, out_buf, MAX_PIXEL_BUF_SIZE, &out_size_actual) == ESP_OK)) {
                    write_idx = next_write;
                    read_idx = next_write; 

                    if (vidFSMode) {
                        if (unlikely(!ppa_srm)) ppa_srm = new lgfx::PPASrm(&M5.Display, false); 
                        float zoom_x = 1280.0f / pic_info.width;
                        float zoom_y = 720.0f / pic_info.height;
                        float zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y; 
                        
                        // ==========================================================
                        // BUGFIX: HARDWARE-MUTEX FUER PPA & DMA
                        // ==========================================================
                        M5.Display.waitDisplay(); // Warten, bis eventuelle DMA-Transfers (z.B. LVGL) fertig sind
                        M5.Display.startWrite();  // Hardware-Bus exklusiv fuer diesen Task sperren
                        
                        ppa_srm->pushImageSRM((1280 - (int)(pic_info.width*zoom)) / 2, 
                                             (720 - (int)(pic_info.height*zoom)) / 2, 
                                             0, 0, 0, zoom, zoom, pic_info.width, pic_info.height, (uint16_t*)out_buf);

                        if (showFps) {
                            M5.Display.setCursor(20, 80); 
                            M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                            M5.Display.setFont(&fonts::FreeSansBold18pt7b);
                            M5.Display.printf("FPS: %d ", currentFps);
                        }
                        
                        M5.Display.endWrite(); // Hardware-Bus wieder freigeben
                        // ==========================================================
                    } else {
                        if (lvgl_port_lock(0)) { 
                            cam_img_dsc[read_idx].header.w = pic_info.width; 
                            cam_img_dsc[read_idx].header.h = pic_info.height; 
                            cam_img_dsc[read_idx].header.cf = LV_IMG_CF_TRUE_COLOR;
                            cam_img_dsc[read_idx].data = out_buf; 
                            ViewBaby_SetImage(&cam_img_dsc[read_idx]); 
                            lvgl_port_unlock();
                        }
                    }
                    
                    static uint32_t fCount = 0; static uint32_t lFps = 0;
                    fCount++;
                    if (unlikely(millis() - lFps >= 1000)) { currentFps = fCount; fCount = 0; lFps = millis(); }
                }
            }
        }
        vTaskDelay(1); 
    }
}

static void videoTask(void * pvParameters) {
    if (download_buf && jpg_bufs[0] && jpg_bufs[1] && jpg_bufs[2]) {
        jpeg_decode_engine_cfg_t eng_cfg = { .timeout_ms = 1000 };
        jpeg_new_decoder_engine(&eng_cfg, &jpeg_engine);

        while(isStreamActive) {
            if (WiFi.status() != WL_CONNECTED) { 
                vTaskDelay(1000); 
                continue; 
            }
            
            String url = camEntity;
            if (!url.startsWith("http")) url = "http://" + haIP + ":" + String(haPort) + url;

            if (camHackMode > 0) {
                setUiStatus("Init Aufloesung...");
                int protoEnd = url.indexOf("://");
                if (protoEnd != -1) {
                    int ipStart = protoEnd + 3;
                    int ipEnd = url.indexOf(':', ipStart);
                    if (ipEnd == -1) ipEnd = url.indexOf('/', ipStart);
                    if (ipEnd != -1) {
                        String ipStr = url.substring(ipStart, ipEnd);
                        
                        int reqW = 0, reqH = 0;
                        if (camHackMode == 1) { reqW = 320; reqH = 240; }
                        else if (camHackMode == 2) { reqW = 480; reqH = 360; }
                        else if (camHackMode == 3) { reqW = 640; reqH = 360; }
                        else if (camHackMode == 4) { reqW = 640; reqH = 480; }
                        else if (camHackMode == 5) { reqW = 800; reqH = 450; }
                        else if (camHackMode == 6) { reqW = 800; reqH = 600; }
                        else if (camHackMode == 7) { reqW = 1024; reqH = 768; }
                        else if (camHackMode == 8) { reqW = 1280; reqH = 720; }
                        else if (camHackMode == 9) { reqW = 1280; reqH = 960; }
                        
                        int finalW = reqW;
                        int finalH = reqH;

                        // 1. Verfuegbare Aufloesungen abrufen
                        HTTPClient httpRes;
                        httpRes.setTimeout(2000);
                        httpRes.begin("http://" + ipStr + ":8080/api/v1/camera/resolutions?_=0");
                        httpRes.addHeader("accept", "application/json");
                        int resCode = httpRes.GET();
                        
                        if (resCode == HTTP_CODE_OK) {
                            String resPayload = httpRes.getString();
                            JsonDocument doc; 
                            DeserializationError err = deserializeJson(doc, resPayload);
                            
                            if (!err && doc.containsKey("resolutions")) {
                                JsonArray resArray = doc["resolutions"];
                                bool exactMatch = false;
                                int bestArea = 0;
                                int fallbackW = 0, fallbackH = 0;
                                int reqArea = reqW * reqH;
                                
                                int minArea = 99999999;
                                int minW = 0, minH = 0;

                                for (JsonVariant v : resArray) {
                                    String rStr = v.as<String>();
                                    int xIdx = rStr.indexOf('x');
                                    if (xIdx != -1) {
                                        int w = rStr.substring(0, xIdx).toInt();
                                        int h = rStr.substring(xIdx + 1).toInt();
                                        int area = w * h;

                                        if (area < minArea && area > 0) { 
                                            minArea = area; minW = w; minH = h; 
                                        }

                                        if (w == reqW && h == reqH) {
                                            exactMatch = true;
                                            break;
                                        }

                                        if (area <= reqArea && area > bestArea) {
                                            bestArea = area;
                                            fallbackW = w;
                                            fallbackH = h;
                                        }
                                    }
                                }

                                if (!exactMatch) {
                                    if (bestArea > 0) {
                                        finalW = fallbackW; finalH = fallbackH;
                                    } else if (minW > 0) {
                                        finalW = minW; finalH = minH; 
                                    }
                                }
                            }
                        }
                        httpRes.end();

                        // 2. Aufloesung anwenden
                        HTTPClient httpHack;
                        httpHack.setTimeout(2000);
                        httpHack.begin("http://" + ipStr + ":8080/api/v1/camera/change-resolution");
                        httpHack.addHeader("accept", "application/json");
                        httpHack.addHeader("content-type", "application/x-www-form-urlencoded");

                        String payload = "{width:" + String(finalW) + ",height:" + String(finalH) + "}";

                        httpHack.POST(payload);
                        httpHack.end();
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                }
            }

            HTTPClient http;
            http.setReuse(false);
            
            String connMsg = "Verbinde: " + url;
            setUiStatus(connMsg.c_str());
            
            http.begin(url);
            int httpCode = http.GET();
            
            if (httpCode == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    stream->setNoDelay(true);
                    stream->setTimeout(10); 
                    processStream<true>(stream, download_buf);
                } 
            } else {
                String errMsg = "HTTP Fehler: " + String(httpCode);
                setUiStatus(errMsg.c_str());
                vTaskDelay(2000); 
            }
            http.end();
            if (isStreamActive) vTaskDelay(100); 
        }
    } else {
        setUiStatus("Fehler: RAM voll!");
        vTaskDelay(3000);
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