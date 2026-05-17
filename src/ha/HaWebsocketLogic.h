#pragma once
#include <Arduino.h>
#include <vector>
#include "HaEntityCache.h"
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

extern uint32_t lastLovelaceReqId;
extern uint32_t lastLovelaceDashboardsReqId;
extern uint32_t lastMediaBrowseReqId; 
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

extern std::vector<MediaBrowserItem> currentMediaFolder;
extern volatile bool pendingMediaBrowserUpdate;
extern volatile bool pendingMediaBrowserError;

void HaWebsocketLogic_Start();
void HaWebsocketLogic_Stop();
bool HaWebsocketLogic_IsConnected();
uint32_t HaWebsocketLogic_GetNextMessageId();
void HaWebsocketLogic_SendPayload(const String& payload);

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
inline bool HaWebsocketLogic_EntityExists(String id) { return HaEntityCache::EntityExists(id); }
inline String HaWebsocketLogic_GetEntityName(String id) { return HaEntityCache::GetEntityName(id); }
inline void HaWebsocketLogic_UpdateTrackedEntities() { HaEntityCache::UpdateTrackedEntities(); }
inline int HaWebsocketLogic_GetBattery(String id) { return HaEntityCache::GetBattery(id); }

// --- NEU: Getter für Options/Limits durchleiten ---
inline std::vector<String> HaWebsocketLogic_GetOptions(String id) { return HaEntityCache::GetOptions(id); }
inline float HaWebsocketLogic_GetMin(String id) { return HaEntityCache::GetMin(id); }
inline float HaWebsocketLogic_GetMax(String id) { return HaEntityCache::GetMax(id); }
inline float HaWebsocketLogic_GetStep(String id) { return HaEntityCache::GetStep(id); }

inline std::vector<String> HaWebsocketLogic_GetGlobalOptions(String id) { return HaEntityCache::GetGlobalOptions(id); }
inline float HaWebsocketLogic_GetGlobalMin(String id) { return HaEntityCache::GetGlobalMin(id); }
inline float HaWebsocketLogic_GetGlobalMax(String id) { return HaEntityCache::GetGlobalMax(id); }
inline float HaWebsocketLogic_GetGlobalStep(String id) { return HaEntityCache::GetGlobalStep(id); }

// --- NEU: Die Funktion für komplexe Payloads durchleiten ---
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
inline bool HaWebsocketLogic_SupportsBrightness(String id) { return HaEntityCache::SupportsBrightness(id); }
inline bool HaWebsocketLogic_SupportsColor(String id) { return HaEntityCache::SupportsColor(id); }
inline bool HaWebsocketLogic_SupportsColorTemp(String id) { return HaEntityCache::SupportsColorTemp(id); }
inline int HaWebsocketLogic_GetColorTemp(String id) { return HaEntityCache::GetColorTemp(id); }
inline void HaWebsocketLogic_CallLightServiceTemp(String id, int bri, int mireds) { HaServiceCaller::CallLightServiceTemp(id, bri, mireds); }