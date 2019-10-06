#include "taskmgr.h"
#include "driver.h"
#include "i2c.h"
#include "ioexp.h"
#include "battery.h"
#include "key.h"
#include "graphics.h"
#include "dacstream.h"
#include "lcddma.h"
#include "loader.h"
#include "player.h"
#include "esp_log.h"
#include "channels.h"
#include "ui.h"
#include "userled.h"
#include "options.h"

static const char* TAG = "Taskmgr";

TaskHandle_t Taskmgr_Handles[TASK_COUNT];

void Taskmgr_Debug() {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void Taskmgr_CreateTasks() {
    for (uint8_t i=0;i<TASK_COUNT;i++) {
        if (i == TASK_LCDDMA) continue;
        Taskmgr_Handles[i] = NULL;
    }
    xTaskCreatePinnedToCore(BatteryMgr_Main, "BatteryMgr", 2048, NULL, 3, &Taskmgr_Handles[TASK_BATTERYMGR], 0);
    xTaskCreatePinnedToCore(Loader_Main, "Loader", 4096, NULL, 10, &Taskmgr_Handles[TASK_LOADER], 0);
    xTaskCreatePinnedToCore(IoExp_Main, "IoExp", 2048, NULL, 19, &Taskmgr_Handles[TASK_IOEXP], 0);
    xTaskCreatePinnedToCore(KeyMgr_Main, "KeyMgr", 2048, NULL, 19, &Taskmgr_Handles[TASK_KEYMGR], 0);
    xTaskCreatePinnedToCore(DacStream_FindTask, "DacStream_Find", 4096, NULL, 9, &Taskmgr_Handles[TASK_DACSTREAM_FIND], 0);
    xTaskCreatePinnedToCore(DacStream_FillTask, "DacStream_Fill", 4096, NULL, 11, &Taskmgr_Handles[TASK_DACSTREAM_FILL], 0);
    xTaskCreatePinnedToCore(Player_Main, "Player", 4096, NULL, 5, &Taskmgr_Handles[TASK_PLAYER], 0);
    xTaskCreatePinnedToCore(ChannelMgr_Main, "ChannelMgr", 1024, NULL, 18, &Taskmgr_Handles[TASK_CHANNELMGR], 0);
    xTaskCreatePinnedToCore(Ui_Main, "Ui", 3000, NULL, 12, &Taskmgr_Handles[TASK_UI], 0);
    xTaskCreatePinnedToCore(UserLedMgr_Main, "UserLed", 1024, NULL, 17, &Taskmgr_Handles[TASK_USERLED], 0);
    xTaskCreatePinnedToCore(OptionsMgr_Main, "OptionsMgr", 2048, NULL, 8, &Taskmgr_Handles[TASK_OPTIONS], 0);

    xTaskCreatePinnedToCore(Driver_Main, "Driver", 4096, NULL, configMAX_PRIORITIES-2, &Taskmgr_Handles[TASK_DRIVER], 1);
}

char buf[50*TASK_COUNT];
void Taskmgr_Monitor() {
    ESP_LOGI(TAG, "Task start");
    while (1) {
        ESP_LOGI(TAG, "Task stack high water mark follows");
        for (uint8_t i=0;i<TASK_COUNT;i++) {
            if (Taskmgr_Handles[i] != NULL) {
                ESP_LOGI(TAG, "#%d: %d bytes free", i, uxTaskGetStackHighWaterMark(Taskmgr_Handles[i]));
            } else {
                ESP_LOGI(TAG, "#%d: not started", i);
            }
        }
        ESP_LOGI(TAG, "Task runtime stats follow");
        vTaskGetRunTimeStats((char*)buf);
        printf(buf);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}