#pragma once
// ==============================================================================
// AudioCodecTab5 - direkter Zugriff auf die Audio-Kette des Tab5 (Diagnose)
// ==============================================================================
// Befund (14.11.2026): Nach einem Absturz ODER einem Software-Neustart waehrend
// der Wiedergabe bleibt der Lautsprecher stumm, obwohl M5.Speaker.begin()
// gelingt. Stromlos-Machen oder der Reset-Knopf des Tab5 hilft.
//
// Schaltplan Tab5 (docs.m5stack.com, Tab5_Schematics_PDF):
//   - ES8388 (Codec), ES7210 (Mikrofon-ADC) und NS4150B (Verstaerker) haengen
//     an AUDIO_VDD, einem 3,3-V-Regler OHNE Enable: sie bleiben bei jedem
//     Reset des ESP32-P4 unter Strom und behalten ihren Zustand.
//   - Die IO-Expander PI4IOE5V6408 (0x43: SPK_EN = P1, 0x44) haben nRESET an
//     SOC_RST - dem Reset-Netz des ESP32-P4 (RC 10k/1uF). Der Reset-Knopf
//     setzt sie zurueck, ein Absturz/Software-Neustart nicht.
//   - Der Reset-Knopf setzt ausserdem den ESP32-P4 vollstaendig zurueck
//     (inkl. Always-on-Bereich), ein Software-Reset nur den Digitalteil.
//
// Die erste Vermutung (Ablaufsteuerung des ES8388) ist widerlegt: der Codec-
// Reset half nicht - und der Codec wird auch vom Reset-Knopf nicht zurueck-
// gesetzt, der aber hilft. Deshalb einzeln pruefbare Theorien:
//   Codec   ES8388: Register + Ablaufsteuerung (widerlegt, zum Vergleich)
//   I2s     ESP32-P4: I2S-Peripherie, Takt-Register und I2S-Pins
//   MicAdc  ES7210: teilt MCLK/BCLK/LRCK - darf die Leitungen nicht treiben
//   Amp     NS4150B/Expander: SPK_EN lange aus (> 80 ms), Pin neu konfiguriert
//   UsbPads ESP32-P4: GPIO26/27 sind zugleich die Pads der internen USB-PHY 1
//           (USB_OTG FS, USB_INT_PHY1_DM/DP). Am Tab5 liegen dort DOUT und
//           BCLK des Lautsprechers. Bleibt die PHY nach einem Software-Reset
//           auf den Pads aktiv (USB_WRAP.otg_conf.usb_pad_enable), kommen
//           Daten und Bittakt nicht beim Codec an. Pads freigeben.
//   All     alles nacheinander (ohne die beiden folgenden)
//
// Ergebnis 14.11.: Codec, I2s, MicAdc, Amp und All einzeln ohne Erfolg; alle
// Register von Codec, ES7210 und Expandern sind nach dem Absturz plausibel.
// Gleiches Fehlerbild im M5Stack-Forum ("Tab5 - Speaker init after crash",
// felmue reproduziert es mit dem M5Unified-Speaker-Beispiel): nur Reset-Knopf
// oder Akku-Abziehen hilft, mit Akku manchmal zweimal; ein Nutzer berichtet,
// dass ein beim Einschalten eingesteckter Kopfhoerer den Lautsprecher ebenfalls
// zurueckholt. Ursache dort unbekannt. Weitere Theorien:
//   CodecClocked  ES8388 mit LAUFENDEM Takt in den Ruhezustand: DAC stumm,
//                 Ausgaenge aus, Ablaufsteuerung im Reset, Referenz/VMID aus
//                 (500 ms entladen), Register-Reset, VMID schnell laden. Der
//                 Ablauf im Chip braucht MCLK - der fruehere Codec-Reset lief
//                 bei gestopptem Takt und kam daher womoeglich nie an.
// (Neustart ueber Tiefschlaf: siehe CrashDiag_DeepSleepRestart.)
//
// Ergebnis Runde 4: Codec mit Takt und Tiefschlaf ohne Erfolg. Ein Software-
// Reset des IO-Expanders 0x43 fuehrte zu schwarzem Bildschirm (nur der Reset-
// Knopf half) - deshalb wieder entfernt. Neu beobachtet: Nach dem
// Absturz knackt der Lautsprecher beim Umschalten (Verstaerker/Lautsprecher
// aus/ein), Toene sind aber nicht zu hoeren. Verstaerker und Lautsprecher
// arbeiten also - es fehlt das Signal vom Codec. Daher:
//   PadHold    Pads GPIO26-30 koennen per "Hold" eingefroren sein (Register
//              im HP_SYSTEM/PMU, ueberstehen Software-Reset und Tiefschlaf).
//              Eingefrorene Pads aendern sich nicht, egal was der I2S-Treiber
//              einstellt. Hold je Pin loesen (gpio_hold_dis).
//   SoundTest  Kein Reset, sondern Hoertest in festen Abstaenden (je 1,5 s):
//              Lautsprecher aus, Lautsprecher an, Klick, 1-kHz-Ton, Klick.
//              Trennt das Umschalt-Knacken des Verstaerkers von echten Toenen
//              und zeigt, ob Toene kurz nach dem Einschalten noch durchkommen.
//              Ablauf im Besitzer-Task (AudioStreamLogic), nicht hier.
//   ToneSweep  Kein Reset, sondern Hoertest ueber Frequenz und Dauer (je 1 s
//              Abstand): 150, 400, 800, 1600, 3200, 6400 Hz je 300 ms, dann
//              800 Hz und 150 Hz je 15 ms. Befund 14.11.: Im stummen Zustand
//              ist der Tastenton (150 Hz, 15 ms) hoerbar, Lautstaerke-"Bing"
//              (800 Hz, 100 ms), Testton (1 kHz) und Babyphone nicht -
//              Hinweis auf frequenz-/dauerabhaengigen Fehler im Codec.
//              Ablauf im Besitzer-Task (AudioStreamLogic).
//   FirstTone  Kein Reset, sondern Hoertest: Lautsprecher aus/an, dann sechs
//              Pieptoene bei 0 / 150 / 400 / 800 / 1500 / 3000 ms.
//              Befund 14.11.: Im stummen Zustand ist nur der Tastenton der
//              Diagnose-Knoepfe hoerbar. Der wartet waehrend des Resets in der
//              Warteschlange und ist damit der ERSTE Ton nach dem Neustart der
//              I2S-Ausgabe. Vermutung: Nur der erste Ton nach dem Start der
//              Takte kommt durch, danach verstummt der Codec.
//   LineCheck  Kein Reset, sondern Messung: waehrend ein Ton laeuft, werden
//              die Pegel an MCLK/BCLK/LRCK/DOUT direkt am Pad abgetastet.
//              Wechseln sie, verlassen die Signale den ESP32-P4 -> Ursache
//              im Codec. Stehen sie, liegt es am ESP32-P4 (Pad/Takt).
//
// Nur aus dem Besitzer-Task der Audio-Hardware aufrufen (AudioStreamLogic),
// bei abgeschaltetem Lautsprecher (nach M5.Speaker.end()) und unter
// AudioHw_Lock(). Danach M5.Speaker.begin().
// ==============================================================================

