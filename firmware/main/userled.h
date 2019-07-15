#ifndef AGR_USERLEDS_H
#define AGR_USERLEDS_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

extern volatile uint8_t UserLedMgr_States[3];

bool UserLedMgr_Setup();
void UserLedMgr_Main();
void UserLedMgr_Notify();

#endif