// ==============================================================================
// CrashDiag - Implementierung
// Siehe CrashDiag.h und src/doc/video_refactoring.md (Experiment E-1).
// ==============================================================================

#include "CrashDiag.h"
#include "SharedData.h"
#include <esp_system.h>
#include <esp_attr.h>

// Magic zur Gueltigkeitspruefung des RTC-Blocks. Nach einem Kaltstart enthaelt
// der RTC-Speicher Zufallswerte, die Magic stimmt dann nicht mehr.
static const uint32_t CRASHDIAG_MAGIC = 0xC0FFEE42;

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
} CrashDiagState;

RTC_NOINIT_ATTR static CrashDiagState s_state;

static bool s_wasColdStart = false;
static bool s_initDone     = false;
static bool s_rtcLostWithoutPoweron = false;
static bool s_published             = false;
static esp_reset_reason_t s_reason = ESP_RST_UNKNOWN;

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
// Initialisierung und Bericht des vorherigen Laufs
// ------------------------------------------------------------------
void CrashDiag_Init() {
    s_reason = esp_reset_reason();

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

    s_initDone = true;
}

void CrashDiag_Report() {
    if (!s_initDone) {
        Serial.println("[CrashDiag] FEHLER: Report vor Init aufgerufen.");
        return;
    }

    Serial.println("==============================================");
    Serial.printf("[CrashDiag] Reset-Grund: %s\n", resetReasonToText(s_reason));

    if (s_wasColdStart) {
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

    char payload[224];
    snprintf(payload, sizeof(payload),
             "{\"reason\":\"%s\",\"cold\":%d,\"prev_ms\":%lu,\"video\":%d,"
             "\"fs\":%d,\"fps\":%u,\"boots\":%lu,\"crashes\":%lu}",
             resetReasonToText(s_reason),
             s_wasColdStart ? 1 : 0,
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

bool CrashDiag_WasColdStart() {
    return s_wasColdStart;
}

bool CrashDiag_AudioMayBeMuted() {
    return !s_wasColdStart && s_state.panicCount > 0;
}
