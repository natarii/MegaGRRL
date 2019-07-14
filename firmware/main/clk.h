#ifndef AGR_CLK_H
#define AGR_CLK_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

#define CLK_FM  0
#define CLK_PSG 1

void Clk_Set(uint8_t ch, uint32_t freq);

#endif