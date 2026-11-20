#include "ViewHomeAssistant.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "ViewTopbar.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"
#include "HaWidgetFactory.h" 

#include "HaDialogEdit.h"
#include "HaDialogAdd.h"
#include "HaDialogImport.h"
#include "HaDialogMedia.h" 
#include "HaDialogVacuum.h" 
#include "HaDialogLight.h" 
#include "HaColorPicker.h" 
#include "HaDialogTab.h"
#include "HaEditToolbar.h"
#include "HaDialogFolder.h" 
#include "HaDialogMap.h"
#include "HaTrackHistory.h"
#include "HaTheme.h"
#include "UiTheme.h"
#include "HeapMonitor.h"

#include <algorithm> 

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);

lv_obj_t* ViewHomeAssistant::screen = nullptr;
lv_obj_t* ViewHomeAssistant::tabview = nullptr;
lv_obj_t* ViewHomeAssistant::btn_edit = nullptr;
lv_obj_t* ViewHomeAssistant::label_edit = nullptr;
lv_obj_t* ViewHomeAssistant::trash_btn = nullptr;

std::vector<HAWidget*> ViewHomeAssistant::widgets;
std::vector<lv_obj_t*> ViewHomeAssistant::tab_pages; 
std::vector<size_t> ViewHomeAssistant::tab_progress;

// Hintergrund-Aufbau der uebrigen Tabs (siehe Header):
// Zeit je update()-Durchlauf - die Oberflaeche bleibt bedienbar.
static const uint32_t BACKGROUND_BUILD_BUDGET_MS = 20;
// Erst nach so vielen update()-Durchlaeufen beginnen: bis dahin hat LVGL das
// erste Bild des aktiven Tabs gezeichnet (ein lv_timer_handler zeichnet alle
// ungueltigen Bereiche in einem Zug).
static const uint32_t BACKGROUND_BUILD_START_UPDATES = 2;
static uint32_t s_updatesSinceBuild = 0;
static uint32_t s_backgroundStartMs = 0;
static size_t   s_backgroundWidgets = 0;

volatile bool ViewHomeAssistant::pendingHaReload = false;

// Erzwingt nach build() einen sofortigen Abgleich aller Widgets mit dem Cache.
static bool forceWidgetRefresh = false;
uint16_t ViewHomeAssistant::currentActiveTab = 0;

static lv_obj_t* btn_back = nullptr;

