// ==============================================================================
// CamResolutionUi - Implementierung (siehe Header)
// ==============================================================================

#include "CamResolutionUi.h"
#include "SharedData.h"
#include "BabyService.h"

// Zusatz an einer Einstellung, die die Kamera nicht (mehr) anbietet. Wird beim
// Auslesen der Auswahl wieder abgeschnitten.
static const char MARK_UNAVAILABLE[] = " (nicht verfuegbar)";

void CamResolutionUi_Fill(lv_obj_t* dropdown) {
    if (!dropdown) return;

    if (useCustomUrls) {
        lv_dropdown_set_options(dropdown, "(fremde Kamera)");
        lv_dropdown_set_selected(dropdown, 0);
        lv_obj_add_state(dropdown, LV_STATE_DISABLED);
        return;
    }
    lv_obj_clear_state(dropdown, LV_STATE_DISABLED);

    const String setting = BabyService_GetBabyResolutionSetting();
    const String list    = BabyService_GetResolutionOptions();

    String opts;
    int selected = -1, idx = 0, start = 0;
    while (list.length() > 0 && start <= (int)list.length()) {
        int end = list.indexOf('\n', start);
        if (end < 0) end = list.length();
        const String entry = list.substring(start, end);
        start = end + 1;
        if (entry.length() == 0) continue;
        if (opts.length() > 0) opts += '\n';
        opts += entry;
        if (entry == setting) selected = idx;
        idx++;
    }

    // Einstellung nicht in der Liste (oder noch keine Liste): voranstellen,
    // damit sichtbar bleibt, was eingestellt ist.
    if (selected < 0) {
        String first = setting;
        if (list.length() > 0) first += MARK_UNAVAILABLE;
        opts = (opts.length() > 0) ? first + "\n" + opts : first;
        selected = 0;
    }

    lv_dropdown_set_options(dropdown, opts.c_str());
    lv_dropdown_set_selected(dropdown, selected);
}

String CamResolutionUi_Hint() {
    if (useCustomUrls) return "fremde Kamera: Aufloesung steht in der URL";

    // Zuerst die dauerhafte Abweichung (Kamera kann die Einstellung nicht),
    // dann die voruebergehende (Anforderung noch nicht umgesetzt).
    const String setting   = BabyService_GetBabyResolutionSetting();
    const String effective = BabyService_GetBabyResolution();
    if (effective != setting) {
        return "nicht verfuegbar, verwendet: " + effective;
    }
    if (currentCamRes.length() > 0 && currentCamRes != setting) {
        return "Kamera liefert gerade " + currentCamRes;
    }
    return "";
}

void CamResolutionUi_OnChanged(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    char buf[48];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));

    String res(buf);
    const int mark = res.indexOf(MARK_UNAVAILABLE);
    if (mark >= 0) res = res.substring(0, mark);
    if (res.indexOf('x') <= 0) return;   // Platzhalter wie "(fremde Kamera)"

    playToneI2S(800, 100, true);
    BabyService_SetBabyResolution(res);
}
