#ifndef AGR_I2C_H
#define AGR_I2C_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "esp_system.h"

#include "pins.h"

bool I2cMgr_Setup();
bool I2cMgr_Seize(bool IsIsr, TickType_t Ticks);
bool I2cMgr_Release(bool IsIsr);
uint8_t I2cMgr_Clear();

#endif