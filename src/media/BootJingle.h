#pragma once
// ==============================================================================
// BootJingle - Start-Jingle "Mrrp? + Spieluhr"
// ==============================================================================
// Ersetzt die alte Tonleiter (C-Dur-Arpeggio im Rechteckton). Die Idee passt
// zum Projekt - Katze, Baby, Smart Home:
//   1. "Mrrp?" (0 - 0,26 s): ein kurzer Kaetzchen-Gruss. Weicher Ton, der mit
//      einem kleinen Schnurr-Triller ansteigt und fragend wieder faellt.
//   2. Die Spieluhr antwortet (0,30 - 1,40 s): zwei helle Toene (E6, G6) und
//      ein ausklingender Schlussklang (C6 + G6) wie bei einem Baby-Mobile -
//      zugleich das typische "Bereit" eines Smart-Home-Geraets.
//
// Klang: Sinus mit wenigen Obertoenen und Huellkurven statt Rechteck - leise
// und rund statt piepsig. Der Jingle wird beim Start einmal als PCM berechnet
// (1,4 s, 24 kHz, 16 Bit = 67 KB im PSRAM, Rechenzeit wenige ms) und mit
// EINEM playRaw() abgespielt - unabhaengig davon, wie regelmaessig loop()
// waehrend des Hochfahrens drankommt. Danach wird der Puffer freigegeben.
//
// Lautstaerke: Kanal 1 (UI) wie die Tastentoene, also Mixer "UI" x "Master".
// Ein Tastenton waehrend des Jingles bricht ihn ab (wie bisher).
// ==============================================================================

// In setup() nach M5.Speaker.begin(): berechnet den Jingle und merkt ihn zum
// Abspielen vor.
void BootJingle_Start();

// Aus loop(): spielt den vorgemerkten Jingle, sobald der Lautsprecher frei ist,
// und gibt den Puffer nach dem Abspielen wieder frei.
void BootJingle_Update();
