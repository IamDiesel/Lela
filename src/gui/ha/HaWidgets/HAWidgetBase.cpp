#include "HAWidgetBase.h"
#include "HaWebsocketLogic.h"
#include "MdiMapper.h"
#include <algorithm> 

bool HAWidget::editModeActive = false;
void (*HAWidget::onEditRequested)(HAWidget*) = nullptr;
void (*HAWidget::onDeleteRequested)(HAWidget*) = nullptr;
void (*HAWidget::onLightControlRequested)(HAWidget*) = nullptr; 
void (*HAWidget::onMediaControlRequested)(HAWidget*) = nullptr; 
void (*HAWidget::onVacuumControlRequested)(HAWidget*) = nullptr; 

static bool widget_is_dragging = false;

std::vector<HAWidget*> HAWidget::all_active_widgets;

void HAWidget::checkAllConditions() {
    for (auto w : all_active_widgets) {
        if (w) w->checkConditions();
    }
}

// ------------------------------------------------------------------------------
// Sichtbarkeitsbedingungen
// ------------------------------------------------------------------------------
// Liest eine Zahl streng: die ganze Zeichenkette muss eine Zahl sein.
// String::toFloat() liefert fuer "unavailable" still 0 - das fuehrte zu
// falsch erfuellten Bedingungen.
bool HAWidget::parseNumber(const String& s, float& out) {
    const char* begin = s.c_str();
    if (*begin == '\0') return false;
    char* end = nullptr;
    out = strtof(begin, &end);
    if (end == begin) return false;
    while (*end == ' ') end++;
    return *end == '\0';
}

bool HAWidget::evaluateConditions(const String& type, const std::vector<HACondition>& conds) {
    if (conds.empty()) return true;

    const bool isAnd = (type != "OR");

    for (const auto& c : conds) {
        const String state = HaWebsocketLogic_GetState(c.entity);
        bool met = false;

        if (c.op == "==")      met = (state == c.value);
        else if (c.op == "!=") met = (state != c.value);
        else {
            float s_f, v_f;
            if (parseNumber(state, s_f) && parseNumber(c.value, v_f)) {
                if (c.op == ">")       met = (s_f >  v_f);
                else if (c.op == "<")  met = (s_f <  v_f);
                else if (c.op == ">=") met = (s_f >= v_f);
                else if (c.op == "<=") met = (s_f <= v_f);
            }
        }

        // Kurzschluss: AND scheitert am ersten Fehlschlag, OR gelingt am ersten Treffer.
        if (isAnd && !met) return false;
        if (!isAnd && met) return true;
    }
    return isAnd;
}

bool HAWidget::applyOwnVisibility() {
    if (marked_deleted || !container) return false;
    if (in_folder || conditions.empty()) return true;

    const bool visible = editModeActive || evaluateConditions(conditions_type, conditions);

    // Nur bei Aenderung anfassen: das Flag zu setzen loest sonst alle 500 ms
    // ein Neuzeichnen aus.
    const bool hidden = lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
    if (visible && hidden)        lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    else if (!visible && !hidden) lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);

    return visible;
}

void HAWidget::checkConditions() {
    applyOwnVisibility();
}

void HAWidget::markDeleted() {
    marked_deleted = true;
    if (container && lv_obj_is_valid(container)) {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }
}

