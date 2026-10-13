#pragma once
// ==============================================================================
// HaEntitySync - Live-Abo der platzierten Entitaeten (subscribe_entities)
// ==============================================================================
// Haelt HaEntityCache mit Home Assistant synchron.
//
// Protokoll (HA-WebSocket "subscribe_entities" mit entity_ids-Filter):
//   - Pro Abo meldet HA zuerst den vollstaendigen Zustand aller angefragten
//     Entitaeten inkl. Attribute ("a"), danach nur noch Aenderungen ("c")
//     und Entfernungen ("r"). Gefiltert wird serverseitig - es kommt nichts
//     aus dem restlichen Haus an.
//   - Aenderungen sind Diffs: "+" enthaelt nur geaenderte Attribute, "-" die
//     entfallenen. Das Einarbeiten uebernimmt HaEntityCache, der je Entitaet
//     ein vollstaendiges Abbild im PSRAM haelt.
//   - Es werden nur die Attribute behalten, die die Oberflaeche auswertet
//     (Whitelist beim Parsen). Grosse Listen wie effect_list oder
//     entity_picture kosten so weder Speicher noch Parse-Zeit.
//
// Ablauf:
//   - Die Entitaeten werden in Abos zu je ENTITIES_PER_SUBSCRIPTION aufgeteilt
//     und nacheinander angelegt. Der erste Block steht damit schnell auf dem
//     Bildschirm, und keine Einzelnachricht wird uebermaessig gross.
//   - Vor jedem Neuaufbau werden die bisherigen Abos abgemeldet
//     (unsubscribe_events). Frueher sammelten sie sich bei jedem Speichern an.
//   - Mehrere Anforderungen kurz hintereinander (Speichern + Neuaufbau)
//     werden zu einem Neuaufbau zusammengefasst.
//
// Threading: Alles ausser RequestResync() laeuft ausschliesslich im HA_WS_Task.
// ==============================================================================

#include <Arduino.h>

class HaEntitySync {
public:
    // Fordert einen Neuaufbau der Abos an (z. B. nach Aenderung der
    // Entitaetenliste). Aus jedem Task aufrufbar, nicht blockierend.
    static void RequestResync();

    // --- Nur HA_WS_Task ---

    // Neue Verbindung authentifiziert: alte Abos existieren serverseitig nicht
    // mehr. Ihre IDs werden verworfen und sofort neu abonniert.
    static void OnAuthenticated();

    // Zustandsmaschine: Abos abmelden/anlegen, Zeitueberschreitungen behandeln.
    static void Tick();

    // Verarbeitet eine eingehende Nachricht, sofern sie zu einem eigenen Abo
    // oder einer eigenen Abmeldung gehoert. Rueckgabe true = verarbeitet.
    static bool HandleMessage(const String& type, uint32_t id, bool success, const String& payload);

    // Verwirft alle Abos (Task-Ende, Neustart der Verbindung).
    static void Reset();
};
