#pragma once
// ==============================================================================
// HaEntityCatalog - Liste ALLER waehlbaren Entitaeten (auf Anforderung)
// ==============================================================================
// Grundlage fuer die Entitaetssuche (Widget hinzufuegen, Klick-Aktion,
// Bedingungen) und fuer gebackene Werte neuer Widgets (Optionen, Min/Max/Step).
//
// Geladen wird NUR auf Anforderung (RequestLoad), also wenn der Nutzer etwas
// hinzufuegen oder suchen will - nicht beim Start. Der Start bleibt damit so
// schnell wie seit der Juli-Optimierung.
//
// Abfrage: ein einmaliges render_template, das serverseitig eine kompakte
// Liste baut (ID, Name, Icon, Optionen/Grenzen) - nur fuer die Domains, die
// sich als Widget anlegen lassen. Typisch 10-50 KB statt des kompletten
// get_states. Nach der ersten Antwort wird das Template sofort abgemeldet.
//
// Speicher: ein JsonDocument im PSRAM, freigegeben beim Verlassen des
// HA-Screens (Clear).
//
// Threading: Getter und RequestLoad aus der GUI, Tick/HandleMessage aus dem
// HA_WS_Task. Die Liste ist durch einen Mutex geschuetzt.
// ==============================================================================

#include <Arduino.h>
#include <vector>

class HaEntityCatalog {
public:
    // Einmalig vor der ersten Nutzung (HaWebsocketLogic_Start).
    static void Init();

    // --- GUI-Seite ---

    // Laedt die Liste, falls noch keine da ist oder sie aelter als
    // MAX_AGE_MS ist. Nicht blockierend; mehrfacher Aufruf ist unschaedlich.
    static void RequestLoad();

    static bool IsReady();     // Liste vorhanden
    static bool IsLoading();   // Anfrage laeuft oder ist angefordert

    // Steigt mit jeder neu geladenen Liste. Die GUI erkennt daran, dass sich
    // eine offene Vorschau aktualisieren sollte.
    static uint32_t GetVersion();

    // Teilstring-Suche in ID und Name, ohne Gross-/Kleinschreibung.
    static std::vector<String> Search(const String& query, size_t maxResults);

    static bool   Contains(const String& entityId);
    static String GetName(const String& entityId);
    static String GetIcon(const String& entityId);
    static std::vector<String> GetOptions(const String& entityId);

    // false, wenn die Entitaet keine Grenzen kennt (oder nicht in der Liste ist).
    static bool GetLimits(const String& entityId, float& minVal, float& maxVal, float& stepVal);

    // --- HA_WS_Task ---
    static void OnAuthenticated();
    static void Tick();
    static bool HandleMessage(const String& type, uint32_t id, bool success, const String& payload);

    // Gibt den Speicher frei (Task-Ende).
    static void Clear();
};
