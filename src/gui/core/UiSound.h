#pragma once
// ==============================================================================
// UiSound - Tastentoene fuer Bedienelemente
// ==============================================================================
// Einheitliche akustische Rueckmeldung, wie sie die uebrigen Ansichten schon
// mit playToneI2S(..., true) geben (kurzer Klick, Lautstaerke "UI" im Mixer).
//
//   UiSound_AttachClick()        Klick beim Antippen (LV_EVENT_CLICKED)
//   UiSound_AttachSlider()       Regler: Klick beim Anfassen und beim
//                                Loslassen, waehrend des Schiebens Ruhe.
//
// Regler bewusst ohne Ton waehrend des Schiebens (frueher "Rasterklick" je
// 10 %, empfunden als Dauergeraeusch). So halten es auch iOS, Android und
// Material Design: Schieben ist still (Rastungen gibt es dort nur haptisch),
// Rueckmeldung beim Anfassen und Loslassen; Lautstaerkeregler spielen beim
// Loslassen eine Hoerprobe (Schnelleinstellungen: playBingI2S).
// Ein kurzes Antippen ergibt nur EINEN Klick (Sperre 150 ms in playToneI2S).
//
// Spielt eine Ansicht im eigenen Callback schon einen Ton, bleibt es bei
// einem: playToneI2S laesst UI-Toene hoechstens alle 150 ms durch. Deshalb
// die Klick-Callbacks NACH denen der Ansicht anhaengen.
// ==============================================================================

#include <lvgl.h>

void UiSound_Click();
void UiSound_AttachClick(lv_obj_t* obj);
void UiSound_AttachSlider(lv_obj_t* slider);
