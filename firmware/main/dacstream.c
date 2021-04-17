#include "dacstream.h"
#include "mallocs.h"
#include "esp_log.h"
#include "vgm.h"
#include "freertos/semphr.h"
#include "ioexp.h"
#include "driver.h"
#include "string.h"
#include "userled.h"
#include "loader.h"
#include "player.h"
#include "queue.h"
#include "sdcard.h"
#include "ui.h"
#include "ui/modal.h"
#include "taskmgr.h"

static const char* TAG = "DacStream";

volatile DacStreamEntry_t DacStreamEntries[DACSTREAM_PRE_COUNT];

static uint8_t DacStream_CurLoop = 0;

static StaticSemaphore_t DacStream_MutexBuf;
static SemaphoreHandle_t DacStream_Mutex = NULL;
static volatile uint8_t DacStream_VgmDataBlockIndex = 0;
FILE *DacStream_FindFile;
FILE *DacStream_FillFile;
static VgmInfoStruct_t *DacStream_VgmInfo;
volatile static VgmDataBlockStruct_t DacStream_VgmDataBlocks[MAX_REALTIME_DATABLOCKS+1];
EventGroupHandle_t DacStream_FindStatus;
static StaticEventGroup_t DacStream_FindStatusBuf;
EventGroupHandle_t DacStream_FillStatus;
static StaticEventGroup_t DacStream_FillStatusBuf;
static uint8_t DsFind_VgmBuf[FREAD_LOCAL_BUF];
static uint16_t DsFind_VgmBufPos = FREAD_LOCAL_BUF;
static IRAM_ATTR uint32_t DsFind_VgmFilePos = 0;

#define DSFIND_BUF_FILL \
    fseek(DacStream_FindFile, DsFind_VgmFilePos, SEEK_SET); \
    fread(&DsFind_VgmBuf[0], 1, sizeof(DsFind_VgmBuf), DacStream_FindFile); \
    DsFind_VgmBufPos = 0; \
    if (ferror(DacStream_FindFile)) { \
        file_error(); \
        continue; \
    }
#define DSFIND_BUF_CHECK \
    if (DsFind_VgmBufPos >= sizeof(DsFind_VgmBuf)) { \
        DSFIND_BUF_FILL; \
    }
#define DSFIND_BUF_SEEK_SET(offset) \
    DsFind_VgmFilePos = offset; \
    DSFIND_BUF_FILL;
#define DSFIND_BUF_SEEK_REL(offset) \
    DsFind_VgmFilePos += offset; \
    DsFind_VgmBufPos += offset; \
    DSFIND_BUF_CHECK;
#define DSFIND_BUF_READ(var) \
    var = DsFind_VgmBuf[DsFind_VgmBufPos]; \
    DsFind_VgmBufPos++; \
    DsFind_VgmFilePos++; \
    DSFIND_BUF_CHECK;
#define DSFIND_BUF_READ4(var) \
    if (sizeof(DsFind_VgmBuf)-DsFind_VgmBufPos < 4) { \
        DSFIND_BUF_FILL; \
    } \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wstrict-aliasing\"") \
    var = *(uint32_t*)&DsFind_VgmBuf[DsFind_VgmBufPos]; \
    _Pragma("GCC diagnostic pop") \
    DsFind_VgmBufPos += 4; \
    DsFind_VgmFilePos += 4; \
    DSFIND_BUF_CHECK;
#define DSFIND_BUF_READ2(var) \
    if (sizeof(DsFind_VgmBuf)-DsFind_VgmBufPos < 2) { \
        DSFIND_BUF_FILL; \
    } \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wstrict-aliasing\"") \
    var = *(uint16_t*)&DsFind_VgmBuf[DsFind_VgmBufPos]; \
    _Pragma("GCC diagnostic pop") \
    DsFind_VgmBufPos += 2; \
    DsFind_VgmFilePos += 2; \
    DSFIND_BUF_CHECK;

