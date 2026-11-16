// ==============================================================================
// HangGuard - Implementierung (siehe HangGuard.h)
// ==============================================================================
#include "HangGuard.h"

#if HANG_GUARD

#include "LVGL_Driver.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

namespace {

// ------------------------------------------------------------------------------
// Grenzen
// ------------------------------------------------------------------------------
// Taetigkeit: Die laengste regulaere Einzeltaetigkeit ist der JPEG-Decoder mit
// 400 ms Zeitlimit; 3 s sind gut das Siebenfache und lassen Luft fuer
// Flash-Schreibvorgaenge anderer Tasks, die kurz alles anhalten.
constexpr uint32_t kActivityLimitMs = 3000;
// Oberflaeche: Fremde Tasks halten die LVGL-Sperre regulaer hoechstens 100 ms.
constexpr uint32_t kUiLimitMs       = 8000;
constexpr uint32_t kLoopFreshMs     = 3000;    // "Hauptschleife laeuft" fuer Pruefung 2
// Hauptschleife: Sie blockiert regulaer in Netzaufrufen (MQTT-Anmeldung bis
// ca. 18 s, Bildschirmfoto-Download). 60 s liegen sicher darueber.
constexpr uint32_t kLoopLimitMs     = 60000;

constexpr uint32_t    kPeriodMs  = 1000;
constexpr uint32_t    kTaskStack = 6144;       // snprintf + NVS-Schreiben
constexpr UBaseType_t kTaskPrio  = 10;         // ueber Video-Netz (5), unter lwIP (18)
constexpr BaseType_t  kTaskCore  = 0;          // der ueberwachte Kern ist 1

constexpr const char* kNvsNs     = "hangguard";
constexpr const char* kKeyReport = "last";
constexpr const char* kKeyCount  = "count";
constexpr const char* kKeyFresh  = "fresh";    // Bericht stammt vom vorherigen Lauf
constexpr size_t      kReportLen = 288;

// ------------------------------------------------------------------------------
// Zustand
// ------------------------------------------------------------------------------
struct Slot {
    const char* volatile   what    = nullptr;   // nullptr = wartet auf Arbeit
    volatile uint32_t      sinceMs = 0;
    volatile TaskHandle_t  owner   = nullptr;
    HangContextFn          context = nullptr;
};

constexpr size_t kSlots = (size_t)HangSlot::Count;
constexpr const char* kSlotNames[kSlots] = { "Video-Decoder" };

Slot              s_slots[kSlots];
volatile uint32_t s_loopMs   = 0;
volatile uint32_t s_uiMs     = 0;
TaskHandle_t      s_loopTask = nullptr;
bool              s_started  = false;

char     s_lastReport[kReportLen] = "";
uint32_t s_count = 0;
bool     s_trippedLastRun = false;

// ------------------------------------------------------------------------------
// Hilfen
// ------------------------------------------------------------------------------
// "wartet" = blockiert an Semaphor/Warteschlange, "rechnet" = laeuft oder will
// laufen. Genau diese Unterscheidung trennt "Treiber wartet ewig" von
// "Schleife dreht sich endlos".
const char* stateText(TaskHandle_t task) {
    if (!task) return "?";
    switch (eTaskGetState(task)) {
        case eRunning:
        case eReady:     return "rechnet";
        case eBlocked:
        case eSuspended: return "wartet";
        default:         return "?";
    }
}

// Alter einer Zeitmarke, die ein ANDERER Task laufend erneuert.
//
// Reihenfolge: erst die Marke lesen, dann die Uhr. Umgekehrt (eine Uhrzeit fuer
// die ganze Pruefung, die Marken danach) kann eine Marke juenger sein als
// "jetzt": Der andere Kern schreibt zwischen den beiden Lesezugriffen, oder der
// Waechter wird dazwischen fuer ein paar Millisekunden verdraengt. Die
// vorzeichenlose Differenz laeuft dann unter null und sieht aus wie 49 Tage
// Stillstand - so loeste der Waechter am 16.11.2026 bei gesundem Geraet aus
// ("Hauptschleife seit 4294967 s still"). Als zweite Sicherung zaehlt eine
// Marke aus der Zukunft als Alter 0.
uint32_t ageMs(const volatile uint32_t& stamp) {
    const uint32_t then = stamp;
    const int32_t  age  = (int32_t)(millis() - then);
    return (age > 0) ? (uint32_t)age : 0;
}

void appendf(char* buf, size_t len, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void appendf(char* buf, size_t len, const char* fmt, ...) {
    const size_t used = strlen(buf);
    if (used + 1 >= len) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + used, len - used, fmt, args);
    va_end(args);
}

// Bericht: Zeit | Ursache | Sperre | Taetigkeiten und Zusatzangaben.
void buildReport(char* buf, size_t len, const char* cause) {
    buf[0] = '\0';
    const time_t now = time(nullptr);
    if (now > 1600000000) {                      // Uhr gestellt (NTP oder RTC)
        struct tm t;
        localtime_r(&now, &t);
        appendf(buf, len, "%02d.%02d. %02d:%02d | ", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
    }
    appendf(buf, len, "%s | Sperre: %s", cause, Lvgl_LockHolderName());

    for (size_t i = 0; i < kSlots; i++) {
        const Slot& s = s_slots[i];
        const char* what = s.what;
        if (what) {
            appendf(buf, len, " | %s: '%s' seit %lu ms, %s", kSlotNames[i], what,
                    (unsigned long)ageMs(s.sinceMs), stateText(s.owner));
        }
        if (s.context) {
            char ctx[112] = "";
            s.context(ctx, sizeof(ctx));
            appendf(buf, len, " | %s", ctx);
        }
    }
}

// Bericht sichern und gezielt abbrechen. Kehrt nicht zurueck.
[[noreturn]] void trip(const char* cause) {
    char report[kReportLen];
    buildReport(report, sizeof(report), cause);

    Preferences prefs;
    if (prefs.begin(kNvsNs, false)) {
        prefs.putString(kKeyReport, report);
        prefs.putUInt(kKeyCount, s_count + 1);
        prefs.putBool(kKeyFresh, true);
        prefs.end();
    }
    Serial.printf("[HangGuard] %s\n", report);
    Serial.flush();

    // Abbruch statt Neustart: der Coredump haelt fest, wo jeder Task steht.
    // Danach uebernimmt ColdRestart (Absturz -> kurz stromlos).
    esp_system_abort("HangGuard: Geraet eingefroren (Bericht im NVS)");
}

void check() {
    char cause[96];

    // 1. Ein Teilnehmer steckt in einer Taetigkeit fest. Erst den Text lesen,
    //    dann das Alter: der Teilnehmer schreibt die Zeit vor dem Text.
    for (size_t i = 0; i < kSlots; i++) {
        const Slot& s = s_slots[i];
        if (s.what && ageMs(s.sinceMs) > kActivityLimitMs) {
            snprintf(cause, sizeof(cause), "%s haengt", kSlotNames[i]);
            trip(cause);
        }
    }

    // 2. Die Hauptschleife laeuft, bekommt die LVGL-Sperre aber nicht.
    const uint32_t loopAge = ageMs(s_loopMs);
    const uint32_t uiAge   = ageMs(s_uiMs);
    if (loopAge < kLoopFreshMs && uiAge > kUiLimitMs) {
        snprintf(cause, sizeof(cause), "Oberflaeche seit %lu s gesperrt", (unsigned long)(uiAge / 1000));
        trip(cause);
    }

    // 3. Die Hauptschleife meldet sich nicht mehr.
    if (loopAge > kLoopLimitMs) {
        snprintf(cause, sizeof(cause), "Hauptschleife seit %lu s still (%s)",
                 (unsigned long)(loopAge / 1000), stateText(s_loopTask));
        trip(cause);
    }
}

void task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kPeriodMs));
        check();
    }
}

} // namespace

