#pragma once
// ==============================================================================
// BabyService - Babyphone-Betrieb unabhaengig vom angezeigten Bildschirm
// ==============================================================================
// Frueher hing der gesamte Babyphone-Betrieb an der Baby-Ansicht: Betreten
// setzte den Stream zurueck, Verlassen stoppte Video und Audio. Damit waren
// Hintergrund-Audio und ein Video-Overlay auf anderen Bildschirmen unmoeglich.
//
// Jetzt gehoert der Zustand diesem Dienst. Die Oberflaeche (Baby-Ansicht,
// QuickSettings, Video-Overlay) und MQTT aeussern nur Wuensche ueber
// BabyService_SetMode() und Co.; BabyService_Update() gleicht in jedem
// Durchlauf der Hauptschleife Wunsch und Wirklichkeit ab:
//
//   Modus (Wunsch)  --->  Kamerasitzung  (BabyCamApi: Heartbeat, Statusabfrage)
//                   --->  Videoempfang   (VideoLogic: Netz- und Decoder-Task)
//                   --->  Videoausgabe   (VideoLogic: Baby-Ansicht, Overlay, keine)
//                   --->  Aufloesung     (Kamera: je Abnehmer eine eigene)
//                   --->  Audio          (AudioStreamLogic: Babyphone-Ton)
//
// Abhaengigkeitsrichtung: Oberflaeche -> Dienst -> Medienmodule. Der Dienst
// kennt keine Ansicht; was er ueber die Oberflaeche wissen muss (Baby-Ansicht
// sichtbar, Overlay sichtbar, Display an), wird ihm gemeldet.
//
// Alle Funktionen nur aus der Hauptschleife (loopTask) aufrufen. LVGL-Callbacks
// und der MQTT-Callback laufen dort ebenfalls.
// ==============================================================================

#include <Arduino.h>

// Betriebsart des Babyphones. Bewusst kein Bitfeld nach aussen: die vier
// Zustaende sind genau die, die der Benutzer waehlen kann.
enum class BabyMode : uint8_t {
    Off = 0,      // kein Kontakt zur Kamera (ausser die Baby-Ansicht ist offen)
    AudioOnly,    // Babyphone-Ton im Hintergrund, kein Video
    Video,        // Video ohne Ton
    VideoAudio,   // Video und Ton
};

inline bool BabyMode_HasVideo(BabyMode m) { return m == BabyMode::Video || m == BabyMode::VideoAudio; }
inline bool BabyMode_HasAudio(BabyMode m) { return m == BabyMode::AudioOnly || m == BabyMode::VideoAudio; }

// Setzt Video- und Audioanteil zu einem Modus zusammen.
BabyMode BabyMode_From(bool video, bool audio);

// Kurzer Name fuer Logausgaben ("Aus", "Audio", "Video", "Video+Audio").
const char* BabyMode_Name(BabyMode m);

void BabyService_Init();

// Abgleich von Wunsch und Wirklichkeit. In jedem Durchlauf der Hauptschleife
// aufrufen; die Funktion ist billig, solange sich nichts aendert.
void BabyService_Update();

// --- Wuensche --------------------------------------------------------------
void     BabyService_SetMode(BabyMode m);
BabyMode BabyService_GetMode();
void     BabyService_SetVideo(bool on);   // Audioanteil bleibt, wie er ist
void     BabyService_SetAudio(bool on);   // Videoanteil bleibt, wie er ist

// true, wenn Video gerade laufen soll (Modus mit Video und Dienst nicht
// pausiert, z.B. durch das Web-Setup). Grundlage fuer die Overlay-Anzeige.
bool     BabyService_IsVideoActive();

// true, solange das Video wegen knappen internen Speichers pausiert ist
// (Sicherheitsventil, siehe BabyService.cpp). Der Modus bleibt dabei
// unveraendert; Ton und Alarme laufen weiter.
bool     BabyService_IsVideoMemoryPaused();

// --- Meldungen der Oberflaeche ----------------------------------------------
// Baby-Ansicht sichtbar: haelt die Kamerasitzung auch im Modus "Aus" offen
// (Akkustand, Aufloesungsliste, Einstellungen) und ist Ziel der Videoausgabe.
void BabyService_SetBabyViewVisible(bool visible);

// Video-Overlay sichtbar (Rahmen steht, nichts liegt darueber). Wirkt sofort
// auf die Videoausgabe - beim Ausblenden darf kein weiteres Bild mehr ueber
// einen Dialog oder ein Alarm-Popup gezeichnet werden.
void BabyService_SetPipVisible(bool visible);

// Display per "Bildschirm aus" abgedunkelt: Video wird nicht mehr dekodiert,
// Audio laeuft weiter.
void BabyService_SetDisplayOn(bool on);

// Kurzbeschreibung des Zustands fuer Logzeilen, z.B. "Video+Audio | Overlay".
const char* BabyService_Describe();

// --- Aufloesungen -----------------------------------------------------------
// Die Kamera hat genau EINE Aufloesung; jede Aenderung startet ihren Stream
// neu (~3 s). Deshalb gilt sie je Abnehmer: die Baby-Ansicht bekommt die vom
// Benutzer gewaehlte, das Overlay eine eigene (Standard: die kleinste). Ohne
// sichtbaren Abnehmer (z.B. in den Einstellungen) bleibt die zuletzt gesetzte.

// Aufloesung fuer die Baby-Ansicht, Format "BxH". Wird gespeichert.
void   BabyService_SetBabyResolution(const String& res);

// Aufloesung fuer das Overlay. "" = automatisch (kleinste der Kamera).
void   BabyService_SetPipResolution(const String& res);
String BabyService_GetPipResolutionSetting();     // gespeicherter Wert, ggf. ""
String BabyService_GetPipResolution();            // tatsaechlich verwendete

// Von der Kamera gemeldete Aufloesungen, mit '\n' getrennt (Format fuer
// lv_dropdown). Ist die Kamera gerade nicht erreichbar, die zuletzt bekannte
// Liste aus dem Flash - die Liste haengt von Kamera und Linse ab.
String BabyService_GetResolutionOptions();

// Bildrate des Overlays (1..15 fps). Wird gespeichert.
void BabyService_SetPipFps(int fps);
int  BabyService_GetPipFps();
