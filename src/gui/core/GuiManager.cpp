#include "GuiManager.h"
#include "ViewDashboard.h"
#include "ViewCatMat.h"
#include "ViewBaby.h"
#include "ViewSettings.h"
#include "ViewQuickSettings.h"
#include "ViewBootScreen.h"
#include "ViewPower.h" 
#include "LVGL_Driver.h" 
#include "SharedData.h" 
#include <WiFi.h>       
#include <M5Unified.h>
#include "ViewHomeAssistant.h" 
#include "HAWidgets.h"
#include "HaWebsocketLogic.h"

GuiManager gui;

void GuiManager::init() {
    currentScreen = (ScreenID)99; 

    HaWebsocketLogic_Start();   // HA Hintergrund-Dienst starten
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
        // HaWebsocketLogic_Start(); // Wird nun global in init() gestartet
    } else if (currentScreen == SCREEN_HA && newScreen != SCREEN_HA) {
        // WICHTIG: Alter Screen wird sauber aus RAM geloescht, um Fehler zu vermeiden!
        ViewHomeAssistant::clearWidgets(); 
        // HaWebsocketLogic_Stop(); // Websocket laeuft im Hintergrund weiter!
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
        // FIX: Wir ignorieren aufwaendige Slide-Animationen und erzwingen einen sauberen Schnitt (NONE),
        // damit der ESP32 die 1280x720 Pixel nicht in Zeitlupe ueber den Bildschirm ziehen muss.
        lv_scr_load_anim(nextScr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
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
        // --- FIX: ALLE WECHSEL WIEDER AUF 'LV_SCR_LOAD_ANIM_NONE' GEAENDERT ---
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