void HAWidget::applyEditModeScrollChain() {
    for (auto w : all_active_widgets) {
        if (!w || !w->container || !lv_obj_is_valid(w->container)) continue;
        if (editModeActive) lv_obj_clear_flag(w->container, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        else                lv_obj_add_flag(w->container, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    }
}

String formatEntityName(String entity_id, String name) {
    if (name.length() > 0) return name; 
    int dotIdx = entity_id.indexOf('.');
    String res = (dotIdx != -1) ? entity_id.substring(dotIdx + 1) : entity_id;
    res.replace("_", " ");
    bool capitalizeNext = true;
    for (int i = 0; i < res.length(); i++) {
        if (res[i] == ' ') capitalizeNext = true;
        else if (capitalizeNext) { res[i] = toupper(res[i]); capitalizeNext = false; }
    }
    return res;
}

String getSafeIcon(String mdi) {
    String res = getAutoIcon(mdi);
    if (res != "") return res;
    return String(LV_SYMBOL_DUMMY);
}

String getIconForEntity(String entity_id, String mdi_icon) {
    if (mdi_icon.length() > 0 && mdi_icon.length() <= 4) {
        uint8_t firstByte = (uint8_t)mdi_icon[0];
        if (firstByte == 0xEF || firstByte == 0xF3) {
            return mdi_icon; 
        }
    }
    
    if (mdi_icon.length() > 4) {
        String l = mdi_icon; l.replace(" ", ""); l.trim();
        int start = 0;
        while (start < l.length()) {
            int end = l.indexOf(',', start);
            if (end == -1) end = l.length();
            String single_mdi = l.substring(start, end);
            String found = getAutoIcon(single_mdi);
            if (found != "") return found;
            start = end + 1;
        }
    }
    
    if (entity_id.indexOf("switch.") != -1 || entity_id.indexOf("input_boolean.") != -1 || 
        entity_id.indexOf("button.") != -1 || entity_id.indexOf("input_button.") != -1 ||
        entity_id.indexOf("binary_sensor.") != -1) {
        String icon = getAutoIcon("mdi:power");
        if (icon != "") return icon;
        icon = getAutoIcon("mdi:toggle-switch");
        if (icon != "") return icon;
        return String(LV_SYMBOL_POWER); 
    }
    
    if (entity_id.indexOf("script.") != -1 || entity_id.indexOf("automation.") != -1 || entity_id.indexOf("scene.") != -1) {
        String icon = getAutoIcon("mdi:play");
        if (icon != "") return icon;
        return String(LV_SYMBOL_PLAY);
    }
    
    if (entity_id.indexOf("light.") != -1) return getSafeIcon("mdi:lightbulb"); 
    if (entity_id.indexOf("media_player.") != -1) return getSafeIcon("mdi:television");
    if (entity_id.indexOf("camera.") != -1) return getSafeIcon("mdi:cctv");
    if (entity_id.indexOf("climate.") != -1 || entity_id.indexOf("temp") != -1) return getSafeIcon("mdi:thermometer");
    if (entity_id.indexOf("vacuum.") != -1) return getSafeIcon("mdi:robot-vacuum");
    if (entity_id.indexOf("battery") != -1) return getSafeIcon("mdi:battery");
    if (entity_id.indexOf("solar") != -1) return getSafeIcon("mdi:solar-panel");
    if (entity_id.indexOf("katze") != -1 || entity_id.indexOf("cat") != -1) return getSafeIcon("mdi:cat");
    if (entity_id.indexOf("kaffee") != -1 || entity_id.indexOf("coffee") != -1) return getSafeIcon("mdi:coffee");
    
    return String(LV_SYMBOL_DUMMY); 
}

HAWidget::HAWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi, const char* c_on, const char* c_off) {
    this->tab_index = tab_idx; this->type = type; this->kind = HaWidgetKindFromString(type); this->entity_id = entity; 
    this->mdi_icon = mdi; this->color_on = c_on; this->color_off = c_off; 
    this->widget_name = formatEntityName(entity_id, String(name));
    
    // Startwert "init" (nicht "unknown"): Widgets ueberspringen updateState()
    // bei unveraendertem Zustand - der erste Aufruf mit "unknown" muss aber
    // durchlaufen, damit die Icons sofort ihre Farbe bekommen.
    this->current_state = "init";
    
    this->icon_align = LV_ALIGN_TOP_MID; this->text_align = LV_ALIGN_BOTTOM_MID; this->state_align = LV_ALIGN_CENTER;
    this->icon_margin = 5; this->text_margin = 5; this->state_margin = 0; this->snap_to_grid = true; 

    container = lv_obj_create(parent);
    lv_obj_set_size(container, w, h); lv_obj_set_pos(container, x, y);
    lv_obj_set_style_radius(container, 25, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x222222), 0); 
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x555555), 0);
    
    lv_obj_set_style_pad_all(container, 0, 0);
    
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // Scroll-Kette: waagerecht gekappt (waagerecht wischen ist eine Geste,
    // kein Scrollen), senkrecht offen. So scrollt ein senkrechter Wisch ueber
    // eine Kachel den Tab, und LVGL sendet danach keinen Klick mehr.
    // Im Edit-Modus wird auch die senkrechte Kette gekappt, sonst scrollt der
    // Tab, statt dass sich das Widget verschieben laesst.
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    if (editModeActive) lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLL_CHAIN_VER);

    lv_obj_add_flag(container, LV_OBJ_FLAG_PRESS_LOCK);     
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(container, widget_event_cb, LV_EVENT_ALL, this);

    icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(icon_label, &lela_icons, 0); 
    lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str());

    name_label = lv_label_create(container);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT); 
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(name_label, widget_name.c_str());
    lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
    
    setSize(w, h); 
    all_active_widgets.push_back(this);
}

