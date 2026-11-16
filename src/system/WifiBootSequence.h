#pragma once
// ==============================================================================
// WifiBootSequence - Startfolge der WLAN-Netze: welches Netz wann probiert wird
// ==============================================================================
// Beim Einschalten soll das Geraet von selbst ein erreichbares der gemerkten
// Netze finden, ohne dass der Normalfall laenger dauert:
//
//   1. Versuch: das zuletzt verbundene Netz. Zu Hause (und unterwegs ab dem
//      zweiten Start) ist das sofort das richtige - kein Zeitverlust.
//   danach:     die gemerkten Netze in der Reihenfolge ihrer Prioritaet
//               (WifiNetworks), ohne das schon probierte.
//
// Ein Versuch gilt als gescheitert, wenn der WLAN-Chip zweimal meldet, dass er
// das Netz nicht findet oder die Anmeldung abgelehnt wurde (ein fehlendes Netz
// faellt so nach wenigen Sekunden auf; zweimal, weil ein einzelner Suchlauf
// ein vorhandenes Netz auch einmal verfehlt), oder nach einem Zeitlimit.
// Der Nutzer kann den laufenden Versuch ueberspringen oder alles abbrechen
// (Knoepfe im Bootscreen).
//
// Die Folge laeuft genau einmal durch und nur beim Start. Findet sie nichts
// (oder wird abgebrochen), kehrt das Geraet zum ersten Netz der Folge zurueck
// und versucht es wie bisher im Hintergrund weiter. Im Betrieb wird nicht
// selbsttaetig gewechselt.
//
// Diese Klasse entscheidet nur. Anmelden, Ereignisse zaehlen und die Uhr
// gehoeren WifiConnect - so ist die Folge auf dem PC pruefbar
// (tools/tests/wifi_networks/test_wifi_boot_sequence.cpp).
// ==============================================================================

#include <Arduino.h>
#include <vector>
#include "WifiNetworks.h"

enum class WifiBootState : uint8_t {
    Idle,        // keine Folge (kein Netz bekannt, WLAN aus oder nie gestartet)
    Trying,      // ein Versuch laeuft
    Connected,   // verbunden
    Failed,      // alle Netze probiert, keines erreichbar
    Aborted,     // vom Nutzer abgebrochen
};

// Was WifiConnect nach einer Entscheidung zu tun hat.
enum class WifiBootStep : uint8_t {
    Stay,        // nichts - weiter warten
    Next,        // current() anmelden (naechster Versuch)
    Connected,   // Folge beendet, current() ist das verbundene Netz
    GaveUp,      // Folge beendet ohne Verbindung: zu home() zurueckkehren
};

struct WifiCandidate {
    String ssid;
    String pass;
};

class WifiBootSequence {
public:
    // Zeitlimit je Versuch. Der erste enthaelt das Hochfahren des WLAN-Chips
    // (~3 s) und entspricht dem frueheren Gesamtlimit des Bootscreens.
    static constexpr uint32_t kFirstAttemptMs = 15000;
    static constexpr uint32_t kAttemptMs      = 10000;
    // So viele Fehlschlag-Meldungen des WLAN-Chips beenden einen Versuch.
    static constexpr uint8_t  kFailEvents     = 2;

    // Folge aufstellen und beginnen. false: kein Netz bekannt (bleibt Idle).
    // Bei true ist current() der erste Versuch - der Aufrufer meldet ihn an.
    bool start(const String& lastSsid, const String& lastPass, const std::vector<WifiNetwork>& known);

    // Einmal je Durchlauf, solange state() == Trying.
    //   connected:    WLAN steht (IP vorhanden)
    //   failEvents:   Fehlschlag-Meldungen seit Beginn dieses Versuchs
    //   attemptAgeMs: Dauer dieses Versuchs
    WifiBootStep step(bool connected, uint8_t failEvents, uint32_t attemptAgeMs);

    // Knopf "Naechstes Netz": laufenden Versuch aufgeben.
    WifiBootStep skip();
    // Knopf "Ohne WLAN starten" bzw. WLAN wurde abgeschaltet.
    WifiBootStep abort();

    WifiBootState state() const { return m_state; }
    uint8_t attempt() const { return (uint8_t)(m_index + 1); }          // ab 1
    uint8_t count() const   { return (uint8_t)m_candidates.size(); }
    // Netz des laufenden bzw. letzten Versuchs. Nur gueltig, wenn count() > 0.
    const WifiCandidate& current() const { return m_candidates[m_index]; }
    // Erstes Netz der Folge - dorthin kehrt das Geraet nach GaveUp zurueck.
    const WifiCandidate& home() const { return m_candidates[0]; }

private:
    WifiBootStep advance();

    std::vector<WifiCandidate> m_candidates;
    size_t        m_index = 0;
    WifiBootState m_state = WifiBootState::Idle;
};
