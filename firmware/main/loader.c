#include "loader.h"
#include "esp_log.h"
#include "driver.h"
#include "mallocs.h"
#include "ioexp.h"
#include "dacstream.h"
#include "userled.h"
#include "player.h"
#include "taskmgr.h"
#include "clk.h"
#include "ui.h"
#include "ui/modal.h"
#include "sdcard.h"

static const char* TAG = "Loader";

uint8_t Loader_CurLoop = 0;

EventGroupHandle_t Loader_Status;
StaticEventGroup_t Loader_StatusBuf;
EventGroupHandle_t Loader_BufStatus;
StaticEventGroup_t Loader_BufStatusBuf;
FILE *Loader_File;
FILE *Loader_PcmFile;
VgmInfoStruct_t *Loader_VgmInfo;
uint8_t Loader_VgmDataBlockIndex = 0;
volatile VgmDataBlockStruct_t Loader_VgmDataBlocks[MAX_REALTIME_DATABLOCKS+1];
bool Loader_RequestedDacStreamFindStart = false;
uint8_t Loader_VgmBuf[FREAD_LOCAL_BUF];
uint16_t Loader_VgmBufPos = FREAD_LOCAL_BUF;
IRAM_ATTR uint32_t Loader_VgmFilePos = 0;
volatile bool Loader_IgnoreZeroSampleLoops = true;
volatile bool Loader_FastOpnaUpload = false;
bool Loader_HitLoop = false;
static bool Loader_IsBad = false;

//local buffer thingie. big speedup
#define LOADER_BUF_FILL \
    fseek(Loader_File, Loader_VgmFilePos, SEEK_SET); \
    fread(&Loader_VgmBuf[0], 1, sizeof(Loader_VgmBuf), Loader_File); \
    Loader_VgmBufPos = 0; \
    if (ferror(Loader_File)) { \
        file_error(); \
        continue; \
    }
#define LOADER_BUF_FILL_NOLOOP \
    fseek(Loader_File, Loader_VgmFilePos, SEEK_SET); \
    fread(&Loader_VgmBuf[0], 1, sizeof(Loader_VgmBuf), Loader_File); \
    Loader_VgmBufPos = 0; \
    if (ferror(Loader_File)) { \
        file_error(); \
        Loader_VgmBufPos = 0; \
        return false; \
    }
#define LOADER_BUF_CHECK \
    if (Loader_VgmBufPos >= sizeof(Loader_VgmBuf)) { \
        LOADER_BUF_FILL; \
    }
#define LOADER_BUF_SEEK_SET(offset) \
    Loader_VgmFilePos = offset; \
    LOADER_BUF_FILL;
#define LOADER_BUF_SEEK_REL(offset) \
    Loader_VgmFilePos += offset; \
    Loader_VgmBufPos += offset; \
    LOADER_BUF_CHECK;
#define LOADER_BUF_READ(var) \
    var = Loader_VgmBuf[Loader_VgmBufPos]; \
    Loader_VgmBufPos++; \
    Loader_VgmFilePos++; \
    LOADER_BUF_CHECK;
#define LOADER_BUF_READ4(var) \
    if (sizeof(Loader_VgmBuf)-Loader_VgmBufPos < 4) { \
        LOADER_BUF_FILL; \
    } \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wstrict-aliasing\"") \
    var = *(uint32_t*)&Loader_VgmBuf[Loader_VgmBufPos]; \
    _Pragma("GCC diagnostic pop") \
    Loader_VgmBufPos += 4; \
    Loader_VgmFilePos += 4; \
    LOADER_BUF_CHECK;

bool Loader_Setup() {
    ESP_LOGI(TAG, "Setting up");
    
    ESP_LOGI(TAG, "Creating status event group");
    Loader_Status = xEventGroupCreateStatic(&Loader_StatusBuf);
    if (Loader_Status == NULL) {
        ESP_LOGE(TAG, "Failed !!");
        return false;
    }
    xEventGroupSetBits(Loader_Status, LOADER_STOPPED);
    
    ESP_LOGI(TAG, "Creating buffer status event group");
    Loader_BufStatus = xEventGroupCreateStatic(&Loader_BufStatusBuf);
    if (Loader_BufStatus == NULL) {
        ESP_LOGE(TAG, "Failed !!");
        return false;
    }


    return true;
}

