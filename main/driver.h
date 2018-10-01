#ifndef AGR_DRIVER_H
#define AGR_DRIVER_H

#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

//PlayEvents
#define DRIVER_EVENT_START_REQUEST  0x01 //incoming request to begin playback
#define DRIVER_EVENT_STOP_REQUEST   0x02 //incoming request to stop playback
#define DRIVER_EVENT_RUNNING        0x04 //status flag
#define DRIVER_EVENT_FINISHED       0x08 //status flag
#define DRIVER_EVENT_RESET_REQUEST  0x20 //incoming request to reset chips - only driver has access to spi
#define DRIVER_EVENT_ERROR          0x40 //status flag

//QueueEvents
#define DRIVER_EVENT_COMMAND_UNDERRUN       0x01 //status flag
#define DRIVER_EVENT_PCM_UNDERRUN           0x02 //status flag - this should never ever happen, this should throw up a big error if it is ever set
#define DRIVER_EVENT_COMMAND_HALF           0x04 //status flag

extern QueueHandle_t Driver_CommandQueue;
extern QueueHandle_t Driver_PcmQueue;
extern EventGroupHandle_t Driver_CommandEvents;
extern EventGroupHandle_t Driver_QueueEvents;
extern uint8_t DacStreamId;

bool Driver_Setup();
void Driver_Main();

#endif