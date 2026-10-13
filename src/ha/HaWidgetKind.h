#pragma once
// ==============================================================================
// HaWidgetKind - Widget-Typen als Aufzaehlung
// ==============================================================================
// In der Konfiguration (ha_config.json) und in HAWidgetDef bleibt der Typ ein
// Text ("light", "sensor", ...) - das Dateiformat aendert sich nicht.
// Im Code wird er einmal beim Anlegen in diese Aufzaehlung uebersetzt. So
// faellt ein Tippfehler beim Kompilieren auf, statt still nie zuzutreffen,
// und Vergleiche kosten keine String-Operation.
// ==============================================================================

#include <Arduino.h>

enum class HaWidgetKind : uint8_t {
    Light,         // "light"        Schaltkachel (Licht, Schalter, ...); auch Rueckfall
    Action,        // "action"       Taste (button, script, scene, ...)
    Sensor,        // "sensor"       Anzeige, optional mit Diagramm
    MediaPlayer,   // "media_player"
    MediaItem,     // "media_item"   Favorit aus dem Medien-Browser
    Vacuum,        // "vacuum"
    Cover,         // "cover"
    Climate,       // "climate"
    Text,          // "text"
    Select,        // "select"
    Number,        // "number"
    Folder,        // "folder"       Container mit bedingt sichtbaren Kindern
    Unknown        // alles andere (wird als Light-Widget dargestellt)
};

inline HaWidgetKind HaWidgetKindFromString(const String& type) {
    if (type == "light")        return HaWidgetKind::Light;
    if (type == "action")       return HaWidgetKind::Action;
    if (type == "sensor")       return HaWidgetKind::Sensor;
    if (type == "media_player") return HaWidgetKind::MediaPlayer;
    if (type == "media_item")   return HaWidgetKind::MediaItem;
    if (type == "vacuum")       return HaWidgetKind::Vacuum;
    if (type == "cover")        return HaWidgetKind::Cover;
    if (type == "climate")      return HaWidgetKind::Climate;
    if (type == "text")         return HaWidgetKind::Text;
    if (type == "select")       return HaWidgetKind::Select;
    if (type == "number")       return HaWidgetKind::Number;
    if (type == "folder")       return HaWidgetKind::Folder;
    return HaWidgetKind::Unknown;
}