IRAM_ATTR uint32_t Loader_PcmPos = 0;
IRAM_ATTR uint32_t Loader_PcmOff = 0;
uint32_t Loader_GetPcmOffset(uint32_t PcmPos) {
    uint32_t consumed = 0;
    for (uint8_t i=0;i<Loader_VgmDataBlockIndex;i++) {
        if (Loader_VgmDataBlocks[i].Type == 0) { //0 = ym2612 pcm
            if (PcmPos >= consumed && PcmPos < consumed + Loader_VgmDataBlocks[i].Size) {
                return Loader_VgmDataBlocks[i].Offset + (PcmPos - consumed);
            }
            consumed += Loader_VgmDataBlocks[i].Size;
        }
    }
    return 0xffffffff;
}

static uint8_t running = false;
static void file_error() {
    modal_show_simple(TAG, "SD Card Error", "There was an error reading the VGM from the SD card.\nPlease check that the card is inserted and try again.", LV_SYMBOL_OK " OK");
    running = false;
    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
    Ui_Screen = UISCREEN_MAINMENU;
    Sdcard_Online = false;
    ESP_LOGE(TAG, "IO error");
}

IRAM_ATTR uint32_t Loader_Pending = 0;
bool Loader_EndReached = false;
uint8_t Loader_PcmBuf[FREAD_LOCAL_BUF];
uint16_t Loader_PcmBufUsed = FREAD_LOCAL_BUF;
static IRAM_ATTR uint32_t adjustedprio = false;
void Loader_Main() {
    ESP_LOGI(TAG, "Task start");
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(Loader_Status, LOADER_START_REQUEST | LOADER_RUNNING | LOADER_STOP_REQUEST, false, false, pdMS_TO_TICKS(75));
        if (bits & LOADER_START_REQUEST) {
            ESP_LOGI(TAG, "Loader starting");
            xEventGroupClearBits(Loader_Status, LOADER_STOPPED);
            xEventGroupSetBits(Loader_Status, LOADER_RUNNING);
            xEventGroupClearBits(Loader_Status, LOADER_START_REQUEST);
            if (Loader_IsBad) {
                uint8_t badval = 0xff;
                MegaStream_Send(&Driver_CommandStream, &badval, 1);
            }
            running = true;
        } else if (bits & LOADER_STOP_REQUEST) {
            ESP_LOGI(TAG, "Loader stopping");
            running = false;
            xEventGroupClearBits(Loader_Status, LOADER_RUNNING);
            xEventGroupSetBits(Loader_Status, LOADER_STOPPED);
            xEventGroupClearBits(Loader_Status, LOADER_STOP_REQUEST);
        }
        if (running) {
            uint16_t spaces = MegaStream_Free(&Driver_CommandStream);
            EventBits_t bbits = xEventGroupGetBits(Loader_BufStatus);
            if (spaces == 0 && !(bbits & LOADER_BUF_FULL)) {
                xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_FULL);
                xEventGroupClearBits(Loader_BufStatus, 0xff ^ LOADER_BUF_FULL);
            } else if (spaces == DRIVER_QUEUE_SIZE) {
                xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_EMPTY);
                xEventGroupClearBits(Loader_BufStatus, 0xff ^ LOADER_BUF_EMPTY);
            } else if (spaces < (DRIVER_QUEUE_SIZE-(DRIVER_QUEUE_SIZE/4)) || Loader_EndReached || Loader_HitLoop) {
                xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_OK);
                xEventGroupClearBits(Loader_BufStatus, 0xff ^ LOADER_BUF_OK);
            } else {
                xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_LOW);
                xEventGroupClearBits(Loader_BufStatus, 0xff ^ LOADER_BUF_LOW);
            }
            if (Loader_HitLoop) Loader_HitLoop = false;
            if (adjustedprio) {
                vTaskPrioritySet(Taskmgr_Handles[TASK_LOADER], LOADER_TASK_PRIO_NORM);
                ESP_LOGW(TAG, "Returning to normal priority");
                adjustedprio = false;
            }
            if (!Loader_EndReached && (spaces > DRIVER_QUEUE_SIZE/16)) {
                UserLedMgr_DiskState[DISKSTATE_VGM] = true;
                UserLedMgr_Notify();
                while (running && MegaStream_Free(&Driver_CommandStream) >= 11) { //11 is the biggest fixed-size vgm command, so make sure there's at least that much space
                    if (MegaStream_Free(&Driver_CommandStream) > (DRIVER_QUEUE_SIZE/3)) {
                        if (!adjustedprio) {
                            ESP_LOGW(TAG, "Switching to high priority");
                            vTaskPrioritySet(Taskmgr_Handles[TASK_LOADER], LOADER_TASK_PRIO_HIGH);
                            adjustedprio = true;
                        }
                    }
                    uint8_t d = 0x00;
                    LOADER_BUF_READ(d);
                    if (d == 0xe0) { //pcm seek
                        uint32_t NewPos = 0;
                        LOADER_BUF_READ4(NewPos);
                        uint32_t NewOff = Loader_GetPcmOffset(NewPos);
                        if (NewOff != (Loader_PcmOff+1)) {
                            ESP_LOGD(TAG, "Pcm seeking to NewOff %d BufUsed %d PcmOff %d", NewOff, Loader_PcmBufUsed, Loader_PcmOff);
                            if (NewOff <= Loader_PcmOff && Loader_PcmOff - NewOff <= Loader_PcmBufUsed) { //don't seek the file if we can just rewind the buffer. todo: possibly handle optimize handling skipping forward within the buf's range?
                                ESP_LOGD(TAG, "PCM buffer rewind");
                                Loader_PcmBufUsed -= Loader_PcmOff - NewOff;
                                Loader_PcmOff = NewOff;
                            } else { //it's either ahead of our current position, or too far behind that it's not in the buffer
                                ESP_LOGD(TAG, "PCM actual read");
                                fseek(Loader_PcmFile, NewOff, SEEK_SET);
                                Loader_PcmOff = NewOff;
                                Loader_PcmBufUsed = FREAD_LOCAL_BUF;
                            }
                        }
                        Loader_PcmPos = NewPos;
                    } else if (d == 0x67) { //datablock
                        if (Loader_VgmDataBlockIndex == MAX_REALTIME_DATABLOCKS) {
                            ESP_LOGE(TAG, "loader datablocks over !!");
                            Loader_EndReached = true;
                            running = false;
                            xEventGroupClearBits(Loader_Status, LOADER_RUNNING);
                            xEventGroupSetBits(Loader_Status, LOADER_STOPPED);
                            xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_OK);
                            xEventGroupClearBits(Loader_BufStatus, 0xff ^ LOADER_BUF_OK);
                        } else {
                            //here are some hacks to wrap around VgmParseDataBlock not using our local buffer thing
                            fseek(Loader_File, Loader_VgmFilePos, SEEK_SET); //destroys buf
                            if (Loader_CurLoop == 0) { //only load datablocks on first loop through
                                VgmParseDataBlock(Loader_File, (VgmDataBlockStruct_t *)&Loader_VgmDataBlocks[Loader_VgmDataBlockIndex++]);
                            } else {
                                //can't simply skip over the datablock because they're variable-length. use the last entry as a garbage can
                                VgmParseDataBlock(Loader_File, (VgmDataBlockStruct_t *)&Loader_VgmDataBlocks[MAX_REALTIME_DATABLOCKS]);
                            }
                            LOADER_BUF_SEEK_SET(ftell(Loader_File)); //fix buf

                            //handle opna pcm datablocks, since they need to be uploaded
                            if (Loader_VgmDataBlocks[Loader_VgmDataBlockIndex-1].Type == 0x81 && Loader_VgmDataBlocks[Loader_VgmDataBlockIndex-1].Size > 8) {
                                ESP_LOGI(TAG, "Requesting OPNA PCM upload");
                                Clk_TempSet(0,Loader_FastOpnaUpload?12000000:8000000); //avoid timing issues / speed reduction if chip underclocked
                                Driver_Opna_PcmUploadId = Loader_VgmDataBlockIndex-1;
                                Driver_Opna_PcmUpload = true;
                                while (Driver_Opna_PcmUpload) {
                                    vTaskDelay(pdMS_TO_TICKS(10));
                                    //todo: timeout?
                                }
                                Clk_Restore(0);
                                ESP_LOGI(TAG, "Done");
                            }
                        }
                    } else if (d == 0x68) {

                    } else if ((d&0xf0) == 0x80) { //pcm and wait
                        if (Loader_PcmOff == 0) { //if this is the first sample being played, need to do an initial seek
                            Loader_PcmOff = Loader_GetPcmOffset(Loader_PcmPos);
                            fseek(Loader_PcmFile, Loader_PcmOff, SEEK_SET);
                            Loader_PcmBufUsed = FREAD_LOCAL_BUF;
                        }
                        if (Loader_PcmBufUsed == FREAD_LOCAL_BUF) {
                            fread(&Loader_PcmBuf[0], 1, FREAD_LOCAL_BUF, Loader_PcmFile); //todo: fix read past eof
                            if (ferror(Loader_PcmFile)) {
                                file_error();
                                continue;
                            }
                            Loader_PcmBufUsed = 0;
                        }
                        MegaStream_Send(&Driver_PcmStream, &Loader_PcmBuf[Loader_PcmBufUsed++], 1); //theoretically pcmqueue should never ever be full while there are still spaces in commandqueue
                        Loader_PcmPos++;
                        #ifdef PARANOID_THAT_THERE_MIGHT_BE_VGMS_THAT_PLAY_PCM_ACROSS_BLOCK_BOUNDARIES
                        uint32_t NewOff = Loader_GetPcmOffset(Loader_PcmPos);
                        if (NewOff != (Loader_PcmOff+1)) {
                            ESP_LOGI(TAG, "pcm seeking to %d after sample load", NewOff);
                            fseek(Loader_PcmFile, NewOff, SEEK_SET);
                            Loader_PcmOff = NewOff;
                            Loader_PcmBufUsed = FREAD_LOCAL_BUF;
                        }
                        #else
                        Loader_PcmOff++;
                        #endif
                        MegaStream_Send(&Driver_CommandStream, &d, 1);
                    } else if (d == 0x66) { //end of music, optionally loop
                        ESP_LOGI(TAG, "reached end of music");
                        if (Loader_VgmInfo->LoopOffset == 0 || (Loader_IgnoreZeroSampleLoops && Loader_VgmInfo->LoopSamples == 0)) { //there is no loop point at all
                            ESP_LOGI(TAG, "no loop point");
                            MegaStream_Send(&Driver_CommandStream, &d, 1); //let driver figure out it's the end
                            Loader_EndReached = true;
                            break;
                        }
                        ESP_LOGI(TAG, "looping");
                        if (Loader_VgmInfo->LoopSamples == 0) ESP_LOGW(TAG, "looping despite LoopSamples == 0 !!");
                        if (Loader_CurLoop == 0) Loader_HitLoop = true;
                        Loader_CurLoop++; //still need to keep track of this so dacstream loads aren't duplicated
                        MegaStream_Send(&Driver_CommandStream, &d, 1);
                        LOADER_BUF_SEEK_SET(Loader_VgmInfo->LoopOffset);
                        break;
                    } else if (d >= 0x90 && d <= 0x95) { //dacstream command
                        if (!Loader_RequestedDacStreamFindStart) {
                            DacStream_BeginFinding((VgmDataBlockStruct_t *)&Loader_VgmDataBlocks, Loader_VgmDataBlockIndex, Loader_VgmFilePos-1);
                            Loader_RequestedDacStreamFindStart = true;
                        }
                        MegaStream_Send(&Driver_CommandStream, &d, 1); //command
                        uint8_t cmdlen = VgmCommandLength(d);
                        for (uint8_t i=1;i<cmdlen;i++) { //command data
                            LOADER_BUF_READ(d);
                            MegaStream_Send(&Driver_CommandStream, &d, 1);
                        }
                    } else { //just a regular command
                        MegaStream_Send(&Driver_CommandStream, &d, 1); //command
                        uint8_t cmdlen = VgmCommandLength(d)-1; //it's really the command's attached data
                        if (cmdlen == 0) continue; //don't bother with any of the below if there is no attached data

                        //fill up the fread buf if necessary, so we can copy the whole command in one call
                        if (Loader_VgmBufPos + cmdlen >= FREAD_LOCAL_BUF) {
                            LOADER_BUF_FILL;
                        }

                        MegaStream_Send(&Driver_CommandStream, &Loader_VgmBuf[Loader_VgmBufPos], cmdlen);

                        //fix up the buffer tracking vars that the LOADER_BUF_READ macro normally handles
                        Loader_VgmBufPos += cmdlen;
                        Loader_VgmFilePos += cmdlen;
                        LOADER_BUF_CHECK;
                    }
                }
                UserLedMgr_DiskState[DISKSTATE_VGM] = false;
                UserLedMgr_Notify();
                vTaskDelay(pdMS_TO_TICKS(75)); //just filled, big delay now
            } else {
                vTaskDelay(pdMS_TO_TICKS(25)); //keep an eye on queue spaces
            }
        }
    }
}

