#pragma once
#include "HAWidgets.h"

class HaDialogLight {
private:
    static lv_obj_t* overlay;
    static lv_obj_t* slider_bri;
    static lv_obj_t* slider_white;
    static lv_obj_t* slider_temp;
    static lv_obj_t* color_wheel;
    static String current_entity;

    static lv_obj_t* lbl_bri_val;
    static lv_obj_t* lbl_w_val;
    static lv_obj_t* lbl_temp_val;

    // Sendet den Zustand an HA - nur beim Loslassen eines Reglers bzw. des
    // Farbrads. Frueher ging bei jedem Zwischenwert ein Aufruf raus (>100 je Zug).
    static void send_state_cb(lv_event_t * e);
    // Aktualisiert waehrend des Ziehens nur die Wertanzeigen.
    static void update_labels_cb(lv_event_t * e);
    static void close_event_cb(lv_event_t * e);

    // Setzt alle Objektzeiger des Dialogs auf nullptr.
    static void clearPointers();
    // LV_EVENT_DELETE des Overlays: auch beim Loeschen zusammen mit dem
    // Screen (Bildschirmwechsel) duerfen keine haengenden Zeiger bleiben.
    static void overlay_delete_cb(lv_event_t * e);

public:
    static void show(HAWidget* w);
    static void hide();
    static bool isActive();
};