HAWidget::~HAWidget() { 
    all_active_widgets.erase(std::remove(all_active_widgets.begin(), all_active_widgets.end(), this), all_active_widgets.end());
    if (container && lv_obj_is_valid(container)) { 
        lv_obj_remove_event_cb(container, widget_event_cb); 
    }
}

void HAWidget::setAlignments(int i_align, int t_align, int s_align, int i_margin, int t_margin, int s_margin) {
    icon_align = i_align; text_align = t_align; state_align = s_align;
    icon_margin = i_margin; text_margin = t_margin; state_margin = s_margin;
    
    auto apply = [](lv_obj_t* obj, int align_val, int margin) {
        if (!obj) return;
        int ox = 0; int oy = 0;
        switch(align_val) {
            case LV_ALIGN_TOP_MID: oy = margin; break;
            case LV_ALIGN_BOTTOM_MID: oy = -margin; break;
            case LV_ALIGN_LEFT_MID: ox = margin; break;
            case LV_ALIGN_RIGHT_MID: ox = -margin; break;
            case LV_ALIGN_CENTER: oy = margin; break; 
        }
        lv_obj_align(obj, align_val, ox, oy);
    };
    
    apply(icon_label, icon_align, icon_margin); 
    apply(name_label, text_align, text_margin);
}

void HAWidget::setName(String n) { widget_name = formatEntityName(entity_id, n); lv_label_set_text(name_label, widget_name.c_str()); }
void HAWidget::setMdiIcon(String mdi) { mdi_icon = mdi; lv_label_set_text(icon_label, getIconForEntity(entity_id, mdi_icon).c_str()); }
void HAWidget::setColors(String on, String off) { color_on = on; color_off = off; String old = current_state; current_state = "force_update"; updateState(old); }

void HAWidget::setSize(int w, int h) {
    lv_obj_set_size(container, w, h);
    
    if (w >= 20) {
        lv_obj_set_size(name_label, w - 20, 45);
    } else {
        lv_obj_set_width(name_label, LV_PCT(80));
        lv_obj_set_height(name_label, 45);
    }
    
    setAlignments(icon_align, text_align, state_align, icon_margin, text_margin, state_margin); 
}

void HAWidget::setPosition(int x, int y, bool snap) { this->snap_to_grid = snap; if (snap) lv_obj_align(container, LV_ALIGN_TOP_LEFT, x, y); else lv_obj_set_pos(container, x, y); }
void HAWidget::setSnapToGrid(bool snap) { this->snap_to_grid = snap; }
void HAWidget::setChartConfig(bool show, int w_p, int h_p, int x_ofs, int y_ofs, String c_min, String c_max) { show_chart = show; chart_w_pct = w_p; chart_h_pct = h_p; chart_x_ofs = x_ofs; chart_y_ofs = y_ofs; chart_min = c_min; chart_max = c_max; }

void HAWidget::setTapAction(String domain, String service, String target) { tap_domain = domain; tap_service = service; tap_target = target; }
String HAWidget::getTapDomain() { return tap_domain; }
String HAWidget::getTapService() { return tap_service; }
String HAWidget::getTapTarget() { return tap_target; }

String HAWidget::getEntityId() { return entity_id; } String HAWidget::getType() { return type; } String HAWidget::getName() { return widget_name; }
String HAWidget::getMdiIcon() { return mdi_icon; } String HAWidget::getColorOn() { return color_on; } String HAWidget::getColorOff() { return color_off; }
int HAWidget::getX() { return lv_obj_get_x(container); } int HAWidget::getY() { return lv_obj_get_y(container); }
int HAWidget::getW() { return lv_obj_get_width(container); } int HAWidget::getH() { return lv_obj_get_height(container); }
int HAWidget::getTabIndex() { return tab_index; }
int HAWidget::getIconAlign() { return icon_align; } int HAWidget::getTextAlign() { return text_align; } int HAWidget::getStateAlign() { return state_align; }
int HAWidget::getIconMargin() { return icon_margin; } int HAWidget::getTextMargin() { return text_margin; } int HAWidget::getStateMargin() { return state_margin; }
bool HAWidget::getSnapToGrid() { return snap_to_grid; }
bool HAWidget::getShowChart() { return show_chart; } int HAWidget::getChartWPct() { return chart_w_pct; } int HAWidget::getChartHPct() { return chart_h_pct; }
int HAWidget::getChartXOfs() { return chart_x_ofs; } int HAWidget::getChartYOfs() { return chart_y_ofs; }
String HAWidget::getChartMin() { return chart_min; } String HAWidget::getChartMax() { return chart_max; }
String HAWidget::getMediaContentType() { return ""; } String HAWidget::getMediaContentId() { return ""; }

