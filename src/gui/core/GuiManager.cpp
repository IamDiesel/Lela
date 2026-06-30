#include "GuiManager.h"
#include "ViewDashboard.h"
#include "ViewCatMat.h"
#include "ViewBaby.h"
#include "ViewSettings.h"
#include "ViewQuickSettings.h"
#include "ViewBootScreen.h"
#include "ViewPower.h" // NEU
#include "LVGL_Driver.h" 
#include "SharedData.h" 
#include <WiFi.h>       
#include <M5Unified.h>
#include "ViewHomeAssistant.h" 
#include "HAWidgets.h"
#include "HaWebsocketLogic.h"
#include "HaConfigLogic.h"

GuiManager gui;

// --- NEU: Der Fire-and-Forget Boot-Task ---
static void configPreloadTask(void* parameter) {
    // Initialisiert LittleFS und laedt ha_config.json direkt in den PSRAM.
    // Läuft im Hintergrund, blockiert nicht die UI.
    HaConfigLogic::Init();
    
    // Task ist fertig und beendet sich selbst (gibt seinen Arbeitsspeicher wieder frei)
    vTaskDelete(NULL);
}

void GuiManager::init() {
    currentScreen = (ScreenID)99; 
    
    // --- NEU: Task auf Core 0 (Netzwerk-Kern) abfeuern ---
    // Stacksize 8192 reicht locker, Prio 1 ist niedrig genug, um nichts zu stören.
    xTaskCreatePinnedToCore(configPreloadTask, "Preload_HA_Config", 8192, NULL, 1, NULL, 0);

    ViewPower::init();          // Timer fuer Display-Wakeup bei Alarmen
    ViewBootScreen::show();     // Vektorgrafiken und Ladesequenz
}

ScreenID GuiManager::getCurrentScreen() const { 
    return currentScreen; 
}

void GuiManager::switchScreen(ScreenID newScreen, lv_scr_load_anim_t anim_type) {
    if (currentScreen == newScreen) {
        return;
    }

    if (currentScreen == SCREEN_BABY) {
        requestBabyStream = false;
    }

    if (newScreen == SCREEN_HA) {
        HaWebsocketLogic_Start();
    } else if (currentScreen == SCREEN_HA && newScreen != SCREEN_HA) {
        ViewHomeAssistant::clearWidgets();
        HaWebsocketLogic_Stop(); 
    }

    ViewQuickSettings::hide();
    
    if (HAWidget::editModeActive) {
        HAWidget::editModeActive = false;
        ViewHomeAssistant::pendingHaReload = true; 
    }

    lv_obj_t* nextScr = nullptr;
    switch(newScreen) {
        case SCREEN_DASHBOARD: 
            nextScr = ViewDashboard::build(); 
            break;
        case SCREEN_CATMAT:    
            nextScr = ViewCatMat::build(); 
            break;
        case SCREEN_BABY:      
            nextScr = ViewBaby::build(); 
            break;
        case SCREEN_SETTINGS:  
            nextScr = ViewSettings::build(); 
            break;
        case SCREEN_HA:        
            nextScr = ViewHomeAssistant::build(); 
            break;
    }
    
    if (nextScr != nullptr) {
        lv_scr_load_anim(nextScr, anim_type, 0, 0, true);
        currentScreen = newScreen;
    }
}

void GuiManager::gestureEventWrapper(lv_event_t * e) { 
    GuiManager * manager = (GuiManager *)lv_event_get_user_data(e); 
    manager->handleGesture(e); 
}

void GuiManager::handleGesture(lv_event_t * e) {
    if (vidFSMode) {
        return;
    }
    
    if (currentScreen == SCREEN_HA && HAWidget::editModeActive) {
        return; 
    }
    
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    
    if (ViewQuickSettings::isActive() && dir == LV_DIR_TOP) { 
        ViewQuickSettings::hide(); 
        return; 
    }
    
    if (!ViewQuickSettings::isActive()) {
        if (currentScreen == SCREEN_DASHBOARD) {
            if (dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_CATMAT, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_RIGHT) {
                switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_TOP) {
                switchScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE);
            }
            if (dir == LV_DIR_BOTTOM) {
                ViewQuickSettings::toggle(this);
            }
        } 
        else if (currentScreen == SCREEN_CATMAT) { 
            if (dir == LV_DIR_RIGHT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                ViewQuickSettings::toggle(this); 
            }
        }
        else if (currentScreen == SCREEN_BABY) { 
            if (dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                ViewQuickSettings::toggle(this); 
            }
        }
        else if (currentScreen == SCREEN_SETTINGS) { 
            if (dir == LV_DIR_RIGHT || dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
        }
        else if (currentScreen == SCREEN_HA) { 
            if (dir == LV_DIR_RIGHT || dir == LV_DIR_LEFT) {
                switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE); 
            }
            if (dir == LV_DIR_BOTTOM) {
                ViewQuickSettings::toggle(this); 
            }
        }
    }
}