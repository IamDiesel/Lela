#pragma once
#include "HAWidgets.h"

namespace HaDialogMedia {
    void resetState();
    void showMediaControlDialog(HAWidget* w);
    void update();   // zyklisch: Ergebnisse des Medien-Browsers anzeigen
}