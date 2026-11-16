#include "ViewBootScreen.h"
#include "GuiManager.h"
#include "ViewQuickSettings.h"
#include "LVGL_Driver.h"
#include "SharedData.h"
#include "WifiConnect.h"   // Knoepfe der Startfolge
#include "UiSound.h"       // Tastenton

lv_obj_t * splash_status_label = nullptr;

// Globale Referenzen fuer die laufenden Animationen
static lv_obj_t * g_splash_bar = nullptr;
static lv_obj_t * g_cat_walker = nullptr;
static BootState g_current_boot_state = BOOT_STATE_INIT;

// Knoepfe der Startfolge (siehe Header). Farben des Bootscreens - er hat sein
// eigenes, festes dunkles Bild und folgt nicht dem Farbschema.
static lv_obj_t * g_btn_bar  = nullptr;
static lv_obj_t * g_btn_next = nullptr;
static const uint32_t BOOT_ACCENT = 0x00A0FF;
static const uint32_t BOOT_MUTED  = 0xAAAAAA;

static const lv_point_t walk_pts[] = {{0,0}, {12,36}, {42,36}, {66,24}, {72,9}, {84,9}, {96,27}, {84,42}, {84,72}, {72,72}, {72,54}, {42,54}, {42,72}, {30,72}, {30,54}, {12,36}};
static const lv_point_t sit_pts[] = {{66, 72}, {30, 72}, {30, 48}, {36, 24}, {30, 6}, {39, 15}, {57, 15}, {66, 6}, {60, 24}, {66, 48}, {66, 72}};

