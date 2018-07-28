#ifndef AGR_VOLUME_H
#define AGR_VOLUME_H

#include "freertos/FreeRTOS.h"

bool Volume_Setup();
bool Volume_SetVolume(uint8_t Left, uint8_t Right);

#endif