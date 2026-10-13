#include "HaConfigLogic.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <algorithm> 
#include "lvgl.h" 
#include <esp_heap_caps.h>
#include "HaPsramAllocator.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


std::vector<HADashboardDef> HaConfigLogic::dashboards;

// ------------------------------------------------------------------------------
// Dateien und Grenzen
// ------------------------------------------------------------------------------
static const char* const CONFIG_PATH     = "/ha_config.json";
static const char* const CONFIG_TMP_PATH = "/ha_config.tmp";   // Zwischenstand beim Speichern
static const char* const CONFIG_BAD_PATH = "/ha_config.bad";   // beiseitegelegte defekte Datei

// Maximale Verschachtelungstiefe beim Parsen.
// Der ArduinoJson-Standard (10) reichte nicht: Jede Ordnerebene kostet zwei
// Stufen (children-Array + Kind-Objekt), Bedingungen zwei weitere. Schon ein
// Enkel mit Bedingung brauchte 11 - die Datei liess sich dann nicht mehr laden.
// 20 erlaubt sechs Ordnerebenen mit Bedingungen und haelt die Rekursion in
// Parser und deserializeWidget() klein genug fuer den 8-KB-Preload-Task.
static const uint8_t CONFIG_NESTING_LIMIT = 20;

// Rekursiver Mutex: Save() wird auch aus Load() und ImportFromFile() heraus
// aufgerufen, waehrend der Mutex bereits gehalten wird.
static SemaphoreHandle_t configMutex = NULL;

static bool lockConfig() {
    if (configMutex == NULL) configMutex = xSemaphoreCreateRecursiveMutex();
    return configMutex != NULL && xSemaphoreTakeRecursive(configMutex, portMAX_DELAY) == pdTRUE;
}

static void unlockConfig() {
    xSemaphoreGiveRecursive(configMutex);
}

// ------------------------------------------------------------------------------
// Dateisystem einbinden
// ------------------------------------------------------------------------------
// true, solange LittleFS eingebunden ist. Ohne Dateisystem laeuft die
// Oberflaeche mit dem Standard-Tab im RAM weiter, schreibt aber nichts.
static bool s_fsAvailable = false;

// Merker im NVS: Wurde LittleFS auf diesem Geraet schon einmal erfolgreich
// eingebunden? Nur wenn NICHT, darf formatiert werden.
static const char* FS_PREF_NAMESPACE = "lelafs";
static const char* FS_PREF_KEY       = "mounted";

static bool fsWasMountedBefore() {
    Preferences prefs;
    if (!prefs.begin(FS_PREF_NAMESPACE, true)) return false;   // Namespace fehlt = nie eingebunden
    const bool v = prefs.getBool(FS_PREF_KEY, false);
    prefs.end();
    return v;
}

static void rememberFsMounted() {
    Preferences prefs;
    if (!prefs.begin(FS_PREF_NAMESPACE, false)) return;
    if (!prefs.getBool(FS_PREF_KEY, false)) prefs.putBool(FS_PREF_KEY, true);
    prefs.end();
}

// Bindet LittleFS ein, OHNE bei einem Fehler still zu formatieren.
// Frueher: LittleFS.begin(true) - ein einziger Lesefehler beim Start
// formatierte die Partition, und das Dashboard-Layout war weg.
// Jetzt:
//   1. Einbinden ohne Formatieren, bei Fehler einmal wiederholen.
//   2. Formatieren nur, wenn das Dateisystem auf diesem Geraet noch nie
//      eingebunden war (Erstinbetriebnahme, Partition leer).
//   3. Sonst: ohne Dateisystem weiterlaufen, nichts schreiben, laut melden.
static bool mountFileSystem() {
    if (LittleFS.begin(false)) {
        rememberFsMounted();
        return true;
    }
    delay(200);
    if (LittleFS.begin(false)) {
        rememberFsMounted();
        return true;
    }

    if (!fsWasMountedBefore()) {
        Serial.println("[HaConfig] Erstinbetriebnahme: Dateisystem wird formatiert.");
        if (LittleFS.begin(true)) {
            rememberFsMounted();
            return true;
        }
    }

    Serial.println("[HaConfig] FEHLER: Dateisystem nicht lesbar. Es wird NICHT formatiert -");
    Serial.println("[HaConfig] die gespeicherte Konfiguration bleibt erhalten, Aenderungen werden aber nicht gespeichert.");
    return false;
}