bool DacStream_Setup() {
    ESP_LOGI(TAG, "Setting up");

    ESP_LOGI(TAG, "Setting up DacStreamEntry streams");
    for (uint8_t i=0; i<DACSTREAM_PRE_COUNT; i++) {
        DacStreamEntries[i].SlotFree = true;
        DacStreamEntries[i].Seq = 0;
        MegaStream_Create((MegaStreamContext_t *)&DacStreamEntries[i].Stream, &Driver_PcmBuf[i*DACSTREAM_BUF_SIZE], DACSTREAM_BUF_SIZE);
    }

    ESP_LOGI(TAG, "Creating find thread status event group");
    DacStream_FindStatus = xEventGroupCreateStatic(&DacStream_FindStatusBuf);
    if (DacStream_FindStatus == NULL) {
        ESP_LOGE(TAG, "Failed !!");
        return false;
    }
    xEventGroupSetBits(DacStream_FindStatus, DACSTREAM_STOPPED);

    ESP_LOGI(TAG, "Creating fill thread status event group");
    DacStream_FillStatus = xEventGroupCreateStatic(&DacStream_FillStatusBuf);
    if (DacStream_FillStatus == NULL) {
        ESP_LOGE(TAG, "Failed !!");
        return false;
    }
    xEventGroupSetBits(DacStream_FillStatus, DACSTREAM_STOPPED);

    ESP_LOGI(TAG, "Creating task mutex");
    DacStream_Mutex = xSemaphoreCreateMutexStatic(&DacStream_MutexBuf);

    ESP_LOGI(TAG, "Ready");
    return true;
}

uint32_t DacStream_GetDataOffset(uint8_t BankType, uint32_t BankOff) {
    uint32_t consumed = 0;
    for (uint8_t i=0;i<DacStream_VgmDataBlockIndex;i++) {
        if (DacStream_VgmDataBlocks[i].Type == BankType) {
            if (BankOff >= consumed && BankOff < consumed + DacStream_VgmDataBlocks[i].Size) {
                return DacStream_VgmDataBlocks[i].Offset + (BankOff - consumed);
            }
            consumed += DacStream_VgmDataBlocks[i].Size;
        }
    }
    return 0xffffffff;
}

uint32_t DacStream_GetBlockOffset(uint8_t BankType, uint16_t BlockId) {
    uint16_t cblock = 0;
    uint32_t off = 0;
    for (uint8_t i=0;i<DacStream_VgmDataBlockIndex;i++) {
        if (DacStream_VgmDataBlocks[i].Type == BankType) {
            if (cblock++ == BlockId) {
                return off;
            }
            off += DacStream_VgmDataBlocks[i].Size;
        }
    }
    return 0xffffffff;
}

uint32_t DacStream_GetBlockSize(uint8_t BankType, uint16_t BlockId) {
    uint16_t cblock = 0;
    for (uint8_t i=0;i<DacStream_VgmDataBlockIndex;i++) {
        if (DacStream_VgmDataBlocks[i].Type == BankType) {
            if (cblock++ == BlockId) {
                return DacStream_VgmDataBlocks[i].Size;
            }
        }
    }
    return 0xffffffff;
}

static uint8_t d = 0;
static IRAM_ATTR uint32_t DacStream_Seq = 1;
static uint32_t DacStream_CurSampleRate = 0;
static uint8_t DacStream_CurDataBank = 0;
static uint16_t DacStream_CurDataBlock = 0;
static uint8_t DacStream_CurChipCommand = 0;
static uint8_t DacStream_CurChipPort = 0;

