#pragma once
// ==============================================================================
// SettingsUi - Bausteine der Einstellungs-Ansicht
// ==============================================================================
// Aufbau einer Seite (Entwurf "Lela Settings Redesign"):
//
//   page ── header (Zurueck-Link, Titel, Untertitel)
//        ├─ section "TITEL" ── card ── row ─ [Titel / Beschreibung / Chip]  [Bedienelement]
//        │                          └─ row ...
//        │  (Fussnote unter der Karte)
//   actions [Verwerfen] [Uebernehmen]   (nur Unterseiten; steht fest am unteren
//                                        Rand, scrollt nicht mit)
//
// Seiten mit "Uebernehmen": Was dort geaendert wird, gilt erst nach dem
// Speichern. Damit das nicht unbemerkt verloren geht, ist die Leiste immer
// sichtbar, und die Bausteine merken sich, ob seit dem Aufbau etwas geaendert
// wurde (hasUnsavedChanges). Die Navigation (ViewSettings) fragt dann nach,
// bevor sie die Seite verlaesst.
//
// Gestaltung: flach (keine Schatten, Verlaeufe, Transparenz), Radius 20 fuer
// Karten, Pillenform fuer Knoepfe, Zeilen ab 80 px, Beruehrflaechen >= 50 px.
// Farben ausschliesslich aus UiTheme. Die Stile sind gemeinsam genutzte
// lv_style_t (einmal je Aufbau mit den aktuellen Farben gefuellt) statt
// Einzelstilen je Objekt - spart RAM und haelt alle Seiten einheitlich.
//
// Alle Funktionen nur unter der LVGL-Sperre (Aufbau, Event-Callbacks).
// ==============================================================================

#include <lvgl.h>

