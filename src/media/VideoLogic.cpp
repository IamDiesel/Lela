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

// Einbinden der Inline-Komponente
#include "VideoOverlay.h"

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// ==============================================================================
// PARAMETER FUER DEN VIDEO-STREAM
// ==============================================================================
// 0 = Kein Drop (Maximale Fluessigkeit, moegliches Delay)
// 1 = Jedes 2. Bild verwerfen (Gute Balance)
// 2 = Zwei Bilder verwerfen, eins zeigen (Echtzeit garantiert, Slideshow-Effekt)
int camDropFrames = 0; 
// ==============================================================================

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

    // Cache-Variablen für Float-Berechnungen
    static int last_pic_w = 0;
    static int last_pic_h = 0;
    static float cached_zoom_fs = 1.0f;
    static float cached_zoom_win = 1.0f;

    uint32_t fCount = 0;
    uint32_t lastFpsTime = millis();
    uint32_t lastFrameTime = millis(); 


    auto draw_and_push_fullscreen = [&](uint8_t buf_idx) {
        if (!lastFSMode) { 
            M5.Display.clear(TFT_BLACK); 
            lastFSMode = true; 
        }
        
        VideoOverlay::applyOverlay((uint16_t*)jpg_bufs[buf_idx], pic_info.width, pic_info.height, currentFps, camBatteryPercent, showFps);
        
        if (unlikely(!ppa_srm)) ppa_srm = new lgfx::PPASrm(&M5.Display, false); 
        float zoom_x = 1280.0f / pic_info.width;
        float zoom_y = 720.0f / pic_info.height;
        float zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y; 
        
        M5.Display.startWrite();  
        ppa_srm->pushImageSRM((1280 - (int)(pic_info.width*zoom)) / 2, 
                             (720 - (int)(pic_info.height*zoom)) / 2, 
                             0, 0, 0, zoom, zoom, pic_info.width, pic_info.height, (uint16_t*)jpg_bufs[buf_idx]);
        M5.Display.endWrite();
    };


    while (isStreamActive && stream->connected()) {
        int frameSize = 0;
        uint32_t headerStartMs = millis();
        int garbage_lines = 0; 
        
        stream->setTimeout(10); 
        
        while (isStreamActive && stream->connected()) {
            size_t len = stream->readBytesUntil('\n', header_buf, sizeof(header_buf) - 1);
            
            if (len == 0) {
                if (millis() - headerStartMs > 15000) { 
                    if (isStreamActive) {
                        setUiStatus("Stream stockt (Header Timeout)...");
                    }
                    return; 
                }
                vTaskDelay(pdMS_TO_TICKS(1)); // Hier ist ein Delay okay, da wir auf den Header warten
                continue; 
            }
            
            header_buf[len] = '\0';
            if (len <= 2 && frameSize > 0) break; 
            
            if (strncasecmp(header_buf, "Content-Length:", 15) == 0) {
                frameSize = atoi(header_buf + 15);
                garbage_lines = 0; 
            } else {
                garbage_lines++;
                if (garbage_lines > 25) return; 
            }
        }

        if (!isStreamActive || !stream->connected()) return;
        if (unlikely(frameSize <= 0 || frameSize > MAX_JPEG_DOWNLOAD_SIZE)) return; 

        size_t bytesRead = 0;
        uint32_t startMs = millis();
        
        // Optimierte Lese-Schleife ohne 1ms Delay-Bremse
        while (bytesRead < frameSize && isStreamActive && stream->connected()) {
            size_t toRead = frameSize - bytesRead;
            // read() blockiert automatisch bis zu stream->setTimeout(), das lastet das Netzwerk ideal aus
            int readNow = stream->read(d_buf + bytesRead, toRead);
            
            if (readNow > 0) {
                bytesRead += readNow;
                startMs = millis(); 
            } else {
                // Nur aufgeben/schlafen, wenn das Timeout des Sockets gegriffen hat und nichts kam
                vTaskDelay(pdMS_TO_TICKS(1)); 
                if (millis() - startMs > 3000) {
                    if (isStreamActive) setUiStatus("Download blockiert! (Timeout)...");
                    return; 
                }
            }
        }

        if (!isStreamActive || !stream->connected()) return;

        // Optimierung: Dynamisches (Intelligentes) Frame Dropping
        // Berechne die reale Zeit seit dem letzten Frame
        uint32_t frameProcessTime = millis() - headerStartMs;
        
        if (camDropFrames > 0) {
            // Modus 1: Sanftes Droppen (nur wenn der ESP hinterherhinkt, z.B. Download > 150ms)
            // Modus 2: Hartes Droppen wie vorher gewünscht, wenn der Wert extrem hoch gesetzt ist.
            if ((camDropFrames == 1 && frameProcessTime > 120) || camDropFrames >= 2) {
                // Wir tun so als hätten wir den Frame verarbeitet, decodieren ihn aber nicht.
                if (camDropFrames >= 2) camDropFrames--; // Zähler abbauen falls hartes Dropping
                lastFrameTime = millis();
                continue; 
            }
        }

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
            setUiStatus("Bild-Sync verloren! (Reset)...");
            return; 
        }

        if (likely(jpeg_decoder_get_info(d_buf, actual_jpeg_size, &pic_info) == ESP_OK)) {
            
            // Optimierung: Float-Werte nur bei Auflösungsänderung neu berechnen
            if (pic_info.width != last_pic_w || pic_info.height != last_pic_h) {
                last_pic_w = pic_info.width;
                last_pic_h = pic_info.height;
                
                float zx = 1280.0f / pic_info.width;
                float zy = 720.0f / pic_info.height;
                cached_zoom_fs = (zx < zy) ? zx : zy;
                cached_zoom_win = min(1.0f, min(1024.0f / pic_info.width, 576.0f / pic_info.height));
            }

            uint8_t next_write = (write_idx + 1) % 3;
            if (unlikely(next_write == read_idx)) next_write = (next_write + 1) % 3;
            
            uint8_t* out_buf = jpg_bufs[next_write];
            dec_cfg.rgb_order = vidFSMode ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;

            esp_err_t dec_res = jpeg_decoder_process(jpeg_engine, &dec_cfg, d_buf, actual_jpeg_size, out_buf, MAX_PIXEL_BUF_SIZE, &out_size_actual);

            if (likely(dec_res == ESP_OK)) {
                write_idx = next_write;
                read_idx = next_write; 

                lastFrameTime = millis();
                fCount++;
                if (unlikely(millis() - lastFpsTime >= 1000)) { 
                    currentFps = fCount; 
                    fCount = 0; 
                    lastFpsTime = millis(); 
                }

                if (vidFSMode) {
                    draw_and_push_fullscreen(read_idx);
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
                }
            } else {
                reset_jpeg_engine();
            }
        }
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

            setUiStatus("Sende Aufloesung...");
            int reqW = 1280, reqH = 720;
            int xIdx = currentCamRes.indexOf('x');
            if(xIdx != -1) {
                reqW = currentCamRes.substring(0, xIdx).toInt();
                reqH = currentCamRes.substring(xIdx+1).toInt();
            }
            
            BabyCamApi_SetResolution(reqW, reqH);
            
            vTaskDelay(pdMS_TO_TICKS(1500)); 

            HTTPClient http;
            http.setReuse(false);
            http.setTimeout(5000); 
            
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