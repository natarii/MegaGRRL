#ifndef AGR_BATTERY_H
#define AGR_BATTERY_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

extern uint16_t BatteryMgr_Voltage;

bool BatteryMgr_Setup();
void BatteryMgr_Main();

#endif