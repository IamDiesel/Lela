#include "ViewBootScreen.h"
#include "GuiManager.h"
#include "ViewQuickSettings.h"
#include "LVGL_Driver.h"
#include "SharedData.h"

lv_obj_t * splash_status_label = nullptr;

// Globale Referenzen fuer die laufenden Animationen
static lv_obj_t * g_splash_bar = nullptr;
static lv_obj_t * g_cat_walker = nullptr;
static BootState g_current_boot_state = BOOT_STATE_INIT;

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
    gui.switchScreen(SCREEN_DASHBOARD, LV_SCR_LOAD_ANIM_NONE);
}

// Koppelt den Balkenwert mathematisch an die X-Position der Katze (0-405 Pixel)
static void prog_anim_cb(void * var, int32_t v) { 
    if (g_splash_bar) lv_bar_set_value(g_splash_bar, v, LV_ANIM_OFF); 
    if (g_cat_walker) lv_obj_set_style_translate_x(g_cat_walker, (v * 405) / 100, 0);
}

// Wird getriggert, sobald eine Animation ihren Zielwert (z.B. 100%) erreicht hat
static void prog_anim_ready_cb(lv_anim_t * a) {
    if (g_current_boot_state == BOOT_STATE_DONE || g_current_boot_state == BOOT_STATE_TIMEOUT) {
        if (g_cat_walker) {
            lv_line_set_points(g_cat_walker, sit_pts, 11); // Katze setzt sich hin!
        }
        
        // Exakt 0,5s Bedenkzeit beim Erfolg, 2,5s Zeit zum Lesen bei Fehler
        uint32_t wait_time = (g_current_boot_state == BOOT_STATE_TIMEOUT) ? 2500 : 500;
        lv_timer_t * t = lv_timer_create(splash_timer_cb, wait_time, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
}

// Die neue State-Machine Schnittstelle
void ViewBootScreen::setBootState(BootState state, const char* extraInfo) {
    if (!g_splash_bar || !g_cat_walker) return;

    g_current_boot_state = state;
    int start_pct = lv_bar_get_value(g_splash_bar); // Liest den echten Ist-Wert aus!
    int target_pct = start_pct;
    int anim_time = 1000;
    String txt = "";

    switch(state) {
        case BOOT_STATE_WIFI:
            target_pct = 50;
            anim_time = 3500; // Lauft geschmeidig ueber 3,5s zur Haelfte
            txt = String("Verbinde mit WLAN '") + extraInfo + "'...";
            break;
        case BOOT_STATE_HA:
            target_pct = 90;
            anim_time = 2500; // Zieht das Verbinden optisch etwas in die Laenge
            txt = "WLAN verbunden. Verbinde Home Assistant...";
            break;
        case BOOT_STATE_DONE:
            target_pct = 100;
            anim_time = 400; // Rasantes Auffuellen
            txt = "Home Assistant verbunden! Starte Dashboard...";
            break;
        case BOOT_STATE_TIMEOUT:
            target_pct = 100;
            anim_time = 400; // Rasantes Auffuellen
            txt = "Verbindung dauert zu lange. Starte Offline-Modus...";
            break;
        default:
            return;
    }

    if (splash_status_label) {
        lv_label_set_text(splash_status_label, txt.c_str());
    }

    // Feuert die LVGL Interpolation vom Ist-Wert zum Soll-Wert
    static lv_anim_t prog_anim;
    lv_anim_init(&prog_anim);
    lv_anim_set_var(&prog_anim, g_splash_bar); // Dummy-Ziel fuer den Callback
    lv_anim_set_values(&prog_anim, start_pct, target_pct);
    lv_anim_set_time(&prog_anim, anim_time);
    lv_anim_set_exec_cb(&prog_anim, prog_anim_cb);
    lv_anim_set_path_cb(&prog_anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&prog_anim, prog_anim_ready_cb);
    lv_anim_start(&prog_anim);
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

    lv_scr_load(splash_screen);
}