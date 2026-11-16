// ==============================================================================
// CrashDiag - Implementierung
// Siehe CrashDiag.h und src/doc/video_refactoring.md (Experiment E-1).
// ==============================================================================

#include "CrashDiag.h"
#include "SharedData.h"
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <esp_rom_sys.h>

// Magic zur Gueltigkeitspruefung des RTC-Blocks. Nach einem Kaltstart enthaelt
// der RTC-Speicher Zufallswerte, die Magic stimmt dann nicht mehr.
// Bei jeder Aenderung an CrashDiagState hochzaehlen: der Block ueberlebt auch
// einen Firmware-Upload und wuerde sonst mit altem Aufbau gelesen.
static const uint32_t CRASHDIAG_MAGIC = 0xC0FFEE43;

// Zustand im RTC-Speicher: ueberlebt Panic-/Software-Reset, nicht aber
// einen echten Stromausfall.
typedef struct {
    uint32_t magic;           // Gueltigkeitskennung
    uint32_t bootCount;       // Starts seit dem letzten Kaltstart
    uint32_t panicCount;      // Abstuerze seit dem letzten Kaltstart
    uint32_t lastUptimeMs;    // Laufzeit beim letzten Lebenszeichen
    uint32_t lastReason;      // esp_reset_reason() des vorherigen Laufs
    uint8_t  lastStreamActive;// Video aktiv beim letzten Lebenszeichen
    uint8_t  lastFullscreen;  // Vollbild aktiv beim letzten Lebenszeichen
    uint16_t lastFps;         // zuletzt gemessene Bildrate
    uint32_t resumeWord;      // Merkwort der Oberflaeche (CrashDiag_SetResumeWord)
} CrashDiagState;

RTC_NOINIT_ATTR static CrashDiagState s_state;

static bool s_wasColdStart = false;
static bool s_initDone     = false;
static bool s_rtcLostWithoutPoweron = false;
static bool s_published             = false;
static esp_reset_reason_t s_reason = ESP_RST_UNKNOWN;

// Automatischer Kaltstart (ColdRestart): Grund des Warmstarts davor und der
// daraus gebildete Text fuer Bericht und Diagnose-Seite.
static bool s_autoCold = false;
static bool s_autoPlanned = false;   // ... und der war gewollt (System > Neustart)

// Roher Reset-Grund des Chips (ROM, wie "rst:0x.." im Boot-Log) und der Text
// fuer Bericht und Diagnose-Seite: "<Grund> [rst 0x..]" bzw. nach einem
// automatischen Kaltstart "<Grund des Warmstarts> [rst 0x..], danach stromlos".
static uint32_t s_romReason = 0;
static char     s_reasonText[80] = "";

// Uebergabe ueber den Stromausfall hinweg (NVS, siehe SaveForColdRestart).
static const char*    CARRY_NS    = "crashdiag";
static const char*    CARRY_KEY   = "carry";
static const uint32_t CARRY_MAGIC = 0xC01D0002;   // mit CrashDiagState hochzaehlen

typedef struct {
    uint32_t       magic;
    uint32_t       reason;       // esp_reset_reason() des Warmstarts
    uint32_t       bootCount;    // Zaehler zum Zeitpunkt des Warmstarts
    uint32_t       panicCount;
    uint32_t       romReason;    // roher Reset-Grund (ROM) des Warmstarts
    uint32_t       planned;      // 1 = gewollter Neustart (kein Absturz)
    CrashDiagState prev;         // Kontext des Laufs vor dem Neustart
} CrashDiagCarry;

// Kopie des vorherigen Laufs. Wird in CrashDiag_Init() gesichert, weil der
// RTC-Block dort fuer den neuen Lauf zurueckgesetzt wird, der Bericht aber
// erst spaeter ausgegeben werden kann.
static CrashDiagState s_prev;