namespace SettingsUi {

enum class Btn  : uint8_t { Primary, Tonal, Danger };
enum class Chip : uint8_t { Neutral, Ok, Warn, Danger };

// Eine Zeile. `text` ist die linke Spalte (Titel, Beschreibung, Chip), die
// Bedienelemente haengen rechts an `obj`.
struct Row {
    lv_obj_t* obj   = nullptr;
    lv_obj_t* text  = nullptr;
    lv_obj_t* label = nullptr;
    lv_obj_t* desc  = nullptr;
};

// Zu Beginn jedes Aufbaus der Einstellungs-Ansicht: setzt die Zeiger des
// vorherigen Aufbaus zurueck und fuellt die Stile (initStyles).
void begin();

// Stile mit den Farben des aktuellen Schemas fuellen (nur bei Aenderung).
// Fuer andere Ansichten, die die Bausteine nutzen (Schnelleinstellungen).
void initStyles();

// Tastatur fuer alle Textfelder (eine je Bildschirm, liegt ueber dem Inhalt).
void setKeyboard(lv_obj_t* kb);

// Scrollende Inhaltsspalte; ersetzt den bisherigen Inhalt von `parent`.
lv_obj_t* page(lv_obj_t* parent);

// Kopf der Seite. Mit backText erscheint darueber ein Zurueck-Link ("‹ Babyphone").
void header(lv_obj_t* page, const char* title, const char* sub,
            const char* backText = nullptr, lv_event_cb_t backCb = nullptr);

// Abschnitt mit Titel, Karte und optionaler Fussnote. Gibt die Karte zurueck.
lv_obj_t* section(lv_obj_t* page, const char* title, const char* foot = nullptr);

Row row(lv_obj_t* card, const char* label, const char* desc = nullptr);

// Ganze Zeile antippbar, rechts Wert und Chevron (fuehrt auf eine Unterseite).
Row linkRow(lv_obj_t* card, const char* label, const char* desc, const char* value,
            lv_event_cb_t cb, void* userData = nullptr, lv_obj_t** valueLabel = nullptr);

void setDesc(const Row& r, const char* text);

// Status-Chip unter dem Titel (Punkt + Text).
lv_obj_t* chip(const Row& r, const char* text, Chip kind);
void setChip(lv_obj_t* chip, const char* text, Chip kind);

lv_obj_t* addSwitch(const Row& r, bool on, lv_event_cb_t cb, void* userData = nullptr);
lv_obj_t* addDropdown(const Row& r, const char* options, uint16_t selected,
                      lv_event_cb_t cb, lv_coord_t width = 260);
// Regler mit Wertfeld daneben; den Text des Wertfelds setzt der Aufrufer.
lv_obj_t* addSlider(const Row& r, int min, int max, int value, lv_event_cb_t cb,
                    lv_obj_t** valueLabel);
lv_obj_t* addTextarea(const Row& r, const char* text, const char* placeholder,
                      lv_coord_t width, bool digitsOnly = false, bool password = false);
lv_obj_t* addValue(const Row& r, const char* text);
lv_obj_t* addButton(lv_obj_t* parent, const char* text, Btn kind,
                    lv_event_cb_t cb, void* userData = nullptr);
// Art eines Knopfs nachtraeglich aendern (z.B. Stumm: Tonal <-> Danger).
void setButtonKind(lv_obj_t* btn, Btn kind);

// Stile der Bausteine auf vorhandene LVGL-Objekte anderer Ansichten anwenden
// (Groesse und Lage bleiben, wie die Ansicht sie setzt). initStyles() vorher.
void styleSwitch(lv_obj_t* sw);
void styleSlider(lv_obj_t* slider);
void styleDropdown(lv_obj_t* dd);
void styleButton(lv_obj_t* btn, Btn kind);

// Segmentierte Auswahl. Meldet LV_EVENT_VALUE_CHANGED am zurueckgegebenen Objekt.
lv_obj_t* segmented(lv_obj_t* parent, const char* const* items, uint8_t count,
                    uint8_t selected, lv_event_cb_t cb, void* userData = nullptr);
uint8_t segmentedSelected(lv_obj_t* seg);

// Gedaempfter Fliesstext (Hinweise, Diagnose).
lv_obj_t* note(lv_obj_t* parent, const char* text);

// Leiste fuer Verwerfen/Uebernehmen, rechtsbuendig. Sie steht fest unter dem
// scrollenden Inhalt und ist damit immer sichtbar. Als LETZTES beim Aufbau
// rufen: Eine Seite mit Leiste gilt als Seite "mit Uebernehmen", und ab diesem
// Aufruf zaehlen Aenderungen an ihren Bedienelementen als ungespeichert.
lv_obj_t* actions(lv_obj_t* page);

// Knopfzeile rechtsbuendig an Ort und Stelle - fuer Ansichten, die die
// Bausteine ohne page() nutzen (Schnelleinstellungen). Sie liegt im Fluss des
// uebergebenen Objekts und hat nichts mit "Uebernehmen" zu tun.
lv_obj_t* buttonRow(lv_obj_t* parent);

// Das Objekt, in dem die Seite liegt (was page() als parent bekam).
lv_obj_t* pageHost(lv_obj_t* page);

// --- Ungespeicherte Aenderungen -------------------------------------------------
// true: Die Seite hat eine Leiste (actions) und seit dem Aufbau wurde ein
// Schalter, Regler, Textfeld, eine Auswahl oder ein Segment veraendert - auch
// durch die Seite selbst (z.B. gefundene Kamera-IP ins Feld uebernommen).
bool hasUnsavedChanges();
// Fuer Aenderungen, die nicht ueber die Bausteine laufen (z.B. Auswahl einer
// Listenzeile, die erst "Uebernehmen" speichert).
void markDirty();
// Nach dem Speichern oder dem ausdruecklichen Verwerfen.
void clearDirty();
// Bedienelement ohne zu speichernden Wert ("Passwort anzeigen"): zaehlt nicht.
void setTransient(lv_obj_t* control);

// --- Rueckfrage -------------------------------------------------------------------
// Kasten in der Bildschirmmitte ueber allem (lv_layer_top), sperrt die
// Bedienung dahinter. cancelText schliesst nur; okText schliesst und ruft
// onOk(userData). Eine noch offene Rueckfrage wird ersetzt.
void confirm(const char* title, const char* message, const char* cancelText,
             const char* okText, Btn okKind, void (*onOk)(void*), void* userData = nullptr);
// Offene Rueckfrage ohne Antwort schliessen (Bildschirmwechsel).
void closeConfirm();

// Karte mit QR-Code links und Anleitung rechts. Der QR-Code ist immer schwarz
// auf weiss mit Ruhezone - so lesen ihn alle Kamera-Apps, auch im dunklen Schema.
lv_obj_t* qrCard(lv_obj_t* page, const char* data, const char* steps, const char* hint = nullptr);

}  // namespace SettingsUi