void HaConfigLogic::Init() {
    lockConfig();   // legt den Mutex an, bevor ein zweiter Task ihn braucht
    unlockConfig();

    s_fsAvailable = mountFileSystem();
    if (!s_fsAvailable) {
        Load();   // Standard-Tab im RAM
        return;
    }

    // Reste eines unterbrochenen Speicherns aufraeumen.
    // Fehlt die Live-Datei, die temporaere aber ist da, ist Save() genau
    // zwischen Loeschen und Umbenennen unterbrochen worden. Die temporaere
    // Datei wurde vorher vollstaendig geschrieben und geprueft - sie ist der
    // juengste gueltige Stand.
    if (LittleFS.exists(CONFIG_TMP_PATH)) {
        if (!LittleFS.exists(CONFIG_PATH)) {
            Serial.println("[HaConfig] Unterbrochenes Speichern erkannt, stelle letzten Stand wieder her.");
            LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH);
        } else {
            LittleFS.remove(CONFIG_TMP_PATH);
        }
    }
    if (LittleFS.exists(UPLOAD_PATH)) LittleFS.remove(UPLOAD_PATH);

    Load(); 
}

// --- Rekursiver Helfer zum Deserialisieren (Laden) von Widgets ---
static void deserializeWidget(JsonObject wObj, HAWidgetDef& wDef) {
    if (!wObj["entity"].isNull()) wDef.entity_id = wObj["entity"].as<String>();
    if (!wObj["type"].isNull()) wDef.type = wObj["type"].as<String>();
    
    wDef.x = wObj["x"] | 20;
    wDef.y = wObj["y"] | 20;
    wDef.w = wObj["w"] | 160;
    wDef.h = wObj["h"] | 105;
    
    wDef.name = wObj["name"] | "";
    wDef.mdi_icon = wObj["mdi_icon"] | "";
    wDef.color_on = wObj["color_on"] | "";
    wDef.color_off = wObj["color_off"] | "";
    
    wDef.icon_align = wObj["icon_align"] | LV_ALIGN_TOP_MID;
    wDef.text_align = wObj["text_align"] | LV_ALIGN_BOTTOM_MID;
    wDef.state_align = wObj["state_align"] | LV_ALIGN_CENTER;
    
    wDef.icon_margin = !wObj["icon_margin"].isNull() ? wObj["icon_margin"].as<int>() : 5;
    wDef.text_margin = !wObj["text_margin"].isNull() ? wObj["text_margin"].as<int>() : 5;
    wDef.state_margin = wObj["state_margin"] | 0;
    
    wDef.snap_to_grid = !wObj["snap_to_grid"].isNull() ? wObj["snap_to_grid"].as<bool>() : true;
    
    wDef.show_chart = wObj["show_chart"] | false;
    wDef.chart_w_pct = wObj["chart_w_pct"] | 95;
    wDef.chart_h_pct = wObj["chart_h_pct"] | 50;
    wDef.chart_x_ofs = wObj["chart_x_ofs"] | 0;
    wDef.chart_y_ofs = wObj["chart_y_ofs"] | -15;
    wDef.chart_min = wObj["chart_min"] | "";
    wDef.chart_max = wObj["chart_max"] | "";
    
    wDef.media_content_type = wObj["media_content_type"] | "";
    wDef.media_content_id = wObj["media_content_id"] | "";

    wDef.tap_action_domain = wObj["ta_dom"] | "";
    wDef.tap_action_service = wObj["ta_srv"] | "";
    wDef.tap_action_target = wObj["ta_tgt"] | "";

    wDef.select_options = wObj["select_options"] | "";
    wDef.slider_min = wObj["slider_min"] | 0.0f;
    wDef.slider_max = wObj["slider_max"] | 100.0f;
    wDef.slider_step = wObj["slider_step"] | 1.0f;

    wDef.conditions_type = wObj["conditions_type"] | "AND";
    if (wObj["conditions"].is<JsonArray>()) {
        for (JsonObject j_c : wObj["conditions"].as<JsonArray>()) {
            HACondition c;
            c.entity = j_c["entity"] | "";
            c.op = j_c["op"] | "==";
            c.value = j_c["value"] | "";
            wDef.conditions.push_back(c);
        }
    }

    if (wObj["children"].is<JsonArray>()) {
        for (JsonObject j_child : wObj["children"].as<JsonArray>()) {
            HAWidgetDef childDef;
            deserializeWidget(j_child, childDef);
            wDef.children.push_back(childDef);
        }
    }
}

