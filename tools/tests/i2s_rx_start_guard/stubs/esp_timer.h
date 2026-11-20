#pragma once
// PC-Stub: die Uhr des Tests. Jeder Aufruf laesst die simulierte Zeit laufen
// und gibt der simulierten Hardware Gelegenheit zu quittieren (siehe Test).
#include <stdint.h>
int64_t esp_timer_get_time(void);
