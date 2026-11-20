# ==============================================================================
# patch_usbhostserial.py - Dongle-Suche nur, wenn ein USB-Geraet angemeldet ist
# ==============================================================================
# Hintergrund (Messung 16./17.11.2026, Projekt-Doku "Scroll-Messung"):
#
# Die Bibliothek USBHostSerial sucht in einem eigenen Task nach dem BLE-Dongle
# der Katzenmatte. Steckt keiner, laeuft diese Suche in Dauerschleife:
# VCP::open() probiert acht Chip-Varianten (FT23x, CP210x, CH34x) durch und
# wirft fuer jede eine C++-Ausnahme, danach 0,5 s Pause. Eine Runde dauert rund
# 1 s - und solange sie laeuft, braucht die Oberflaeche fuer jedes Bild etwa
# das 1,5-Fache (Einstellungen: 142 statt 98 ms). Sichtbar als Scrollen in zwei
# Geschwindigkeiten, zwei Drittel der Zeit in der langsamen.
#
# Der Eingriff: Vor der Suche fragt der Task den USB-Host, ob ueberhaupt ein
# Geraet angemeldet ist (usb_host_device_addr_list_fill - dieselbe Liste, die
# die Suche selbst durchgeht). Ohne Geraet schlaeft er nur seine 0,5 s. Steckt
# ein Geraet, laeuft alles wie im Original; der Dongle wird wie bisher
# spaetestens 0,5 s nach dem Anmelden am Bus gefunden.
#
# Nebenwirkung, gewollt: Die Meldung "USB VCP open returned null, falling back
# to CDC" kommt ohne Geraet nicht mehr. main.cpp (usbHostLogger) hat sie ohnehin
# unterdrueckt; der Merker "Dongle steckt" wird weiter ueber die Meldungen zum
# Oeffnen und Trennen gefuehrt.
#
# Warum als Skript: wie bei tools/patch_m5gfx.py - .pio/libdeps darf PlatformIO
# jederzeit neu befuellen. Das Skript laeuft vor jedem Build, ist idempotent
# und legt beim ersten Mal eine unveraenderte Sicherung als *.orig daneben.
# Die Bibliothek ist in platformio.ini auf einen Commit festgelegt; aendert sie
# sich, meldet das Skript die fehlende Ankerzeile, statt still nichts zu tun.
# ==============================================================================

import os
import shutil

Import("env")

REL_PATH = os.path.join("USBHostSerial", "src", "USBHostSerial.cpp")

# Erkennungsmerkmal des Eingriffs (steht im eingefuegten Kommentar).
MARKER = "LELA-PATCH: Suche nur mit angemeldetem USB-Geraet"

# Ankerzeile: der Beginn der Suche im Geraete-Task. Kommt genau einmal vor.
NEEDLE = "    auto vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev_config));"

INSERT = (
    "    // " + MARKER + "\n"
    "    // (tools/patch_usbhostserial.py; Original: *.orig). VCP::open() probiert\n"
    "    // sonst alle 0,5 s acht Chip-Varianten durch und wirft fuer jede eine\n"
    "    // C++-Ausnahme - rund 1 s Arbeit je Runde, die die Oberflaeche bremst.\n"
    "    // Steckt ein Geraet, laeuft alles wie im Original.\n"
    "    {\n"
    "      uint8_t lela_addr[1];\n"
    "      int lela_count = 0;\n"
    "      if (usb_host_device_addr_list_fill(sizeof(lela_addr), lela_addr, &lela_count) == ESP_OK\n"
    "          && lela_count == 0) {\n"
    "        vTaskDelay(pdMS_TO_TICKS(500));\n"
    "        continue;\n"
    "      }\n"
    "    }\n"
)


def patch_usb_host_serial():
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    envname = env.subst("$PIOENV")
    target = os.path.join(libdeps, envname, REL_PATH)

    if not os.path.isfile(target):
        # Erster Lauf vor dem Bibliotheksdownload: beim naechsten Build greift es.
        print("[patch_usbhostserial] USBHostSerial.cpp noch nicht vorhanden - uebersprungen.")
        return

    with open(target, "r", encoding="utf-8", errors="replace") as f:
        source = f.read()

    if MARKER in source:
        print("[patch_usbhostserial] Dongle-Suche ist bereits angepasst.")
        return

    if source.count(NEEDLE) != 1:
        # Lieber laut scheitern als leise das falsche Binary bauen.
        print("[patch_usbhostserial] WARNUNG: Ankerzeile nicht (eindeutig) gefunden - "
              "USBHostSerial hat sich geaendert, Eingriff bitte pruefen!")
        return

    backup = target + ".orig"
    if not os.path.exists(backup):
        shutil.copyfile(target, backup)
        print("[patch_usbhostserial] Sicherung angelegt: %s" % backup)

    with open(target, "w", encoding="utf-8") as f:
        f.write(source.replace(NEEDLE, INSERT + NEEDLE, 1))

    print("[patch_usbhostserial] Dongle-Suche laeuft nur noch mit angemeldetem USB-Geraet.")


patch_usb_host_serial()