// --- Rekursiver Helfer zum Serialisieren (Speichern) von Widgets ---
static void serializeWidget(JsonObject wObj, const HAWidgetDef& w) {
    wObj["entity"] = w.entity_id;
    wObj["type"] = w.type;
    wObj["x"] = w.x;
    wObj["y"] = w.y;
    wObj["w"] = w.w;
    wObj["h"] = w.h;
    
    wObj["icon_align"] = w.icon_align;
    wObj["text_align"] = w.text_align;
    wObj["state_align"] = w.state_align;
    wObj["icon_margin"] = w.icon_margin;
    wObj["text_margin"] = w.text_margin;
    wObj["state_margin"] = w.state_margin;
    wObj["snap_to_grid"] = w.snap_to_grid;
    
    wObj["show_chart"] = w.show_chart;
    wObj["chart_w_pct"] = w.chart_w_pct;
    wObj["chart_h_pct"] = w.chart_h_pct;
    wObj["chart_x_ofs"] = w.chart_x_ofs;
    wObj["chart_y_ofs"] = w.chart_y_ofs;
    if (w.chart_min.length() > 0) wObj["chart_min"] = w.chart_min;
    if (w.chart_max.length() > 0) wObj["chart_max"] = w.chart_max;
    
    if (w.name.length() > 0) wObj["name"] = w.name;
    if (w.mdi_icon.length() > 0) wObj["mdi_icon"] = w.mdi_icon;
    if (w.color_on.length() > 0) wObj["color_on"] = w.color_on;
    if (w.color_off.length() > 0) wObj["color_off"] = w.color_off;
    
    if (w.media_content_type.length() > 0) wObj["media_content_type"] = w.media_content_type;
    if (w.media_content_id.length() > 0) wObj["media_content_id"] = w.media_content_id;

    if (w.tap_action_domain.length() > 0) wObj["ta_dom"] = w.tap_action_domain;
    if (w.tap_action_service.length() > 0) wObj["ta_srv"] = w.tap_action_service;
    if (w.tap_action_target.length() > 0) wObj["ta_tgt"] = w.tap_action_target;

    if (w.select_options.length() > 0) wObj["select_options"] = w.select_options;
    wObj["slider_min"] = w.slider_min;
    wObj["slider_max"] = w.slider_max;
    wObj["slider_step"] = w.slider_step;

    if (w.conditions.size() > 0) {
        wObj["conditions_type"] = w.conditions_type;
        JsonArray j_conds = wObj["conditions"].to<JsonArray>();
        for (const auto& c : w.conditions) {
            JsonObject j_c = j_conds.add<JsonObject>();
            j_c["entity"] = c.entity;
            j_c["op"] = c.op;
            j_c["value"] = c.value;
        }
    }

    if (w.children.size() > 0) {
        JsonArray j_children = wObj["children"].to<JsonArray>();
        for (const auto& child : w.children) {
            JsonObject j_child = j_children.add<JsonObject>();
            serializeWidget(j_child, child);
        }
    }
}

// ------------------------------------------------------------------------------
// Datei lesen
// ------------------------------------------------------------------------------
// Liest eine Konfigurationsdatei und baut daraus eine Tab-Liste.
// Schreibt ausschliesslich in 'out'. Der Aufrufer uebernimmt das Ergebnis nur
// bei Erfolg - eine defekte Datei kann den laufenden Stand so nie leeren.
// Eine Datei mit leerer Tab-Liste gilt als gueltig; was dann passiert,
// entscheidet der Aufrufer.
static bool parseConfigFile(const char* path, std::vector<HADashboardDef>& out, String& err) {
    out.clear();

    File file = LittleFS.open(path, "r");
    if (!file) { err = "Datei nicht lesbar"; return false; }

    const size_t size = file.size();
    if (size == 0 || size > HaConfigLogic::MAX_FILE_BYTES) {
        file.close();
        err = (size == 0) ? "Datei ist leer" : "Datei ist zu gross";
        return false;
    }

    // Ganze Datei in einem Zug lesen: deutlich schneller als zeichenweises
    // Parsen direkt aus dem Dateistrom. Puffer im PSRAM, internes RAM ist knapp.
    char* buf = (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buf) { file.close(); err = "Kein Speicher"; return false; }

    const size_t got = file.readBytes(buf, size);
    file.close();
    if (got != size) { heap_caps_free(buf); err = "Lesefehler"; return false; }

    // Mit Laengenangabe parsen: der Puffer ist NICHT nullterminiert.
    // Als const uebergeben, damit ArduinoJson die Zeichenketten kopiert und
    // der Puffer direkt danach freigegeben werden darf.
    JsonDocument doc(HaPsramAlloc());
    DeserializationError jerr = deserializeJson(doc, (const char*)buf, size,
                                                DeserializationOption::NestingLimit(CONFIG_NESTING_LIMIT));
    heap_caps_free(buf);

    if (jerr) { err = String("JSON-Fehler: ") + jerr.c_str(); return false; }
    if (doc.overflowed()) { err = "Kein Speicher beim Parsen"; return false; }

    JsonArray tabs = doc["tabs"];
    if (tabs.isNull()) { err = "Feld 'tabs' fehlt"; return false; }

    out.reserve(tabs.size());
    for (JsonObject tObj : tabs) {
        HADashboardDef tDef;

        // Doppelte Namen werden umbenannt, nicht verworfen. Frueher fiel ein
        // Duplikat hier samt aller Widgets weg, und das naechste Save()
        // machte den Verlust dauerhaft.
        const String wanted = tObj["name"] | "Tab";
        tDef.name = HaConfigLogic::MakeUniqueTabName(out, wanted);
        if (tDef.name != wanted) {
            Serial.printf("[HaConfig] Doppelter Tab-Name '%s' -> '%s'\n", wanted.c_str(), tDef.name.c_str());
        }

        JsonArray widgets = tObj["widgets"];
        tDef.widgets.reserve(widgets.size());
        for (JsonObject wObj : widgets) {
            HAWidgetDef wDef;
            deserializeWidget(wObj, wDef);
            tDef.widgets.push_back(std::move(wDef));
        }
        out.push_back(std::move(tDef));
    }
    return true;
}

