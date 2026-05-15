#pragma once
#include <Arduino.h>

class HaLovelaceParser {
public:
    static void parseDashboardList(const String& payload);
    static void parseViewsList(const String& payload);
    static void parseCards(const String& payload, int targetViewIndex, int currentImportTab);
};