// ------------------------------------------------------------------
// Reset-Grund in lesbaren Text uebersetzen
// ------------------------------------------------------------------
static const char* resetReasonToText(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "Kaltstart (Power-On)";
        case ESP_RST_EXT:       return "Externer Reset-Pin";
        case ESP_RST_SW:        return "Software-Reset (esp_restart)";
        case ESP_RST_PANIC:     return "PANIC / Exception";
        case ESP_RST_INT_WDT:   return "Interrupt-Watchdog";
        case ESP_RST_TASK_WDT:  return "Task-Watchdog";
        case ESP_RST_WDT:       return "Sonstiger Watchdog";
        case ESP_RST_BROWNOUT:  return "Brownout (Spannungseinbruch)";
        case ESP_RST_DEEPSLEEP: return "Aufwachen aus Deep Sleep";
        case ESP_RST_SDIO:      return "SDIO-Reset";
        // Reset ueber die USB-Schnittstelle des Chips - so setzt esptool den
        // Tab5 nach dem Hochladen zurueck ("Hard resetting via RTS pin"). Das
        // ist KEIN Reset-Knopf: die Peripherie bleibt unter Strom.
        case ESP_RST_USB:       return "USB-Reset (z.B. nach Upload)";
        case ESP_RST_JTAG:      return "JTAG-Reset";
        case ESP_RST_PWR_GLITCH:return "Spannungs-Glitch";
        case ESP_RST_CPU_LOCKUP:return "CPU-Lockup";
        default:                return "Unbekannt";
    }
}

// Zaehlt dieser Reset-Grund als Absturz?
static bool isCrashReason(esp_reset_reason_t reason) {
    return reason == ESP_RST_PANIC
        || reason == ESP_RST_INT_WDT
        || reason == ESP_RST_TASK_WDT
        || reason == ESP_RST_WDT
        || reason == ESP_RST_BROWNOUT;
}

// ------------------------------------------------------------------
// Uebergabe nach einem automatischen Kaltstart (ColdRestart) uebernehmen.
// Der Eintrag wird in jedem Fall geloescht: er gilt nur fuer genau diesen Start.
// ------------------------------------------------------------------
static void applyCarryOver() {
    Preferences prefs;
    if (!prefs.begin(CARRY_NS, false)) return;
    if (!prefs.isKey(CARRY_KEY)) { prefs.end(); return; }

    CrashDiagCarry carry;
    const bool ok = prefs.getBytesLength(CARRY_KEY) == sizeof(carry)
                 && prefs.getBytes(CARRY_KEY, &carry, sizeof(carry)) == sizeof(carry)
                 && carry.magic == CARRY_MAGIC;
    prefs.remove(CARRY_KEY);
    prefs.end();
    if (!ok) return;

    // Die Hardware war stromlos (s_wasColdStart bleibt), die Vorgeschichte
    // stammt aber aus dem Warmstart davor.
    s_autoCold         = true;
    s_autoPlanned      = carry.planned != 0;
    s_prev             = carry.prev;
    s_state.bootCount  = carry.bootCount + 1;
    s_state.panicCount = carry.panicCount;
    // Blieb die Versorgung an (USB), kam das Geraet nur aus dem Tiefschlaf.
    const char* after = (s_reason == ESP_RST_DEEPSLEEP) ? "danach nur Tiefschlaf (USB?)"
                                                        : "danach stromlos";
    if (carry.planned) {
        snprintf(s_reasonText, sizeof(s_reasonText), "Neustart (gewollt), %s", after);
    } else {
        snprintf(s_reasonText, sizeof(s_reasonText), "%s [rst 0x%X], %s",
                 resetReasonToText((esp_reset_reason_t)carry.reason),
                 (unsigned)carry.romReason, after);
    }
}

// ------------------------------------------------------------------
// Initialisierung und Bericht des vorherigen Laufs
// ------------------------------------------------------------------
void CrashDiag_Init() {
    s_reason    = esp_reset_reason();
    s_romReason = (uint32_t)esp_rom_get_reset_reason(0);

    // Zwei unabhaengige Signale:
    //  - rtcValid: haelt der RTC-Block noch gueltige Daten? Er ueberlebt einen
    //    Panic-/Software-Reset, nicht aber einen Stromausfall oder das Flashen.
    //  - poweron:  meldet der Chip selbst einen Kaltstart?
    bool rtcValid = (s_state.magic == CRASHDIAG_MAGIC);
    bool poweron  = (s_reason == ESP_RST_POWERON);

    s_wasColdStart = !rtcValid || poweron;

    // Sonderfall fuer den Bericht: RTC-Block verloren, der Chip meldet aber
    // keinen Power-On. Typisch unmittelbar nach dem Flashen.
    s_rtcLostWithoutPoweron = !rtcValid && !poweron;

    // Zustand des vorherigen Laufs sichern, bevor er ueberschrieben wird.
    s_prev = s_state;

    if (s_wasColdStart) {
        // Die Kopie enthaelt nach einem Kaltstart Zufallswerte - nullen, damit
        // sie nicht in den MQTT-Bericht gelangen.
        memset(&s_prev, 0, sizeof(s_prev));

        // Frischer Start: es gibt keine Vorgeschichte, auf die sich Zaehler
        // beziehen koennten.
        s_state.magic      = CRASHDIAG_MAGIC;
        s_state.bootCount  = 0;
        s_state.panicCount = 0;
    }

    s_state.bootCount++;

    // Ein Absturz wird nur gezaehlt, wenn es einen vorherigen Lauf gibt, dem er
    // zugeordnet werden kann. Nach einem Kaltstart ist die Historie leer - der
    // Reset-Grund dieses Starts gehoert dann zu keinem beobachteten Lauf.
    if (!s_wasColdStart && isCrashReason(s_reason)) {
        s_state.panicCount++;
    }

    s_state.lastReason = (uint32_t)s_reason;

    // Zaehler fuer den laufenden Betrieb zuruecksetzen.
    s_state.lastUptimeMs     = 0;
    s_state.lastStreamActive = 0;
    s_state.lastFullscreen   = 0;
    s_state.lastFps          = 0;
    s_state.resumeWord       = 0;

    applyCarryOver();
    if (!s_autoCold) {
        snprintf(s_reasonText, sizeof(s_reasonText), "%s [rst 0x%X]",
                 resetReasonToText(s_reason), (unsigned)s_romReason);
    }
    s_initDone = true;
}