// ==============================================================================
// Oeffentliche Funktionen
// ==============================================================================
void HangGuard_Init() {
    if (s_started) return;

    // Schreibend oeffnen: legt den Namensraum beim ersten Start an (lesend
    // gaebe es dann eine Fehlermeldung im Log). isKey aus demselben Grund.
    Preferences prefs;
    if (prefs.begin(kNvsNs, false)) {
        if (prefs.isKey(kKeyReport)) prefs.getString(kKeyReport, s_lastReport, sizeof(s_lastReport));
        if (prefs.isKey(kKeyCount))  s_count = prefs.getUInt(kKeyCount, 0);
        // Einmalig: nur der Start direkt nach dem Haenger sieht "frisch". Der
        // Zwischenstart vor der Stromlos-Phase (ColdRestart) kommt nicht bis
        // hierher und laesst die Marke stehen.
        if (prefs.isKey(kKeyFresh) && prefs.getBool(kKeyFresh, false)) {
            s_trippedLastRun = true;
            prefs.putBool(kKeyFresh, false);
        }
        prefs.end();
    }
    if (s_count > 0) Serial.printf("[HangGuard] Letzter Haenger (%lu insgesamt): %s\n",
                                   (unsigned long)s_count, s_lastReport);

    s_loopMs = s_uiMs = millis();
    if (xTaskCreatePinnedToCore(task, "HangGuard", kTaskStack, nullptr, kTaskPrio, nullptr, kTaskCore) != pdPASS) {
        Serial.println("[HangGuard] Task nicht gestartet - keine Ueberwachung.");
        return;
    }
    s_started = true;
}

void HangGuard_LoopAlive() {
    if (!s_loopTask) s_loopTask = xTaskGetCurrentTaskHandle();
    s_loopMs = millis();
}

void HangGuard_UiAlive() {
    s_uiMs = millis();
}

void HangGuard_Activity(HangSlot slot, const char* what) {
    Slot& s = s_slots[(size_t)slot];
    // Reihenfolge: erst die Zeit, dann der Text - der Waechter wertet die Zeit
    // nur aus, wenn ein Text gesetzt ist.
    s.sinceMs = millis();
    s.owner   = xTaskGetCurrentTaskHandle();
    s.what    = what;
}

void HangGuard_SetContext(HangSlot slot, HangContextFn fn) {
    s_slots[(size_t)slot].context = fn;
}

const char* HangGuard_LastReport() { return s_lastReport; }
uint32_t    HangGuard_Count()      { return s_count; }
bool        HangGuard_TrippedLastRun() { return s_trippedLastRun; }

#endif // HANG_GUARD
