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
    uint32_t last_states = 0;
    uint32_t time_cleared_writes = 0;
    while (1) {
        xTaskNotifyWait(0,0xffffffff, NULL, pdMS_TO_TICKS(25));

        EventBits_t playerevents;
        uint32_t new_states = 0;
        for (uint8_t led=0;led<3;led++) {
            bool set = false;
            switch (UserLedMgr_Source[led]) {
                case USERLED_SRC_NONE:
                    //nothing to do
                    break;
                case USERLED_SRC_DISK_ALL:
                    for (uint8_t i=0;i<DISKSTATE_COUNT;i++) {
                        if (UserLedMgr_DiskState[i]) {
                            set = true;
                            new_states |= (0xff << (led*8));
                            break;
                        }
                    }
                    break;
                case USERLED_SRC_DISK_DSALL:
                    for (uint8_t i=1;i<DISKSTATE_COUNT;i++) {
                        if (UserLedMgr_DiskState[i]) {
                            new_states |= (0xff << (led*8));
                            break;
                        }
                    }
                    break;
                case USERLED_SRC_DISK_DSFIND:
                    if (UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FIND]) new_states |= (0xff << (led*8));
                    break;
                case USERLED_SRC_DISK_DSFILL:
                    if (UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FILL]) new_states |= (0xff << (led*8));
                    break;
                case USERLED_SRC_DISK_VGM:
                    if (UserLedMgr_DiskState[DISKSTATE_VGM]) new_states |= (0xff << (led*8));
                    break;
                case USERLED_SRC_PLAYPAUSE:
                    playerevents = xEventGroupGetBits(Player_Status);
                    if (playerevents & PLAYER_STATUS_PAUSED) {
                        new_states |= (16 << (led*8));
                    } else if (playerevents & PLAYER_STATUS_RUNNING) {
                        new_states |= (255 << (led*8));
                    } else {
                        //nothing to do
                    }
                    break;
                case USERLED_SRC_DRIVERCPU:
                    if (Driver_CpuPeriod && Driver_CpuUsageDs+Driver_CpuUsageVgm) {
                        uint16_t usage = map(Driver_CpuUsageDs+Driver_CpuUsageVgm, 0, Driver_CpuPeriod, 2, 255);
                        if (usage > 255) usage = 255;
                        new_states |= (usage << (led*8));
                    } else {
                        //nothing to do
                    }
                    Driver_CpuPeriod = 0;
                    Driver_CpuUsageDs = 0;
                    Driver_CpuUsageVgm = 0;
                    break;
                case USERLED_SRC_FM_WRITE:
                    if (Driver_WroteFm) new_states |= (0xff << (led*8));
                    break;
                case USERLED_SRC_DCSG_WRITE:
                    if (Driver_WroteDcsg) new_states |= (0xff << (led*8));
                    break;
                default:
                    break;
            }
        }
        if (new_states != last_states) {
            if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
                ESP_LOGE(TAG, "Couldn't seize bus !!");
                return;
            }
            for (uint8_t i=0;i<3;i++) {
                LedDrv_States[11+i] = (new_states >> (i*8)) & 0xff;
            }
            LedDrv_Update();
            I2cMgr_Release(false);
            last_states = new_states;
        }
        if (esp_timer_get_time() - time_cleared_writes > 25000) {
            Driver_WroteFm = false;
            Driver_WroteDcsg = false;
            time_cleared_writes = esp_timer_get_time();
        }
    }
}

void UserLedMgr_Notify() {
    xTaskNotify(Taskmgr_Handles[TASK_USERLED], 1, eIncrement);
}