#include <stdint.h>

enum class AudioResetKind : uint8_t { Codec = 0, I2s, MicAdc, Amp, UsbPads, All, CodecClocked,
                                      PadHold, LineCheck, SoundTest, ToneSweep, FirstTone };

// Name fuer Log und Oberflaeche.
const char* AudioCodec_KindName(AudioResetKind kind);

// true: dieser Reset braucht den LAUFENDEN Lautsprecher (Takt an); sonst muss
// der Lautsprecher vorher aus sein.
bool AudioCodec_NeedsClock(AudioResetKind kind);

// Fuehrt den gewaehlten Reset aus (Lautsprecher gemaess AudioCodec_NeedsClock).
void AudioCodec_Reset(AudioResetKind kind);

// Auszug auf die serielle Konsole: ES8388 (0x00-0x34), ES7210 (0x00-0x14,
// 0x40-0x4C), beide IO-Expander, I2S-Takt-Register des ESP32-P4, USB-PHY-
// Pad-Steuerung und IO-MUX/GPIO-Matrix der I2S-Pins.
// Zum Vergleich Kaltstart / nach Absturz.
void AudioCodec_Dump(const char* tag);

// Eine Zeile: Empfangsseite des I2S (Mikrofon) mit Takt-Registern. Ohne
// I2C-Zugriffe, deshalb auch bei jedem Gegensprechen brauchbar. Aus den Werten
// lassen sich Taktquelle, Teiler, Bit-Takt und Kanalwahl nachrechnen.
void AudioCodec_DumpI2sRx(const char* tag);
