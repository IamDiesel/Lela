// ==============================================================================
// PC-Test fuer system/WifiNetworks
// ==============================================================================
// Bauen und starten (im Projektordner, Linux/WSL), Befehl in einer Zeile:
//   g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined
//       -I tools/tests/wifi_networks/mock -I src/system
//       tools/tests/wifi_networks/test_wifi_networks.cpp src/system/WifiNetworks.cpp
//       -o /tmp/wifitest
//   /tmp/wifitest
//
// mock/ ersetzt Arduino.h und Preferences.h (NVS als Tabelle im Speicher).
// Geprueft werden: Uebernahme der frueheren festen IP, Merken (neu ans Ende,
// Bekanntes bleibt an seiner Stelle), Reihenfolge aendern, Vormerk-Platz fuer
// ein noch nicht gemerktes Netz, Obergrenze, Vergessen, sparsame Schreibzugriffe.
// ==============================================================================
#include "WifiNetworks.h"
#include <Preferences.h>

SerialMock Serial;
uint32_t g_mockMillis = 0;
std::map<std::string, std::map<std::string, std::string>> g_nvs;
unsigned g_nvsWrites = 0;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { g_failures++; printf("FEHLER Zeile %d: %s\n", __LINE__, #cond); } } while (0)

static String order() {
    String o;
    for (const WifiNetwork& n : WifiNetworks_List()) { if (o.length()) o += ","; o += n.ssid; }
    return o;
}

int main() {
    // Ausgangslage wie auf dem Geraet vor der Umstellung: zwei gemerkte Netze
    // (zuletzt verbundenes zuerst), eine feste IP fuer alle im Bereich "catmat".
    g_nvs["wifinets"] = { {"n", "2"}, {"s0", "Heim"}, {"p0", "geheim"}, {"c0", "8"},
                          {"s1", "Oma"}, {"p1", "kekse"}, {"c1", "1"} };
    g_nvs["catmat"] = { {"wifiSsid", "Heim"}, {"ipFix", "1"}, {"ipAddr", "192.168.2.50"},
                        {"ipGw", "192.168.2.1"}, {"ipMask", "255.255.255.0"}, {"ipDns", ""}, {"other", "x"} };

    // 1. Uebernahme: die feste IP gehoert danach "Heim", die alten Schluessel sind weg.
    CHECK(order() == "Heim,Oma");
    WifiIpConfig ip = WifiNetworks_IpConfigFor("Heim");
    CHECK(ip.fixed && ip.ip == "192.168.2.50" && ip.gateway == "192.168.2.1" && ip.subnet == "255.255.255.0");
    CHECK(WifiIpConfig_Valid(ip));
    CHECK(!WifiNetworks_IpConfigFor("Oma").fixed);
    CHECK(!g_nvs["catmat"].count("ipFix") && !g_nvs["catmat"].count("ipAddr") && g_nvs["catmat"].count("other"));
    WifiNetwork heim;
    CHECK(WifiNetworks_Find("Heim", &heim) && heim.pass == "geheim" && heim.channel == 8);
    CHECK(!WifiNetworks_Find("Fremd", nullptr));

    // 2. Merken: Bekanntes bleibt an seiner Stelle und behaelt die IP; ohne
    //    Aenderung wird nicht geschrieben.
    unsigned writes = g_nvsWrites;
    WifiNetworks_Remember("Oma", "kekse", 1);
    CHECK(g_nvsWrites == writes);
    WifiNetworks_Remember("Oma", "kekse", 6);
    CHECK(g_nvsWrites > writes && order() == "Heim,Oma");
    WifiNetworks_Remember("Heim", "geheim", 11);
    CHECK(order() == "Heim,Oma" && WifiNetworks_IpConfigFor("Heim").fixed);

    // 3. Neues Netz ans Ende, mit vorgemerkter IP.
    WifiIpConfig hotel;
    hotel.fixed = true; hotel.ip = "10.0.0.7"; hotel.gateway = "10.0.0.1"; hotel.subnet = "255.255.255.0";
    WifiNetworks_SetIpConfig("Hotel", hotel);
    CHECK(order() == "Heim,Oma");                         // noch nicht gemerkt
    CHECK(WifiNetworks_IpConfigFor("Hotel").ip == "10.0.0.7");
    CHECK(!WifiNetworks_IpConfigFor("Anderes").fixed);
    WifiNetworks_Remember("Hotel", "zimmer", 3);
    CHECK(order() == "Heim,Oma,Hotel");
    CHECK(WifiNetworks_IpConfigFor("Hotel").fixed && WifiNetworks_IpConfigFor("Hotel").ip == "10.0.0.7");
    CHECK(!g_nvs["wifinets"].count("sx"));                 // Vormerkung verbraucht

    // 4. Reihenfolge aendern.
    CHECK(WifiNetworks_Move("Hotel", -1) && order() == "Heim,Hotel,Oma");
    CHECK(WifiNetworks_Move("Hotel", -1) && order() == "Hotel,Heim,Oma");
    CHECK(!WifiNetworks_Move("Hotel", -1));               // schon oben
    CHECK(WifiNetworks_Move("Hotel", +2) && order() == "Heim,Oma,Hotel");
    CHECK(!WifiNetworks_Move("Hotel", +1) && !WifiNetworks_Move("Fremd", 1));
    CHECK(WifiNetworks_IpConfigFor("Heim").ip == "192.168.2.50" && WifiNetworks_IpConfigFor("Hotel").ip == "10.0.0.7");

    // 5. IP eines gemerkten Netzes aendern; DHCP behaelt die Felder.
    ip.fixed = false;
    WifiNetworks_SetIpConfig("Heim", ip);
    CHECK(!WifiNetworks_IpConfigFor("Heim").fixed && WifiNetworks_IpConfigFor("Heim").ip == "192.168.2.50");
    writes = g_nvsWrites;
    WifiNetworks_SetIpConfig("Heim", ip);
    CHECK(g_nvsWrites == writes);

    // 6. Gueltigkeit.
    WifiIpConfig bad = hotel;
    bad.subnet = "0.255.255.0";  CHECK(!WifiIpConfig_Valid(bad));
    bad = hotel; bad.ip = "10.0.0";        CHECK(!WifiIpConfig_Valid(bad));
    bad = hotel; bad.dns = "abc";          CHECK(!WifiIpConfig_Valid(bad));
    bad = hotel; bad.dns = "8.8.8.8";      CHECK(WifiIpConfig_Valid(bad));

    // 7. Obergrenze: das Netz mit der niedrigsten Prioritaet faellt heraus.
    for (int i = 0; i < 6; i++) WifiNetworks_Remember(String("N") + String(i), "p", 1);
    CHECK(WifiNetworks_List().size() == WIFI_NETWORKS_MAX);
    CHECK(order() == "Heim,Oma,Hotel,N0,N1,N2,N3,N5");

    // 8. Vergessen und alles loeschen.
    CHECK(WifiNetworks_Forget("Oma") && !WifiNetworks_Forget("Oma"));
    CHECK(order() == "Heim,Hotel,N0,N1,N2,N3,N5");
    WifiNetworks_Clear();
    CHECK(WifiNetworks_List().empty());

    printf("%s (%d Fehler)\n", g_failures == 0 ? "ALLES OK" : "FEHLGESCHLAGEN", g_failures);
    return g_failures == 0 ? 0 : 1;
}
