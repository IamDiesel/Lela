#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include "HaEntityCache.h"
#include "HaEntityCatalog.h"
#include "HaEntitySync.h"
#include "HaServiceCaller.h"

extern std::vector<String> availableDashboardUrls;
extern std::vector<String> availableDashboardTitles;
extern volatile bool pendingDashboardList;
extern volatile bool pendingImportError;
extern String importErrorMessage; 

extern std::vector<String> availableViewTitles;
extern volatile bool pendingViewList;
extern String currentImportUrl;
extern volatile bool isImporting;

extern volatile uint32_t lastLovelaceReqId;
extern volatile uint32_t lastLovelaceDashboardsReqId;
extern volatile uint32_t lastMediaBrowseReqId; 
extern bool requestingViewsOnly; 
extern int targetViewIndex;
extern int currentImportTab;

struct MediaBrowserItem {
    String title;
    String media_content_type;
    String media_content_id;
    bool can_expand;
    bool can_play;
};

// Aktueller Ordner des Medien-Browsers. Liegt als JSON im PSRAM - eine
// Liste mit je drei Strings pro Eintrag kostete frueher bei grossen Ordnern
// (z. B. "Applications") spuerbar internes RAM. Nur im loopTask benutzen.
size_t HaWebsocketLogic_MediaFolderCount();
bool   HaWebsocketLogic_GetMediaItem(size_t index, MediaBrowserItem& out);
void   HaWebsocketLogic_ClearMediaFolder();
extern volatile bool pendingMediaBrowserUpdate;
extern volatile bool pendingMediaBrowserError;

void HaWebsocketLogic_Start();
void HaWebsocketLogic_Stop();
bool HaWebsocketLogic_IsConnected();

// Sendet eine Anfrage. Die Nachrichten-ID wird hier vergeben (in 'doc'
// eingetragen) - unter derselben Sperre wie das Senden, damit die IDs bei HA
// streng steigend ankommen. 'idOut' wird VOR dem Senden gesetzt, damit eine
// sofort eintreffende Antwort schon zugeordnet werden kann.
// Rueckgabe: vergebene ID, 0 = nicht gesendet (nicht angemeldet, Sperre belegt).
uint32_t HaWebsocketLogic_SendRequest(JsonDocument& doc, volatile uint32_t* idOut = nullptr);

// Wertet im loopTask die Antworten fuer Medien-Browser und Lovelace-Import aus
// (vom HA_WS_Task nur abgelegt). Vor HaDialogImport/HaDialogMedia aufrufen.
void HaWebsocketLogic_ProcessPendingResponses();

// Laufende Anfragen verwerfen; spaete Antworten werden ignoriert.
void HaWebsocketLogic_CancelImport();
void HaWebsocketLogic_CancelMediaBrowse();

inline String HaWebsocketLogic_GetState(String id) { return HaEntityCache::GetState(id); }
inline String HaWebsocketLogic_GetCachedIcon(String id) { return HaEntityCache::GetCachedIcon(id); }
inline int HaWebsocketLogic_GetBrightness(String id) { return HaEntityCache::GetBrightness(id); }
inline uint32_t HaWebsocketLogic_GetRGB(String id) { return HaEntityCache::GetRGB(id); }
inline int HaWebsocketLogic_GetWhite(String id) { return HaEntityCache::GetWhite(id); }
inline bool HaWebsocketLogic_IsRGBW(String id) { return HaEntityCache::IsRGBW(id); }
inline String HaWebsocketLogic_GetUnit(String id) { return HaEntityCache::GetUnit(id); }
inline String HaWebsocketLogic_GetMediaTitle(String id) { return HaEntityCache::GetMediaTitle(id); }
inline String HaWebsocketLogic_GetMediaArtist(String id) { return HaEntityCache::GetMediaArtist(id); }
inline float HaWebsocketLogic_GetMediaVolume(String id) { return HaEntityCache::GetMediaVolume(id); }
inline std::vector<String> HaWebsocketLogic_GetMediaSourceList(String id) { return HaEntityCache::GetMediaSourceList(id); }
inline String HaWebsocketLogic_GetMediaSource(String id) { return HaEntityCache::GetMediaSource(id); }
// --- Entitaeten: Live-Cache (platzierte) + Katalog (alle, auf Anforderung) ---
// Bekannt ist eine Entitaet, wenn sie abonniert ist oder im Katalog steht.
inline bool HaWebsocketLogic_EntityExists(String id) { return HaEntityCache::EntityExists(id) || HaEntityCatalog::Contains(id); }
inline String HaWebsocketLogic_GetEntityName(String id) {
    String n = HaEntityCache::GetEntityName(id);
    return n.length() > 0 ? n : HaEntityCatalog::GetName(id);
}
inline String HaWebsocketLogic_GetEntityIcon(String id) {
    String ic = HaEntityCache::GetCachedIcon(id);
    return ic.length() > 0 ? ic : HaEntityCatalog::GetIcon(id);
}
// Laedt die Liste aller Entitaeten, falls noch nicht vorhanden (nicht blockierend).
inline void HaWebsocketLogic_RequestEntityCatalog() { HaEntityCatalog::RequestLoad(); }
inline std::vector<String> HaWebsocketLogic_SearchEntities(String q, size_t maxResults = 5) { return HaEntityCatalog::Search(q, maxResults); }

// Liste der abonnierten Entitaeten neu aufbauen und Abos nachziehen.
inline void HaWebsocketLogic_UpdateTrackedEntities() {
    HaEntityCache::UpdateTrackedEntities();
    HaEntitySync::RequestResync();
}
inline int HaWebsocketLogic_GetBattery(String id) { return HaEntityCache::GetBattery(id); }

