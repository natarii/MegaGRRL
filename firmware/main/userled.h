#ifndef AGR_USERLEDS_H
#define AGR_USERLEDS_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

typedef enum {
    DISKSTATE_VGM,
    DISKSTATE_DACSTREAM_FIND,
    DISKSTATE_DACSTREAM_FILL,
    DISKSTATE_COUNT
} DiskState_t;

extern volatile uint8_t UserLedMgr_States[3];
extern volatile uint8_t UserLedMgr_DiskState[DISKSTATE_COUNT];

bool UserLedMgr_Setup();
void UserLedMgr_Main();
void UserLedMgr_Notify();

#endif