void HAWidget::updateState(String state) { this->current_state = state; }

uint32_t HAWidget::parseColor(const String& hex, uint32_t fallback) {
    if (hex.length() < 2 || hex[0] != '#') return fallback;
    char* end = nullptr;
    const unsigned long v = strtoul(hex.c_str() + 1, &end, 16);
    return (end && *end == '\0') ? (uint32_t)v : fallback;
}

bool HAWidget::runTapAction() {
    if (tap_domain.length() == 0 || tap_service.length() == 0) return false;
    const String target = (tap_target.length() > 0) ? tap_target : entity_id;
    HaWebsocketLogic_CallService(tap_domain, tap_service, target);
    return true;
}

std::vector<String> HAWidget::optionsOrBaked() {
    std::vector<String> opts = HaWebsocketLogic_GetOptions(entity_id);
    if (!opts.empty() || baked_options.length() == 0) return opts;

    // Gebackene Optionen sind kommagetrennt gespeichert.
    int start = 0;
    while (start < (int)baked_options.length()) {
        int end = baked_options.indexOf(',', start);
        if (end == -1) end = baked_options.length();
        opts.push_back(baked_options.substring(start, end));
        start = end + 1;
    }
    return opts;
}

void HAWidget::holdRemoteUpdates(uint32_t ms) {
    hold_remote_until = millis() + ms;
    refresh_after_hold = true;
}

bool HAWidget::remoteUpdatesHeld() const {
    return (int32_t)(hold_remote_until - millis()) > 0;
}

bool HAWidget::refreshAfterHoldDue() {
    if (!refresh_after_hold || remoteUpdatesHeld()) return false;
    refresh_after_hold = false;
    return true;
}

// Haelt ein Widget beim Verschieben innerhalb der Tab-Breite. Nach unten ist
// der Tab scrollbar und daher offen, nach oben/links ist bei 0 Schluss.
static void clampToParent(lv_obj_t* box, lv_coord_t& x, lv_coord_t& y) {
    lv_obj_t* parent = lv_obj_get_parent(box);
    const lv_coord_t maxX = parent ? lv_obj_get_content_width(parent) - lv_obj_get_width(box) : x;
    if (x > maxX) x = maxX;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
}

