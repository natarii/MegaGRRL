#ifndef AGR_DACSTREAM_H
#define AGR_DACSTREAM_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mallocs.h"
#include "esp_system.h"
#include "vgm.h"

typedef struct {
    bool SlotFree;
    uint32_t Seq;
    uint8_t DataBankId;
    uint8_t DataBlockId;
    uint8_t ChipCommand;
    uint8_t ChipPort;
    uint32_t SampleRate;
    uint32_t DataStart;
    uint8_t LengthMode;
    uint32_t DataLength;
    uint32_t ReadOffset;
    uint8_t QueueBuf[DACSTREAM_BUF_SIZE];
    uint32_t BytesFilled;
    QueueHandle_t Queue;
    StaticQueue_t StaticQueue;
} DacStreamEntry_t;

enum {
    DACSTREAM_RUNNING = 0x01,
    DACSTREAM_STOPPED = 0x02,
    DACSTREAM_START_REQUEST = 0x04,
    DACSTREAM_STOP_REQUEST = 0x08,
};

extern volatile DacStreamEntry_t DacStreamEntries[DACSTREAM_PRE_COUNT];
extern EventGroupHandle_t DacStream_FindStatus;
extern EventGroupHandle_t DacStream_FillStatus;

bool DacStream_Setup();
void DacStream_FindTask();
void DacStream_FillTask();
bool DacStream_Start(FILE *FindFile, FILE *FillFile, VgmInfoStruct_t *info);
bool DacStream_Stop();

#endif