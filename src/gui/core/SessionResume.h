#pragma once
// ==============================================================================
// SessionResume - Babyphone nach einem ungewollten Neustart fortsetzen
// ==============================================================================
// Ein Babyphone, das nach einem Absturz oder Haenger neu startet und dann
// stumm auf dem Dashboard steht, hat seinen Zweck verfehlt. Dieses Modul merkt
// sich laufend, was der Benutzer eingestellt hatte (Babyphone-Modus, Baby-
// Ansicht offen), und stellt es nach einem UNGEWOLLTEN Neustart wieder her:
//
//   - Absturz, Watchdog oder vom HangGuard beendeter Haenger
//     (CrashDiag_WasUnplannedRestart), mit oder ohne Stromlos-Phase.
//   - Nicht nach Einschalten, Reset-Knopf, Upload oder "Neustart" in den
//     Einstellungen - dann beginnt das Geraet wie gewohnt im Dashboard.
//
// Wiederhergestellt werden Modus (Ton / Video / beides) und die Baby-Ansicht.
// Das Vollbild bewusst nicht: Dort zeichnet LVGL nicht, der Hinweis waere
// unsichtbar. Ein Hinweis-Kasten nennt den Grund und wo der Bericht steht.
//
// Schutz vor Absturzschleifen: Lief der vorherige Lauf kuerzer als
// kMinPrevUptimeMs, wird nicht fortgesetzt - sonst koennte eine Fortsetzung,
// die selbst zum Absturz fuehrt, das Geraet unbedienbar machen.
//
// Der Merker liegt bei CrashDiag (RTC-Speicher bzw. NVS ueber die Stromlos-
// Phase); dieses Modul gibt ihm nur die Bedeutung. Abhaengigkeit:
// Oberflaeche -> BabyService / CrashDiag / HangGuard.
// ==============================================================================

// Aus loop(), unter der LVGL-Sperre, in jedem Durchlauf: merkt den Zustand und
// setzt - einmalig, kurz nach dem Startbildschirm - den vorherigen fort.
void SessionResume_Update();