void HAWidget::widget_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    HAWidget * widget = (HAWidget *)lv_event_get_user_data(e);
    if (!widget) return;

    // Eine Wischgeste ist kein Klick. Ohne Scroll-Objekt (Tab passt komplett
    // auf den Bildschirm, oder waagerechter Wisch) meldet LVGL beim Loslassen
    // trotzdem SHORT_CLICKED - die Kachel wuerde nebenbei schalten.
    if (code == LV_EVENT_SHORT_CLICKED) {
        lv_indev_t* indev = lv_indev_get_act();
        if (indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) return;
    }

    if (HAWidget::editModeActive) {
        // Immer den Widget-Container bewegen, nicht das getroffene Objekt:
        // Slider, Dropdowns usw. reichen ihre Ereignisse hoch (EVENT_BUBBLE),
        // und frueher wurde dann der Slider statt der Kachel verschoben.
        lv_obj_t * box = widget->container;
        if (!box) return;

        if (code == LV_EVENT_PRESSED) { widget_is_dragging = false; lv_obj_move_foreground(box); } 
        else if (code == LV_EVENT_LONG_PRESSED) { widget_is_dragging = true; } 
        else if (code == LV_EVENT_PRESSING) {
            if (widget_is_dragging) {
                lv_indev_t * indev = lv_indev_get_act();
                if (indev == NULL) return;
                lv_point_t vect; lv_indev_get_vect(indev, &vect);
                lv_coord_t x = lv_obj_get_x(box) + vect.x;
                lv_coord_t y = lv_obj_get_y(box) + vect.y;
                clampToParent(box, x, y);
                lv_obj_set_pos(box, x, y);
            }
        } 
        else if (code == LV_EVENT_SHORT_CLICKED) { 
            if (widget->getKind() == HaWidgetKind::Folder) {
                widget->onClick();
            } else if (onEditRequested) {
                onEditRequested(widget); 
            }
        } 
        else if (code == LV_EVENT_RELEASED) {
            if (widget_is_dragging) {
                widget_is_dragging = false;
                if (widget->getSnapToGrid()) {
                    lv_coord_t nx = round(lv_obj_get_x(box) / 10.0) * 10;
                    lv_coord_t ny = round(lv_obj_get_y(box) / 10.0) * 10;
                    clampToParent(box, nx, ny);
                    lv_obj_set_pos(box, nx, ny);
                }
                if (onDeleteRequested) onDeleteRequested(widget);
            }
        }
    } else {
        if (code == LV_EVENT_SHORT_CLICKED) { 
            widget->onClick(); 
        } 
        else if (code == LV_EVENT_LONG_PRESSED) {
            if (widget->getKind() == HaWidgetKind::Light && widget->getEntityId().startsWith("light.") && HAWidget::onLightControlRequested) {
                if (HaWebsocketLogic_SupportsBrightness(widget->getEntityId()) || HaWebsocketLogic_SupportsColor(widget->getEntityId()) || HaWebsocketLogic_SupportsColorTemp(widget->getEntityId())) {
                    HAWidget::onLightControlRequested(widget);
                }
            } else if ((widget->getKind() == HaWidgetKind::MediaPlayer || widget->getKind() == HaWidgetKind::MediaItem) && HAWidget::onMediaControlRequested) {
                HAWidget::onMediaControlRequested(widget);
            } else if (widget->getKind() == HaWidgetKind::Vacuum && HAWidget::onVacuumControlRequested) {
                HAWidget::onVacuumControlRequested(widget); 
            }
        }
    }
}

// =========================================================
// LIGHT WIDGET
// =========================================================
HALightWidget::HALightWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str());
    updateState("unknown");
}

void HALightWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) {
        return; 
    }
    
    HAWidget::updateState(state);
    lv_obj_set_style_text_opa(icon_label, 255, 0); 
    
    const uint32_t c_on_val = colorOnOr(0xFFD700);
    const uint32_t c_off_val = colorOffOr(0x555555);
    
    if (state == "on" || state == "open") {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0); 
    } else {
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0); 
        lv_obj_set_style_text_color(icon_label, lv_color_hex(c_off_val), 0); 
    }
}

void HALightWidget::onClick() {
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    
    if (!runTapAction()) {
        int dotIdx = entity_id.indexOf('.'); 
        String domain = (dotIdx != -1) ? entity_id.substring(0, dotIdx) : "homeassistant";
        HaWebsocketLogic_CallService(domain, "toggle", entity_id);
    }
}

// =========================================================
// ACTION WIDGET
// =========================================================
HAActionWidget::HAActionWidget(lv_obj_t* parent, int tab_idx, String type, String entity, int x, int y, int w, int h, const char* name, const char* mdi_icon, const char* c_on, const char* c_off)
    : HAWidget(parent, tab_idx, type, entity, x, y, w, h, name, mdi_icon, c_on, c_off) {
    lv_label_set_text(icon_label, getIconForEntity(entity, mdi_icon).c_str()); 
    updateState("unknown");
}

void HAActionWidget::updateState(String state) {
    if (this->current_state == state && lv_obj_get_style_text_opa(icon_label, 0) == 255) {
        return; 
    }
    
    this->current_state = state; 
    lv_obj_set_style_text_opa(icon_label, 255, 0); 
    
    const uint32_t c_on_val = colorOnOr(0x9B59B6);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(c_on_val), 0);
}

void HAActionWidget::onClick() {
    lv_obj_set_style_text_opa(icon_label, 127, 0); 
    
    if (!runTapAction()) {
        String domain = entity_id.substring(0, entity_id.indexOf('.')); 
        String service = "turn_on"; 
        if (domain == "button" || domain == "input_button") service = "press"; 
        else if (domain == "automation") service = "trigger";
        HaWebsocketLogic_CallService(domain, service, entity_id);
    }
}