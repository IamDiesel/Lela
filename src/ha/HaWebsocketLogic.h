#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern std::vector<String> availableDashboardUrls;
extern std::vector<String> availableDashboardTitles;
extern volatile bool pendingDashboardList;
extern volatile bool pendingImportError;
extern String importErrorMessage; 

extern std::vector<String> availableViewTitles;
extern volatile bool pendingViewList;
extern String currentImportUrl;

extern volatile bool isImporting;

// NEU: Struktur fuer den Medien-Browser
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

String HaWebsocketLogic_GetState(String entity_id);
String HaWebsocketLogic_GetCachedIcon(String entity_id);

int HaWebsocketLogic_GetBrightness(String entity_id);
uint32_t HaWebsocketLogic_GetRGB(String entity_id);
int HaWebsocketLogic_GetWhite(String entity_id);
bool HaWebsocketLogic_IsRGBW(String entity_id);

String HaWebsocketLogic_GetUnit(String entity_id);
String HaWebsocketLogic_GetMediaTitle(String entity_id);
String HaWebsocketLogic_GetMediaArtist(String entity_id);
float HaWebsocketLogic_GetMediaVolume(String entity_id);
std::vector<String> HaWebsocketLogic_GetMediaSourceList(String entity_id);
String HaWebsocketLogic_GetMediaSource(String entity_id);

bool HaWebsocketLogic_EntityExists(String entity_id);
String HaWebsocketLogic_GetEntityName(String entity_id);

bool HaWebsocketLogic_IsConnected();
void HaWebsocketLogic_CallService(String domain, String service, String entity_id);
void HaWebsocketLogic_CallLightService(String entity_id, int brightness, int r, int g, int b, int w);

void HaWebsocketLogic_CallMediaService(String entity_id, String service);
void HaWebsocketLogic_CallMediaVolumeService(String entity_id, float volume);
void HaWebsocketLogic_CallMediaSelectSource(String entity_id, String source);

// NEU: Medien durchsuchen und abspielen
void HaWebsocketLogic_RequestBrowseMedia(String entity_id, String media_content_type = "", String media_content_id = "");
void HaWebsocketLogic_CallPlayMedia(String entity_id, String media_content_type, String media_content_id);

void HaWebsocketLogic_RequestDashboardList(int target_tab_index);
void HaWebsocketLogic_RequestDashboardViews(String url_path);
void HaWebsocketLogic_RequestDashboardCards(String url_path, int view_index);

void HaWebsocketLogic_UpdateTrackedEntities();