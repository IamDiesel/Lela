#ifndef LVGL_DRIVER_H
#define LVGL_DRIVER_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

void Lvgl_Init();
void Set_Backlight(uint8_t val);
bool lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

#endif