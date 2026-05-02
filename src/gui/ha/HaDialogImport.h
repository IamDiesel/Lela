#pragma once

namespace HaDialogImport {
    void resetState(); // NEU
    void showLoadingPopup(const char* text);
    void hideLoadingPopup();
    void showDashboardSelectionPopup();
    void showViewSelectionPopup();
    void showImportErrorPopup();
    void checkPendingEvents();
    bool isAnyOverlayActive();
}