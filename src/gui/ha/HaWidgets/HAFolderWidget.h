#pragma once
#include "HAWidgetBase.h"
#include "HaConfigLogic.h"
#include <vector>

struct FolderChildInfo {
    HAWidget* widget;
    HAWidgetDef def; 
};

class HAFolderWidget : public HAWidget {
public:
    HAFolderWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    ~HAFolderWidget(); // FIX: Speicherleck verhindern!
    
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
    
    bool evaluate(const HAWidgetDef& def);
};