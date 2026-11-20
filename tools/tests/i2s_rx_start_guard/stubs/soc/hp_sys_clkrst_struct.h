#pragma once
// PC-Stub: die vier Taktregister, die der Schutz bei einem abgelehnten Start
// festhaelt.
#include <stdint.h>

typedef union { uint32_t val; } hp_sys_clkrst_reg_t;

typedef struct {
    volatile hp_sys_clkrst_reg_t peri_clk_ctrl11;
    volatile hp_sys_clkrst_reg_t peri_clk_ctrl12;
    volatile hp_sys_clkrst_reg_t peri_clk_ctrl13;
    volatile hp_sys_clkrst_reg_t peri_clk_ctrl14;
} hp_sys_clkrst_dev_t;

extern hp_sys_clkrst_dev_t HP_SYS_CLKRST;