static volatile bool DacStream_FindRunning = false;
static volatile bool DacStream_FillRunning = false;
static void file_error() {
    modal_show_simple(TAG, "SD Card Error", "There was an error reading the VGM from the SD card.\nPlease check that the card is inserted and try again.", LV_SYMBOL_OK " OK");
    DacStream_FindRunning = false;
    DacStream_FillRunning = false;
    xEventGroupClearBits(DacStream_FindStatus, DACSTREAM_RUNNING);
    xEventGroupClearBits(DacStream_FillStatus, DACSTREAM_RUNNING); //since the delay on the fill task is ONLY on the eventgroup check, we have to clear this to prevent it spinning, since it's at higher priority than the tasks that are supposed to stop it!
    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
    QueueLength = 0;
    QueuePosition = 0;
    Ui_Screen = UISCREEN_MAINMENU;
    Sdcard_Online = false;
    ESP_LOGE(TAG, "IO error");
}
static bool DacStream_FoundAny = false;
void DacStream_FindTask() {
    ESP_LOGI(TAG, "Find task start");

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(DacStream_FindStatus, DACSTREAM_START_REQUEST | DACSTREAM_RUNNING | DACSTREAM_STOP_REQUEST, false, false, pdMS_TO_TICKS(75));
        if (bits & DACSTREAM_START_REQUEST) {
            ESP_LOGI(TAG, "Find starting");
            xEventGroupClearBits(DacStream_FindStatus, DACSTREAM_STOPPED);
            xEventGroupSetBits(DacStream_FindStatus, DACSTREAM_RUNNING);
            xEventGroupClearBits(DacStream_FindStatus, DACSTREAM_START_REQUEST);
            DacStream_FindRunning = true;
        } else if (bits & DACSTREAM_STOP_REQUEST) {
            ESP_LOGI(TAG, "Find stopping");
            DacStream_FindRunning = false;
            xEventGroupClearBits(DacStream_FindStatus, DACSTREAM_RUNNING);
            xEventGroupSetBits(DacStream_FindStatus, DACSTREAM_STOPPED);
            xEventGroupClearBits(DacStream_FindStatus, DACSTREAM_STOP_REQUEST);
        }
        if (DacStream_FindRunning) {
            xSemaphoreTake(DacStream_Mutex, pdMS_TO_TICKS(1000));
            uint8_t FreeSlot = 0xff;
            for (uint8_t i=0;i<DACSTREAM_PRE_COUNT;i++) {
                if (DacStreamEntries[i].SlotFree) {
                    FreeSlot = i;
                    break;
                }
            }
            if (FreeSlot != 0xff) {
                uint32_t start = xTaskGetTickCount();
                UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FIND] = true;
                UserLedMgr_Notify();
                while (xTaskGetTickCount() - start <= pdMS_TO_TICKS(50)) {
                    DSFIND_BUF_READ(d)
                    if (!VgmCommandIsFixedSize(d)) {
                        if (d == 0x67) { //datablock
                            if (DacStream_CurLoop == 0) { //only load datablocks on first loop through
                                VgmParseDataBlock(DacStream_FindFile, (VgmDataBlockStruct_t *)&DacStream_VgmDataBlocks[DacStream_VgmDataBlockIndex++]);
                            } else {
                                //can't simply skip over the datablock because they're variable-length. use the last entry as a garbage can
                                VgmParseDataBlock(DacStream_FindFile, (VgmDataBlockStruct_t *)&DacStream_VgmDataBlocks[MAX_REALTIME_DATABLOCKS]);
                            }
                            //todo: bounds check
                        }
                    } else {
                        if (d == 0x90) { //dacstream setup
                            DSFIND_BUF_SEEK_REL(2) //skip stream id and chip type
                            DSFIND_BUF_READ(DacStream_CurChipPort)
                            DSFIND_BUF_READ(DacStream_CurChipCommand)
                        } else if (d == 0x91) { //dacstream set data
                            DSFIND_BUF_SEEK_REL(1) //skip stream id
                            DSFIND_BUF_READ(DacStream_CurDataBank)
                            DSFIND_BUF_SEEK_REL(2) //skip step size and step base
                        } else if (d == 0x92) { //set sample rate
                            DSFIND_BUF_SEEK_REL(1) //skip stream id
                            DSFIND_BUF_READ4(DacStream_CurSampleRate)
                        } else if (d == 0x93) { //start
                            DSFIND_BUF_SEEK_REL(1) //skip stream id
                            DSFIND_BUF_READ4(DacStreamEntries[FreeSlot].DataStart) //todo: figure out what to do with -1
                            DSFIND_BUF_READ(DacStreamEntries[FreeSlot].LengthMode)
                            DSFIND_BUF_READ4(DacStreamEntries[FreeSlot].DataLength)
                            //assign other attributes
                            DacStreamEntries[FreeSlot].DataBankId = DacStream_CurDataBank;
                            DacStreamEntries[FreeSlot].ChipCommand = DacStream_CurChipCommand;
                            DacStreamEntries[FreeSlot].ChipPort = DacStream_CurChipPort;
                            DacStreamEntries[FreeSlot].SampleRate = DacStream_CurSampleRate;
                            DacStreamEntries[FreeSlot].Seq = DacStream_Seq++;
                            //reset
                            DacStreamEntries[FreeSlot].ReadOffset = 0;
                            DacStreamEntries[FreeSlot].BytesFilled = 0;
                            MegaStream_Reset((MegaStreamContext_t *)&DacStreamEntries[FreeSlot].Stream);
                            DacStreamEntries[FreeSlot].SlotFree = false;
                            DacStream_FoundAny = true;
                            break;
                        } else if (d == 0x94) { //stop
                            DSFIND_BUF_SEEK_REL(1) //skip stream id
                        } else if (d == 0x95) { //fast start
                            DSFIND_BUF_SEEK_REL(1) //skip stream id
                            DSFIND_BUF_READ2(DacStream_CurDataBlock)
                            DacStreamEntries[FreeSlot].DataLength = DacStream_GetBlockSize(DacStream_CurDataBank, DacStream_CurDataBlock);
                            DacStreamEntries[FreeSlot].DataStart = DacStream_GetBlockOffset(DacStream_CurDataBank, DacStream_CurDataBlock);
                            DSFIND_BUF_SEEK_REL(1) //skip flags
                            //assign other attributes
                            DacStreamEntries[FreeSlot].DataBankId = DacStream_CurDataBank;
                            DacStreamEntries[FreeSlot].ChipCommand = DacStream_CurChipCommand;
                            DacStreamEntries[FreeSlot].ChipPort = DacStream_CurChipPort;
                            DacStreamEntries[FreeSlot].SampleRate = DacStream_CurSampleRate;
                            DacStreamEntries[FreeSlot].Seq = DacStream_Seq++;
                            //reset
                            DacStreamEntries[FreeSlot].LengthMode = 0; //always for fast starts
                            DacStreamEntries[FreeSlot].ReadOffset = 0;
                            DacStreamEntries[FreeSlot].BytesFilled = 0;
                            MegaStream_Reset((MegaStreamContext_t *)&DacStreamEntries[FreeSlot].Stream);
                            DacStreamEntries[FreeSlot].SlotFree = false;
                            DacStream_FoundAny = true;
                            break;
                        } else if (d == 0x66) { //end of music, optionally loop
                            ESP_LOGI(TAG, "reached end of music");
                            if (DacStream_FoundAny) { //some were found so what we do now matters
                                if (DacStream_VgmInfo->LoopOffset == 0 || (Loader_IgnoreZeroSampleLoops && DacStream_VgmInfo->LoopSamples == 0)) { //there is no loop point at all
                                    ESP_LOGI(TAG, "no loop point");
                                    DacStream_FindRunning = false;
                                    break;
                                }
                                ESP_LOGI(TAG, "looping");
                                DacStream_CurLoop++; //still need to keep track of this so dacstream loads aren't duplicated
                                DSFIND_BUF_SEEK_SET(DacStream_VgmInfo->LoopOffset)
                                continue;
                            } else { //none were found, so just die
                                ESP_LOGI(TAG, "Find task reached end of music without finding any starts");
                                DacStream_FindRunning = false;
                                break;
                            }
                        } else {
                            //unimplemented command
                            DSFIND_BUF_SEEK_REL((VgmCommandLength(d)-1))
                        }
                    }
                }
                UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FIND] = false;
                UserLedMgr_Notify();
            }
            xSemaphoreGive(DacStream_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

static uint8_t DacStream_FillBuf[DACSTREAM_BUF_SIZE];
static IRAM_ATTR uint32_t LastOffset = 0xffffffff;
bool DacStream_FillTask_DoPre(uint8_t idx) { //returns whether or not it had to hit the card
    bool ret = false;
    xSemaphoreTake(DacStream_Mutex, pdMS_TO_TICKS(1000));
    if (!DacStreamEntries[idx].SlotFree) {
        if (MegaStream_Free((MegaStreamContext_t *)&DacStreamEntries[idx].Stream) > DACSTREAM_BUF_SIZE/3 && DacStreamEntries[idx].ReadOffset < DacStreamEntries[idx].DataLength) {
            UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FILL] = true;
            UserLedMgr_Notify();
            uint32_t o = DacStream_GetDataOffset(DacStreamEntries[idx].DataBankId, DacStreamEntries[idx].DataStart + DacStreamEntries[idx].ReadOffset);
            uint16_t dsbufused;
            if (o >= LastOffset && o < LastOffset + DACSTREAM_BUF_SIZE) { //if we're going to end up reading the same chunk, don't bother, reuse the buffer
                ESP_LOGD(TAG, "Reused buf");
                dsbufused = o - LastOffset;
            } else { //it's a different chunk than what's in the buf
                ESP_LOGD(TAG, "Couldn't reuse buf");
                fseek(DacStream_FillFile,o,SEEK_SET);
                fread(&DacStream_FillBuf[0], 1, DACSTREAM_BUF_SIZE, DacStream_FillFile); //slight code repetition, makes flow a bit easier
                LastOffset = o;
                dsbufused = 0;
                ret = true;
            }
            uint32_t freespaces = MegaStream_Free((MegaStreamContext_t *)&DacStreamEntries[idx].Stream);
            //try to find a dead dacstream that has the data we need. has to be a dead one, any active ones might have data removed 
            while (freespaces && DacStreamEntries[idx].ReadOffset < DacStreamEntries[idx].DataLength) {
                if (dsbufused == DACSTREAM_BUF_SIZE) {
                    ESP_LOGD(TAG, "Read past buffer");
                    fread(&DacStream_FillBuf[0], 1, DACSTREAM_BUF_SIZE, DacStream_FillFile);
                    dsbufused = 0;
                    LastOffset += DACSTREAM_BUF_SIZE;
                    ret = true;
                }

                //we want to write as much to the stream in one shot as we possibly can, so figure out how much we can do!
                uint32_t streamremaining = DacStreamEntries[idx].DataLength - DacStreamEntries[idx].ReadOffset;
                uint32_t dsbufremaining = DACSTREAM_BUF_SIZE - dsbufused;
                uint32_t writesize = streamremaining;
                if (dsbufremaining < writesize) writesize = dsbufremaining;
                if (freespaces < writesize) writesize = freespaces;
                MegaStream_Send((MegaStreamContext_t *)&DacStreamEntries[idx].Stream, &DacStream_FillBuf[dsbufused], writesize);
                dsbufused += writesize;
                DacStreamEntries[idx].ReadOffset += writesize;
                freespaces -= writesize;
            }
            UserLedMgr_DiskState[DISKSTATE_DACSTREAM_FILL] = false;
            UserLedMgr_Notify();
        }
    }
    xSemaphoreGive(DacStream_Mutex);
    if (ferror(DacStream_FillFile)) {
        ESP_LOGE(TAG, "Pre IO ERROR");
        file_error();
    }
    return ret;
}

