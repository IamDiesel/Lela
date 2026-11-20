#pragma once
// PC-Stub des I2S-Treibers: Kanal-Handle und Auskunft ueber die Richtung.
#include "esp_err.h"

typedef enum { I2S_DIR_RX = 1, I2S_DIR_TX = 2 } i2s_dir_t;

typedef struct {
    int       id;      // I2S-Port
    i2s_dir_t dir;     // Richtung
} i2s_chan_info_t;

// Im Test ist der Handle ein Zeiger auf seine eigene Auskunft; NULL = ungueltig.
typedef i2s_chan_info_t *i2s_chan_handle_t;

static inline esp_err_t i2s_channel_get_info(i2s_chan_handle_t handle, i2s_chan_info_t *info)
{
    if (!handle || !info) return ESP_ERR_INVALID_ARG;
    *info = *handle;
    return ESP_OK;
}
