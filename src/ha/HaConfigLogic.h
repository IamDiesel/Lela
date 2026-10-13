#pragma once
#include <Arduino.h>
#include <vector>

// Eine Sichtbarkeitsbedingung: Entitaet, Vergleich, Wert.
struct HACondition {
    String entity;
    String op;    // "==", "!=", ">", "<", ">=", "<="
    String value;
};

struct HAWidgetDef {
    String entity_id = "";
    String type = "light";
    int x = 20;
    int y = 20;
    int w = 160;
    int h = 105;
    String name = "";
    String mdi_icon = "";
    String color_on = "";
    String color_off = "";
    
    // Gleiche Standardwerte wie beim Laden (deserializeWidget). Frueher stand
    // hier 0/2/1 (DEFAULT/TOP_MID/TOP_LEFT) - manuell hinzugefuegte Widgets
    // bekamen dadurch uebereinanderliegende Beschriftungen.
    int icon_align = 2;    // LV_ALIGN_TOP_MID
    int text_align = 5;    // LV_ALIGN_BOTTOM_MID
    int state_align = 9;   // LV_ALIGN_CENTER
    
    int icon_margin = 5;
    int text_margin = 5;
    int state_margin = 0;
    
    bool snap_to_grid = true;
    
    // --- DIAGRAMM SETTINGS ---
    bool show_chart = false; 
    int chart_w_pct = 95;
    int chart_h_pct = 50;
    int chart_x_ofs = 0;     
    int chart_y_ofs = -15;   
    String chart_min = "";
    String chart_max = "";
    
    // --- MEDIA SETTINGS ---
    String media_content_type = "";
    String media_content_id = "";

    // --- ENTKOPPELTE KLICK AKTION ---
    String tap_action_domain = "";
    String tap_action_service = "";
    String tap_action_target = "";

    // --- GEBACKENE OPTIONEN & LIMITS ---
    String select_options = "";
    float slider_min = 0.0f;
    float slider_max = 100.0f;
    float slider_step = 1.0f;

    // Sichtbarkeitsbedingungen und Ordner-Kinder
    String conditions_type = "AND"; // Entweder "AND" oder "OR"
    std::vector<HACondition> conditions;
    std::vector<HAWidgetDef> children; 
};

struct HADashboardDef {
    String name;
    std::vector<HAWidgetDef> widgets;
};

class HaConfigLogic {
public:
    // Alle Tabs samt Widgets. Invariante: nach Init()/Load() mindestens ein Tab.
    // Wird nur im loopTask veraendert (GUI, Webserver); der Preload-Task liest
    // beim Start einmalig ein.
    static std::vector<HADashboardDef> dashboards;

    static void Init();
    static void Load();

    // Schreibt atomar (temporaere Datei + Umbenennen). false = Datei unveraendert.
    static bool Save();

    static void AddTab(String name);
    static void DeleteTab(int index);
    static void RenameTab(int index, String newName);
    static void MoveTabLeft(int index);
    static void MoveTabRight(int index);

    // Uebernimmt eine hochgeladene Sicherung (Web-Setup).
    // Die Datei wird vollstaendig geprueft, BEVOR etwas ersetzt wird. Bei
    // Erfolg ersetzt sie 'dashboards' und die Live-Datei und wird geloescht.
    // Bei Fehler bleibt alles unveraendert, 'err' beschreibt den Grund.
    // Nur im loopTask aufrufen (gleicher Thread wie die GUI).
    static bool ImportFromFile(const char* path, String& err);

    // Liefert einen Tab-Namen, der noch nicht vergeben ist: getrimmt, leer
    // wird zu "Tab", bei Kollision " 2", " 3", ... angehaengt.
    // ignoreIndex: dieser Tab zaehlt nicht als Kollision (Umbenennen).
    static String MakeUniqueTabName(const std::vector<HADashboardDef>& list, String wanted, int ignoreIndex = -1);

    // Zwischenablage fuer den Web-Upload. Wird beim Start aufgeraeumt.
    static constexpr const char* UPLOAD_PATH = "/ha_config.upload";

    // Groesste akzeptierte Konfigurationsdatei. Schutz gegen versehentlich
    // hochgeladene Fremddateien; reale Konfigurationen liegen weit darunter.
    static constexpr size_t MAX_FILE_BYTES = 512u * 1024u;
};
