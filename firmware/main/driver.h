#ifndef AGR_DRIVER_H
#define AGR_DRIVER_H

#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "mallocs.h"
#include "megastream.h"

typedef enum {
    MEGAMOD_NONE = 0xff,
    MEGAMOD_FAULT = 0xfe,
    MEGAMOD_OPNA = 2,
    MEGAMOD_OPL3 = 3,
    MEGAMOD_OPLLPSG = 4,
    MEGAMOD_OPM = 7,
} MegaMod_t;

//PlayEvents
#define DRIVER_EVENT_START_REQUEST  0x01 //incoming request to begin playback
#define DRIVER_EVENT_STOP_REQUEST   0x02 //incoming request to stop playback
#define DRIVER_EVENT_RUNNING        0x04 //status flag
#define DRIVER_EVENT_FINISHED       0x08 //status flag
#define DRIVER_EVENT_RESET_REQUEST  0x20 //incoming request to reset chips - only driver has access to spi
#define DRIVER_EVENT_RESET_ACK      0x10 //reset is finished
#define DRIVER_EVENT_UPDATE_MUTING  0x40 //force muting update
#define DRIVER_EVENT_RESUME_REQUEST 0x80 //unpause

//QueueEvents
#define DRIVER_EVENT_COMMAND_UNDERRUN       0x01 //status flag
#define DRIVER_EVENT_PCM_UNDERRUN           0x02 //status flag - this should never ever happen, this should throw up a big error if it is ever set
#define DRIVER_EVENT_COMMAND_HALF           0x04 //status flag

extern uint8_t *Driver_CommandStreamBuf;
extern uint8_t Driver_PcmBuf[DACSTREAM_BUF_SIZE*DACSTREAM_PRE_COUNT];

extern volatile MegaMod_t Driver_DetectedMod;

extern MegaStreamContext_t Driver_CommandStream;
extern MegaStreamContext_t Driver_PcmStream;
extern EventGroupHandle_t Driver_CommandEvents;
extern EventGroupHandle_t Driver_StreamEvents;
extern uint8_t DacStreamId;
extern volatile IRAM_ATTR uint32_t Driver_CpuPeriod;
extern volatile IRAM_ATTR uint32_t Driver_CpuUsageVgm;
extern volatile IRAM_ATTR uint32_t Driver_CpuUsageDs;
extern volatile bool Driver_FixPsgFrequency;
extern volatile bool Driver_FixPsgPeriodic;
extern volatile bool Driver_MitigateVgmTrim;
extern volatile bool Driver_FirstWait;
extern volatile uint8_t Driver_FmMask;
extern volatile uint8_t Driver_PsgMask;
extern volatile bool Driver_ForceMono;
extern volatile IRAM_ATTR uint32_t Driver_Opna_PcmUploadId;
extern volatile bool Driver_Opna_PcmUpload;
extern FILE *Driver_Opna_PcmUploadFile;
extern volatile int16_t Driver_SpeedMult;
extern volatile bool Driver_FadeEnabled;
extern volatile uint8_t Driver_FadeLength;

extern IRAM_ATTR uint32_t Driver_Sample;
extern IRAM_ATTR uint32_t Driver_NextSample;

bool Driver_Setup();
void Driver_Main();
void Driver_ModDetect();
void Driver_ResetChips();

#endif