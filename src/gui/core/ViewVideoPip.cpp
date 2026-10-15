// ==============================================================================
// ViewVideoPip - Implementierung (siehe Header)
// ==============================================================================

#include "ViewVideoPip.h"
#include "GuiManager.h"
#include "SharedData.h"
#include "LVGL_Driver.h"
#include "BabyService.h"
#include "VideoLogic.h"
#include "UIHelper.h"
#include "HAWidgets.h"
#include <Preferences.h>

// ------------------------------------------------------------------------------
// Abstimmung
// ------------------------------------------------------------------------------
static const int SCREEN_W     = 1280;
static const int SCREEN_H     = 720;
static const int TOPBAR_H     = 80;    // das Overlay bleibt unter der Kopfleiste
static const int MARGIN       = 16;    // Abstand zum Rand in der Ausgangslage
static const int BAR_H        = 28;    // Statuszeile unter dem Bild
static const int GRIP_SIZE    = 28;    // Griff zum Groesseaendern (in der Statuszeile)

// Breite der Videoflaeche. Die Hoehe folgt dem Seitenverhaeltnis der
// Overlay-Aufloesung (4:3 oder 16:9).
static const int W_DEFAULT    = 320;
static const int W_MIN        = 200;
static const int W_MAX        = 800;

// Ab dieser Bewegung (px) wird aus einem Tippen ein Verschieben.
static const int DRAG_THRESHOLD = 12;

// Rahmen waehrend des Ziehens
static const int      GHOST_LINE  = 3;
static const uint32_t GHOST_COLOR = 0x00A0FF;

// Kommt so lange kein Bild, gilt die Verbindung als unterbrochen: die
// Aussparung wird freigegeben und der Rahmen zeigt "Verbindung ...".
static const uint32_t STALE_MS      = 3000;
// Danach ein deutlicherer Hinweis.
static const uint32_t LOST_MS       = 30000;

// Kamera-Akku: Warnung erst ab diesem Stand (Entscheidung: nur bei niedrigem
// Stand anzeigen).
static const int CAM_BAT_WARN_PCT   = 20;

static const char* NVS_PIP = "pip";
static const char* KEY_X   = "x";      // linke obere Ecke des Rahmens, -1 = Ausgangslage
static const char* KEY_Y   = "y";
static const char* KEY_W   = "w";      // Breite der Videoflaeche

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
static lv_obj_t* s_box       = nullptr;   // Rahmen (Videoflaeche + Statuszeile)
static lv_obj_t* s_video     = nullptr;   // Videoflaeche (Platzhalter ohne Bild)
static lv_obj_t* s_lblCenter = nullptr;   // "Verbindung ..." in der Videoflaeche
static lv_obj_t* s_lblStatus = nullptr;   // links in der Statuszeile
static lv_obj_t* s_lblIcons  = nullptr;   // rechts: Ton, Kamera-Akku
static lv_obj_t* s_grip      = nullptr;   // Griff unten rechts
static lv_obj_t* s_ghost[4]  = { nullptr, nullptr, nullptr, nullptr };   // Zieh-Rahmen

// Gewuenschte Geometrie (gespeichert). x/y < 0 = Ausgangslage unten rechts.
static int s_x = -1, s_y = -1, s_w = W_DEFAULT;

static bool     s_shown       = false;    // Rahmen sichtbar
static bool     s_reserved    = false;    // Videoflaeche fuer den PPA ausgespart
static uint32_t s_waitSince   = 0;        // seit wann auf das erste Bild gewartet wird
static lv_area_t s_videoArea  = { 0, 0, -1, -1 };   // absolute Lage der Videoflaeche

// Zuletzt angewendete Geometrie, um nur bei Aenderungen neu zu setzen.
static int s_appliedX = -1, s_appliedY = -1, s_appliedW = 0, s_appliedH = 0;

// Ziehen
enum class Drag : uint8_t { None, Move, Resize };
static Drag       s_drag          = Drag::None;
static bool       s_suppressClick = false;   // Loslassen nach Ziehen ist kein Tippen
static lv_point_t s_pressPt       = { 0, 0 };
static int        s_startX = 0, s_startY = 0, s_startW = 0;
static int        s_ghostX = 0, s_ghostY = 0, s_ghostW = 0;

static String s_lastStatus;
static String s_lastCenter;
static String s_lastIcons;

// ------------------------------------------------------------------------------
// Geometrie
// ------------------------------------------------------------------------------

