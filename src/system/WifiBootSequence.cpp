// ==============================================================================
// WifiBootSequence - Implementierung (siehe Header)
// ==============================================================================

#include "WifiBootSequence.h"

bool WifiBootSequence::start(const String& lastSsid, const String& lastPass,
                             const std::vector<WifiNetwork>& known) {
    m_candidates.clear();
    m_index = 0;
    m_state = WifiBootState::Idle;

    // Zuerst das zuletzt verbundene Netz - mit dem dort gespeicherten Passwort
    // (es kann neuer sein als das in der Liste, z.B. frisch im Web-Setup).
    if (lastSsid.length() > 0) m_candidates.push_back({ lastSsid, lastPass });

    // Dann die gemerkten Netze nach Prioritaet.
    for (const WifiNetwork& n : known) {
        if (n.ssid.length() == 0 || n.ssid == lastSsid) continue;
        m_candidates.push_back({ n.ssid, n.pass });
    }

    if (m_candidates.empty()) return false;
    m_state = WifiBootState::Trying;
    return true;
}

WifiBootStep WifiBootSequence::step(bool connected, uint8_t failEvents, uint32_t attemptAgeMs) {
    if (m_state != WifiBootState::Trying) return WifiBootStep::Stay;

    if (connected) {
        m_state = WifiBootState::Connected;
        return WifiBootStep::Connected;
    }

    const uint32_t limit = (m_index == 0) ? kFirstAttemptMs : kAttemptMs;
    if (failEvents >= kFailEvents || attemptAgeMs > limit) return advance();
    return WifiBootStep::Stay;
}

WifiBootStep WifiBootSequence::skip() {
    if (m_state != WifiBootState::Trying) return WifiBootStep::Stay;
    return advance();
}

WifiBootStep WifiBootSequence::abort() {
    if (m_state != WifiBootState::Trying) return WifiBootStep::Stay;
    m_state = WifiBootState::Aborted;
    return WifiBootStep::GaveUp;
}

// Naechster Versuch - oder Schluss, wenn die Folge einmal durch ist.
WifiBootStep WifiBootSequence::advance() {
    if (m_index + 1 < m_candidates.size()) {
        m_index++;
        return WifiBootStep::Next;
    }
    m_state = WifiBootState::Failed;
    return WifiBootStep::GaveUp;
}
