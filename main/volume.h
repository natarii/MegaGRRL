#ifndef AGR_VOLUME_H
#define AGR_VOLUME_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

extern uint8_t Volume_SystemVolume;

bool Volume_Setup();
bool Volume_SetVolume(uint8_t Left, uint8_t Right);

#endif