// Seitenverhaeltnis aus der Overlay-Aufloesung ("BxH"). Ohne Angabe 16:9.
static void aspect(int* aw, int* ah) {
    const String res = BabyService_GetPipResolution();
    const int x = res.indexOf('x');
    *aw = 16; *ah = 9;
    if (x <= 0) return;
    const int w = res.substring(0, x).toInt();
    const int h = res.substring(x + 1).toInt();
    if (w > 0 && h > 0) { *aw = w; *ah = h; }
}

static int videoHeightFor(int w) {
    int aw, ah;
    aspect(&aw, &ah);
    return ((w * ah / aw) + 1) & ~1;   // gerade Hoehe
}

// Passt eine gewuenschte Geometrie in den Bildschirm: Breite in den Grenzen,
// Rahmen vollstaendig sichtbar und unter der Kopfleiste. x/y < 0 ergeben die
// Ausgangslage unten rechts.
static void clampGeometry(int* x, int* y, int* w) {
    int aw, ah;
    aspect(&aw, &ah);

    // Hoechstens so breit, dass der Rahmen in die Hoehe unter der Kopfleiste passt.
    const int maxWByHeight = (SCREEN_H - TOPBAR_H - BAR_H) * aw / ah;
    int maxW = W_MAX;
    if (maxWByHeight < maxW) maxW = maxWByHeight;
    if (SCREEN_W < maxW)     maxW = SCREEN_W;
    if (*w > maxW)  *w = maxW;
    if (*w < W_MIN) *w = W_MIN;

    const int boxH = videoHeightFor(*w) + BAR_H;
    if (*x < 0 || *y < 0) {
        *x = SCREEN_W - MARGIN - *w;
        *y = SCREEN_H - MARGIN - boxH;
    }
    if (*x > SCREEN_W - *w)    *x = SCREEN_W - *w;
    if (*x < 0)                *x = 0;
    if (*y > SCREEN_H - boxH)  *y = SCREEN_H - boxH;
    if (*y < TOPBAR_H)         *y = TOPBAR_H;
}

static void saveGeometry() {
    Preferences p;
    p.begin(NVS_PIP, false);
    p.putShort(KEY_X, (int16_t)s_x);
    p.putShort(KEY_Y, (int16_t)s_y);
    p.putShort(KEY_W, (int16_t)s_w);
    p.end();
}

// ------------------------------------------------------------------------------
// Sichtbarkeit
// ------------------------------------------------------------------------------
static bool isOwnObject(lv_obj_t* obj) {
    if (obj == s_box) return true;
    for (int i = 0; i < 4; i++) if (obj == s_ghost[i]) return true;
    return false;
}

// Liegt sonst etwas Sichtbares auf der obersten oder der System-Ebene?
// Dazu gehoeren Alarm-Popups, QuickSettings und das Abdunkeln ("Bildschirm
// aus").
static bool layerOccupied(lv_obj_t* layer) {
    const uint32_t n = lv_obj_get_child_cnt(layer);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t* c = lv_obj_get_child(layer, i);
        if (isOwnObject(c)) continue;
        if (!lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) return true;
    }
    return false;
}

// Ist auf dem aktuellen Bildschirm eine Auswahlliste aufgeklappt? LVGL 8
// legt die Liste eines Dropdowns als direktes Kind des BILDSCHIRMS an (nicht
// auf der obersten Ebene) und blendet sie nur aus, wenn sie zu ist.
static bool dropdownListOpen() {
    lv_obj_t* scr = lv_scr_act();
    const uint32_t n = lv_obj_get_child_cnt(scr);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t* c = lv_obj_get_child(scr, i);
        if (lv_obj_check_type(c, &lv_dropdownlist_class) &&
            !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
            return true;
        }
    }
    return false;
}

// Soll das Overlay gerade zu sehen sein?
static bool shouldShow() {
    if (!BabyService_IsVideoActive()) return false;

    const ScreenID scr = gui.getCurrentScreen();
    if (scr != SCREEN_DASHBOARD && scr != SCREEN_HA && scr != SCREEN_CATMAT) return false;

    if (HAWidget::editModeActive)      return false;   // Bearbeiten braucht die ganze Flaeche
    if (UIHelper::isModalOpen())       return false;   // HA-Dialoge und Tastatur
    if (screenshotModeActive)          return false;   // Privatsphaere
    if (layerOccupied(lv_layer_top())) return false;
    if (layerOccupied(lv_layer_sys())) return false;
    if (dropdownListOpen())            return false;
    return true;
}

