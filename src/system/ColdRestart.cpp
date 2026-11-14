// ==============================================================================
// ColdRestart - Implementierung (siehe ColdRestart.h)
// ==============================================================================
#include "ColdRestart.h"
#include "CrashDiag.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_system.h>

namespace {

// ------------------------------------------------------------------------------
// Einstellungen
// ------------------------------------------------------------------------------
// Sekunden vom Stellen der Uhr bis zum Wecken. Darin enthalten: das Abschalten
// selbst (~0,6 s: Display aus, 10 Pulse a 50 ms) und bis zu 1 s Unschaerfe, weil
// der Sekundentakt der Uhr beim Stellen evtl. nicht neu beginnt. Kommt der
// Wecker, BEVOR das Geraet aus ist, bliebe es bis zum naechsten Tag aus (der
// Wecker vergleicht nur Stunde:Minute) - deshalb 3 s Reserve statt 2.
constexpr int      kLeadS    = 3;
constexpr uint8_t  kMaxInRow = 2;       // automatische Kaltstarts in Folge
constexpr uint32_t kStableMs = 60000;   // danach gilt der Lauf als stabil

constexpr const char* kNvsNs    = "coldrst";
constexpr const char* kKeyShift = "shift";   // Verschiebung der Uhr in Sekunden
constexpr const char* kKeyRow   = "row";     // Kaltstarts in Folge

uint8_t s_inRow      = 0;
bool    s_rowChecked = false;

// Gewollter Neustart (ColdRestart_WarmRestart): Kennung im RTC-Speicher, der
// einen Warmstart ueberlebt. Wird bei jedem Start gelesen und geloescht.
constexpr uint32_t kPlannedMagic = 0x504C414E;   // "PLAN"
RTC_NOINIT_ATTR uint32_t s_plannedRestart;

// ------------------------------------------------------------------------------
// Reset-Gruende, nach denen automatisch kalt neu gestartet wird: nur Abstuerze.
//
// Die Hardware meldet bei Panic und esp_restart() gleichermassen einen
// Software-Reset. Unterschieden wird ueber den "Hint", den der Panic- bzw.
// Watchdog-Handler von ESP-IDF vor dem Reset in ein Register des Always-on-
// Bereichs schreibt; esp_reset_reason() wertet beides aus.
//
// Bewusst NICHT ESP_RST_SW (System > Neustart, Web-Setup): ein gewollter
// Neustart soll nicht laenger dauern (Entscheidung Daniel, 14.11.2026). Der Ton
// kann danach stumm sein (Befund 14.11.2026 bei laufendem Babyphone-Ton).
// ------------------------------------------------------------------------------
bool needsColdRestart(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_CPU_LOCKUP:
            return true;
        default:
            return false;
    }
}

// ------------------------------------------------------------------------------
// Datum <-> Tage seit 1970-01-01 (gregorianisch, Verfahren nach H. Hinnant).
// Bewusst ohne mktime(): unabhaengig von der Zeitzone (TZ).
// ------------------------------------------------------------------------------
int32_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int      era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int32_t)doe - 719468;
}

void civilFromDays(int32_t z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const int      era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = (mp < 10) ? mp + 3 : mp - 9;
    y = (int)yoe + era * 400 + (m <= 2);
}

int64_t toSeconds(const m5::rtc_datetime_t& dt) {
    return (int64_t)daysFromCivil(dt.date.year, dt.date.month, dt.date.date) * 86400
         + dt.time.hours * 3600 + dt.time.minutes * 60 + dt.time.seconds;
}

m5::rtc_datetime_t fromSeconds(int64_t s) {
    int32_t days = (int32_t)(s / 86400);
    int32_t rem  = (int32_t)(s % 86400);
    if (rem < 0) { rem += 86400; days--; }

    int y; unsigned m, d;
    civilFromDays(days, y, m, d);
    const int8_t weekDay = (int8_t)((days % 7 + 11) % 7);   // 1970-01-01 = Donnerstag (4)

    return m5::rtc_datetime_t(
        m5::rtc_date_t((int16_t)y, (int8_t)m, (int8_t)d, weekDay),
        m5::rtc_time_t((int8_t)(rem / 3600), (int8_t)(rem / 60 % 60), (int8_t)(rem % 60)));
}

bool readClock(m5::rtc_datetime_t& dt) {
    return M5.Rtc.isEnabled() && M5.Rtc.getDateTime(&dt);
}

