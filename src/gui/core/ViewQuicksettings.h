#pragma once
#include <lvgl.h>

// Forward Declaration, um Zirkelbezüge zu vermeiden
class GuiManager;

class ViewQuickSettings {
public:
    // Initialisiert Hintergrund-Timer (z.B. für Display-Wake bei Alarm)
    static void init();
    
    // Öffnet oder schließt das Overlay
    static void toggle(GuiManager* guiManager);
    
    // Erzwingt das Schließen (z.B. bei Screen-Wechsel)
    static void hide();
    
    // Gibt zurück, ob das Overlay gerade sichtbar ist
    static bool isActive();
};