// Aussparung setzen oder freigeben. Beim Freigeben muss LVGL die Flaeche neu
// zeichnen, sonst bliebe das letzte Videobild stehen.
static void setReserved(bool on) {
    if (on == s_reserved) return;
    s_reserved = on;
    if (on) {
        Lvgl_SetReservedArea(&s_videoArea);
    } else {
        Lvgl_SetReservedArea(nullptr);
        if (s_video) lv_obj_invalidate(s_video);
    }
}

// Geometrie anwenden - nur bei einer Aenderung (Ziehen, Seitenverhaeltnis).
static void applyLayout() {
    int x = s_x, y = s_y, w = s_w;
    clampGeometry(&x, &y, &w);
    const int h = videoHeightFor(w);

    if (x == s_appliedX && y == s_appliedY && w == s_appliedW && h == s_appliedH) return;
    s_appliedX = x; s_appliedY = y; s_appliedW = w; s_appliedH = h;

    // Erst die Aussparung freigeben und das Video anhalten: an der alten
    // Stelle muss LVGL wieder zeichnen duerfen.
    setReserved(false);
    VideoLogic_SetPipRect(0, 0, 0, 0);

    lv_obj_set_size(s_box, w, h + BAR_H);
    lv_obj_set_pos(s_box, x, y);
    s_waitSince = millis();
    lv_obj_set_size(s_video, w, h);

    s_videoArea.x1 = x;
    s_videoArea.y1 = y;
    s_videoArea.x2 = x + w - 1;
    s_videoArea.y2 = y + h - 1;

    if (s_shown) VideoLogic_SetPipRect(x, y, w, h);
}

static void show() {
    applyLayout();
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_box);
    s_shown      = true;
    s_waitSince  = millis();
    VideoLogic_SetPipRect(s_videoArea.x1, s_videoArea.y1,
                          s_videoArea.x2 - s_videoArea.x1 + 1,
                          s_videoArea.y2 - s_videoArea.y1 + 1);
    BabyService_SetPipVisible(true);
}

