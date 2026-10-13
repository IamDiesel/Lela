#pragma once

namespace HaDialogImport {
    void resetState();   // Zeiger zuruecksetzen (beim Neuaufbau des HA-Screens)
    void showLoadingPopup(const char* text);
    void hideLoadingPopup();
    void showDashboardSelectionPopup();
    void showViewSelectionPopup();
    void showImportErrorPopup();
    void checkPendingEvents();
    bool isAnyOverlayActive();
}