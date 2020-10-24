#ifndef AGR_CLK_H
#define AGR_CLK_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

#define CLK_FM  0
#define CLK_PSG 1

extern volatile bool Clk_Unlock;

void Clk_Set(uint8_t ch, uint32_t freq);
void Clk_AdjustMult(uint8_t ch, int16_t mult);
uint32_t Clk_GetCh(uint8_t ch);
void Clk_TempSet(uint8_t ch, uint32_t freq);
void Clk_Restore(uint8_t ch);
bool Clk_CreateMutex();

#endif