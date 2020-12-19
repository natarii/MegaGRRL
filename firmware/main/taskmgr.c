#include "taskmgr.h"
#include "driver.h"
#include "i2c.h"
#include "ioexp.h"
#include "battery.h"
#include "key.h"
#include "dacstream.h"
#include "lcddma.h"
#include "loader.h"
#include "player.h"
#include "esp_log.h"
#include "channels.h"
#include "ui.h"
#include "userled.h"
#include "options.h"

//static const char* TAG = "Taskmgr";

TaskHandle_t Taskmgr_Handles[TASK_COUNT];

void Taskmgr_CreateTasks() {
    for (uint8_t i=0;i<TASK_COUNT;i++) {
        if (i == TASK_LCDDMA) continue;
        Taskmgr_Handles[i] = NULL;
    }
    xTaskCreatePinnedToCore(BatteryMgr_Main, "BatMgr", 2048, NULL, 3, &Taskmgr_Handles[TASK_BATTERYMGR], 0);
    xTaskCreatePinnedToCore(Loader_Main, "Loader", 2560, NULL, LOADER_TASK_PRIO_NORM, &Taskmgr_Handles[TASK_LOADER], 0);
    xTaskCreatePinnedToCore(IoExp_Main, "IoExp ", 2048, NULL, 19, &Taskmgr_Handles[TASK_IOEXP], 0);
    xTaskCreatePinnedToCore(KeyMgr_Main, "KeyMgr", 2048, NULL, 19, &Taskmgr_Handles[TASK_KEYMGR], 0);
    xTaskCreatePinnedToCore(DacStream_FindTask, "DsFind", 2560, NULL, 9, &Taskmgr_Handles[TASK_DACSTREAM_FIND], 0);
    xTaskCreatePinnedToCore(DacStream_FillTask, "DsFill", 2560, NULL, 14, &Taskmgr_Handles[TASK_DACSTREAM_FILL], 0);
    xTaskCreatePinnedToCore(Player_Main, "Player", 3072, NULL, 5, &Taskmgr_Handles[TASK_PLAYER], 0);
    xTaskCreatePinnedToCore(ChannelMgr_Main, "ChnMgr", 2048, NULL, 18, &Taskmgr_Handles[TASK_CHANNELMGR], 0);
    xTaskCreatePinnedToCore(Ui_Main, "Ui    ", 3000, NULL, 12, &Taskmgr_Handles[TASK_UI], 0);
    xTaskCreatePinnedToCore(UserLedMgr_Main, "UsrLed", 1536, NULL, 17, &Taskmgr_Handles[TASK_USERLED], 0);
    xTaskCreatePinnedToCore(OptionsMgr_Main, "OpnMgr", 2048, NULL, 15, &Taskmgr_Handles[TASK_OPTIONS], 0);

    xTaskCreatePinnedToCore(Driver_Main, "Driver", 3072, NULL, configMAX_PRIORITIES-2, &Taskmgr_Handles[TASK_DRIVER], 1);
}