bool Loader_Start(FILE *File, FILE *PcmFile, VgmInfoStruct_t *info, bool IsBad) {
    if (xEventGroupGetBits(Loader_Status) & LOADER_RUNNING) {
        //running, can't start
        return false;
    }

    Loader_File = File;
    Loader_PcmFile = PcmFile;
    Loader_VgmInfo = info;
    Loader_PcmPos = 0;
    Loader_PcmOff = 0;
    Loader_Pending = 0;
    Loader_EndReached = false;
    Loader_RequestedDacStreamFindStart = false;
    Loader_VgmDataBlockIndex = 0;
    Loader_CurLoop = 0;
    Loader_IsBad = IsBad;
    Loader_PcmBufUsed = FREAD_LOCAL_BUF;

    Loader_VgmFilePos = Loader_VgmInfo->DataOffset;
    LOADER_BUF_FILL_NOLOOP;

    xEventGroupSetBits(Loader_Status, LOADER_START_REQUEST);

    return true;
}

bool Loader_Stop() {
    if (xEventGroupGetBits(Loader_Status) & LOADER_STOPPED) {
        ESP_LOGW(TAG, "Loader_Stop() called but loader is already stopped !!");
    }
    xEventGroupSetBits(Loader_Status, LOADER_STOP_REQUEST);
    EventBits_t bits = xEventGroupWaitBits(Loader_Status, LOADER_STOPPED, false, false, pdMS_TO_TICKS(3000));
    if (bits & LOADER_STOPPED) {
        //cleanup stuff
        Loader_VgmDataBlockIndex = 0;
        xEventGroupSetBits(Loader_BufStatus, LOADER_BUF_EMPTY);
        xEventGroupClearBits(Loader_BufStatus, 0xff & ~LOADER_BUF_EMPTY);
        //resetting streams here is actually unsafe, but we're protected by player ensuring that driver is stopped before getting this far
        MegaStream_Reset(&Driver_CommandStream);
        MegaStream_Reset(&Driver_PcmStream);
        return true;
    } else {
        ESP_LOGE(TAG, "Loader stop request timeout !!");
        return false;
    }
}