// Stellt die Invariante "mindestens ein Tab" her.
// Gibt true zurueck, wenn ein Standard-Tab angelegt werden musste.
static bool ensureAtLeastOneTab(std::vector<HADashboardDef>& list) {
    if (!list.empty()) return false;
    HADashboardDef defTab;
    defTab.name = "Wohnen";
    list.push_back(defTab);
    return true;
}

void HaConfigLogic::Load() {
    if (!lockConfig()) return;

    std::vector<HADashboardDef> loaded;

    if (s_fsAvailable && LittleFS.exists(CONFIG_PATH)) {
        String err;
        if (!parseConfigFile(CONFIG_PATH, loaded, err)) {
            // Defekte Datei NICHT ueberschreiben, sondern beiseitelegen.
            // Sonst ersetzt das erste Save() sie durch den Standard-Tab und
            // der Inhalt ist endgueltig weg.
            Serial.printf("[HaConfig] %s nicht ladbar (%s). Gesichert als %s, starte mit Standard-Tab.\n",
                          CONFIG_PATH, err.c_str(), CONFIG_BAD_PATH);
            LittleFS.remove(CONFIG_BAD_PATH);
            LittleFS.rename(CONFIG_PATH, CONFIG_BAD_PATH);
            loaded.clear();
        }
    }

    const bool createdDefault = ensureAtLeastOneTab(loaded);
    dashboards.swap(loaded);

    if (createdDefault) Save();

    unlockConfig();
}

// ------------------------------------------------------------------------------
// Datei schreiben
// ------------------------------------------------------------------------------
// Schreibt zuerst in eine temporaere Datei, prueft die Laenge und benennt erst
// dann um. Ein Stromausfall oder voller Flash hinterlaesst so immer entweder
// den alten oder den neuen Stand - nie eine halbe Datei.
static bool writeConfigAtomically(JsonDocument& doc) {
    const size_t expected = measureJson(doc);

    File f = LittleFS.open(CONFIG_TMP_PATH, "w");
    if (!f) {
        Serial.println("[HaConfig] Speichern: temporaere Datei nicht anlegbar.");
        return false;
    }
    const size_t written = serializeJson(doc, f);
    f.close();

    if (written != expected) {
        Serial.printf("[HaConfig] Speichern: nur %u von %u Bytes geschrieben (Flash voll?).\n",
                      (unsigned)written, (unsigned)expected);
        LittleFS.remove(CONFIG_TMP_PATH);
        return false;
    }

    if (!LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH)) {
        // Falls die Dateisystemschicht beim Umbenennen nicht ueberschreibt:
        // alte Datei entfernen und erneut versuchen. Init() stellt den Stand
        // wieder her, falls genau hier der Strom wegbleibt.
        LittleFS.remove(CONFIG_PATH);
        if (!LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH)) {
            Serial.println("[HaConfig] Speichern: Umbenennen fehlgeschlagen.");
            return false;
        }
    }
    return true;
}