static void hideGhost() {
    for (int i = 0; i < 4; i++) {
        if (s_ghost[i]) lv_obj_add_flag(s_ghost[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// Reihenfolge ist wesentlich: erst die Videoausgabe stoppen (wirkt sofort im
// Decoder), dann das Ziel entziehen, dann die Aussparung freigeben - erst
// danach darf LVGL die Flaeche wieder bemalen.
static void hide() {
    BabyService_SetPipVisible(false);
    VideoLogic_SetPipRect(0, 0, 0, 0);
    setReserved(false);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    s_shown = false;

    // Ein laufendes Ziehen verwerfen (z.B. Alarm waehrend des Ziehens).
    s_drag = Drag::None;
    hideGhost();
}

static void setLabel(lv_obj_t* lbl, String& cache, const String& text) {
    if (text == cache) return;
    cache = text;
    lv_label_set_text(lbl, text.c_str());
}

// Statuszeile und Platzhaltertext aktualisieren (nur bei Aenderung).
static void updateContent(bool fresh, uint32_t sinceFrame) {
    // Kurz nach dem Einblenden oder Verschieben steht noch kein Bild - das
    // ist normal (ein Bild alle 1000/fps ms) und bekommt keinen Hinweis.
    String status;
    if (BabyService_IsVideoMemoryPaused()) {
        status = "Video pausiert - Speicher knapp";
    } else if (!fresh) {
        const uint32_t waited = (sinceFrame == UINT32_MAX) ? (millis() - s_waitSince) : sinceFrame;
        if (waited > LOST_MS)        status = "Keine Verbindung zur Kamera";
        else if (waited > STALE_MS)  status = "Verbindung ...";
    }
    setLabel(s_lblStatus, s_lastStatus, status);
    setLabel(s_lblCenter, s_lastCenter, status);

    String icons;
    if (BabyMode_HasAudio(BabyService_GetMode())) icons += LV_SYMBOL_VOLUME_MAX;
    // 0 heisst bei uns "noch nicht gemeldet" - eine leere Kamera sendet nicht.
    if (camBatteryPercent > 0 && camBatteryPercent < CAM_BAT_WARN_PCT) {
        if (icons.length()) icons += "  ";
        icons += LV_SYMBOL_BATTERY_1 " " + String(camBatteryPercent) + "%";
    }
    setLabel(s_lblIcons, s_lastIcons, icons);
}

// ------------------------------------------------------------------------------
// Ziehen
// ------------------------------------------------------------------------------
// Der Zieh-Rahmen besteht aus vier duennen Linien statt aus einem Objekt mit
// Rand: LVGL erklaert bei jeder Bewegung die GANZE Flaeche eines Objekts fuer
// ungueltig - bei einem 640 px breiten Rahmen waere jede Bewegung eine
// grossflaechige (langsame) Neuzeichnung. Vier Linien kosten nur Streifen.
static void placeGhost(int x, int y, int w) {
    clampGeometry(&x, &y, &w);
    s_ghostX = x; s_ghostY = y; s_ghostW = w;
    const int boxH = videoHeightFor(w) + BAR_H;

    struct { int x, y, w, h; } r[4] = {
        { x,                  y,                   w,          GHOST_LINE },   // oben
        { x,                  y + boxH - GHOST_LINE, w,        GHOST_LINE },   // unten
        { x,                  y,                   GHOST_LINE, boxH       },   // links
        { x + w - GHOST_LINE, y,                   GHOST_LINE, boxH       },   // rechts
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_set_pos(s_ghost[i], r[i].x, r[i].y);
        lv_obj_set_size(s_ghost[i], r[i].w, r[i].h);
        lv_obj_clear_flag(s_ghost[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ghost[i]);
    }
}

static lv_point_t pointerNow() {
    lv_point_t p = { 0, 0 };
    lv_indev_t* indev = lv_indev_get_act();
    if (indev) lv_indev_get_point(indev, &p);
    return p;
}

static void beginDrag(Drag kind) {
    s_drag = kind;
    playToneI2S(1000, 50, true);
    placeGhost(s_startX, s_startY, s_startW);
}

// Loslassen: neue Lage uebernehmen, speichern, Bild dorthin.
static void endDrag() {
    if (s_drag == Drag::None) return;
    s_drag = Drag::None;
    s_suppressClick = true;
    hideGhost();

    s_x = s_ghostX; s_y = s_ghostY; s_w = s_ghostW;
    saveGeometry();
    applyLayout();
}

// Gemeinsamer Anfang fuer Rahmen und Griff: Ausgangslage merken.
static void pressStart() {
    s_pressPt       = pointerNow();
    s_startX        = s_appliedX;
    s_startY        = s_appliedY;
    s_startW        = s_appliedW;
    s_drag          = Drag::None;
    s_suppressClick = false;
}

static void box_event_cb(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        pressStart();
    } else if (code == LV_EVENT_PRESSING) {
        const lv_point_t p = pointerNow();
        const int dx = p.x - s_pressPt.x;
        const int dy = p.y - s_pressPt.y;
        if (s_drag == Drag::None &&
            (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)) {
            beginDrag(Drag::Move);
        }
        if (s_drag == Drag::Move) placeGhost(s_startX + dx, s_startY + dy, s_startW);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        // Halten ohne Bewegung: Verschieben beginnt, der Rahmen erscheint.
        if (s_drag == Drag::None) beginDrag(Drag::Move);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        endDrag();
    } else if (code == LV_EVENT_SHORT_CLICKED) {
        if (s_suppressClick) return;
        // In die Baby-App; der Modus bleibt, wie er ist.
        playToneI2S(800, 100, true);
        gui.switchScreen(SCREEN_BABY, LV_SCR_LOAD_ANIM_NONE);
    }
}

// Griff: die linke obere Ecke bleibt stehen, die Breite folgt dem Finger.
// Senkrechte Bewegung zaehlt ueber das Seitenverhaeltnis mit, damit auch ein
// Ziehen schraeg nach unten vergroessert.
static void grip_event_cb(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        pressStart();
        beginDrag(Drag::Resize);
    } else if (code == LV_EVENT_PRESSING) {
        if (s_drag != Drag::Resize) return;
        int aw, ah;
        aspect(&aw, &ah);
        const lv_point_t p = pointerNow();
        const int dx = p.x - s_pressPt.x;
        const int dyAsW = (p.y - s_pressPt.y) * aw / ah;
        const int grow = (abs(dx) > abs(dyAsW)) ? dx : dyAsW;
        placeGhost(s_startX, s_startY, s_startW + grow);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        endDrag();
    }
}

// ------------------------------------------------------------------------------
// Aufbau
// ------------------------------------------------------------------------------
static lv_obj_t* createPlainObj(lv_obj_t* parent, uint32_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// Griff in der Statuszeile: zwei schraege Striche, wie man es von
// Fenstern kennt. Die Punkte muessen dauerhaft gueltig sein (lv_line
// kopiert sie nicht).
static void createGrip() {
    static const lv_point_t LINE_A[] = { { GRIP_SIZE - 6, 8 },  { 8,  GRIP_SIZE - 6 } };
    static const lv_point_t LINE_B[] = { { GRIP_SIZE - 6, 16 }, { 16, GRIP_SIZE - 6 } };

    s_grip = lv_obj_create(s_box);
    lv_obj_set_size(s_grip, GRIP_SIZE, GRIP_SIZE);
    lv_obj_set_style_bg_opa(s_grip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_grip, 0, 0);
    lv_obj_set_style_pad_all(s_grip, 0, 0);
    lv_obj_clear_flag(s_grip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_grip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_grip, LV_OBJ_FLAG_PRESS_LOCK);   // Finger darf den Griff verlassen
    lv_obj_clear_flag(s_grip, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(s_grip, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_align(s_grip, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(s_grip, grip_event_cb, LV_EVENT_ALL, nullptr);

    const lv_point_t* lines[2] = { LINE_A, LINE_B };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* l = lv_line_create(s_grip);
        lv_line_set_points(l, lines[i], 2);
        lv_obj_set_style_line_width(l, 2, 0);
        lv_obj_set_style_line_color(l, lv_color_hex(0x888888), 0);
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    }
}

// ------------------------------------------------------------------------------
// Oeffentliche Schnittstelle
// ------------------------------------------------------------------------------
void ViewVideoPip::init() {
    if (s_box) return;

    // Schreibbar oeffnen: ein lesendes begin() auf einen noch nie benutzten
    // Namensraum schlaegt fehl und meldet "nvs_open failed: NOT_FOUND".
    Preferences p;
    p.begin(NVS_PIP, false);
    s_x = p.getShort(KEY_X, -1);
    s_y = p.getShort(KEY_Y, -1);
    s_w = p.getShort(KEY_W, W_DEFAULT);
    p.end();

    s_box = createPlainObj(lv_layer_top(), 0x111111);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_PRESS_LOCK);    // Ziehen ueber den Rand hinaus
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_SCROLL_CHAIN);   // Ziehen ist kein Scrollen
    lv_obj_add_event_cb(s_box, box_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);

    // Videoflaeche: schwarz, solange kein Bild kommt. Nicht anklickbar, damit
    // Beruehrungen beim Rahmen landen.
    s_video = createPlainObj(s_box, 0x000000);
    lv_obj_set_pos(s_video, 0, 0);

    s_lblCenter = lv_label_create(s_video);
    lv_obj_set_style_text_color(s_lblCenter, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_lblCenter, "");
    lv_obj_center(s_lblCenter);

    // Statuszeile unter dem Bild: Text links, Symbole vor dem Griff.
    s_lblStatus = lv_label_create(s_box);
    lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_lblStatus, "");
    lv_obj_align(s_lblStatus, LV_ALIGN_BOTTOM_LEFT, 8, -4);

    s_lblIcons = lv_label_create(s_box);
    lv_obj_set_style_text_color(s_lblIcons, lv_color_hex(0x00A0FF), 0);
    lv_label_set_text(s_lblIcons, "");
    lv_obj_align(s_lblIcons, LV_ALIGN_BOTTOM_RIGHT, -(GRIP_SIZE + 4), -4);

    createGrip();

    // Zieh-Rahmen: vier Linien auf der obersten Ebene, anfangs verborgen.
    for (int i = 0; i < 4; i++) {
        s_ghost[i] = createPlainObj(lv_layer_top(), GHOST_COLOR);
        lv_obj_add_flag(s_ghost[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void ViewVideoPip::update() {
    if (!s_box) return;

    const bool want = shouldShow();
    if (!want) {
        if (s_shown) hide();
        return;
    }

    if (!s_shown) show();
    else          applyLayout();   // Seitenverhaeltnis kann sich geaendert haben

    // Aussparung nur, solange Bilder kommen. Bleiben sie aus, zeichnet LVGL
    // den Platzhalter mit "Verbindung ...".
    // Pausiert das Sicherheitsventil das Video, kommen keine Bilder mehr -
    // sofort den Hinweis zeigen statt 3 s auf "veraltet" zu warten.
    const uint32_t since = VideoLogic_MsSinceLastPipFrame();
    const bool fresh = !BabyService_IsVideoMemoryPaused() &&
                       (since != UINT32_MAX) && (since < STALE_MS);
    setReserved(fresh);

    updateContent(fresh, since);
}

void ViewVideoPip::resetLayout() {
    s_x = -1;
    s_y = -1;
    s_w = W_DEFAULT;
    saveGeometry();
    if (s_box) applyLayout();
}