void CrashDiag_Report() {
    if (!s_initDone) {
        Serial.println("[CrashDiag] FEHLER: Report vor Init aufgerufen.");
        return;
    }

    Serial.println("==============================================");
    Serial.printf("[CrashDiag] Reset-Grund: %s\n", CrashDiag_ResetReasonText());

    if (s_autoCold) {
        Serial.println("[CrashDiag] AUTOMATISCHER KALTSTART (ColdRestart) - Audio-Chips waren stromlos.");
        Serial.printf("[CrashDiag] Lauf vor dem Warmstart: %lu.%03lu s | Video: %s | Vollbild: %s | %u fps\n",
                      (unsigned long)(s_prev.lastUptimeMs / 1000),
                      (unsigned long)(s_prev.lastUptimeMs % 1000),
                      s_prev.lastStreamActive ? "AN" : "aus",
                      s_prev.lastFullscreen   ? "AN" : "aus",
                      (unsigned)s_prev.lastFps);
    } else if (s_wasColdStart) {
        Serial.println("[CrashDiag] KALTSTART - keine Vorgeschichte, Zaehler auf null.");
        if (s_rtcLostWithoutPoweron) {
            Serial.println("[CrashDiag] Hinweis: RTC-Block verloren, Grund war aber kein");
            Serial.println("[CrashDiag] Power-On. Typisch direkt nach dem Flashen.");
        } else {
            Serial.println("[CrashDiag] Auch der DMA2D-Block war stromlos (Experiment E-1).");
        }
    } else {
        Serial.println("[CrashDiag] WARMSTART - Peripherie wurde NICHT stromlos!");
        Serial.printf("[CrashDiag] Vorheriger Lauf: %lu.%03lu s | Video: %s | Vollbild: %s | %u fps\n",
                      (unsigned long)(s_prev.lastUptimeMs / 1000),
                      (unsigned long)(s_prev.lastUptimeMs % 1000),
                      s_prev.lastStreamActive ? "AN" : "aus",
                      s_prev.lastFullscreen   ? "AN" : "aus",
                      (unsigned)s_prev.lastFps);
    }

    Serial.printf("[CrashDiag] Seit Kaltstart: %lu Starts, %lu Abstuerze\n",
                  (unsigned long)s_state.bootCount,
                  (unsigned long)s_state.panicCount);
    if (CrashDiag_AudioMayBeMuted()) {
        Serial.println("[CrashDiag] Audio-Ausgabe evtl. stumm - Geraet einmal stromlos neu starten.");
    }
    Serial.println("==============================================");
    Serial.flush();
}