static void anim_zzz_y_cb(void * var, int32_t v) {
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

static void anim_zzz_opa_cb(void * var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

// Der finale Exit: Baut den Screen ab und wechselt ins Dashboard
static void splash_timer_cb(lv_timer_t * timer) {
    lv_anim_del_all();
    splash_status_label = nullptr;
    g_splash_bar = nullptr;
    g_cat_walker = nullptr;
    g_btn_bar = nullptr;
    g_btn_next = nullptr;
    Serial.printf("[Boot] Dashboard nach %lu ms\n", (unsigned long)millis());
    gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE);
}

// Koppelt den Balkenwert mathematisch an die X-Position der Katze (0-405 Pixel)
static void prog_anim_cb(void * var, int32_t v) {
    if (g_splash_bar) lv_bar_set_value(g_splash_bar, v, LV_ANIM_OFF);
    if (g_cat_walker) lv_obj_set_style_translate_x(g_cat_walker, (v * 405) / 100, 0);
}

// Wird getriggert, sobald eine Animation ihren Zielwert (z.B. 100%) erreicht hat
static void prog_anim_ready_cb(lv_anim_t * a) {
    if (g_current_boot_state == BOOT_STATE_DONE || g_current_boot_state == BOOT_STATE_FAILED ||
        g_current_boot_state == BOOT_STATE_SKIPPED) {
        if (g_cat_walker) {
            lv_line_set_points(g_cat_walker, sit_pts, 11); // Katze setzt sich hin!
        }

        // Exakt 0,5s Bedenkzeit beim Erfolg (und wenn bewusst ohne WLAN
        // gestartet wird), 2,5s Zeit zum Lesen bei Fehler
        uint32_t wait_time = (g_current_boot_state == BOOT_STATE_FAILED) ? 2500 : 500;
        lv_timer_t * t = lv_timer_create(splash_timer_cb, wait_time, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
}

// Ladebalken und Katze (die Katze folgt dem Balkenwert, prog_anim_cb):
//   WIFI:    laeuft langsam und immer langsamer Richtung 85 % - die Katze
//            bleibt in Bewegung, solange das WLAN braucht. Wechselt die
//            Startfolge das Netz, laeuft sie von dort die halbe Reststrecke
//            bis RETRY_LIMIT_PCT weiter (setWifiAttempt) - kein Ruecksprung.
//   DONE / FAILED / SKIPPED: laeuft vom aktuellen Stand bis 100 %. Die Dauer
//            richtet sich nach der Reststrecke (gleiches Tempo, egal wie frueh
//            das WLAN fertig ist), mindestens FINISH_MIN_MS. Erst am Ziel setzt
//            sich die Katze (prog_anim_ready_cb) - die Animation laeuft also
//            immer vollstaendig durch, auch bei sehr schnellem Start.
// lv_anim_start ersetzt eine laufende Animation desselben Ziels; deren
// ready_cb wird dabei nicht gerufen.
static const uint32_t WIFI_PHASE_MS  = 8000;   // 0 -> 85 %, ease_out
static const int      WIFI_PHASE_PCT = 85;
static const uint32_t FINISH_MS_PER_PCT = 12;  // Tempo der Schlussstrecke
static const uint32_t FINISH_MIN_MS  = 600;
static const int      RETRY_LIMIT_PCT = 94;    // weitere Versuche naehern sich dem nur an

// Startet die Balken-/Katzenanimation vom Ist-Wert zum Zielwert.
static void startProgress(int target_pct, uint32_t anim_time, lv_anim_path_cb_t path) {
    static lv_anim_t prog_anim;
    lv_anim_init(&prog_anim);
    lv_anim_set_var(&prog_anim, g_splash_bar); // Dummy-Ziel fuer den Callback
    lv_anim_set_values(&prog_anim, lv_bar_get_value(g_splash_bar), target_pct); // echter Ist-Wert
    lv_anim_set_time(&prog_anim, anim_time);
    lv_anim_set_exec_cb(&prog_anim, prog_anim_cb);
    lv_anim_set_path_cb(&prog_anim, path);
    lv_anim_set_ready_cb(&prog_anim, prog_anim_ready_cb);
    lv_anim_start(&prog_anim);
}

void ViewBootScreen::setBootState(BootState state, const char* extraInfo) {
    if (!g_splash_bar || !g_cat_walker) return;
    g_current_boot_state = state;

    const int start_pct = lv_bar_get_value(g_splash_bar);
    int target_pct = start_pct;
    uint32_t anim_time = 1000;
    lv_anim_path_cb_t path = lv_anim_path_ease_out;

    String txt = "";

    switch(state) {
        case BOOT_STATE_WIFI:
            target_pct = WIFI_PHASE_PCT;
            anim_time = WIFI_PHASE_MS;
            txt = "Verbinde mit WLAN...";   // das Netz nennt setWifiAttempt
            break;
        case BOOT_STATE_DONE:
        case BOOT_STATE_FAILED:
        case BOOT_STATE_SKIPPED: {
            target_pct = 100;
            const uint32_t remaining = (uint32_t)(100 - start_pct);
            anim_time = remaining * FINISH_MS_PER_PCT;
            if (anim_time < FINISH_MIN_MS) anim_time = FINISH_MIN_MS;
            path = lv_anim_path_ease_in_out;   // gleichmaessig anlaufen und ankommen
            if (state == BOOT_STATE_DONE) txt = String("WLAN '") + extraInfo + "' verbunden. Starte Dashboard...";
            else                          txt = extraInfo;
            // Die Entscheidung ist gefallen: Knoepfe weg.
            if (g_btn_bar) lv_obj_add_flag(g_btn_bar, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        default:
            return;
    }

    if (splash_status_label) {
        lv_label_set_text(splash_status_label, txt.c_str());
    }

    // Feuert die LVGL Interpolation vom Ist-Wert zum Soll-Wert
    startProgress(target_pct, anim_time, path);
}

// ------------------------------------------------------------------------------
// Knoepfe der Startfolge
// ------------------------------------------------------------------------------
// Umrandete Pillen am unteren Rand, in den Farben des Bootscreens: sichtbar,
// aber zurueckhaltend - im Normalfall ist das WLAN nach wenigen Sekunden da,
// und niemand muss sie beachten.
static lv_obj_t* createBootButton(lv_obj_t* parent, const char* text, uint32_t color, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 48);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(btn, 26, 0);
    lv_obj_set_ext_click_area(btn, 12);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    UiSound_AttachClick(btn);
    return btn;
}

static void createBootButtons(lv_obj_t* screen) {
    g_btn_bar = lv_obj_create(screen);
    lv_obj_remove_style_all(g_btn_bar);
    lv_obj_clear_flag(g_btn_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_btn_bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_btn_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g_btn_bar, 20, 0);
    lv_obj_align(g_btn_bar, LV_ALIGN_BOTTOM_MID, 0, -22);

    g_btn_next = createBootButton(g_btn_bar, "Naechstes Netz", BOOT_ACCENT,
                                  [](lv_event_t*) { WifiConnect_BootSkip(); });
    createBootButton(g_btn_bar, "Ohne WLAN starten", BOOT_MUTED,
                     [](lv_event_t*) { WifiConnect_BootAbort(); });

    // Erst zeigen, wenn die Startfolge laeuft (setWifiAttempt).
    lv_obj_add_flag(g_btn_bar, LV_OBJ_FLAG_HIDDEN);
}

void ViewBootScreen::setWifiAttempt(const char* ssid, uint8_t attempt, uint8_t count) {
    if (!g_splash_bar || g_current_boot_state != BOOT_STATE_WIFI) return;

    if (splash_status_label) {
        if (count > 1) lv_label_set_text_fmt(splash_status_label, "Verbinde mit WLAN '%s' (%u/%u)...",
                                             ssid, (unsigned)attempt, (unsigned)count);
        else           lv_label_set_text_fmt(splash_status_label, "Verbinde mit WLAN '%s'...", ssid);
    }

    if (g_btn_bar) {
        lv_obj_clear_flag(g_btn_bar, LV_OBJ_FLAG_HIDDEN);
        // "Naechstes Netz" nur, solange danach noch eines kommt.
        if (g_btn_next) {
            if (attempt < count) lv_obj_clear_flag(g_btn_next, LV_OBJ_FLAG_HIDDEN);
            else                 lv_obj_add_flag(g_btn_next, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Ab dem zweiten Versuch: von der jetzigen Stelle die halbe Reststrecke.
    if (attempt > 1) {
        const int now_pct = lv_bar_get_value(g_splash_bar);
        if (now_pct < RETRY_LIMIT_PCT) {
            startProgress(now_pct + (RETRY_LIMIT_PCT - now_pct + 1) / 2, WIFI_PHASE_MS, lv_anim_path_ease_out);
        }
    }
}

void ViewBootScreen::show() {
    lv_obj_t * splash_screen = lv_obj_create(NULL); 
    if (!splash_screen) return; 

    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x111111), 0); 
    lv_obj_set_style_border_width(splash_screen, 0, 0);
    lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0); 

    lv_obj_t * splash_title = lv_label_create(splash_screen); 
    lv_label_set_text(splash_title, "Lela OS 2"); 
    lv_obj_set_style_text_font(splash_title, &lv_font_montserrat_36, 0); 
    lv_obj_set_style_text_color(splash_title, lv_color_hex(0x00A0FF), 0); 
    lv_obj_align(splash_title, LV_ALIGN_TOP_MID, 0, 60); 

    g_splash_bar = lv_bar_create(splash_screen); 
    lv_obj_set_size(g_splash_bar, 600, 20); 
    lv_obj_align(g_splash_bar, LV_ALIGN_CENTER, 0, 10); 
    lv_bar_set_range(g_splash_bar, 0, 100); 
    lv_obj_set_style_bg_color(g_splash_bar, lv_color_hex(0x00A0FF), LV_PART_INDICATOR);

    splash_status_label = lv_label_create(splash_screen);
    lv_obj_set_style_text_font(splash_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(splash_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(splash_status_label, "Initialisiere Hardware...");
    lv_obj_align(splash_status_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t * splash_mat = lv_obj_create(splash_screen); 
    lv_obj_set_size(splash_mat, 180, 12); 
    lv_obj_set_style_bg_color(splash_mat, lv_color_hex(0x555555), 0); 
    lv_obj_set_style_border_width(splash_mat, 0, 0); 
    lv_obj_set_style_radius(splash_mat, 6, 0); 
    lv_obj_align(splash_mat, LV_ALIGN_CENTER, 180, 0); 

    lv_color_t baby_col = lv_color_hex(0x4FA5D6); 
    lv_obj_t * baby_cont = lv_obj_create(splash_screen);
    lv_obj_set_size(baby_cont, 180, 210); 
    lv_obj_align(baby_cont, LV_ALIGN_BOTTOM_MID, 0, -90); 
    lv_obj_set_style_bg_opa(baby_cont, 0, 0); 
    lv_obj_set_style_border_width(baby_cont, 0, 0);
    lv_obj_set_style_pad_all(baby_cont, 0, 0);

    static const lv_point_t body_pts[] = {{45, 90}, {135, 90}, {150, 135}, {135, 180}, {45, 180}, {30, 135}, {45, 90}};
    lv_obj_t * l_body = lv_line_create(baby_cont); 
    lv_line_set_points(l_body, body_pts, 7); 
    lv_obj_set_style_line_width(l_body, 9, 0); 
    lv_obj_set_style_line_color(l_body, baby_col, 0); 
    lv_obj_set_style_line_rounded(l_body, true, 0);
    
    static const lv_point_t head_pts[] = {{60, 45}, {120, 45}, {135, 66}, {120, 90}, {60, 90}, {45, 66}, {60, 45}};
    lv_obj_t * l_head = lv_line_create(baby_cont); 
    lv_line_set_points(l_head, head_pts, 7); 
    lv_obj_set_style_line_width(l_head, 6, 0); 
    lv_obj_set_style_line_color(l_head, lv_color_white(), 0);
    
    static const lv_point_t eye_l_pts[] = {{69, 66}, {75, 72}, {81, 66}};
    static const lv_point_t eye_r_pts[] = {{99, 66}, {105, 72}, {111, 66}};
    lv_obj_t * l_e_l = lv_line_create(baby_cont); 
    lv_line_set_points(l_e_l, eye_l_pts, 3); 
    lv_obj_set_style_line_width(l_e_l, 3, 0); 
    lv_obj_set_style_line_color(l_e_l, lv_color_white(), 0);
    lv_obj_t * l_e_r = lv_line_create(baby_cont); 
    lv_line_set_points(l_e_r, eye_r_pts, 3); 
    lv_obj_set_style_line_width(l_e_r, 3, 0); 
    lv_obj_set_style_line_color(l_e_r, lv_color_white(), 0);
    
    static const lv_point_t paci_pts[] = {{84, 78}, {96, 78}, {96, 84}, {84, 84}, {84, 78}}; 
    static const lv_point_t paci_handle[] = {{87, 84}, {93, 84}, {93, 90}, {87, 90}, {87, 84}}; 
    lv_obj_t * l_paci = lv_line_create(baby_cont); 
    lv_line_set_points(l_paci, paci_pts, 5); 
    lv_obj_set_style_line_width(l_paci, 6, 0); 
    lv_obj_set_style_line_color(l_paci, lv_color_hex(0xE67E22), 0); 
    lv_obj_t * l_paci_h = lv_line_create(baby_cont); 
    lv_line_set_points(l_paci_h, paci_handle, 5); 
    lv_obj_set_style_line_width(l_paci_h, 6, 0); 
    lv_obj_set_style_line_color(l_paci_h, lv_color_white(), 0);
    
    lv_obj_t * lbl_zzz = lv_label_create(splash_screen); 
    lv_label_set_text(lbl_zzz, "zzzZZZ"); 
    lv_obj_set_style_text_color(lbl_zzz, lv_color_white(), 0); 
    lv_obj_align_to(lbl_zzz, baby_cont, LV_ALIGN_TOP_MID, 30, 15); 
    
    lv_anim_t a_zzz_y; 
    lv_anim_init(&a_zzz_y); 
    lv_anim_set_var(&a_zzz_y, lbl_zzz); 
    lv_anim_set_values(&a_zzz_y, 0, -90); 
    lv_anim_set_time(&a_zzz_y, 1500); 
    lv_anim_set_exec_cb(&a_zzz_y, anim_zzz_y_cb); 
    lv_anim_set_path_cb(&a_zzz_y, lv_anim_path_ease_out); 
    lv_anim_set_delay(&a_zzz_y, 500); 
    lv_anim_set_repeat_count(&a_zzz_y, 100); // 100x reicht massig als "unendlich"
    lv_anim_start(&a_zzz_y);
    
    lv_anim_t a_zzz_opa; 
    lv_anim_init(&a_zzz_opa); 
    lv_anim_set_var(&a_zzz_opa, lbl_zzz); 
    lv_anim_set_values(&a_zzz_opa, 255, 0); 
    lv_anim_set_time(&a_zzz_opa, 1500); 
    lv_anim_set_exec_cb(&a_zzz_opa, anim_zzz_opa_cb); 
    lv_anim_set_path_cb(&a_zzz_opa, lv_anim_path_linear); 
    lv_anim_set_delay(&a_zzz_opa, 500); 
    lv_anim_set_repeat_count(&a_zzz_opa, 100); 
    lv_anim_start(&a_zzz_opa);

    g_cat_walker = lv_line_create(splash_screen); 
    lv_line_set_points(g_cat_walker, walk_pts, 16); 
    lv_obj_set_style_line_width(g_cat_walker, 9, 0); 
    lv_obj_set_style_line_color(g_cat_walker, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_set_style_line_rounded(g_cat_walker, true, 0); 
    lv_obj_align(g_cat_walker, LV_ALIGN_CENTER, -240, -42); 

    createBootButtons(splash_screen);

    lv_scr_load(splash_screen);
}