#pragma once
// NVS-Ersatz: je Namensraum eine Tabelle Schluessel -> Text. Zaehlt Schreibzugriffe.
#include "Arduino.h"
#include <map>
#include <string>
extern std::map<std::string, std::map<std::string, std::string>> g_nvs;
extern unsigned g_nvsWrites;
class Preferences {
    std::map<std::string, std::string>* t = nullptr; bool ro = true;
public:
    bool begin(const char* ns, bool readOnly) {
        if (readOnly && !g_nvs.count(ns)) return false;
        t = &g_nvs[ns]; ro = readOnly; return true;
    }
    void end() { t = nullptr; }
    void clear() { t->clear(); g_nvsWrites++; }
    bool isKey(const char* k) { return t && t->count(k); }
    bool remove(const char* k) { g_nvsWrites++; return t->erase(k) > 0; }
    String getString(const char* k, const char* d = "") { return isKey(k) ? String((*t)[k]) : String(d); }
    String getString(const char* k, const String& d) { return isKey(k) ? String((*t)[k]) : d; }
    void putString(const char* k, const String& v) { (*t)[k] = v.s; g_nvsWrites++; }
    void putString(const char* k, const char* v) { (*t)[k] = v; g_nvsWrites++; }
    uint8_t getUChar(const char* k, uint8_t d = 0) { return isKey(k) ? (uint8_t)std::stoi((*t)[k]) : d; }
    void putUChar(const char* k, uint8_t v) { (*t)[k] = std::to_string(v); g_nvsWrites++; }
    bool getBool(const char* k, bool d = false) { return isKey(k) ? (*t)[k] == "1" : d; }
    void putBool(const char* k, bool v) { (*t)[k] = v ? "1" : "0"; g_nvsWrites++; }
};
