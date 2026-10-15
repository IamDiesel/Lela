#pragma once
#include <Arduino.h>
#include "LVGL_Driver.h"

class UIHelper {
public:
    // Bildschirmgroesse des Tab5 (Querformat).
    static constexpr lv_coord_t SCREEN_W = 1280;
    static constexpr lv_coord_t SCREEN_H = 720;

    // Legt ein bildschirmfuellendes, modales Overlay an - die gemeinsame
    // Grundlage aller Dialoge. Frueher war dieser Aufbau rund 16-mal kopiert,
    // und genau die Abweichungen zwischen den Kopien (fehlendes
    // GESTURE_BUBBLE, fehlende Zeigerbindung) verursachten Abstuerze und
    // tote Dialoge nach einem Bildschirmwechsel.
    //   - faengt alle Beruehrungen ab, ist nicht scrollbar
    //   - reicht keine Wischgesten an den Screen weiter
    //   - 'slot': optional per trackOverlay() an die Lebensdauer gebunden
    //     (nullptr, wenn der Dialog einen eigenen DELETE-Handler hat)
    //   - 'black': schwarzer Hintergrund mit Deckkraft 'opa'; false = Farbe
    //     des Themes (nur der Bearbeiten-Dialog, leicht abgedunkelt)
    static lv_obj_t* createModalOverlay(lv_obj_t* parent, lv_opa_t opa, lv_obj_t** slot = nullptr, bool black = true);

    // true, solange mindestens ein ueber createModalOverlay() angelegter
    // Dialog offen ist. Das Video-Overlay blendet sich dann aus - es wuerde
    // sonst ueber den Dialog gezeichnet.
    static bool isModalOpen();

    static lv_obj_t* createLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, uint32_t color = 0xFFFFFFFF);
    static lv_obj_t* createButton(lv_obj_t* parent, int w, int h, int align, int x, int y, uint32_t bg_color, const char* text, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createSlider(lv_obj_t* parent, int w, int h, int align, int x, int y, int min_val, int max_val, int val, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createDropdown(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* options, int selected, lv_event_cb_t cb = nullptr, void* user_data = nullptr);
    static lv_obj_t* createTextarea(lv_obj_t* parent, int w, int h, int align, int x, int y, const char* text, const char* placeholder = "", bool one_line = true);
    static lv_obj_t* createCheckbox(lv_obj_t* parent, const char* text, const lv_font_t* font, int align, int x, int y, bool checked, lv_event_cb_t cb = nullptr, void* user_data = nullptr);

    // Bindet einen statischen Overlay-Zeiger an die Lebensdauer des Objekts:
    // Wird 'obj' geloescht - egal ob per Schliessen-Knopf, lv_obj_del_async()
    // oder zusammen mit dem ganzen Screen beim Bildschirmwechsel -, setzt ein
    // LV_EVENT_DELETE-Handler '*slot' auf nullptr.
    // Aber nur, wenn '*slot' noch auf genau dieses Objekt zeigt: ein neues
    // Overlay kann bereits angelegt sein, waehrend das alte noch asynchron
    // geloescht wird.
    // Ohne diese Bindung zeigte der Zeiger nach einem Screenwechsel ins Leere
    // (Use-after-free beim naechsten hide()/del_async, bzw. der Dialog liess
    // sich nie wieder oeffnen, weil der Zeiger nie mehr nullptr wurde).
    static void trackOverlay(lv_obj_t* obj, lv_obj_t** slot);

    // Vorschlagsliste fuer Entitaeten (Hinzufuegen, Klick-Aktion, Bedingung):
    // sucht ab 2 Zeichen in der Entitaetsliste, fuellt 'roller' mit bis zu 5
    // Treffern und blendet ihn ein - sonst aus. Mit 'anchor' wird der Roller
    // direkt unter diesem Eingabefeld platziert.
    static void showEntitySuggestions(lv_obj_t* roller, lv_obj_t* anchor, const String& query);
};