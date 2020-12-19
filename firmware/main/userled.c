#include "freertos/FreeRTOS.h"
#include "userled.h"
#include "leddrv.h"
#include "freertos/task.h"
#include "i2c.h"
#include "esp_log.h"
#include "taskmgr.h"
#include <string.h>
#include "player.h"
#include "driver.h"

static const char* TAG = "UserLed";

volatile uint8_t UserLedMgr_States[3];
volatile uint8_t UserLedMgr_DiskState[DISKSTATE_COUNT];
volatile UserLedSource_t UserLedMgr_Source[3] = {0,0,0};

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void UserLedMgr_Reset() {
    memset((uint8_t *)&UserLedMgr_States[0], 0, sizeof(UserLedMgr_States));
}

bool UserLedMgr_Setup() {
    ESP_LOGI(TAG, "Setting up !!");
    UserLedMgr_Reset();
    return true;
}

void UserLedMgr_Main() {
    while (1) {
        xTaskNotifyWait(0,0xffffffff, NULL, pdMS_TO_TICKS(50));

        bool set = false;
        EventBits_t playerevents;
        for (uint8_t led=0;led<3;led++) {
            switch (UserLedMgr_Source[led]) {
                case USERLED_SRC_NONE:
                    LedDrv_States[7+4+led] = 0;
                    break;
                case USERLED_SRC_DISK_ALL:
                    for (uint8_t i=0;i<DISKSTATE_COUNT;i++) {
                        if (UserLedMgr_DiskState[i]) {
                            set = true;
                            break;
                        }
                    }
                    LedDrv_States[7+4+led] = set?255:0;
                    break;
                case USERLED_SRC_DISK_DSALL:
                    for (uint8_t i=1;i<DISKSTATE_COUNT;i++) {
                        if (UserLedMgr_DiskState[i]) {
                            set = true;
                            break;
                        }
                    }
                    LedDrv_States[7+4+led] = set?255:0;
                    break;
                case USERLED_SRC_DISK_DSFIND:
                    LedDrv_States[7+4+led] = UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FIND]?255:0;
                    break;
                case USERLED_SRC_DISK_DSFILL:
                    LedDrv_States[7+4+led] = UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FILL]?255:0;
                    break;
                case USERLED_SRC_DISK_VGM:
                    LedDrv_States[7+4+led] = UserLedMgr_DiskState[DISKSTATE_VGM]?255:0;
                    break;
                case USERLED_SRC_PLAYPAUSE:
                    playerevents = xEventGroupGetBits(Player_Status);
                    if (playerevents & PLAYER_STATUS_PAUSED) {
                        LedDrv_States[7+4+led] = 16;
                    } else if (playerevents & PLAYER_STATUS_RUNNING) {
                        LedDrv_States[7+4+led] = 255;
                    } else {
                        LedDrv_States[7+4+led] = 0;
                    }
                    break;
                case USERLED_SRC_DRIVERCPU:
                    if (Driver_CpuPeriod && Driver_CpuUsageDs+Driver_CpuUsageVgm) {
                        LedDrv_States[7+4+led] = map(Driver_CpuUsageDs+Driver_CpuUsageVgm, 0, Driver_CpuPeriod, 2, 255);
                    } else {
                        LedDrv_States[7+4+led] = 0;
                    }
                    Driver_CpuPeriod = 0;
                    Driver_CpuUsageDs = 0;
                    Driver_CpuUsageVgm = 0;
                    break;
                default:
                    break;
            }
            if (LedDrv_States[7+4+led]) LedDrv_States_ULatch[led] = LedDrv_States[7+4+led];
        }
    }
}

void UserLedMgr_Notify() {
    xTaskNotify(Taskmgr_Handles[TASK_USERLED], 1, eIncrement);
}
