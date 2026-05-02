#pragma once
#include <lvgl.h>

void VideoLogic_Init(); // <--- Die sichere Speicher-Reservierung beim Boot
void VideoLogic_Start();
void VideoLogic_Stop();
void VideoLogic_TriggerImageLoad();