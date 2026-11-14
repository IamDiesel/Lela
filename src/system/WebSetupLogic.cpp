#include "WebSetupLogic.h"
#include "SharedData.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "ViewHomeAssistant.h"
#include "HaConfigLogic.h"
#include "HaWebsocketLogic.h"
#include <M5Unified.h> // NEU: Für den Display-Zugriff
#include "WifiNetworks.h"
#include "WifiConnect.h"
#include "secrets.h"

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
static volatile bool s_stopRequested = false;   // WebSetupLogic_RequestStop()

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

// ------------------------------------------------------------------------------
// Felder mit Rueckfall auf secrets.h
// ------------------------------------------------------------------------------
// Diese Felder zeigen nur, was im Geraet gespeichert ist. Ist nichts
// gespeichert, bleibt das Feld leer und der Platzhalter nennt den Rueckfall.
// Beim Speichern loescht ein leeres Feld den Schluessel - dann gilt wieder
// secrets.h (siehe prefOrSecret in SharedData.cpp). Frueher standen hier die
// wirksamen Werte; jedes Speichern schrieb so die Rueckfallwerte fest ins NVS.
static String storedText(const char* key) {
    return preferences.isKey(key) ? preferences.getString(key, "") : String();
}

static String storedNumber(const char* key) {
    const int v = preferences.isKey(key) ? preferences.getInt(key, 0) : 0;
    return (v > 0) ? String(v) : String();
}

// Ein Eingabefeld mit Beschriftung. `placeholder` ist bereits HTML-sicher.
static String inputField(const char* label, const char* type, const char* name,
                         const String& value, const String& placeholder) {
    String f = "<label>";
    f += label;
    f += "</label><input type='";
    f += type;
    f += "' name='";
    f += name;
    f += "' value='" + htmlEscape(value) + "'";
    if (placeholder.length() > 0) f += " placeholder='" + placeholder + "'";
    f += ">";
    return f;
}

// Auswahlliste. `options` als "Wert|Text;Wert|Text;...", `selected` = Wert.
static String selectField(const char* label, const char* name, const char* options, int selected) {
    String f = "<label>";
    f += label;
    f += "</label><select name='";
    f += name;
    f += "'>";
    String opts(options);
    int start = 0;
    while (start < (int)opts.length()) {
        int end = opts.indexOf(';', start);
        if (end < 0) end = opts.length();
        const String item = opts.substring(start, end);
        start = end + 1;
        const int bar = item.indexOf('|');
        if (bar <= 0) continue;
        const String value = item.substring(0, bar);
        f += "<option value='" + value + "'";
        if (value.toInt() == selected) f += " selected";
        f += ">" + item.substring(bar + 1) + "</option>";
    }
    f += "</select>";
    return f;
}

// Speichert ein Feld mit Rueckfall: leer -> Schluessel entfernen.
static void saveOrClearText(const char* key) {
    if (!server.hasArg(key)) return;
    String v = server.arg(key);
    v.trim();
    if (v.length() > 0) preferences.putString(key, v);
    else if (preferences.isKey(key)) preferences.remove(key);
}

