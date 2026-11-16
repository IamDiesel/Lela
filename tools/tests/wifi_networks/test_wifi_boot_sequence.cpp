// ==============================================================================
// PC-Test fuer system/WifiBootSequence
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined
//       -I tools/tests/wifi_networks/mock -I src/system
//       tools/tests/wifi_networks/test_wifi_boot_sequence.cpp
//       src/system/WifiBootSequence.cpp -o /tmp/boottest
//   /tmp/boottest
//
// Geprueft werden: Reihenfolge (zuletzt verbundenes Netz zuerst, dann nach
// Prioritaet, ohne Doppelte), Ende eines Versuchs durch Meldungen des
// WLAN-Chips oder Zeitlimit, Ueberspringen, Abbrechen, Folge nur einmal durch.
// ==============================================================================
#include "WifiBootSequence.h"

SerialMock Serial;
uint32_t g_mockMillis = 0;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { g_failures++; printf("FEHLER Zeile %d: %s\n", __LINE__, #cond); } } while (0)

static WifiNetwork net(const char* ssid, const char* pass) {
    WifiNetwork n; n.ssid = ssid; n.pass = pass; return n;
}

int main() {
    const std::vector<WifiNetwork> known = { net("Heim", "alt"), net("Oma", "kekse"), net("Hotel", "zimmer") };

    // 1. Nichts bekannt: keine Folge.
    {
        WifiBootSequence s;
        CHECK(!s.start("", "", {}) && s.state() == WifiBootState::Idle && s.count() == 0);
        CHECK(s.step(false, 9, 99999) == WifiBootStep::Stay && s.skip() == WifiBootStep::Stay);
    }

    // 2. Zuletzt verbunden zuerst (mit SEINEM Passwort), dann nach Prioritaet, ohne Doppelte.
    {
        WifiBootSequence s;
        CHECK(s.start("Oma", "neu", known) && s.count() == 3 && s.attempt() == 1);
        CHECK(s.current().ssid == "Oma" && s.current().pass == "neu" && s.home().ssid == "Oma");
        CHECK(s.step(false, 0, 14999) == WifiBootStep::Stay);                 // erster Versuch: 15 s
        CHECK(s.step(false, 1, 3000) == WifiBootStep::Stay);                  // eine Meldung reicht nicht
        CHECK(s.step(false, 2, 3000) == WifiBootStep::Next && s.attempt() == 2 && s.current().ssid == "Heim");
        CHECK(s.step(false, 0, 10000) == WifiBootStep::Stay);                 // weitere Versuche: 10 s
        CHECK(s.step(false, 0, 10001) == WifiBootStep::Next && s.current().ssid == "Hotel");
        CHECK(s.step(true, 0, 500) == WifiBootStep::Connected && s.state() == WifiBootState::Connected);
        CHECK(s.current().ssid == "Hotel" && s.attempt() == 3);
        CHECK(s.step(false, 9, 99999) == WifiBootStep::Stay);                 // Folge ist beendet
        CHECK(s.skip() == WifiBootStep::Stay && s.abort() == WifiBootStep::Stay);
    }

    // 3. Kein zuletzt verbundenes Netz: nur die Liste.
    {
        WifiBootSequence s;
        CHECK(s.start("", "", known) && s.count() == 3 && s.current().ssid == "Heim" && s.current().pass == "alt");
    }

    // 4. Zuletzt verbundenes Netz steht nicht in der Liste (noch nie gelungen).
    {
        WifiBootSequence s;
        CHECK(s.start("Neu", "x", known) && s.count() == 4 && s.current().ssid == "Neu");
    }

    // 5. Alle durch: einmal, dann Schluss - zurueck zum ersten.
    {
        WifiBootSequence s;
        s.start("Heim", "alt", known);
        CHECK(s.step(false, 0, 15001) == WifiBootStep::Next);
        CHECK(s.skip() == WifiBootStep::Next && s.current().ssid == "Hotel");
        CHECK(s.skip() == WifiBootStep::GaveUp && s.state() == WifiBootState::Failed);
        CHECK(s.home().ssid == "Heim" && s.current().ssid == "Hotel");
    }

    // 6. Abbrechen; ein einziges Netz.
    {
        WifiBootSequence s;
        s.start("Heim", "alt", known);
        s.skip();
        CHECK(s.abort() == WifiBootStep::GaveUp && s.state() == WifiBootState::Aborted && s.home().ssid == "Heim");

        WifiBootSequence one;
        CHECK(one.start("Heim", "alt", { net("Heim", "alt") }) && one.count() == 1);
        CHECK(one.step(false, 2, 4000) == WifiBootStep::GaveUp && one.state() == WifiBootState::Failed);
    }

    printf("%s (%d Fehler)\n", g_failures == 0 ? "ALLES OK" : "FEHLGESCHLAGEN", g_failures);
    return g_failures == 0 ? 0 : 1;
}
