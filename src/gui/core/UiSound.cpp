// ==============================================================================
// UiSound - siehe UiSound.h
// ==============================================================================
#include "UiSound.h"
#include "SharedData.h"   // playToneI2S

void UiSound_Click() {
    playToneI2S(800, 60, true);
}

static void clickCb(lv_event_t*) {
    UiSound_Click();
}

void UiSound_AttachClick(lv_obj_t* obj) {
    if (obj) lv_obj_add_event_cb(obj, clickCb, LV_EVENT_CLICKED, nullptr);
}

// Regler: Klick beim Anfassen und beim Loslassen (auch wenn der Finger den
// Regler verlaesst), waehrend des Schiebens kein Ton.
static void sliderCb(lv_event_t* e) {
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            UiSound_Click();
            break;
        default:
            break;
    }
}

void UiSound_AttachSlider(lv_obj_t* slider) {
    if (slider) lv_obj_add_event_cb(slider, sliderCb, LV_EVENT_ALL, nullptr);
}