void DacStream_FillTask() {
    ESP_LOGI(TAG, "Fill task start");

    uint8_t idx = 0;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(DacStream_FillStatus, DACSTREAM_START_REQUEST | DACSTREAM_RUNNING | DACSTREAM_STOP_REQUEST, false, false, pdMS_TO_TICKS(75));
        if (bits & DACSTREAM_START_REQUEST) {
            ESP_LOGI(TAG, "Fill starting");
            xEventGroupClearBits(DacStream_FillStatus, DACSTREAM_STOPPED);
            xEventGroupSetBits(DacStream_FillStatus, DACSTREAM_RUNNING);
            xEventGroupClearBits(DacStream_FillStatus, DACSTREAM_START_REQUEST);
            LastOffset = 0xffffffff; //invalidate - could be at the same pos in the file, but different file now
            DacStream_FillRunning = true;
        } else if (bits & DACSTREAM_STOP_REQUEST) {
            ESP_LOGI(TAG, "Fill stopping");
            DacStream_FillRunning = false;
            xEventGroupClearBits(DacStream_FillStatus, DACSTREAM_RUNNING);
            xEventGroupSetBits(DacStream_FillStatus, DACSTREAM_STOPPED);
            xEventGroupClearBits(DacStream_FillStatus, DACSTREAM_STOP_REQUEST);
        }
        if (DacStream_FillRunning) {
            if (DacStreamId != 0xff && DacStreamId != idx) DacStream_FillTask_DoPre(DacStreamId);
            bool ret = DacStream_FillTask_DoPre(idx);
            if (++idx == DACSTREAM_PRE_COUNT) idx = 0;
            if (!ret) { //if we didn't hit the card, that pre was basically "free", so go ahead with the next one right away
                DacStream_FillTask_DoPre(idx);
                if (++idx == DACSTREAM_PRE_COUNT) idx = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

bool DacStream_Start(FILE *FindFile, FILE *FillFile, VgmInfoStruct_t *info) {
    if (xEventGroupGetBits(DacStream_FindStatus) & DACSTREAM_RUNNING) {
        //running, can't start
        return false;
    }
    if (xEventGroupGetBits(DacStream_FillStatus) & DACSTREAM_RUNNING) {
        //running, can't start
        return false;
    }

    DacStream_FindFile = FindFile;
    DacStream_FillFile = FillFile;
    DacStream_VgmInfo = info;

    DacStream_Seq = 1;
    DacStream_FoundAny = false;
    DacStream_VgmDataBlockIndex = 0;
    DacStream_CurLoop = 0;

    ESP_LOGI(TAG, "DacStream_Start() requesting fill task start");
    xEventGroupSetBits(DacStream_FillStatus, DACSTREAM_START_REQUEST);

    return true;
}

bool DacStream_BeginFinding(VgmDataBlockStruct_t *SourceBlocks, uint8_t SourceBlockCount, uint32_t StartOffset) {
    ESP_LOGI(TAG, "DacStream_BeginFinding() starting");
    if (xEventGroupGetBits(DacStream_FindStatus) & DACSTREAM_RUNNING) {
        ESP_LOGE(TAG, "Find task already running !!");
        return false;
    }
    ESP_LOGI(TAG, "Copying datablock array");
    memcpy((VgmDataBlockStruct_t *)&DacStream_VgmDataBlocks[0], SourceBlocks, sizeof(VgmDataBlockStruct_t)*SourceBlockCount);
    DacStream_VgmDataBlockIndex = SourceBlockCount;
    ESP_LOGI(TAG, "Seek to start offset");
    uint8_t nastyhack = 1; //this is to avoid having a duplicate version of the buffer fill stuff that doesn't `continue`
    while (nastyhack) {
        nastyhack = 0;
        DSFIND_BUF_SEEK_SET(StartOffset)
    }
    ESP_LOGI(TAG, "Requesting find task start");
    xEventGroupSetBits(DacStream_FindStatus, DACSTREAM_START_REQUEST);
    ESP_LOGI(TAG, "Wait for find task start...");
    EventBits_t bits = xEventGroupWaitBits(DacStream_FindStatus, DACSTREAM_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DACSTREAM_RUNNING) == 0) {
        ESP_LOGE(TAG, "Dacstream find task start timeout !!");
        return false;
    }
    return true;
}

bool DacStream_Stop() {
    if (xEventGroupGetBits(DacStream_FindStatus) & DACSTREAM_STOPPED) {
        ESP_LOGW(TAG, "Warning: Dacstream find task is already stopped in DacStream_Stop() !!");
    } else {
        xEventGroupSetBits(DacStream_FindStatus, DACSTREAM_STOP_REQUEST);
    }

    xEventGroupSetBits(DacStream_FillStatus, DACSTREAM_STOP_REQUEST);

    EventBits_t bits = xEventGroupWaitBits(DacStream_FindStatus, DACSTREAM_STOPPED, false, false, pdMS_TO_TICKS(3000));
    if (bits & DACSTREAM_STOPPED) {
        bits = xEventGroupWaitBits(DacStream_FillStatus, DACSTREAM_STOPPED, false, false, pdMS_TO_TICKS(3000));
        if (bits & DACSTREAM_STOPPED) {
            //cleanup stuff
            for (uint8_t i=0; i<DACSTREAM_PRE_COUNT; i++) {
                DacStreamEntries[i].SlotFree = true;
            }
            return true;
        } else {
            ESP_LOGE(TAG, "DacStream fill task stop request timeout !!");
            return false;
        }
    } else {
        ESP_LOGE(TAG, "DacStream find task stop request timeout !!");
        return false;
    }
}
