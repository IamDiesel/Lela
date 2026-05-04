#include "WebSetupLogic.h"
#include "SharedData.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "ViewHomeAssistant.h"
#include <M5Unified.h> // NEU: Für den Display-Zugriff

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

// Globale Variable für den chunk-weisen Datei-Upload
static File fsUploadFile;
static bool serverRunning = false; // Speichert den Status des Webservers

void WebSetupLogic_Init() {
    // --- HTML AUSLIEFERN (Setup) ---
    server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Lela OS Setup</title>";
        html += "<style>";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #ffffff; padding: 20px; max-width: 600px; margin: 0 auto; }";
        html += "h1 { color: #00A0FF; text-align: center; margin-bottom: 30px; }";
        html += "h2 { border-bottom: 1px solid #333; padding-bottom: 5px; color: #FF8800; margin-top: 30px; }";
        html += "label { display: block; margin-top: 15px; margin-bottom: 5px; font-size: 14px; color: #aaa; }";
        html += "input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 12px; border: none; border-radius: 6px; background-color: #2a2a2a; color: #fff; font-size: 16px; box-sizing: border-box; }";
        html += "input[type='submit'] { width: 100%; padding: 15px; margin-top: 40px; border: none; border-radius: 6px; background-color: #00A0FF; color: white; font-size: 18px; font-weight: bold; cursor: pointer; }";
        html += "input[type='submit']:hover { background-color: #0088CC; }";
        html += ".btn-export { display:block; text-align:center; padding:15px; background-color:#2980B9; color:white; text-decoration:none; border-radius:6px; margin-bottom:15px; font-weight:bold; }";
        html += ".btn-import { background-color:#8E44AD !important; margin-top:10px !important; }";
        html += ".import-box { background-color:#222; padding:15px; border-radius:6px; margin-bottom: 30px; }";
        html += "</style></head><body>";
        
        html += "<h1>Lela OS Setup</h1>";

        html += "<h2>Dashboard Layout Backup</h2>";
        html += "<p style='font-size: 14px; color: #aaa;'>Sichere dein HA-Dashboard oder lade ein altes Layout (JSON) auf das Geraet hoch.</p>";
        html += "<a href='/export_ha' class='btn-export'>Backup herunterladen (.json)</a>";
        
        html += "<div class='import-box'>";
        html += "<form method='POST' action='/import_ha' enctype='multipart/form-data'>";
        html += "<label style='margin-top:0;'>Backup hochladen:</label>";
        html += "<input type='file' name='backup' accept='.json' style='margin-bottom:5px; width: 100%; color: #fff;'>";
        html += "<input type='submit' value='Layout wiederherstellen' class='btn-import'>";
        html += "</form></div>";

        html += "<form action='/save' method='POST'>";
        html += "<h2>WLAN Einstellungen</h2>";
        html += "<label>SSID</label><input type='text' name='wifiSsid' value='" + wifiSsid + "'>";
        html += "<label>Passwort</label><input type='password' name='wifiPass' value='" + wifiPass + "'>";

        html += "<h2>Bluetooth Geräte</h2>";
        html += "<label>Matte MAC (z.B. AA:BB:CC...)</label><input type='text' name='macM' value='" + savedMatMac + "'>";
        html += "<label>Kippy MAC</label><input type='text' name='macK' value='" + savedKippyMac + "'>";

        html += "<h2>Streams (Video & Audio)</h2>";
        html += "<label>Video Stream URL (oder HA Pfad)</label><input type='text' name='camEntity' value='" + camEntity + "'>";
        html += "<label>Audio Stream URL</label><input type='text' name='babyUrl' value='" + babyStreamUrl + "'>";

        html += "<h2>Home Assistant (Optional)</h2>";
        html += "<label>HA IP</label><input type='text' name='haIP' value='" + haIP + "'>";
        html += "<label>HA Port</label><input type='number' name='haPort' value='" + String(haPort) + "'>";

        html += "<h2>MQTT Broker</h2>";
        html += "<label>Broker IP</label><input type='text' name='mqIP' value='" + mqttBroker + "'>";
        html += "<label>Port</label><input type='number' name='mqPort' value='" + String(mqttPort) + "'>";
        html += "<label>Benutzer</label><input type='text' name='mqUser' value='" + mqttUser + "'>";
        html += "<label>Passwort</label><input type='password' name='mqPass' value='" + mqttPass + "'>";

        html += "<input type='submit' value='Speichern & Neustart'>";
        html += "</form></body></html>";

        server.send(200, "text/html", html);
    });

    // =========================================================
    // NEU: SCREENSHOT FRONTEND (HTML)
    // =========================================================
    server.on("/screenshot", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Lela OS Screenshot</title>";
        html += "<style>";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #ffffff; padding: 20px; max-width: 600px; margin: 0 auto; text-align: center; }";
        html += "h1 { color: #00A0FF; margin-bottom: 30px; }";
        html += ".btn { display:block; text-align:center; padding:15px; border-radius:6px; margin-bottom:15px; font-weight:bold; text-decoration:none; border:none; width:100%; box-sizing:border-box; font-size:18px; cursor:pointer; }";
        html += ".btn-capture { background-color:#27AE60; color:white; }";
        html += ".btn-capture:hover { background-color:#219150; }";
        html += ".btn-exit { background-color:#AA0000; color:white; }";
        html += ".btn-exit:hover { background-color:#880000; }";
        html += "</style>";
        html += "<script>";
        html += "function exitScreenshot() { fetch('/exit_screenshot', {method: 'POST'}).then(() => { document.body.innerHTML = '<h2>Screenshot-Modus beendet.</h2><p style=\"color:#aaa;\">Du kannst dieses Fenster nun schliessen.</p>'; }); }";
        html += "</script>";
        html += "</head><body>";
        html += "<h1>Lela OS Screenshot</h1>";
        html += "<p style='color: #aaa; margin-bottom: 30px;'>Klicke auf den Button, um das aktuelle Bild vom Display herunterzuladen. (Achtung: Dies kann ca. 1-2 Sekunden dauern).</p>";
        html += "<a href='/capture' class='btn btn-capture'>Bild aufnehmen & laden</a>";
        html += "<button onclick='exitScreenshot()' class='btn btn-exit'>Screenshot-Modus Beenden</button>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    });

    // =========================================================
    // NEU: SCREENSHOT BACKEND (STREAMING)
    // =========================================================
    server.on("/capture", HTTP_GET, []() {
        uint32_t width = 1280;
        uint32_t height = 720;
        uint32_t rowSize = width * 3; // 3840 Bytes pro Zeile (ist durch 4 teilbar, erfordert kein BMP-Padding!)
        uint32_t imageSize = rowSize * height;
        uint32_t fileSize = 54 + imageSize;

        // Standard BMP Header
        uint8_t header[54] = {
            'B', 'M',
            (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
            0, 0, 0, 0,
            54, 0, 0, 0,
            40, 0, 0, 0,
            (uint8_t)(width), (uint8_t)(width >> 8), (uint8_t)(width >> 16), (uint8_t)(width >> 24),
            (uint8_t)(height), (uint8_t)(height >> 8), (uint8_t)(height >> 16), (uint8_t)(height >> 24),
            1, 0,
            24, 0,
            0, 0, 0, 0,
            (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0
        };

        server.setContentLength(fileSize);
        server.sendHeader("Content-Disposition", "attachment; filename=\"lela_screenshot.bmp\"");
        server.send(200, "image/bmp", "");

        server.client().write(header, 54);

        // Wir reservieren RAM nur für eine EINZIGE Pixelzeile! (Verhindert Abstürze durch Out-of-Memory)
        uint8_t* rowBuffer = (uint8_t*)malloc(rowSize);
        if (!rowBuffer) return;

        // BMPs werden von unten nach oben (Bottom-Up) gespeichert
        for (int y = height - 1; y >= 0; y--) {
            lvgl_port_lock(portMAX_DELAY); // Grafik-Sperre für sauberes Bild
            M5.Display.readRectRGB(0, y, width, 1, rowBuffer);
            lvgl_port_unlock();

            // M5 liefert RGB, BMP verlangt BGR -> Rot und Blau tauschen
            for (int i = 0; i < width; i++) {
                uint8_t r = rowBuffer[i*3];
                rowBuffer[i*3] = rowBuffer[i*3 + 2];
                rowBuffer[i*3 + 2] = r;
            }
            server.client().write(rowBuffer, rowSize);
        }
        free(rowBuffer);
    });

    // =========================================================
    // NEU: EXIT SCREENSHOT ROUTE
    // =========================================================
    server.on("/exit_screenshot", HTTP_POST, []() {
        pendingScreenshotMode = 2; // Signalisiert dem Frontend, das Overlay zu schließen
        server.send(200, "text/plain", "OK");
    });

    // --- IMPORT / EXPORT ROUTEN ---
    server.on("/export_ha", HTTP_GET, []() {
        if (LittleFS.exists("/ha_config.json")) {
            File file = LittleFS.open("/ha_config.json", "r");
            server.sendHeader("Content-Type", "application/json");
            server.sendHeader("Content-Disposition", "attachment; filename=\"lela_ha_backup.json\"");
            server.streamFile(file, "application/json");
            file.close();
        } else {
            server.send(404, "text/plain", "Fehler: Noch kein Dashboard-Layout auf dem Geraet gespeichert.");
        }
    });

    server.on("/import_ha", HTTP_POST, []() {
        String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif; background:#121212; color:#00A0FF; text-align:center; padding-top:50px;} a{color:#fff; text-decoration:none; padding:10px 20px; background:#2980B9; border-radius:5px; display:inline-block; margin-top:20px;}</style></head><body><h2>Backup erfolgreich geladen!</h2><p style='color:#fff;'>Das Dashboard aktualisiert sich jetzt live auf dem Display.</p><a href='/'>Zurueck zum Setup</a></body></html>";
        server.send(200, "text/html", html);
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            fsUploadFile = LittleFS.open("/ha_config.json", "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (fsUploadFile) {
                fsUploadFile.close();
                ViewHomeAssistant::pendingHaReload = true; 
            }
        }
    });

    // --- FORMULAR SPEICHERN ---
    server.on("/save", HTTP_POST, []() {
        preferences.begin("catmat", false);
        if(server.hasArg("wifiSsid")) { wifiSsid = server.arg("wifiSsid"); preferences.putString("wifiSsid", wifiSsid); }
        if(server.hasArg("wifiPass")) { wifiPass = server.arg("wifiPass"); preferences.putString("wifiPass", wifiPass); }
        if(server.hasArg("macM")) { savedMatMac = server.arg("macM"); preferences.putString("macM", savedMatMac); }
        if(server.hasArg("macK")) { savedKippyMac = server.arg("macK"); preferences.putString("macK", savedKippyMac); }
        if(server.hasArg("babyUrl")) { babyStreamUrl = server.arg("babyUrl"); preferences.putString("babyUrl", babyStreamUrl); }
        if(server.hasArg("camEntity")) { camEntity = server.arg("camEntity"); preferences.putString("camEntity", camEntity); }
        if(server.hasArg("haIP")) { haIP = server.arg("haIP"); preferences.putString("haIP", haIP); }
        if(server.hasArg("haPort")) { haPort = server.arg("haPort").toInt(); preferences.putInt("haPort", haPort); }
        if(server.hasArg("mqIP")) { mqttBroker = server.arg("mqIP"); preferences.putString("mqIP", mqttBroker); }
        if(server.hasArg("mqPort")) { mqttPort = server.arg("mqPort").toInt(); preferences.putInt("mqPort", mqttPort); }
        if(server.hasArg("mqUser")) { mqttUser = server.arg("mqUser"); preferences.putString("mqUser", mqttUser); }
        if(server.hasArg("mqPass")) { mqttPass = server.arg("mqPass"); preferences.putString("mqPass", mqttPass); }
        preferences.end();

        String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif; background:#121212; color:#00A0FF; text-align:center; padding-top:50px;}</style></head><body><h2>Einstellungen gespeichert!</h2><p style='color:#fff;'>Das Lela OS startet in wenigen Sekunden neu...</p></body></html>";
        server.send(200, "text/html", html);

        delay(2000);
        ESP.restart();
    });

    server.onNotFound([]() {
        if (webSetupMode == 1) {
            server.sendHeader("Location", "http://192.168.4.1/", true);
            server.send(302, "text/plain", "");
        } else {
            server.send(404, "text/plain", "404: Not Found");
        }
    });
}

void WebSetupLogic_Update() {
    if (pendingWebSetupMode > 0) {
        webSetupMode = pendingWebSetupMode;
        pendingWebSetupMode = 0;
        webSetupStartTime = millis();

        WiFi.setSleep(false); 

        if (webSetupMode == 1) {
            WiFi.disconnect(true, true);
            delay(100);
            
            WiFi.mode(WIFI_AP);
            delay(100);

            IPAddress apIP(192, 168, 4, 1);
            IPAddress netMsk(255, 255, 255, 0);
            WiFi.softAPConfig(apIP, apIP, netMsk);

            WiFi.softAP("Lela-Setup", apPassword.c_str());

            dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
            dnsServer.start(53, "*", apIP);
            
            server.begin();
            serverRunning = true;
        } else if (webSetupMode == 2) {
            server.begin();
            serverRunning = true;
        }
    }

    // --- NEU: Webserver intelligent starten / stoppen ---
    if (pendingScreenshotMode == 1 && !serverRunning) {
        server.begin();
        serverRunning = true;
    }

    if (serverRunning) {
        server.handleClient();
        if (webSetupMode == 1) dnsServer.processNextRequest();
        
        // Timeout nach 5 Minuten im Setup Mode
        if (webSetupMode > 0 && millis() - webSetupStartTime > 300000) ESP.restart();

        // Webserver sauber beenden, wenn Screenshot UND Setup Mode inaktiv sind
        if (webSetupMode == 0 && pendingScreenshotMode == 0) {
            server.stop();
            serverRunning = false;
        }
    }
}