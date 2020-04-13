#ifndef AGR_TASKMGR_H
#define AGR_TASKMGR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_system.h"

enum Tasks {
    TASK_DRIVER,
    TASK_LOADER,
    TASK_IOEXP,
    TASK_BATTERYMGR,
    TASK_KEYMGR,
    TASK_DACSTREAM_FIND,
    TASK_DACSTREAM_FILL,
    TASK_PLAYER,
    TASK_LCDDMA,
    TASK_CHANNELMGR,
    TASK_UI,
    TASK_USERLED,
    TASK_OPTIONS,
    TASK_COUNT
};

#define LOADER_TASK_PRIO_NORM 10
#define LOADER_TASK_PRIO_HIGH 12
#define LOADER_TASK_PRIO_MEGA 13

extern TaskHandle_t Taskmgr_Handles[TASK_COUNT];

void Taskmgr_CreateTasks();
void Taskmgr_Monitor();

#endif