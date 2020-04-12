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

typedef enum {
    USERLED_SRC_NONE,
    USERLED_SRC_PLAYPAUSE,
    USERLED_SRC_DISK_ALL,
    USERLED_SRC_DISK_VGM,
    USERLED_SRC_DISK_DSFIND,
    USERLED_SRC_DISK_DSFILL,
    USERLED_SRC_DISK_DSALL,
    USERLED_SRC_DRIVERCPU,
    USERLED_SRC_COUNT,
} UserLedSource_t;

extern volatile UserLedSource_t UserLedMgr_Source[3];
extern volatile uint8_t UserLedMgr_States[3];
extern volatile uint8_t UserLedMgr_DiskState[DISKSTATE_COUNT];

bool UserLedMgr_Setup();
void UserLedMgr_Main();
void UserLedMgr_Notify();

#endif