// ------------------------------------------------------------------
// Bericht einmalig ueber MQTT ablegen
//
// Die serielle Konsole taugt fuer den Alltagsbetrieb nicht: sie setzt einen
// angeschlossenen Rechner voraus und verliert nach einem Absturz die
// Verbindung. Der Broker ist ohnehin verbunden - dort liegt der Bericht
// retained und ist jederzeit abrufbar.
// ------------------------------------------------------------------
static void publishReportOnce() {
    if (s_published || !s_initDone) return;
    if (!mqttEnabled || !mqttClient.connected()) return;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"reason\":\"%s\",\"cold\":%d,\"auto_cold\":%d,\"prev_ms\":%lu,\"video\":%d,"
             "\"fs\":%d,\"fps\":%u,\"boots\":%lu,\"crashes\":%lu}",
             CrashDiag_ResetReasonText(),
             s_wasColdStart ? 1 : 0,
             s_autoCold ? 1 : 0,
             (unsigned long)s_prev.lastUptimeMs,
             s_prev.lastStreamActive ? 1 : 0,
             s_prev.lastFullscreen   ? 1 : 0,
             (unsigned)s_prev.lastFps,
             (unsigned long)s_state.bootCount,
             (unsigned long)s_state.panicCount);

    if (mqttClient.publish(CRASHDIAG_MQTT_TOPIC, payload, true)) {
        s_published = true;
        Serial.printf("[CrashDiag] Bericht auf %s abgelegt.\n", CRASHDIAG_MQTT_TOPIC);
    }
}

// ------------------------------------------------------------------
// Lebenszeichen, hoechstens einmal pro Sekunde
// ------------------------------------------------------------------
void CrashDiag_Update() {
    static uint32_t lastWrite = 0;
    uint32_t now = millis();

    if (now - lastWrite < 1000) return;
    lastWrite = now;

    // Sobald MQTT steht, den Bericht des vorherigen Laufs ablegen.
    publishReportOnce();

    s_state.lastUptimeMs     = now;
    s_state.lastStreamActive = isStreamActive ? 1 : 0;
    s_state.lastFullscreen   = vidFSMode ? 1 : 0;
    s_state.lastFps          = (uint16_t)currentFps;
}

uint32_t CrashDiag_GetPanicCount() {
    return s_state.panicCount;
}

const char* CrashDiag_ResetReasonText() {
    return s_initDone ? s_reasonText : resetReasonToText(s_reason);
}

bool CrashDiag_WasAutoColdRestart() {
    return s_autoCold;
}

bool CrashDiag_WasUnplannedRestart() {
    // Nach dem automatischen Kaltstart zaehlt dessen Anlass; sonst der
    // Reset-Grund dieses (Warm-)Starts.
    if (s_autoCold) return !s_autoPlanned;
    return !s_wasColdStart && isCrashReason(s_reason);
}

void CrashDiag_SetResumeWord(uint32_t word) {
    s_state.resumeWord = word;
}

uint32_t CrashDiag_PrevResumeWord() {
    return s_prev.resumeWord;
}

uint32_t CrashDiag_PrevUptimeMs() {
    return s_prev.lastUptimeMs;
}

void CrashDiag_SaveForColdRestart(bool planned) {
    CrashDiagCarry carry;
    carry.magic      = CARRY_MAGIC;
    carry.bootCount  = s_state.bootCount;
    carry.panicCount = s_state.panicCount;
    carry.planned    = planned ? 1 : 0;
    if (planned) {
        // Aus dem laufenden Betrieb: dieser Lauf ist der "vorherige".
        carry.reason            = (uint32_t)ESP_RST_SW;
        carry.romReason         = 0;
        carry.prev              = s_state;
        carry.prev.lastUptimeMs = millis();
    } else {
        // Beim Start nach einem Absturz: der abgestuerzte Lauf.
        carry.reason    = (uint32_t)s_reason;
        carry.romReason = s_romReason;
        carry.prev      = s_prev;
    }

    Preferences prefs;
    prefs.begin(CARRY_NS, false);
    prefs.putBytes(CARRY_KEY, &carry, sizeof(carry));
    prefs.end();
}

void CrashDiag_TriggerCrash() {
    Serial.println("[CrashDiag] Absturz wird absichtlich ausgeloest (Test).");
    Serial.flush();
    // volatile: der Compiler darf den Zugriff weder entfernen noch umstellen.
    volatile uint32_t* const nowhere = reinterpret_cast<volatile uint32_t*>(0);
    *nowhere = 0xDEADBEEF;
    abort();   // falls die Adresse wider Erwarten beschreibbar war
}

void CrashDiag_DeepSleepRestart() {
    Serial.println("[CrashDiag] Neustart ueber 1 s Tiefschlaf (Test).");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(1000000ULL);
    esp_deep_sleep_start();
}

bool CrashDiag_WasColdStart() {
    return s_wasColdStart;
}

bool CrashDiag_AudioMayBeMuted() {
    return !s_wasColdStart && s_state.panicCount > 0;
}
