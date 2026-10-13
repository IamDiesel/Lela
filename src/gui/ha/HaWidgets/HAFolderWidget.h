#pragma once
#include "HAWidgetBase.h"
#include "HaConfigLogic.h"
#include <vector>

// 'def' ist der Stand beim Anlegen des Kindes (nur fuer die Anzeige im
// Ordner-Dialog). Fuer Bedingungen gilt immer der aktuelle Stand am Widget,
// damit Aenderungen aus dem Bearbeiten-Dialog sofort wirken.
struct FolderChildInfo {
    HAWidget* widget;
    HAWidgetDef def; 
};

class HAFolderWidget : public HAWidget {
public:
    HAFolderWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    ~HAFolderWidget(); // gibt die Kinder frei (der Ordner besitzt sie)
    
    void updateState(String state) override;
    void checkConditions() override;
    void onClick() override;
    
    void addChild(HAWidget* widget, const HAWidgetDef& def);
    void removeChild(HAWidget* widget); 
    std::vector<HAWidgetDef> getChildrenDefs();
    std::vector<FolderChildInfo>& getFolderChildren() { return folder_children; } 

private:
    std::vector<FolderChildInfo> folder_children;
    lv_obj_t* placeholder_lbl;
};