bool HaConfigLogic::Save() {
    if (!s_fsAvailable) {
        // Ohne Dateisystem nichts schreiben (siehe mountFileSystem).
        Serial.println("[HaConfig] Speichern uebersprungen: Dateisystem nicht verfuegbar.");
        return false;
    }
    if (!lockConfig()) return false;

    JsonDocument doc(HaPsramAlloc());
    JsonArray tabs = doc["tabs"].to<JsonArray>();

    for (const auto& t : dashboards) {
        JsonObject tObj = tabs.add<JsonObject>();
        tObj["name"] = t.name;
        JsonArray widgets = tObj["widgets"].to<JsonArray>();

        for (const auto& w : t.widgets) {
            JsonObject wObj = widgets.add<JsonObject>();
            serializeWidget(wObj, w);
        }
    }

    bool ok = false;
    if (doc.overflowed()) {
        // Unvollstaendiges Dokument nie auf den Flash schreiben.
        Serial.println("[HaConfig] Speichern abgebrochen: kein Speicher fuer das JSON-Dokument.");
    } else {
        ok = writeConfigAtomically(doc);
    }

    unlockConfig();
    return ok;
}

// ------------------------------------------------------------------------------
// Import einer hochgeladenen Sicherung
// ------------------------------------------------------------------------------
bool HaConfigLogic::ImportFromFile(const char* path, String& err) {
    std::vector<HADashboardDef> imported;

    bool ok = parseConfigFile(path, imported, err);
    if (ok && imported.empty()) {
        err = "Die Datei enthaelt keine Tabs";
        ok = false;
    }

    if (ok) {
        if (!lockConfig()) {
            err = "Konfiguration gesperrt";
            ok = false;
        } else {
            // Erst tauschen, dann speichern. Scheitert das Speichern, wird
            // zurueckgetauscht - Anzeige und Datei bleiben beim alten Stand.
            dashboards.swap(imported);
            if (!Save()) {
                dashboards.swap(imported);
                err = "Speichern auf dem Geraet fehlgeschlagen";
                ok = false;
            }
            unlockConfig();
        }
    }

    LittleFS.remove(path);
    return ok;
}

// ------------------------------------------------------------------------------
// Tab-Verwaltung
// ------------------------------------------------------------------------------
String HaConfigLogic::MakeUniqueTabName(const std::vector<HADashboardDef>& list, String wanted, int ignoreIndex) {
    wanted.trim();
    if (wanted.length() == 0) wanted = "Tab";

    auto taken = [&](const String& candidate) {
        for (size_t i = 0; i < list.size(); i++) {
            if ((int)i == ignoreIndex) continue;
            if (list[i].name == candidate) return true;
        }
        return false;
    };

    if (!taken(wanted)) return wanted;

    // Endet sicher: die Liste ist endlich, irgendwann ist ein Suffix frei.
    for (int k = 2; ; k++) {
        const String candidate = wanted + " " + String(k);
        if (!taken(candidate)) return candidate;
    }
}

void HaConfigLogic::AddTab(String name) { 
    if (!lockConfig()) return;
    HADashboardDef d; 
    d.name = MakeUniqueTabName(dashboards, name); 
    dashboards.push_back(d); 
    unlockConfig();
}

void HaConfigLogic::DeleteTab(int index) { 
    if (!lockConfig()) return;
    if (index >= 0 && index < (int)dashboards.size()) { 
        dashboards.erase(dashboards.begin() + index); 
        ensureAtLeastOneTab(dashboards);
    } 
    unlockConfig();
}

void HaConfigLogic::RenameTab(int index, String newName) { 
    if (!lockConfig()) return;
    if (index >= 0 && index < (int)dashboards.size()) {
        dashboards[index].name = MakeUniqueTabName(dashboards, newName, index); 
    }
    unlockConfig();
}

void HaConfigLogic::MoveTabLeft(int index) { 
    if (!lockConfig()) return;
    if (index > 0 && index < (int)dashboards.size()) std::swap(dashboards[index], dashboards[index - 1]); 
    unlockConfig();
}

void HaConfigLogic::MoveTabRight(int index) { 
    if (!lockConfig()) return;
    // Bewusst "index + 1 < size": "index < size - 1" laeuft bei leerer Liste
    // unter (size_t) und erlaubt dann einen Zugriff hinter das Ende.
    if (index >= 0 && index + 1 < (int)dashboards.size()) std::swap(dashboards[index], dashboards[index + 1]); 
    unlockConfig();
}
