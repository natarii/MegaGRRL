#ifndef AGR_PITCH_H
#define AGR_PITCH_H
#include "freertos/FreeRTOS.h"
#include "esp_system.h"

int16_t Pitch_Adjust(int16_t newmult);
int16_t Pitch_Get();

#endif