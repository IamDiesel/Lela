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

#include <algorithm> 

lv_obj_t* ViewHomeAssistant::screen = nullptr;
lv_obj_t* ViewHomeAssistant::tabview = nullptr;
lv_obj_t* ViewHomeAssistant::btn_edit = nullptr;
lv_obj_t* ViewHomeAssistant::label_edit = nullptr;
lv_obj_t* ViewHomeAssistant::trash_btn = nullptr;

std::vector<HAWidget*> ViewHomeAssistant::widgets;
std::vector<lv_obj_t*> ViewHomeAssistant::tab_pages; 

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
    
    for (size_t i = 0; i < HaConfigLogic::dashboards.size(); i++) {
        lv_obj_t * tab = lv_tabview_add_tab(tabview, HaConfigLogic::dashboards[i].name.c_str());
        lv_obj_set_scroll_dir(tab, LV_DIR_VER);
        tab_pages.push_back(tab); 
        
        for (const auto& wDef : HaConfigLogic::dashboards[i].widgets) {
            HAWidget* new_widget = HaWidgetFactory::createWidget(tab, i, wDef);
            if (new_widget) {
                widgets.push_back(new_widget);
            }
        }
    }
    
    lv_tabview_add_tab(tabview, "+");
    lv_obj_add_event_cb(tabview, tabview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
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
        if (currentActiveTab != tab_id) {
            currentActiveTab = tab_id;
        }
    }
}

void ViewHomeAssistant::btn_edit_event_cb(lv_event_t * e) {
    playToneI2S(800, 100, true); 
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

    lv_obj_set_style_bg_color(screen, isDarkMode ? lv_color_hex(0x111111) : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, GuiManager::gestureEventWrapper, LV_EVENT_GESTURE, &gui);

    ViewTopbar_Create(screen);

    tabview = lv_tabview_create(screen, LV_DIR_TOP, 60);
    lv_obj_set_size(tabview, 1280, 640); 
    lv_obj_align(tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(tabview, 0, 0);
    lv_obj_set_scroll_dir(lv_tabview_get_content(tabview), LV_DIR_VER);

    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, isDarkMode ? lv_color_hex(0x222222) : lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    helper_loadWidgets();

    btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 160, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 15);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_back = lv_label_create(btn_back); 
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " ZURUECK"); 
    lv_obj_center(lbl_back);

    btn_edit = lv_btn_create(screen);
    lv_obj_set_size(btn_edit, 200, 50);
    lv_obj_align(btn_edit, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(btn_edit, btn_edit_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_edit, lv_color_hex(0x333333), 0);
    
    label_edit = lv_label_create(btn_edit); 
    lv_label_set_text(label_edit, LV_SYMBOL_EDIT " BEARBEITEN"); 
    lv_obj_center(label_edit);

    if (HaConfigLogic::dashboards.size() > 0) {
        if (currentActiveTab >= HaConfigLogic::dashboards.size()) {
            currentActiveTab = 0;
        }
        lv_tabview_set_act(tabview, currentActiveTab, LV_ANIM_OFF);
    }

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
        
        for (HAWidget* w : widgets) {
            String state = HaWebsocketLogic_GetState(w->getEntityId());
            // Ordner haben keinen eigenen Zustand, reichen das Update aber an ihre Kinder weiter.
            if (state.length() > 0 || w->getKind() == HaWidgetKind::Folder) {
                w->updateState(state);
            }
        }
    }

    // Nach einer Bedienung waren Serverwerte kurz gesperrt. Ist die Sperre
    // abgelaufen, den aktuellen Stand einmal nachziehen.
    static uint32_t lastHoldCheck = 0;
    if (millis() - lastHoldCheck > 250) {
        lastHoldCheck = millis();
        for (HAWidget* w : widgets) {
            if (w->refreshAfterHoldDue()) {
                w->updateState(HaWebsocketLogic_GetState(w->getEntityId()));
            }
        }
    }
}

void ViewHomeAssistant::clearWidgets() {
    for (HAWidget* w : widgets) {
        if (w->container && lv_obj_is_valid(w->container)) {
            lv_obj_remove_event_cb(w->container, nullptr); 
        }
        w->container = nullptr; 
        delete w; 
    }
    widgets.clear();
}