void ViewHomeAssistant::helper_loadWidgets() {
    clearWidgets();
    
    // Die Konfiguration liest normalerweise schon der Preload-Task beim Start
    // (HaConfigLogic::Init). Nur falls das (noch) nicht geschehen ist, hier
    // nachladen.
    if (HaConfigLogic::dashboards.empty()) {
        HaConfigLogic::Load();
    }
    
    HaWebsocketLogic_UpdateTrackedEntities();
    
    tab_pages.clear();
    tab_progress.assign(HaConfigLogic::dashboards.size(), 0);
    
    // Alle Tabs (Reiter) sofort, Widgets zunaechst nur im aktiven Tab.
    for (size_t i = 0; i < HaConfigLogic::dashboards.size(); i++) {
        lv_obj_t * tab = lv_tabview_add_tab(tabview, HaConfigLogic::dashboards[i].name.c_str());
        lv_obj_set_scroll_dir(tab, LV_DIR_VER);
        tab_pages.push_back(tab); 
    }
    
    lv_tabview_add_tab(tabview, "+");
    lv_obj_add_event_cb(tabview, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (currentActiveTab >= HaConfigLogic::dashboards.size()) currentActiveTab = 0;
    const uint32_t t0 = millis();
    ensureTabBuilt(currentActiveTab);
    Serial.printf("[HA-Start] Aktiver Tab: %u Widgets in %lu ms angelegt.\n",
                  (unsigned)widgets.size(), (unsigned long)(millis() - t0));

    s_updatesSinceBuild = 0;
    s_backgroundStartMs = 0;
    s_backgroundWidgets = 0;
}

bool ViewHomeAssistant::buildTabWidgets(size_t tab, uint32_t deadlineMs) {
    if (tab >= tab_progress.size() || tab >= tab_pages.size()) return true;
    const auto& defs = HaConfigLogic::dashboards[tab].widgets;

    while (tab_progress[tab] < defs.size()) {
        HAWidget* w = HaWidgetFactory::createWidget(tab_pages[tab], (int)tab, defs[tab_progress[tab]]);
        tab_progress[tab] = tab_progress[tab] + 1;
        if (w) {
            widgets.push_back(w);
            // Sofort mit dem bekannten Stand fuellen (der allgemeine Abgleich
            // in update() laeuft nur bei Aenderungen) und diesen Stand merken.
            w->needsRefresh();
            w->refreshFromCache();
        }
        if (deadlineMs != 0 && (int32_t)(millis() - deadlineMs) >= 0) break;
    }
    return tab_progress[tab] >= defs.size();
}

void ViewHomeAssistant::ensureTabBuilt(size_t tab) {
    buildTabWidgets(tab, 0);
}

void ViewHomeAssistant::ensureAllTabsBuilt() {
    for (size_t i = 0; i < tab_progress.size(); i++) buildTabWidgets(i, 0);
}

void ViewHomeAssistant::buildPendingTabs() {
    if (s_updatesSinceBuild < BACKGROUND_BUILD_START_UPDATES) {
        s_updatesSinceBuild = s_updatesSinceBuild + 1;
        return;
    }

    // Reihenfolge: Nachbarn des angezeigten Tabs zuerst (rechts, links, dann
    // weiter aussen) - dorthin wischt man als Naechstes. Wechselt der Tab
    // waehrenddessen, richtet sich die Reihenfolge nach dem neuen.
    const size_t count = tab_progress.size();
    const size_t active = (tabview && lv_tabview_get_tab_act(tabview) < count) ? lv_tabview_get_tab_act(tabview) : 0;
    size_t tab = count;
    for (size_t dist = 0; dist < count && tab == count; dist++) {
        const size_t right = active + dist;
        if (right < count && tab_progress[right] < HaConfigLogic::dashboards[right].widgets.size()) { tab = right; break; }
        if (dist > 0 && dist <= active) {
            const size_t left = active - dist;
            if (tab_progress[left] < HaConfigLogic::dashboards[left].widgets.size()) { tab = left; break; }
        }
    }
    if (tab >= count) {
        // Alles angelegt: der Positionsverlauf der Karte darf jetzt laden.
        HaTrackHistory::SetHold(false);
        return;
    }

    if (s_backgroundStartMs == 0) s_backgroundStartMs = millis();
    const size_t before = widgets.size();

    // Unter der LVGL-Sperre: der Video-Task fasst sein Bildobjekt ebenfalls nur
    // unter ihr an. Rekursiv - Aufrufe aus Ereignissen halten sie schon.
    if (!lvgl_port_lock(0)) return;   // gerade belegt: naechster Durchlauf
    buildTabWidgets(tab, millis() + BACKGROUND_BUILD_BUDGET_MS);
    lvgl_port_unlock();
    s_backgroundWidgets += widgets.size() - before;

    // Fertig? Einmal melden.
    bool all = true;
    for (size_t i = 0; i < tab_progress.size(); i++) {
        if (tab_progress[i] < HaConfigLogic::dashboards[i].widgets.size()) { all = false; break; }
    }
    if (all) {
        Serial.printf("[HA-Start] Uebrige Tabs: %u Widgets im Hintergrund angelegt (%lu ms).\n",
                      (unsigned)s_backgroundWidgets, (unsigned long)(millis() - s_backgroundStartMs));
        // Alle Kacheln stehen: ab hier ist der Speicherbedarf der HA-App
        // vollstaendig - der Stand, an dem das interne RAM am knappsten ist.
        HeapMonitor_Log("HA-Tabs fertig");
    }
}

void ViewHomeAssistant::captureActiveTabLayout() {
    // Ohne Tabs gibt es nichts zu uebernehmen. Load() stellt zwar mindestens
    // einen Tab sicher, der Zugriff auf dashboards[0] bleibt trotzdem geschuetzt.
    if (HaConfigLogic::dashboards.empty() || !tabview) return;

    uint16_t act_tab = lv_tabview_get_tab_act(tabview);
    if (act_tab >= HaConfigLogic::dashboards.size()) {
        act_tab = 0;
    }
    currentActiveTab = act_tab;

    // Noch nicht angelegte Widgets fehlten sonst im uebernommenen Layout -
    // der Tab verloere beim Speichern seine Widgets.
    ensureTabBuilt(act_tab);
    
    // Reihenfolge = Zeichenreihenfolge in LVGL (move_foreground beim Ziehen).
    std::stable_sort(widgets.begin(), widgets.end(), [](HAWidget* a, HAWidget* b) {
        int idxA = (a && a->container && lv_obj_is_valid(a->container)) ? lv_obj_get_index(a->container) : 999999;
        int idxB = (b && b->container && lv_obj_is_valid(b->container)) ? lv_obj_get_index(b->container) : 999999;
        return idxA < idxB;
    });

    HaConfigLogic::dashboards[act_tab].widgets.clear();
    HaConfigLogic::dashboards[act_tab].widgets.reserve(widgets.size());

    for (HAWidget* w : widgets) {
        // Geloeschte Widgets ueberspringen. Bewusst NICHT am HIDDEN-Flag
        // erkennen: ausgeblendet ist auch ein Widget, dessen Bedingung gerade
        // nicht erfuellt ist - es wuerde sonst beim Speichern verschwinden.
        if (!w || !w->container || !lv_obj_is_valid(w->container) || w->isMarkedDeleted()) {
            continue; 
        }
        
        if (w->getTabIndex() == act_tab) {
            HAWidgetDef def = HaWidgetFactory::createDefFromWidget(w);
            HaConfigLogic::dashboards[act_tab].widgets.push_back(def);
        }
    }
}

void ViewHomeAssistant::helper_saveWidgets() {
    captureActiveTabLayout();
    HaConfigLogic::Save();
    HaWebsocketLogic_UpdateTrackedEntities(); 
    pendingHaReload = true;
}

String ViewHomeAssistant::generateEntityId(String type, String input) {
    input.trim();
    if (input.indexOf('.') != -1) {
        String id = input; 
        id.toLowerCase(); 
        return id;
    }
    
    String formatted = input;
    formatted.toLowerCase(); 
    formatted.replace(" ", "_"); 
    formatted.replace("ä", "ae");
    formatted.replace("ö", "oe"); 
    formatted.replace("ü", "ue"); 
    formatted.replace("ß", "ss");
    
    return type + "." + formatted;
}

void ViewHomeAssistant::btn_back_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true);
    
    if (HAWidget::editModeActive) {
        HAWidget::editModeActive = false; 
        pendingHaReload = true; 
    } else {
        gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

void ViewHomeAssistant::tabview_event_cb(lv_event_t * e) {
    if (HAWidget::editModeActive) {
        return; 
    }
    
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t tab_id = lv_tabview_get_tab_act(tv);
    
    if (tab_id == HaConfigLogic::dashboards.size()) {
        HaDialogTab::showAddTabDialog();
    } else {
        // Vor dem Hintergrund-Aufbau angewaehlt: jetzt vollstaendig anlegen.
        ensureTabBuilt(tab_id);
        if (currentActiveTab != tab_id) {
            currentActiveTab = tab_id;
        }
    }
}

void ViewHomeAssistant::btn_edit_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
    // Bearbeiten nur mit allen Widgets (Ziehen zwischen Tabs, Speichern).
    ensureAllTabsBuilt();
    HAWidget::editModeActive = !HAWidget::editModeActive;
    HAWidget::applyEditModeScrollChain();
    
    if (HAWidget::editModeActive) {
        HaEditToolbar::show();
        // Widgets mit nicht erfuellter Bedingung sofort einblenden, damit
        // sie bearbeitet werden koennen (nicht erst beim naechsten 500-ms-Takt).
        HAWidget::checkAllConditions();
    } else {
        HaEditToolbar::hide();
        helper_saveWidgets();
    }
}

