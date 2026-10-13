#include "HaWidgetFactory.h"
#include "HAWidgets.h" 

HAWidget* HaWidgetFactory::createWidget(lv_obj_t* parent, int tab_idx, const HAWidgetDef& wDef) {
    HAWidget* new_widget = nullptr;
    const HaWidgetKind kind = HaWidgetKindFromString(wDef.type);

    if (kind == HaWidgetKind::Sensor) {
        new_widget = new HASensorWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str(), 
            wDef.show_chart, wDef.chart_w_pct, wDef.chart_h_pct, 
            wDef.chart_x_ofs, wDef.chart_y_ofs, wDef.chart_min, wDef.chart_max
        );
    } 
    else if (kind == HaWidgetKind::Action) {
        new_widget = new HAActionWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    } 
    else if (kind == HaWidgetKind::MediaPlayer) {
        new_widget = new HAMediaWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    } 
    else if (kind == HaWidgetKind::MediaItem) {
        new_widget = new HAMediaItemWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str(), 
            wDef.media_content_type, wDef.media_content_id
        );
    } 
    else if (kind == HaWidgetKind::Vacuum) {
        new_widget = new HAVacuumWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        ); 
    }
    else if (kind == HaWidgetKind::Cover) {
        new_widget = new HACoverWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    else if (kind == HaWidgetKind::Climate) {
        new_widget = new HAClimateWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    else if (kind == HaWidgetKind::Text) {
        new_widget = new HATextWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    else if (kind == HaWidgetKind::Folder) {
        HAFolderWidget* folder = new HAFolderWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
        
        for (const auto& childDef : wDef.children) {
            // Kinder fuellen den Ordner vollstaendig aus: gleiche Groesse wie der Ordner.
            HAWidgetDef realChildDef = childDef;
            realChildDef.w = wDef.w; 
            realChildDef.h = wDef.h; 
            
            HAWidget* child = HaWidgetFactory::createWidget(folder->getContainer(), tab_idx, realChildDef);
            if (child) {
                lv_obj_set_pos(child->getContainer(), 0, 0);
                // Absolute Pixelgroesse (nicht LV_PCT): die Widgets richten ihre
                // Beschriftungen beim Anlegen danach aus.
                folder->addChild(child, childDef);
            }
        }
        new_widget = folder;
    }
    else if (kind == HaWidgetKind::Select) {
        new_widget = new HASelectWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    else if (kind == HaWidgetKind::Number) {
        new_widget = new HANumberWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    else {
        new_widget = new HALightWidget(
            parent, tab_idx, wDef.type, wDef.entity_id, 
            wDef.x, wDef.y, wDef.w, wDef.h, 
            wDef.name.c_str(), wDef.mdi_icon.c_str(), wDef.color_on.c_str(), wDef.color_off.c_str()
        );
    }
    
    if (new_widget) {
        new_widget->setAlignments(
            wDef.icon_align, wDef.text_align, wDef.state_align, 
            wDef.icon_margin, wDef.text_margin, wDef.state_margin
        );
        new_widget->setSnapToGrid(wDef.snap_to_grid); 
        new_widget->setChartConfig(
            wDef.show_chart, wDef.chart_w_pct, wDef.chart_h_pct, 
            wDef.chart_x_ofs, wDef.chart_y_ofs, wDef.chart_min, wDef.chart_max
        );
        new_widget->setTapAction(
            wDef.tap_action_domain, wDef.tap_action_service, wDef.tap_action_target
        );

        new_widget->setBakedOptions(wDef.select_options);
        new_widget->setBakedLimits(wDef.slider_min, wDef.slider_max, wDef.slider_step);

        new_widget->setConditions(wDef.conditions_type, wDef.conditions);
    }

    return new_widget;
}

HAWidgetDef HaWidgetFactory::createDefFromWidget(HAWidget* w) {
    HAWidgetDef def;
    if (!w) return def;

    def.entity_id = w->getEntityId();
    def.type = w->getType();
    def.name = w->getName(); 
    def.mdi_icon = w->getMdiIcon(); 
    def.color_on = w->getColorOn(); 
    def.color_off = w->getColorOff(); 
    
    def.x = w->getX(); 
    def.y = w->getY(); 
    def.w = w->getW(); 
    def.h = w->getH();
    
    def.icon_align = w->getIconAlign();
    def.text_align = w->getTextAlign();
    def.state_align = w->getStateAlign();
    
    def.icon_margin = w->getIconMargin(); 
    def.text_margin = w->getTextMargin(); 
    def.state_margin = w->getStateMargin(); 
    def.snap_to_grid = w->getSnapToGrid(); 
    
    def.show_chart = w->getShowChart();
    def.chart_w_pct = w->getChartWPct();
    def.chart_h_pct = w->getChartHPct();
    def.chart_x_ofs = w->getChartXOfs();
    def.chart_y_ofs = w->getChartYOfs();
    def.chart_min = w->getChartMin();
    def.chart_max = w->getChartMax();
    
    def.media_content_type = w->getMediaContentType();
    def.media_content_id = w->getMediaContentId();
    
    def.tap_action_domain = w->getTapDomain();
    def.tap_action_service = w->getTapService();
    def.tap_action_target = w->getTapTarget();

    def.select_options = w->getBakedOptions();
    def.slider_min = w->getBakedMin();
    def.slider_max = w->getBakedMax();
    def.slider_step = w->getBakedStep();

    def.conditions_type = w->getConditionsType();
    def.conditions = w->getConditions();

    if (w->getKind() == HaWidgetKind::Folder) {
        def.children = ((HAFolderWidget*)w)->getChildrenDefs();
    }

    return def;
}