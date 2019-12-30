#ifndef AGR_LEDDRV_H
#define AGR_LEDDRV_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"

extern volatile uint8_t LedDrv_States[16];
extern volatile uint8_t LedDrv_Brightness;

bool LedDrv_Setup();
bool LedDrv_Update();
void LedDrv_UpdateBrightness();

#endif