static void saveOrClearNumber(const char* key) {
    if (!server.hasArg(key)) return;
    const int v = server.arg(key).toInt();
    if (v > 0) preferences.putInt(key, v);
    else if (preferences.isKey(key)) preferences.remove(key);
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

        // Ab hier Felder mit Rueckfall auf secrets.h (siehe storedText()).
        // Lesend oeffnen genuegt; der Namensraum existiert nach Data_Init().
        preferences.begin("catmat", true);
        const String fromSecrets = "leer = Standard aus secrets.h";
        html += "<p class='hint'>Leere Felder verwenden den Standard aus secrets.h.</p>";

        html += "<h2>Bluetooth Geräte</h2>";
        html += inputField("Matte MAC (z.B. AA:BB:CC...)", "text", "macM", storedText("macM"), htmlEscape(SECRET_MAC_MAT));
        html += inputField("Kippy MAC", "text", "macK", storedText("macK"), htmlEscape(SECRET_MAC_KIPPY));

        // Kamera: dieselben Felder wie der Dialog "Stream Verbindung" am Geraet.
        // BabyCam: nur die IP - die Stream-URLs baut das Geraet selbst.
        // Fremde Kamera: Video- und Audio-URL gelten, die IP nur fuer die API.
        html += "<h2>Kamera</h2>";
        html += inputField("BabyCam IP-Adresse", "text", "strIp", storedText("strIp"), htmlEscape(SECRET_STREAM_IP));
        html += "<label class='chk'><input type='checkbox' name='useCstUrl' value='1'";
        if (useCustomUrls) html += " checked";
        html += "> Fremde Kamera (eigene URLs statt BabyCam)</label>";
        html += inputField("Video-URL (nur fremde Kamera)", "text", "camEntity", storedText("camEntity"), fromSecrets);

        // Babyphone-Ton: dieselben Felder wie der Dialog am Geraet
        // (AudioSourceDialog). Die Quelle ist unabhaengig von der Videoquelle.
        html += "<h2>Babyphone-Ton</h2>";
        html += selectField("Quelle", "audSrc", "0|BabyCam (UDP);1|HTTP-Stream", audioSource);
        html += "<p class='hint'>BabyCam: rohes PCM per UDP, mit Gegensprechen. Meldet die Kamera eigene Ports, gelten diese.</p>";
        html += inputField("BabyCam: Empfangsport", "number", "audPIn", storedNumber("audPIn"), String(AUDIO_UDP_PORT_IN_DEFAULT));
        html += inputField("BabyCam: Sprechport", "number", "audPOut", storedNumber("audPOut"), String(AUDIO_UDP_PORT_OUT_DEFAULT));
        html += inputField("HTTP: Audio-URL (http://...)", "text", "babyUrl", storedText("babyUrl"), fromSecrets);
        html += selectField("HTTP: Format", "audioFmt", "0|PCM 16 Bit (roh oder WAV);1|AAC (ADTS)", audioFormat);
        html += selectField("HTTP-PCM: Abtastrate", "audRate",
                            "8000|8000 Hz;11025|11025 Hz;16000|16000 Hz;22050|22050 Hz;32000|32000 Hz;44100|44100 Hz;48000|48000 Hz",
                            audioPcmRate);
        html += selectField("HTTP-PCM: Kan&auml;le", "audCh", "1|Mono;2|Stereo", audioPcmChannels);
        html += "<p class='hint'>Abtastrate und Kan&auml;le gelten nur f&uuml;r rohes PCM; ein WAV-Kopf und AAC bringen beides selbst mit.</p>";

        html += "<h2>Home Assistant (Optional)</h2>";
        html += inputField("HA IP", "text", "haIP", storedText("haIP"), htmlEscape(SECRET_HA_IP));
        html += inputField("HA Port", "number", "haPort", storedNumber("haPort"), String(SECRET_HA_PORT));

        html += "<h2>MQTT Broker</h2>";
        html += inputField("Broker IP", "text", "mqIP", storedText("mqIP"), "leer = HA IP");
        html += inputField("Port", "number", "mqPort", storedNumber("mqPort"), String(SECRET_MQTT_PORT));
        html += inputField("Benutzer", "text", "mqUser", storedText("mqUser"), fromSecrets);
        html += "<label>Passwort</label><input type='password' id='mqpw' name='mqPass' value='" + htmlEscape(storedText("mqPass")) + "' placeholder='" + fromSecrets + "'>";
        html += "<label class='chk'><input type='checkbox' onclick=\"showPw(this,'mqpw')\"> Passwort anzeigen</label>";
        preferences.end();

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
        // Felder mit Rueckfall: leer = secrets.h. Die Variablen im RAM werden
        // nicht angefasst - nach dem Speichern folgt ohnehin ein Neustart, und
        // Data_Init() leitet die wirksamen Werte dann einheitlich ab.
        saveOrClearText("macM");
        saveOrClearText("macK");
        saveOrClearText("strIp");
        saveOrClearText("camEntity");
        saveOrClearText("babyUrl");
        saveOrClearText("haIP");
        saveOrClearNumber("haPort");
        saveOrClearText("mqIP");
        saveOrClearNumber("mqPort");
        saveOrClearText("mqUser");
        saveOrClearText("mqPass");
        // Haekchen fehlt im Formular = aus. Nur auswerten, wenn das Formular
        // den Kamera-Abschnitt ueberhaupt enthielt.
        if (server.hasArg("strIp")) preferences.putBool("useCstUrl", server.hasArg("useCstUrl"));
        // Babyphone-Ton. Auswahllisten liefern immer einen gueltigen Wert;
        // Data_Init() prueft beim Neustart trotzdem noch einmal.
        if (server.hasArg("audSrc"))   preferences.putInt("audSrc",   server.arg("audSrc").toInt());
        if (server.hasArg("audioFmt")) preferences.putInt("audioFmt", server.arg("audioFmt").toInt());
        if (server.hasArg("audRate"))  preferences.putInt("audRate",  server.arg("audRate").toInt());
        if (server.hasArg("audCh"))    preferences.putInt("audCh",    server.arg("audCh").toInt());
        saveOrClearNumber("audPIn");
        saveOrClearNumber("audPOut");

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

void WebSetupLogic_RequestStop() {
    pendingWebSetupMode = 0;   // noch nicht gestartetes Setup gar nicht erst starten
    s_stopRequested = true;
}

// Beendet das Setup. Der Webserver selbst stoppt unten im Update, sobald
// auch kein Bildschirmfoto-Modus ihn braucht.
static void endSetup() {
    if (webSetupMode == 0) return;
    // Ersteinrichtung ohne gespeichertes WLAN: Das Setup-WLAN ist der einzige
    // Zugang zum Geraet und bleibt deshalb bestehen.
    if (webSetupMode == 1 && wifiSsid == "") return;
    const int mode = webSetupMode;
    webSetupMode = 0;
    Serial.printf("[Setup] Web-Setup beendet (Modus %d).\n", mode);
    if (mode == 1) {
        // Eigenes Setup-WLAN abbauen und zurueck in den Stationsbetrieb.
        // Die Anmeldung im Heimnetz uebernimmt SystemLogic (wifiStarted = false
        // -> WifiConnect_Begin), wie beim Einschalten des WLANs.
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WifiConnect_Reset();
        wifiStarted = false;
    }
}

void WebSetupLogic_Update() {
#if NET_QUIET_DURING_VIDEO
    // Waehrend eines Videostreams keine Webserver-Bearbeitung (siehe SharedData.h).
    // Das Setup-Portal selbst laeuft ohnehin nur, wenn kein Video aktiv ist.
    if (isStreamActive && webSetupMode == 0) return;
#endif
    // Erst beenden, dann ggf. neu starten (Wechsel Heimnetz <-> Setup-WLAN).
    if (s_stopRequested) {
        s_stopRequested = false;
        endSetup();
    }

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
        
        // Nach 5 Minuten ohne Speichern endet das Setup (ohne Neustart). Ausnahme
        // Ersteinrichtung ohne gespeichertes WLAN: dann ist das Setup-WLAN der
        // einzige Zugang und bleibt bestehen.
        if (webSetupMode > 0 && millis() - webSetupStartTime > 300000) {
            webSetupStartTime = millis();   // Ersteinrichtung: naechstes Fenster
            endSetup();
        }

        // Webserver sauber beenden, wenn Screenshot UND Setup Mode inaktiv sind
        if (webSetupMode == 0 && pendingScreenshotMode == 0) {
            server.stop();
            serverRunning = false;
        }
    }
}