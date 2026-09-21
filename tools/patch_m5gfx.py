# ==============================================================================
# patch_m5gfx.py - haelt den Panel_DSI-Eingriff ueber Neuinstallationen hinweg
# ==============================================================================
# Hintergrund (siehe src/doc/video_refactoring.md, 2.12 und 2.18):
#
# M5GFX meldet das MIPI-DSI-Panel als DMA2D-Client an (use_dma2d = true). Damit
# teilen sich drei Nutzer den 2D-DMA-Controller des ESP32-P4: der Hardware-
# JPEG-Decoder, die PPA und das Panel. Auf Silizium-Revision v1.3 quittiert der
# Kanal-Reset fuer Kanaele mit peri_sel == 7 nie (esp-idf #19108, #18999);
# dma2d_hal_tx_reset_channel() dreht dann in einer Endlosschleife mit
# gesperrten Interrupts - sichtbar als "Interrupt wdt timeout on CPU1".
#
# Der Eingriff kostet nichts Messbares: die Anzeigezeit sank von 7,4 ms auf
# 0,4 ms je Bild.
#
# Warum dieses Skript existiert:
# Der Eingriff liegt in .pio/libdeps und damit in einem Verzeichnis, das
# PlatformIO jederzeit neu befuellen darf - nach einem "pio pkg update", einem
# geloeschten .pio-Ordner oder auf einem frischen Rechner. Ohne dieses Skript
# waere der Fehler danach stillschweigend zurueck, und niemand wuesste warum.
#
# Das Skript laeuft vor jedem Build, ist idempotent und legt beim ersten Mal
# eine unveraenderte Sicherung als *.orig daneben.
# ==============================================================================

import os
import shutil

Import("env")

REL_PATH = os.path.join(
    "M5GFX", "src", "lgfx", "v1", "platforms", "esp32p4", "Panel_DSI.cpp"
)

NEEDLE_ON = "dpi_config.flags.use_dma2d = true;"
NEEDLE_OFF = "dpi_config.flags.use_dma2d = false;"

REPLACEMENT = (
    "dpi_config.flags.use_dma2d = false;  // HEBEL 3 (siehe doc/video_refactoring.md 2.12/2.18):\n"
    "                                     // true hinterlaesst DMA2D-Kanaele mit peri_sel==7, deren\n"
    "                                     // Reset laut esp-idf #19108 nie quittiert wird.\n"
    "                                     // Gesetzt von tools/patch_m5gfx.py. Original: *.orig"
)


def patch_panel_dsi():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    envname = env.subst("$PIOENV")
    target = os.path.join(libdeps, envname, REL_PATH)

    if not os.path.isfile(target):
        # Erster Lauf vor dem Bibliotheksdownload: beim naechsten Build greift es.
        print("[patch_m5gfx] Panel_DSI.cpp noch nicht vorhanden - uebersprungen.")
        return

    with open(target, "r", encoding="utf-8", errors="replace") as f:
        source = f.read()

    if NEEDLE_OFF in source:
        print("[patch_m5gfx] use_dma2d ist bereits deaktiviert.")
        return

    if NEEDLE_ON not in source:
        # Lieber laut scheitern als leise das falsche Binary bauen.
        print("[patch_m5gfx] WARNUNG: Ankerzeile nicht gefunden - "
              "M5GFX hat sich geaendert, Eingriff bitte pruefen!")
        return

    backup = target + ".orig"
    if not os.path.exists(backup):
        shutil.copyfile(target, backup)
        print("[patch_m5gfx] Sicherung angelegt: %s" % backup)

    with open(target, "w", encoding="utf-8") as f:
        f.write(source.replace(NEEDLE_ON, REPLACEMENT, 1))

    print("[patch_m5gfx] use_dma2d deaktiviert (DMA2D-Reset-Haenger, esp-idf #19108).")


patch_panel_dsi()
