#pragma once
#include "HAWidgetBase.h"

class HAMediaWidget : public HAWidget {
public:
    HAMediaWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off);
    
    void updateState(String state) override;
    void onClick() override;
};

class HAMediaItemWidget : public HAWidget {
private:
    String media_content_type;
    String media_content_id;

public:
    HAMediaItemWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off, String m_c_type, String m_c_id);
    
    void updateState(String state) override;
    void onClick() override;
    
    String getMediaContentType() override;
    String getMediaContentId() override;
};