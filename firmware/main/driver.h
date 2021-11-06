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

//#define OPLLDCSG_ORIGINAL_PROTO //natalie's original OPLL+DCSG prototype megamod has a different bus bit arrangement...

typedef enum {
    MEGAMOD_NONE = 0xff,
    MEGAMOD_FAULT = 0xfe,
    MEGAMOD_OPNA = 2,
    MEGAMOD_OPL3 = 3,
    MEGAMOD_OPLLDCSG = 4,
    MEGAMOD_OPM = 7,
} MegaMod_t;

//PlayEvents
#define DRIVER_EVENT_START_REQUEST  (1<<0)  //incoming request to begin playback
#define DRIVER_EVENT_STOP_REQUEST   (1<<1)  //incoming request to stop playback
#define DRIVER_EVENT_RUNNING        (1<<2)  //status flag
#define DRIVER_EVENT_FINISHED       (1<<3)  //status flag
#define DRIVER_EVENT_RESET_REQUEST  (1<<4)  //incoming request to reset chips - only driver has access to spi
#define DRIVER_EVENT_RESET_ACK      (1<<5)  //reset is finished
#define DRIVER_EVENT_UPDATE_MUTING  (1<<6)  //force muting update
#define DRIVER_EVENT_RESUME_REQUEST (1<<7)  //unpause
#define DRIVER_EVENT_FASTFORWARD    (1<<8)
#define DRIVER_EVENT_REWIND         (1<<9)

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
extern volatile bool Driver_FixDcsgFrequency;
extern volatile bool Driver_FixDcsgPeriodic;
extern volatile bool Driver_MitigateVgmTrim;
extern volatile bool Driver_FirstWait;
extern volatile uint8_t Driver_FmMask;
extern volatile uint8_t Driver_DcsgMask;
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