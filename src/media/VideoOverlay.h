#pragma once
#include <M5Unified.hpp>

class VideoOverlay {
public:
    static inline __attribute__((always_inline)) void applyOverlay(uint16_t* out_buf, int pic_w, int pic_h, int fps, int batteryPercent, bool showFps) {
        if (!showFps || pic_w < 150 || pic_h < 34) return;

        static LGFX_Sprite overlay(&M5.Display);
        static bool initialized = false;
        static uint32_t last_update_ms = 0;

        if (!initialized) {
            overlay.setColorDepth(16);
            overlay.createSprite(150, 24);
            initialized = true;
        }

        if (millis() - last_update_ms >= 1000 || fps == 0) {
            last_update_ms = millis();
            overlay.fillSprite(TFT_BLACK);
            
            int batX = 85, batY = 4;
            overlay.drawRect(batX, batY, 40, 16, TFT_WHITE);
            overlay.drawRect(batX + 40, batY + 4, 3, 8, TFT_WHITE);
            
            uint16_t batColor = (batteryPercent < 20) ? TFT_RED : ((batteryPercent < 50) ? TFT_YELLOW : TFT_GREEN);
            int fillW = (batteryPercent * 36) / 100;
            if (fillW > 0) overlay.fillRect(batX + 2, batY + 2, fillW, 12, batColor);

            overlay.setTextColor(TFT_WHITE);
            overlay.setFont(&fonts::Font4); 
            overlay.setCursor(5, 4);
            overlay.printf("%d FPS", fps);
        }

        // Optimierter 32-Bit RAM-Transfer
        uint32_t* src32 = (uint32_t*)overlay.getBuffer();
        
        for (int y = 0; y < 24; y++) {
            // Ein Pixel = 16-Bit. 150 Pixel = 300 Bytes = 75x 32-Bit-Worte
            uint32_t* dst32 = (uint32_t*)&out_buf[(y + 10) * pic_w + 10];
            uint32_t* row_src = &src32[y * 75]; 
            
            for(int x = 0; x < 75; x++) {
                dst32[x] = row_src[x];
            }
        }
    }
};