lv_obj_t* ViewHomeAssistant::build() {
    const uint32_t buildStart = millis();
    // Waehrend des Aufbaus keinen Positionsverlauf laden (grosse Antworten
    // kosten Rechenzeit auf demselben Kern). Freigabe in buildPendingTabs().
    HaTrackHistory::SetHold(true);
    trash_btn = nullptr; 

    HaDialogEdit::resetState(); 
    HaDialogAdd::resetState();
    HaDialogImport::resetState(); 
    HaDialogMedia::resetState(); 
    HaDialogTab::resetState();
    HaEditToolbar::resetState();

    // Ordner-Dialog, Licht-Dialog, Farbwaehler, Sauger-Dialog und
    // Text-Tastatur werden hier bewusst NICHT geschlossen.
    // Ihre Overlays haengen am alten Screen und werden mit ihm geloescht -
    // bei lv_scr_load_anim(..., 0, 0, true) sogar synchron. Ein zusaetzliches
    // hide()/del_async() traf frueher bereits freigegebenen Speicher.
    // Die Zeiger setzen die LV_EVENT_DELETE-Handler der Overlays zurueck.

    pendingHaReload = false; 
    forceWidgetRefresh = true;
    HAWidget::editModeActive = false; 
    
    HaWebsocketLogic_Start();
    
    HAWidget::onEditRequested = HaDialogEdit::showWidgetEditDialog;
    HAWidget::onDeleteRequested = HaDialogEdit::handleWidgetDeleteDrop;
    HAWidget::onLightControlRequested = HaDialogLight::show;
    HAWidget::onMediaControlRequested = HaDialogMedia::showMediaControlDialog;
    HAWidget::onVacuumControlRequested = HaDialogVacuum::showVacuumDialog; 
    
    screen = lv_obj_create(NULL);
    if (!screen) {
        return nullptr;
    }

    const UiColors& c = UiTheme::c();
    lv_obj_set_style_bg_color(screen, c.bg, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(screen, true);

    tabview = lv_tabview_create(screen, LV_DIR_TOP, 60);
    lv_obj_set_size(tabview, 1280, 640); 
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_scroll_dir(lv_tabview_get_content(tabview), LV_DIR_VER);

    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, c.side, 0);
    lv_obj_set_style_border_color(tab_btns, c.divider, 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(tab_btns, c.muted, 0); 
    lv_obj_set_style_text_color(tab_btns, c.text, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, c.accent, LV_PART_ITEMS | LV_STATE_CHECKED);

    helper_loadWidgets();

    btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 160, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 15);
    lv_obj_set_style_bg_color(btn_back, HaTheme::control(), 0);
    lv_obj_set_style_text_color(btn_back, c.text, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_back = lv_label_create(btn_back); 
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " ZURUECK"); 
    lv_obj_center(lbl_back);

    btn_edit = lv_btn_create(screen);
    lv_obj_set_size(btn_edit, 200, 50);
    lv_obj_align(btn_edit, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(btn_edit, btn_edit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_edit, HaTheme::control(), 0);
    lv_obj_set_style_text_color(btn_edit, c.text, 0);
    lv_obj_set_style_shadow_width(btn_edit, 0, 0);
    
    label_edit = lv_label_create(btn_edit); 
    lv_label_set_text(label_edit, LV_SYMBOL_EDIT " BEARBEITEN"); 
    lv_obj_center(label_edit);

    if (HaConfigLogic::dashboards.size() > 0) {
        if (currentActiveTab >= HaConfigLogic::dashboards.size()) {
            currentActiveTab = 0;
        }
        lv_tabview_set_act(tabview, currentActiveTab, LV_ANIM_OFF);
    }

    Serial.printf("[HA-Start] Bildschirm aufgebaut in %lu ms (bei %lu ms).\n",
                  (unsigned long)(millis() - buildStart), (unsigned long)millis());
    return screen;
}

void ViewHomeAssistant::update() {
    // Antworten fuer Import und Medien-Browser, vom HA_WS_Task abgelegt, hier
    // im loopTask auswerten (sie veraendern Daten, die die Oberflaeche liest).
    HaWebsocketLogic_ProcessPendingResponses();

    HaDialogImport::checkPendingEvents();
    HaDialogMedia::update();

    // Entitaetsliste neu geladen -> offene Hinzufuegen-Vorschau aktualisieren.
    static uint32_t lastCatalogVersion = 0;
    const uint32_t catalogVersion = HaEntityCatalog::GetVersion();
    if (catalogVersion != lastCatalogVersion) {
        lastCatalogVersion = catalogVersion;
        HaDialogAdd::onCatalogUpdated();
    }
    
    static uint32_t lastConditionCheck = 0;
    if (millis() - lastConditionCheck > 500) {
        lastConditionCheck = millis();
        HAWidget::checkAllConditions();
    }
    
    static uint32_t reloadWait = 0;
    
    if (pendingHaReload) { 
        if (reloadWait == 0) {
            reloadWait = millis();
        }
        
        if (millis() - reloadWait > 150) {
            pendingHaReload = false; 
            reloadWait = 0;
            
            clearWidgets(); 
            
            lv_obj_t* new_scr = ViewHomeAssistant::build();
            lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
        }
        return; 
    } else {
        reloadWait = 0;
    }

    if (gui.getCurrentScreen() != SCREEN_HA) {
        return;
    }
    
    ViewTopbar_Update();

    // Uebrige Tabs stueckweise anlegen (schneller Start, siehe Header).
    buildPendingTabs();

    // Vollbildkarte: Positionen, Wege und Kacheln nachziehen.
    HaDialogMap::update();

    static uint32_t lastWidgetUpdate = 0;
    static uint32_t localCacheVersion = 0;
    uint32_t currentVersion = HaEntityCache::GetCacheVersion();

    // Nach einem Neuaufbau sofort alle Widgets fuellen, nicht erst bei der
    // naechsten Zustandsaenderung - sonst zeigten alle kurz "unknown".
    const bool force = forceWidgetRefresh;

    if (force || (currentVersion != localCacheVersion && (millis() - lastWidgetUpdate > 100))) {
        forceWidgetRefresh = false;
        localCacheVersion = currentVersion;
        lastWidgetUpdate = millis();
        
        // Nur Widgets, deren Entitaeten sich geaendert haben. needsRefresh()
        // immer aufrufen (merkt sich den Stand), auch beim Erzwingen.
        for (HAWidget* w : widgets) {
            const bool changed = w->needsRefresh();
            if (force || changed) w->refreshFromCache();
        }
    }

    // Widgets mit eigenem Takt (Karte: nachgeladene Kacheln).
    for (HAWidget* w : widgets) {
        w->tick();
    }

    // Nach einer Bedienung waren Serverwerte kurz gesperrt. Ist die Sperre
    // abgelaufen, den aktuellen Stand einmal nachziehen.
    static uint32_t lastHoldCheck = 0;
    if (millis() - lastHoldCheck > 250) {
        lastHoldCheck = millis();
        // Alle lebenden Widgets - auch Ordnerkinder (Licht im Ordner).
        for (HAWidget* w : HAWidget::all_active_widgets) {
            if (w->refreshAfterHoldDue()) {
                w->updateState(HaWebsocketLogic_GetState(w->getEntityId()));
            }
        }
    }
}

void ViewHomeAssistant::clearWidgets() {
    tab_progress.clear();
    for (HAWidget* w : widgets) {
        if (w->container && lv_obj_is_valid(w->container)) {
            lv_obj_remove_event_cb(w->container, nullptr); 
        }
        w->container = nullptr; 
        delete w; 
    }
    widgets.clear();
}