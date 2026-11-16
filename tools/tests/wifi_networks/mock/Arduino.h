#pragma once
// Funktionsfaehiger Ersatz fuer die PC-Tests (String, IPAddress, Serial, millis).
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
class String {
public:
    std::string s;
    String() {}
    String(const char* c) : s(c ? c : "") {}
    String(const std::string& x) : s(x) {}
    explicit String(char c) : s(1, c) {}
    explicit String(unsigned v) : s(std::to_string(v)) {}
    explicit String(int v) : s(std::to_string(v)) {}
    size_t length() const { return s.size(); }
    const char* c_str() const { return s.c_str(); }
    bool operator==(const String& o) const { return s == o.s; }
    bool operator!=(const String& o) const { return s != o.s; }
    bool operator==(const char* o) const { return s == o; }
    bool operator!=(const char* o) const { return s != o; }
    String operator+(const String& o) const { return String(s + o.s); }
    String operator+(const char* o) const { return String(s + o); }
    String& operator+=(const String& o) { s += o.s; return *this; }
    void trim() { size_t a = s.find_first_not_of(" \t\r\n"); size_t b = s.find_last_not_of(" \t\r\n");
                  s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1); }
};
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
class IPAddress {
public:
    uint8_t b[4] = {0, 0, 0, 0};
    IPAddress() {}
    IPAddress(uint32_t) {}
    bool fromString(const String& t) { return fromString(t.c_str()); }
    bool fromString(const char* t) {
        unsigned v[4]; char tail;
        if (sscanf(t, "%u.%u.%u.%u%c", &v[0], &v[1], &v[2], &v[3], &tail) != 4) return false;
        for (int i = 0; i < 4; i++) { if (v[i] > 255) return false; b[i] = (uint8_t)v[i]; }
        return true;
    }
    uint8_t operator[](int i) const { return b[i]; }
    String toString() const { char o[20]; snprintf(o, sizeof(o), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]); return String(o); }
};
struct SerialMock {
    void printf(const char* f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
    void println(const char* t) { puts(t); }
};
extern SerialMock Serial;
extern uint32_t g_mockMillis;
inline uint32_t millis() { return g_mockMillis; }