inline std::vector<String> HaWebsocketLogic_GetOptions(String id) { return HaEntityCache::GetOptions(id); }
inline float HaWebsocketLogic_GetMin(String id) { return HaEntityCache::GetMin(id); }
inline float HaWebsocketLogic_GetMax(String id) { return HaEntityCache::GetMax(id); }
inline float HaWebsocketLogic_GetStep(String id) { return HaEntityCache::GetStep(id); }
// Werte zum "Backen" neuer Widgets: aus dem Katalog, sonst aus dem Live-Cache
// (Entitaet ist bereits platziert), sonst Standard.
inline std::vector<String> HaWebsocketLogic_GetGlobalOptions(String id) {
    std::vector<String> o = HaEntityCatalog::GetOptions(id);
    return o.empty() ? HaEntityCache::GetOptions(id) : o;
}
inline float HaWebsocketLogic_GetGlobalMin(String id) {
    float mn, mx, st;
    return HaEntityCatalog::GetLimits(id, mn, mx, st) ? mn : HaEntityCache::GetMin(id);
}
inline float HaWebsocketLogic_GetGlobalMax(String id) {
    float mn, mx, st;
    return HaEntityCatalog::GetLimits(id, mn, mx, st) ? mx : HaEntityCache::GetMax(id);
}
inline float HaWebsocketLogic_GetGlobalStep(String id) {
    float mn, mx, st;
    return HaEntityCatalog::GetLimits(id, mn, mx, st) ? st : HaEntityCache::GetStep(id);
}

inline int HaWebsocketLogic_GetPosition(String id) { return HaEntityCache::GetPosition(id); }
inline void HaWebsocketLogic_CallCoverService(String id, String s) { HaServiceCaller::CallCoverService(id, s); }
inline void HaWebsocketLogic_CallCoverPosition(String id, int pos) { HaServiceCaller::CallCoverPosition(id, pos); }

inline float HaWebsocketLogic_GetCurrentTemperature(String id) { return HaEntityCache::GetCurrentTemperature(id); }
inline float HaWebsocketLogic_GetTargetTemperature(String id) { return HaEntityCache::GetTargetTemperature(id); }
inline void HaWebsocketLogic_CallClimateSetTemperature(String id, float t) { HaServiceCaller::CallClimateSetTemperature(id, t); }
inline void HaWebsocketLogic_CallClimateSetHvacMode(String id, String m) { HaServiceCaller::CallClimateSetHvacMode(id, m); }
// Text-Widgets
inline void HaWebsocketLogic_CallTextSetValue(String id, String val) { HaServiceCaller::CallTextSetValue(id, val); }
inline void HaWebsocketLogic_CallServiceWithData(String d, String s, String id, String j) { HaServiceCaller::CallServiceWithData(d, s, id, j); }
inline void HaWebsocketLogic_CallVacuumService(String id, String s) { HaServiceCaller::CallVacuumService(id, s); }
inline void HaWebsocketLogic_CallVacuumSetFanSpeed(String id, String s) { HaServiceCaller::CallVacuumSetFanSpeed(id, s); }
inline void HaWebsocketLogic_CallService(String d, String s, String id) { HaServiceCaller::CallService(d, s, id); }
inline void HaWebsocketLogic_CallLightService(String id, int bri, int r, int g, int b, int w) { HaServiceCaller::CallLightService(id, bri, r, g, b, w); }
inline void HaWebsocketLogic_CallMediaService(String id, String s) { HaServiceCaller::CallMediaService(id, s); }
inline void HaWebsocketLogic_CallMediaVolumeService(String id, float v) { HaServiceCaller::CallMediaVolumeService(id, v); }
inline void HaWebsocketLogic_CallMediaSelectSource(String id, String src) { HaServiceCaller::CallMediaSelectSource(id, src); }
inline void HaWebsocketLogic_RequestBrowseMedia(String id, String t="", String cid="") { HaServiceCaller::RequestBrowseMedia(id, t, cid); }
inline void HaWebsocketLogic_CallPlayMedia(String id, String t, String cid) { HaServiceCaller::CallPlayMedia(id, t, cid); }
inline void HaWebsocketLogic_RequestDashboardList(int t) { HaServiceCaller::RequestDashboardList(t); }
inline void HaWebsocketLogic_RequestDashboardViews(String u) { HaServiceCaller::RequestDashboardViews(u); }
inline void HaWebsocketLogic_RequestDashboardCards(String u, int v) { HaServiceCaller::RequestDashboardCards(u, v); }
inline String HaWebsocketLogic_GetFanSpeed(String id) { return HaEntityCache::GetFanSpeed(id); }
inline std::vector<String> HaWebsocketLogic_GetFanSpeedList(String id) { return HaEntityCache::GetFanSpeedList(id); }
inline bool HaWebsocketLogic_SupportsBrightness(String id) { return HaEntityCache::SupportsBrightness(id); }
inline bool HaWebsocketLogic_SupportsColor(String id) { return HaEntityCache::SupportsColor(id); }
inline bool HaWebsocketLogic_SupportsColorTemp(String id) { return HaEntityCache::SupportsColorTemp(id); }
inline int HaWebsocketLogic_GetColorTemp(String id) { return HaEntityCache::GetColorTemp(id); }
inline void HaWebsocketLogic_CallLightServiceTemp(String id, int bri, int mireds) { HaServiceCaller::CallLightServiceTemp(id, bri, mireds); }