// Nach einem eigenen Kaltstart die vorgestellte Uhr zurueckstellen.
void restoreClock(Preferences& prefs) {
    if (!prefs.isKey(kKeyShift)) return;
    const int shift = prefs.getInt(kKeyShift, 0);
    prefs.remove(kKeyShift);   // auf jeden Fall nur einmal anwenden

    m5::rtc_datetime_t dt;
    if (shift == 0 || !readClock(dt)) return;

    M5.Rtc.setDateTime(fromSeconds(toSeconds(dt) - shift));
    M5.Rtc.setSystemTimeFromRtc();
    Serial.printf("[ColdRestart] Uhr um %d s zurueckgestellt.\n", shift);
}

} // namespace

// ==============================================================================
// Oeffentliche Funktionen
// ==============================================================================
void ColdRestart_Init() {
    Preferences prefs;
    prefs.begin(kNvsNs, false);
    restoreClock(prefs);
    s_inRow = prefs.getUChar(kKeyRow, 0);

    const bool planned = (s_plannedRestart == kPlannedMagic);
    s_plannedRestart = 0;

    // Verklemmen koennen die Audio-Chips nur bei einem Warmstart des LAUFENDEN
    // Programms. CrashDiag erkennt das am RTC-Block (ueberlebt Absturz und
    // Neustart, nicht aber Stromlos/Reset-Knopf). Noetig, weil der Chip nach
    // dem Einschalten bzw. dem Reset-Knopf einen Watchdog-Reset meldet
    // (rst 0x7, CORE_MWDT - Befund 14.11.2026), bevor das Programm laeuft.
    const esp_reset_reason_t reason = esp_reset_reason();
    if (planned || CrashDiag_WasColdStart() || !needsColdRestart(reason)) {
        prefs.end();
        return;
    }
    if (s_inRow >= kMaxInRow) {
        // Absturz kurz nach dem Kaltstart: nicht endlos stromlos schalten.
        Serial.printf("[ColdRestart] %u Kaltstarts in Folge - diesmal Warmstart.\n", (unsigned)s_inRow);
        prefs.end();
        return;
    }
    prefs.putUChar(kKeyRow, (uint8_t)(s_inRow + 1));
    prefs.end();

    ColdRestart_Now(CrashDiag_ResetReasonText(), ColdRestartReport::Crash);
    // Nur bei nicht lesbarer Uhr hier: als Warmstart weiterlaufen.
}

void ColdRestart_Update() {
    if (s_rowChecked || millis() < kStableMs) return;
    s_rowChecked = true;
    if (s_inRow == 0) return;

    Preferences prefs;
    prefs.begin(kNvsNs, false);
    prefs.putUChar(kKeyRow, 0);
    prefs.end();
    s_inRow = 0;
}

void ColdRestart_WarmRestart(const char* why) {
    Serial.printf("[ColdRestart] Gewollter Neustart (%s), ohne Stromlos-Phase.\n", why ? why : "");
    Serial.flush();
    s_plannedRestart = kPlannedMagic;
    esp_restart();
}

void ColdRestart_RestartCold(const char* why) {
    ColdRestart_Now(why, ColdRestartReport::Planned);
    // Nur bei nicht lesbarer Uhr hier: dann wenigstens warm neu starten.
    ColdRestart_WarmRestart(why);
}

bool ColdRestart_Now(const char* why, ColdRestartReport report) {
    m5::rtc_datetime_t dt;
    if (!readClock(dt)) {
        Serial.println("[ColdRestart] Abgebrochen: Echtzeituhr nicht lesbar.");
        return false;
    }

    // Uhr auf hh:mm:(60-kLeadS), Wecker auf die naechste volle Minute.
    const int8_t setSec = (int8_t)(60 - kLeadS);
    const int    shift  = setSec - dt.time.seconds;
    const int    wakeAt = (dt.time.hours * 60 + dt.time.minutes + 1) % (24 * 60);

    // Erst sichern, dann verstellen: so wird die Uhr beim naechsten Start in
    // jedem Fall korrigiert.
    Preferences prefs;
    prefs.begin(kNvsNs, false);
    prefs.putInt(kKeyShift, shift);
    prefs.end();
    if (report != ColdRestartReport::None) {
        CrashDiag_SaveForColdRestart(report == ColdRestartReport::Planned);
    }

    Serial.printf("[ColdRestart] %s -> stromlos, Wiedereinschalten in ca. %d s.\n",
                  why ? why : "", kLeadS);
    Serial.flush();

    M5.Rtc.setTime(m5::rtc_time_t(dt.time.hours, dt.time.minutes, setSec));

    // Rueckfall, falls die Versorgung anbleibt (USB): Tiefschlaf mit Timer.
    esp_sleep_enable_timer_wakeup((uint64_t)(kLeadS + 1) * 1000000ULL);
    M5.Power.timerSleep(m5::rtc_time_t((int8_t)(wakeAt / 60), (int8_t)(wakeAt % 60), -1));
    return false;   // nicht erreicht
}
