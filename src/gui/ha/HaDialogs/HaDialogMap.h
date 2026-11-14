#pragma once
// ==============================================================================
// HaDialogMap - Kartenwidget im Vollbild
// ==============================================================================
// Oeffnet sich beim Antippen eines Kartenwidgets. Zeigt dieselben Tracker,
// Zonen und Wege wie das Widget, aber bildschirmfuellend und von Hand
// steuerbar:
//   +/-          Zoom (bis MAP_MAX_VIEW_ZOOM = 20, Stufe 20 vergroessert)
//   Ziehen       Ausschnitt verschieben (wird beim Loslassen neu gezeichnet -
//                ein Vollbild-Neuzeichnen kostet ~0,7 s, Mitziehen wuerde ruckeln)
//   Alle         zurueck zum automatischen Einpassen
//   Springe zu   Auswahlliste aller Tracker/Zonen: dorthin zentrieren (mind.
//                mit der Zoomstufe des Widgets) und Angaben zeigen
//   Tippen       auf einen Marker: Name, Zustand, Genauigkeit, letzte
//                Aktualisierung (veraltet: "Zuletzt gesehen"); auf eine Zone:
//                Name, Radius; daneben: ausblenden
//   X            schliessen
//
// Wege wie im Widget, zusaetzlich mit den Messpunkten als kleine Punkte.
//
// Ausschnitt: Das Vollbild startet mit dem Ausschnitt des Widgets (fest oder
// automatisch). Beim Schliessen mit X uebernimmt das Widget den Ausschnitt:
// von Hand gezoomt/verschoben/angeflogen -> fest; nach "Alle" -> automatisch.
// Geaendert wird nur dann gespeichert (Flash schonen). Wird das Vollbild
// anders beendet (Bildschirmwechsel, z. B. Katzenalarm), bleibt das Widget
// wie es war.
//
// Wischgesten: Das Overlay (UIHelper::createModalOverlay) und die Kartenflaeche
// reichen keine Gesten an den Bildschirm weiter (GESTURE_BUBBLE geloescht).
// Ein Wisch auf der Karte verschiebt also nur die Karte - er wechselt weder
// die App noch oeffnet er die QuickSettings.
//
// Das Video-Overlay blendet sich wie bei jedem Dialog aus (isModalOpen).
// ==============================================================================

class HAMapWidget;

namespace HaDialogMap {
    // Oeffnet das Vollbild fuer dieses Widget. Ohne Wirkung, wenn schon offen.
    // Uebernimmt Entitaeten, Weglaenge, Zoom und Titel - das Widget darf danach
    // geloescht werden (Bildschirmwechsel loescht ohnehin beide).
    void show(HAMapWidget* w);

    // Aus ViewHomeAssistant::update(): Positionen, Wege und Kacheln nachziehen.
    void update();

    bool isOpen();
}
