#pragma once
// ==============================================================================
// VoiceProcessor - Sprachaufbereitung fuer das Gegensprechen (reines C++)
// ==============================================================================
// Raeumt das Mikrofonsignal auf, bevor es an die Kamera geht. Drei Stufen,
// jede einzeln abschaltbar:
//
//   Mikrofon --> [Hochpass] --> [Tiefpass] --> [Pegel + Begrenzer] --> Senden
//
//   Hochpass    schneidet Rumpeln, Griffgeraeusche und Brummen unter der
//               Stimme ab (2. Ordnung, 12 dB je Oktave).
//   Tiefpass    schneidet Zischen und Rauschen ueber der Stimme ab. Sprache
//               braucht bis etwa 7-8 kHz (4. Ordnung, 24 dB je Oktave).
//   Pegel       hebt die Stimme an: entweder um einen festen Wert oder
//               selbsttaetig (Pegelautomatik) bis zu einer Obergrenze. Die
//               Automatik regelt auf einen Zielpegel, senkt bei lautem
//               Sprechen sofort ab und hebt bei leisem langsam an. Angehoben
//               wird nur, waehrend gesprochen wird: in Sprechpausen und unter
//               der Rauschschwelle bleibt die Verstaerkung stehen - Stille
//               wird nicht "aufgedreht", die Lautstaerke pumpt nicht.
//   Begrenzer   faengt Spitzen ab, bevor sie uebersteuern. Immer aktiv, sobald
//               verstaerkt wird.
//
// Befund 17.11.2026, der zu diesem Baustein fuehrte: Der Mitschnitt des
// Gegensprechens hatte einen starken Anteil zwischen 5 und 20 kHz (an der
// Kamera als Zischen hoerbar) und nur 17-37 % Aussteuerung.
//
// Ohne Abhaengigkeiten, damit es am PC pruefbar ist (tools/tests/voice_processor).
// Rechnet mit float: der ESP32-P4 hat eine Gleitkomma-Einheit.
// ==============================================================================

#include <stddef.h>
#include <stdint.h>

// --- Grenzen und Vorgaben der Einstellungen ------------------------------------
static const uint16_t VOICE_HIGHPASS_MIN_HZ     = 50;
static const uint16_t VOICE_HIGHPASS_MAX_HZ     = 400;
static const uint16_t VOICE_HIGHPASS_DEFAULT_HZ = 100;
static const uint16_t VOICE_LOWPASS_MIN_HZ      = 3000;
static const uint16_t VOICE_LOWPASS_MAX_HZ      = 12000;
static const uint16_t VOICE_LOWPASS_DEFAULT_HZ  = 7000;
static const uint8_t  VOICE_GAIN_MAX_DB         = 30;
static const uint8_t  VOICE_GAIN_DEFAULT_DB     = 12;
static const bool     VOICE_AUTO_GAIN_DEFAULT   = true;

// Einstellungen. 0 Hz schaltet das jeweilige Filter ab.
struct VoiceConfig {
    uint16_t highpassHz;   // Grenzfrequenz des Hochpasses, 0 = aus
    uint16_t lowpassHz;    // Grenzfrequenz des Tiefpasses, 0 = aus
    bool     autoGain;     // true: Pegelautomatik, false: feste Verstaerkung
    uint8_t  gainDb;       // feste Verstaerkung bzw. Obergrenze der Automatik (dB)
};

// Bringt Werte ausserhalb der Grenzen in den erlaubten Bereich (0 bleibt 0).
VoiceConfig VoiceConfig_Sanitized(VoiceConfig cfg);

// Ein Filter 2. Ordnung mit seinem Zustand.
struct VoiceBiquad {
    float b0, b1, b2, a1, a2;   // Koeffizienten (a0 = 1)
    float z1, z2;               // Zustand (transponierte Direktform II)
};

struct VoiceProcessor {
    VoiceConfig cfg;
    bool        bypass;         // nichts zu tun: Samples bleiben unveraendert

    bool        useHighpass;
    bool        useLowpass;
    VoiceBiquad highpass;
    VoiceBiquad lowpass[2];     // zwei Stufen = 4. Ordnung

    bool        useGain;
    float       fixedGain;      // Faktor ohne Automatik
    float       maxGain;        // Obergrenze der Automatik (Faktor)
    float       envelope;       // Huellkurve des Eingangspegels (Spitze), traege
    float       activity;       // schnelle Huellkurve: wird gerade gesprochen?
    float       gain;           // aktuell wirksamer Faktor
    float       releaseCoef;    // Abklingen der traegen Huellkurve je Sample
    float       activityCoef;   // Abklingen der schnellen Huellkurve je Sample
    float       attackCoef;     // Absenken der Verstaerkung je Sample
    float       riseCoef;       // Anheben der Verstaerkung je Sample
};

// Neue Sitzung: Einstellungen uebernehmen, Filter berechnen, Zustand loeschen.
void VoiceProcessor_Reset(VoiceProcessor* vp, const VoiceConfig& cfg, uint32_t rateHz);

// true, wenn die Einstellungen das Signal unveraendert lassen.
bool VoiceProcessor_IsBypass(const VoiceProcessor* vp);

// Verarbeitet count Samples an Ort und Stelle.
void VoiceProcessor_Process(VoiceProcessor* vp, int16_t* samples, size_t count);

// Aktuell wirksame Verstaerkung in Zehntel-dB (fuer Logzeilen), 0 ohne Pegelstufe.
int VoiceProcessor_GainTenthDb(const VoiceProcessor* vp);
