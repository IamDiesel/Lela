#pragma once
// ==============================================================================
// UiTheme - Farbschema der Oberflaeche (dunkel / hell / automatisch)
// ==============================================================================
// Ansichten fragen Farben nach ihrer ROLLE ab (Hintergrund, Karte, Text, ...),
// nie als feste Werte. Der Modus entscheidet, welcher Wert dahinter steht.
// Die Rollen und Werte sind im Entwurf "Lela Settings Redesign" festgelegt.
//
// Umstellung in Etappen: Ab D1 nutzen die Einstellungen (und die Topbar auf
// diesem Bildschirm) das Schema. Die uebrigen Ansichten folgen in D2-D4 und
// lesen bis dahin weiter `isDarkMode` (wird hier mitgefuehrt).
//
// Farben gelten beim AUFBAU einer Ansicht. Wechselt das Schema, wird die
// offene Ansicht neu aufgebaut (GuiManager::reloadCurrentScreen) - ein
// einmaliger Neuaufbau kostet weniger als laufend umgefaerbte Objekte.
//
// Bewusst NICHT im Schema (gleich in beiden Modi): Alarmfarben, die Farben der
// Dashboard-Kacheln, HA-Zustandsfarben, Kartenkacheln.
// ==============================================================================

#include <lvgl.h>

enum class UiThemeMode : uint8_t { Dark = 0, Light = 1, Auto = 2 };

struct UiColors {
    // Flaechen
    lv_color_t bg;          // Bildschirmhintergrund
    lv_color_t side;        // Seitenleiste
    lv_color_t surface;     // Karte
    lv_color_t surface2;    // erhoehte Flaeche: Eingabefelder, Segmente, tonale Knoepfe
    lv_color_t divider;     // Trennlinien
    lv_color_t navActive;   // gewaehlter Eintrag der Seitenleiste
    // Text
    lv_color_t text;
    lv_color_t muted;
    lv_color_t link;
    // Akzente
    lv_color_t accent;      // Schalter an, Regler, Fokus
    lv_color_t primary;     // primaere Knoepfe
    lv_color_t onPrimary;
    lv_color_t switchOff;
    lv_color_t off;         // Statuspunkt "aus"
    // Zustaende (Chip-Vordergrund / -Hintergrund)
    lv_color_t ok, okBg;
    lv_color_t warn, warnBg;
    lv_color_t danger, dangerBg;
    // Schwebende Bedienelemente ueber Inhalt (HA-Knopf, Wisch-Hinweis)
    lv_color_t overlay, overlayBorder;
    lv_opa_t   overlayOpa;
    // Topbar (bestehende ViewTopbar; nur ihre Farben folgen dem Schema)
    lv_color_t bar;
    lv_opa_t   barOpa;
    lv_color_t barText, barMuted, barIdle, barOk, barWarn, barErr;
};

// Farben der Bereichs-Kacheln - wie die Dashboard-Kacheln, in beiden Modi gleich.
// Namen mit k-Praefix: Arduino.h definiert u.a. DISPLAY als Makro.
namespace UiTile {
    constexpr uint32_t kCat     = 0xE67E22;
    constexpr uint32_t kBaby    = 0x3D8FC4;
    constexpr uint32_t kNet     = 0x2E9E5B;
    constexpr uint32_t kDisplay = 0x7E57C2;
    constexpr uint32_t kSystem  = 0x6B7280;
}

namespace UiTheme {
    // Laedt den Modus (NVS "theme"; uebernimmt einmalig den alten Schalter
    // "dark"). Nach Data_Init(), vor dem ersten Bildschirm.
    void init();

    UiThemeMode mode();
    bool isDark();
    const UiColors& c();

    // Speichert den Modus und baut die Einstellungen neu auf, falls sich das
    // wirksame Schema aendert. Aus dem LVGL-Kontext (Event-Callback) rufen.
    void setMode(UiThemeMode mode);

    // Modus "Automatisch": prueft die Uhrzeit (hell 07:00-19:59) und schaltet
    // bei Bedarf um. Aus der Hauptschleife; prueft hoechstens einmal je Minute.
    void update();
}
