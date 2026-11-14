#include "WebSetupLogic.h"
#include "SharedData.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "ViewHomeAssistant.h"
#include "HaConfigLogic.h"
#include "HaWebsocketLogic.h"
#include <M5Unified.h> // NEU: Für den Display-Zugriff
#include "WifiNetworks.h"

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

// Globale Variable für den chunk-weisen Datei-Upload
static File fsUploadFile;

// Ergebnis des letzten Layout-Uploads (/import_ha). Der Upload-Handler setzt
// es, der Antwort-Handler meldet es danach an den Browser.
static bool   haImportOk      = false;
static bool   haImportTooBig  = false;
static String haImportMessage = "";
static bool serverRunning = false; // Speichert den Status des Webservers

// Wert fuer ein HTML-Attribut (value='...', data-...='...'). Ohne das
// zerbrach z. B. ein Passwort mit ' oder < das Formular.
static String htmlEscape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        const char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

// Feld fuer die feste IP: gespeicherter Wert, sonst (falls verbunden) der
// aktuelle - dann genuegt meist das Haekchen.
static String ipFieldValue(const String& saved, IPAddress current) {
    if (saved.length() > 0) return saved;
    if (WiFi.status() == WL_CONNECTED && current != IPAddress((uint32_t)0)) return current.toString();
    return "";
}

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
        html += "select { width: 100%; padding: 12px; border: none; border-radius: 6px; background-color: #2a2a2a; color: #fff; font-size: 16px; }";
        html += ".chk { display:flex; align-items:center; gap:8px; color:#ddd; margin-top:10px; }";
        html += ".chk input { width:20px; height:20px; }";
        html += ".btn-small { margin-top:8px; padding:8px 12px; border:none; border-radius:6px; background:#555; color:#fff; font-size:14px; cursor:pointer; }";
        html += ".hint { font-size:13px; color:#888; margin-top:6px; }";
        html += "</style>";
        // Kleine Helfer (kein externes Skript - das Portal laeuft auch ohne Internet).
        html += "<script>";
        html += "function pickNet(s){var o=s.options[s.selectedIndex];if(!o.dataset.s)return;";
        html += "document.getElementById('ssid').value=o.dataset.s;document.getElementById('pw').value=o.dataset.p;}";
        html += "function showPw(c,id){document.getElementById(id).type=c.checked?'text':'password';}";
        html += "function forgetNet(){var s=document.getElementById('known');var o=s.options[s.selectedIndex];if(!o.dataset.s)return;";
        html += "document.getElementById('forgetSsid').value=o.dataset.s;document.getElementById('forgetForm').submit();}";
        html += "function ipToggle(){document.getElementById('ipBox').style.display=document.getElementById('ipFix').checked?'block':'none';}";
        html += "</script></head><body>";
        
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

        // Eigenes Formular zum Vergessen eines Netzes (Formulare duerfen nicht
        // verschachtelt werden; forgetNet() fuellt und sendet es).
        html += "<form id='forgetForm' method='POST' action='/wifi_forget'><input type='hidden' name='ssid' id='forgetSsid'></form>";

        html += "<form action='/save' method='POST'>";
        html += "<h2>WLAN Einstellungen</h2>";

        const std::vector<WifiNetwork> known = WifiNetworks_List();
        if (!known.empty()) {
            html += "<label>Gespeicherte Netze</label><select id='known' onchange='pickNet(this)'>";
            html += "<option value=''>&ndash; neues Netz eingeben &ndash;</option>";
            for (size_t i = 0; i < known.size(); i++) {
                const String s = htmlEscape(known[i].ssid);
                html += "<option value='" + String((unsigned)i) + "' data-s='" + s + "' data-p='" + htmlEscape(known[i].pass) + "'";
                if (known[i].ssid == wifiSsid) html += " selected";
                html += ">" + s + "</option>";
            }
            html += "</select>";
            html += "<button type='button' class='btn-small' onclick='forgetNet()'>Ausgew&auml;hltes Netz vergessen</button>";
            html += "<p class='hint'>Netze werden nach einer erfolgreichen Verbindung gemerkt (mit Passwort).</p>";
        }

        html += "<label>SSID</label><input type='text' id='ssid' name='wifiSsid' value='" + htmlEscape(wifiSsid) + "'>";
        html += "<label>Passwort</label><input type='password' id='pw' name='wifiPass' value='" + htmlEscape(wifiPass) + "'>";
        html += "<label class='chk'><input type='checkbox' onclick=\"showPw(this,'pw')\"> Passwort anzeigen</label>";

        html += "<h2>Netzwerk (IP-Adresse)</h2>";
        html += "<label class='chk'><input type='checkbox' id='ipFix' name='ipFix' value='1' onclick='ipToggle()'";
        if (staticIpEnabled) html += " checked";
        html += "> Feste IP-Adresse statt DHCP</label>";
        html += "<p class='hint'>Startet ca. 2 s schneller. Die Adresse vorher im Router fest f&uuml;r dieses Ger&auml;t reservieren.</p>";
        html += "<div id='ipBox' style='display:" + String(staticIpEnabled ? "block" : "none") + ";'>";
        html += "<label>IP-Adresse</label><input type='text' name='ipAddr' placeholder='z. B. 192.168.2.125' value='" + htmlEscape(ipFieldValue(staticIp, WiFi.localIP())) + "'>";
        html += "<label>Gateway (Router)</label><input type='text' name='ipGw' placeholder='z. B. 192.168.2.1' value='" + htmlEscape(ipFieldValue(staticGateway, WiFi.gatewayIP())) + "'>";
        html += "<label>Subnetzmaske</label><input type='text' name='ipMask' placeholder='255.255.255.0' value='" + htmlEscape(ipFieldValue(staticSubnet, WiFi.subnetMask())) + "'>";
        html += "<label>DNS (leer = Gateway)</label><input type='text' name='ipDns' value='" + htmlEscape(staticDns) + "'>";
        html += "</div>";

        html += "<h2>Bluetooth Geräte</h2>";
        html += "<label>Matte MAC (z.B. AA:BB:CC...)</label><input type='text' name='macM' value='" + htmlEscape(savedMatMac) + "'>";
        html += "<label>Kippy MAC</label><input type='text' name='macK' value='" + htmlEscape(savedKippyMac) + "'>";

        html += "<h2>Streams (Video & Audio)</h2>";
        html += "<label>Video Stream URL (oder HA Pfad)</label><input type='text' name='camEntity' value='" + htmlEscape(camEntity) + "'>";
        html += "<label>Audio Stream URL</label><input type='text' name='babyUrl' value='" + htmlEscape(babyStreamUrl) + "'>";

        html += "<h2>Home Assistant (Optional)</h2>";
        html += "<label>HA IP</label><input type='text' name='haIP' value='" + htmlEscape(haIP) + "'>";
        html += "<label>HA Port</label><input type='number' name='haPort' value='" + String(haPort) + "'>";

        html += "<h2>MQTT Broker</h2>";
        html += "<label>Broker IP</label><input type='text' name='mqIP' value='" + htmlEscape(mqttBroker) + "'>";
        html += "<label>Port</label><input type='number' name='mqPort' value='" + String(mqttPort) + "'>";
        html += "<label>Benutzer</label><input type='text' name='mqUser' value='" + htmlEscape(mqttUser) + "'>";
        html += "<label>Passwort</label><input type='password' name='mqPass' value='" + htmlEscape(mqttPass) + "'>";

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

    // Upload einer Layout-Sicherung.
    // Ablauf: Datei in eine Zwischenablage schreiben -> vollstaendig pruefen ->
    // erst dann Anzeige und Live-Datei ersetzen (HaConfigLogic::ImportFromFile).
    // Frueher wurde direkt in die Live-Datei geschrieben, ohne Pruefung, und die
    // Anzeige lud sie nie - das naechste Speichern schrieb den alten Stand zurueck.
    // Webserver und GUI laufen beide im loopTask; der Austausch von
    // 'dashboards' hier ist damit nicht nebenlaeufig zur Oberflaeche.
    server.on("/import_ha", HTTP_POST, []() {
        const bool ok = haImportOk;
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif; background:#121212; color:#00A0FF; text-align:center; padding-top:50px;} h2.err{color:#E74C3C;} a{color:#fff; text-decoration:none; padding:10px 20px; background:#2980B9; border-radius:5px; display:inline-block; margin-top:20px;}</style></head><body>";
        if (ok) {
            html += "<h2>Backup erfolgreich geladen!</h2><p style='color:#fff;'>Das Dashboard aktualisiert sich jetzt auf dem Display.</p>";
        } else {
            html += "<h2 class='err'>Backup nicht uebernommen</h2><p style='color:#fff;'>" + haImportMessage + "</p><p style='color:#aaa;'>Das bisherige Layout bleibt unveraendert.</p>";
        }
        html += "<a href='/'>Zurueck zum Setup</a></body></html>";
        server.send(ok ? 200 : 400, "text/html", html);
    }, []() {
        HTTPUpload& upload = server.upload();

        if (upload.status == UPLOAD_FILE_START) {
            haImportOk = false;
            haImportTooBig = false;
            haImportMessage = "Keine Datei empfangen.";
            if (fsUploadFile) fsUploadFile.close();
            fsUploadFile = LittleFS.open(HaConfigLogic::UPLOAD_PATH, "w");
            if (!fsUploadFile) haImportMessage = "Datei konnte auf dem Geraet nicht angelegt werden.";

        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (!fsUploadFile || haImportTooBig) return;
            // Groessengrenze schon waehrend des Empfangs pruefen, damit eine
            // falsche (grosse) Datei den Flash nicht volllaufen laesst.
            if (upload.totalSize > HaConfigLogic::MAX_FILE_BYTES) {
                haImportTooBig = true;
                fsUploadFile.close();
                LittleFS.remove(HaConfigLogic::UPLOAD_PATH);
                return;
            }
            if (fsUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
                haImportMessage = "Schreibfehler auf dem Geraet (Speicher voll?).";
                fsUploadFile.close();
                LittleFS.remove(HaConfigLogic::UPLOAD_PATH);
            }

        } else if (upload.status == UPLOAD_FILE_END) {
            if (haImportTooBig) {
                haImportMessage = "Datei ist zu gross.";
                return;
            }
            if (!fsUploadFile) return;   // Fehler ist bereits in haImportMessage
            fsUploadFile.close();

            String err;
            haImportOk = HaConfigLogic::ImportFromFile(HaConfigLogic::UPLOAD_PATH, err);
            if (haImportOk) {
                Serial.println("[WebSetup] Layout-Backup uebernommen.");
                HaWebsocketLogic_UpdateTrackedEntities();
                // Neuaufbau, falls der HA-Screen gerade offen ist. Sonst baut
                // ihn der naechste Aufruf von build() ohnehin aus dem neuen Stand.
                ViewHomeAssistant::pendingHaReload = true;
            } else {
                Serial.printf("[WebSetup] Layout-Backup abgelehnt: %s\n", err.c_str());
                haImportMessage = err;
            }

        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            // Verbindung abgebrochen: halbe Datei verwerfen, nichts uebernehmen.
            if (fsUploadFile) fsUploadFile.close();
            LittleFS.remove(HaConfigLogic::UPLOAD_PATH);
            haImportOk = false;
            haImportMessage = "Upload abgebrochen.";
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

        // Feste IP: Haekchen fehlt im Formular = aus. Nur gueltige Angaben
        // einschalten - sonst bliebe das Geraet nach dem Neustart ohne Netz.
        String ipNote = "";
        if (server.hasArg("ipAddr")) {
            staticIp = server.arg("ipAddr");           staticIp.trim();
            staticGateway = server.arg("ipGw");        staticGateway.trim();
            staticSubnet = server.arg("ipMask");       staticSubnet.trim();
            staticDns = server.arg("ipDns");           staticDns.trim();
            staticIpEnabled = server.hasArg("ipFix");
            if (staticIpEnabled) {
                IPAddress ip, gw, mask, dns;
                const bool ok = ip.fromString(staticIp) && gw.fromString(staticGateway) &&
                                mask.fromString(staticSubnet) && mask[0] == 255 &&
                                (staticDns.length() == 0 || dns.fromString(staticDns));
                if (!ok) {
                    staticIpEnabled = false;
                    ipNote = "<p style='color:#E67E22;'>Feste IP unvollst&auml;ndig oder ung&uuml;ltig &ndash; es bleibt bei DHCP.</p>";
                }
            }
            preferences.putBool("ipFix", staticIpEnabled);
            preferences.putString("ipAddr", staticIp);
            preferences.putString("ipGw", staticGateway);
            preferences.putString("ipMask", staticSubnet);
            preferences.putString("ipDns", staticDns);
        }
        preferences.end();

        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif; background:#121212; color:#00A0FF; text-align:center; padding-top:50px;}</style></head><body><h2>Einstellungen gespeichert!</h2>" + ipNote + "<p style='color:#fff;'>Das Lela OS startet in wenigen Sekunden neu...</p></body></html>";
        server.send(200, "text/html", html);

        delay(2000);
        ESP.restart();
    });

    // --- GEMERKTES WLAN VERGESSEN (Auswahlliste im Setup) ---
    server.on("/wifi_forget", HTTP_POST, []() {
        if (server.hasArg("ssid")) WifiNetworks_Forget(server.arg("ssid"));
        server.sendHeader("Location", "/", true);
        server.send(303, "text/plain", "");
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
#if NET_QUIET_DURING_VIDEO
    // Waehrend eines Videostreams keine Webserver-Bearbeitung (siehe SharedData.h).
    // Das Setup-Portal selbst laeuft ohnehin nur, wenn kein Video aktiv ist.
    if (isStreamActive && webSetupMode == 0) return;
#endif
    if (pendingWebSetupMode > 0) {
        webSetupMode = pendingWebSetupMode;
        pendingWebSetupMode = 0;
        webSetupStartTime = millis();

        WiFi.setSleep(false); 

        if (webSetupMode == 1) {
            WiFi.disconnect(false, false);
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