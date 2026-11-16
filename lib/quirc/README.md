# quirc – QR-Code-Erkennung

Unveraendert uebernommen aus <https://github.com/dlbeer/quirc>, Verzeichnis `lib/`,
Commit `f2e4271ae43a15d9263704e07bc86cba5697d74d` (28.10.2026, "fix/resize-int-overflow").
Lizenz: ISC (siehe `LICENSE`).

Warum als Quellcode im Projekt statt `lib_deps`: Alle Bibliotheken sind exakt
gepinnt (siehe `platformio.ini`); quirc ist nicht in der PlatformIO-Registry.
Bei einem Update die sechs Dateien aus `lib/` des neuen Commits ersetzen und
Commit/Datum hier und in `library.json` nachziehen.

Einstellungen (`library.json`):
* `QUIRC_FLOAT_TYPE=float` und `QUIRC_USE_TGMATH`: einfache statt doppelter
  Genauigkeit – der ESP32-P4 hat nur eine Gleitkomma-Einheit fuer `float`.
* `QUIRC_MAX_REGIONS` bleibt 254 (Bildpunkt-Puffer 1 Byte je Punkt).

Speicher: `quirc_resize` legt Bild und Punkt-Puffer mit `malloc` an (je
Breite x Hoehe Bytes); Anforderungen ueber 4 KB landen im PSRAM
(`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096`).

Verwendet von `src/media/QrScanner.cpp`.
