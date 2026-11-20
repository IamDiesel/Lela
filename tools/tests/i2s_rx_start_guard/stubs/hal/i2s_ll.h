#pragma once
// PC-Stub: Registersatz des I2S, soweit der Schutz ihn benutzt. Bitlage von
// rx_update wie im ESP32-P4 (soc/i2s_struct.h: Bit 8).
#include <stdint.h>

typedef union {
    struct {
        uint32_t rx_reset      : 1;
        uint32_t rx_fifo_reset : 1;
        uint32_t rx_start      : 1;
        uint32_t reserved3     : 5;
        uint32_t rx_update     : 1;
        uint32_t reserved9     : 23;
    };
    uint32_t val;
} i2s_rx_conf_reg_t;

typedef struct {
    volatile i2s_rx_conf_reg_t rx_conf;
} i2s_dev_t;

extern i2s_dev_t I2S0;
extern i2s_dev_t I2S1;
extern i2s_dev_t I2S2;

#define I2S_LL_GET_HW(num)  (((num) == 0) ? (&I2S0) : ((num) == 1) ? (&I2S1) : (&I2S2))
