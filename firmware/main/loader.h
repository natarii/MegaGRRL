#ifndef AGR_LOADER_H
#define AGR_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "vgm.h"
#include "mallocs.h"

enum {
    LOADER_RUNNING = 0x01,
    LOADER_STOPPED = 0x02,
    LOADER_START_REQUEST = 0x04,
    LOADER_STOP_REQUEST = 0x08,
};

enum {
    LOADER_BUF_EMPTY = 0x01,
    LOADER_BUF_FULL = 0x02,
    LOADER_BUF_OK = 0x04,
    LOADER_BUF_LOW = 0x08,
};

extern EventGroupHandle_t Loader_Status;
extern EventGroupHandle_t Loader_BufStatus;

extern volatile bool Loader_IgnoreZeroSampleLoops;
volatile VgmDataBlockStruct_t Loader_VgmDataBlocks[MAX_REALTIME_DATABLOCKS+1];
extern VgmInfoStruct_t *Loader_VgmInfo; //TODO: why does everyone keep their own copy of this? player can just share its one
extern volatile bool Loader_FastOpnaUpload;

bool Loader_Setup();
void Loader_Main();
bool Loader_Stop();
bool Loader_Start(FILE *File, FILE *PcmFile, VgmInfoStruct_t *info);

#endif