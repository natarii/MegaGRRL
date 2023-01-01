#include "player.h"
#include "esp_log.h"
#include "unistd.h"
#include "driver.h"
#include "ioexp.h"
#include "loader.h"
#include "dacstream.h"
#include "freertos/event_groups.h"
#include "clk.h"
#include "queue.h"
#include "vgm.h"
#include "gd3.h"
#include "mallocs.h"
#include "ui/nowplaying.h"
#include "rom/miniz.h"
#include "ui/statusbar.h"
#include "ui/modal.h"
#include <rom/crc.h>
#include "sdcard.h"
#include "ui.h"
#include "taskmgr.h"

//vgms with the project 2612 test register issue
static const uint32_t known_bad_testreg_vgms[] = {
    //comix zone
    0x81f1e23d,
    0x7f5e11fd,
    0xe567d242,
    0x28971f44,
    0xd6ae05f9,
    0x44fc0038,
    0xa2aa195e,
    0x32693be7,
    0x9beea872,
    0x8e8c0fb2,
    0x8d35a542,
    //rocket knight adventures
    0x6f25be56,
    0x58b4cdd2,
    0x68843b29,
    0x3083ff1d,
    0xcedda5c4,
    0x5c2e3d41,
    0x82d42a5d,
    0xf1c6a7f7,
    0xe9eb05e7,
    0xe7f8fcb0,
    0x8474914a,
    0xef317aef,
    0x92213d13,
    0xb2202d10,
    0x3d9ff883,
    0x00ec2d1c,
    0xa2b1fa83,
    0x4a3a32a9,
    0x92fbe00f,
    0x718cb2cf,
    0xbb55680c,
    0x38166a76,
    0x802e0781,
    0x8990b4c1,
    0x08b54cdd,
    //vectorman
    0xed8e98b9, //options
    0x9e0d20a1, //information day 8
};

//vgms that are bad for other reasons, so they don't get reported as bugs
static const uint32_t known_bad_other_vgms[] = {
    //alisia dragoon
    0xced29e07, //opening
};

static const char* TAG = "Player";

FILE *Player_VgmFile;
FILE *Player_PcmFile;
FILE *Player_DsFindFile;
FILE *Player_DsFillFile;
VgmInfoStruct_t Player_Info;
static IRAM_ATTR uint32_t notif = 0;

volatile uint8_t Player_LoopCount = 2;
volatile RepeatMode_t Player_RepeatMode = REPEAT_ALL;
volatile bool Player_UnvgzReplaceOriginal = true;
volatile bool Player_SkipUnsupported = true;

EventGroupHandle_t Player_Status;
StaticEventGroup_t Player_StatusBuf;

char Player_Gd3_Title[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Game[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Author[PLAYER_GD3_FIELD_SIZES+1];

const static char* unvgztmp = "/sd/.mega/unvgz.tmp";

static IRAM_ATTR uint32_t failed_plays = 0;

static uint32_t Player_StartTrack(char *FilePath);
static bool Player_StopTrack();

#define PLAYER_ERR (1<<0) //flag for any failure
#define PLAYER_UNSUPPORTED_CHIPS (1<<1) //unsupported chips present (synth type)
#define PLAYER_UNSUPPORTED_CHIPS_ARE_PCM (1<<2) //unsupported chips present (ONLY "addon" pcm type) - TODO
#define PLAYER_ERR_INTERNAL (1<<3) //when it's not the user's fault
#define PLAYER_ERR_SYS (1<<4) //things that Should Never Happen (TM)

static void file_error(bool writing) {
    if (!writing) {
        modal_show_simple(TAG, "SD Card Error", "There was an error reading the VGM from the SD card.\nPlease check that the card is inserted and try again.", LV_SYMBOL_OK " OK");
    } else {
        modal_show_simple(TAG, "SD Card Error", "There was an error writing to the SD card during VGZ extraction.\nPlease check that the card is inserted and has free space.", LV_SYMBOL_OK " OK");
    }
    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
    QueueLength = 0;
    QueuePosition = 0;
    Ui_Screen = UISCREEN_MAINMENU;
    Sdcard_Invalidate();
}

static bool Player_NextTrk(bool UserSpecified) { //returns true if there is now a track playing
    Player_StopTrack();
    if (!UserSpecified && Player_RepeatMode == REPEAT_ONE) {
        //nothing to do - just start the same track again
    } else {
        bool end = (QueuePosition == QueueLength-1);
        if (end) {
            if (Player_RepeatMode == REPEAT_NONE) {
                return false; //nothing more to play
            } else if (Player_RepeatMode == REPEAT_ALL || Player_RepeatMode == REPEAT_ONE) {
                QueuePosition = 0;
                QueueSetupEntry(false, true);
            }
        } else { //not the end of the queue, just load the next track
            QueueNext();
            QueueSetupEntry(false, true);
        }
    }
    return true;
}

static bool Player_PrevTrk(bool UserSpecified) { //returns true if there is now a track playing
    Player_StopTrack();
    if (!UserSpecified && Player_RepeatMode == REPEAT_ONE) {
        //nothing to do - just start the same track again
    } else {
        bool end = (QueuePosition == 0);
        if (end) {
            if (Player_RepeatMode == REPEAT_NONE) {
                //nothing to do - just start the same track again
            } else if (Player_RepeatMode == REPEAT_ALL || Player_RepeatMode == REPEAT_ONE) {
                QueuePosition = QueueLength-1;
                QueueSetupEntry(false, true);
            }
        } else { //not the end of the queue, just load the prev track
            QueuePrev();
            QueueSetupEntry(false, true);
        }
    }
    return true;
}

void Player_Main() {
    ESP_LOGI(TAG, "Task start");

    while (1) {
        bool failed = false;
        if (xTaskNotifyWait(0,0xffffffff, &notif, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (notif == PLAYER_NOTIFY_START_RUNNING) {
                ESP_LOGI(TAG, "control: start requested");
                failed_plays = 0;
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RAN_OUT);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if ((xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) == 0) {
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING); //do this now, Player_StartTrack could take longer than the timeout of the task waiting for this event.
                    QueueSetupEntry(false, true);
                    if (Player_StartTrack(&QueuePlayingFilename[0]) & PLAYER_ERR) {
                        failed = true;
                        failed_plays++;
                    }
                } else {
                    //already running. yikes!
                }
            } else if (notif == PLAYER_NOTIFY_STOP_RUNNING) {
                ESP_LOGI(TAG, "control: stop requested");
                if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) {
                    ESP_LOGI(TAG, "player running, stopping track");
                    Player_StopTrack();
                }
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RAN_OUT);
            } else if (notif == PLAYER_NOTIFY_NEXT) {
                ESP_LOGI(TAG, "control: next requested");
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if (Player_NextTrk(true)) {
                    ESP_LOGI(TAG, "next track proceeding");
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    if (Player_StartTrack(QueuePlayingFilename) & PLAYER_ERR) {
                        failed = true;
                        failed_plays++;
                    }
                } else {
                    ESP_LOGI(TAG, "next track failed");
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RAN_OUT);
                }
            } else if (notif == PLAYER_NOTIFY_PREV) {
                ESP_LOGI(TAG, "control: prev requested");
                failed_plays = 0; //also reset here in case they change the setting and retry, or something like that
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if (Driver_FirstWait || (Driver_Sample < 3*44100)) { //actually change track
                    ESP_LOGI(TAG, "within 3 second window");
                    if (Player_PrevTrk(true)) {
                        ESP_LOGI(TAG, "prev track proceeding");
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                        if (Player_StartTrack(QueuePlayingFilename) & PLAYER_ERR) {
                            failed = true;
                            failed_plays++;
                        }
                    } else {
                        ESP_LOGI(TAG, "prev track failed");
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    }
                } else { //just restart
                    ESP_LOGI(TAG, "outside 3 second window, just restarting track");
                    Player_StopTrack();
                    if (Player_StartTrack(&QueuePlayingFilename[0]) & PLAYER_ERR) {
                        failed = true;
                        failed_plays++;
                    }
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                }
            } else if (notif == PLAYER_NOTIFY_PAUSE) {
                ESP_LOGI(TAG, "control: pause requested");
                if (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) {
                    ESP_LOGI(TAG, "Request driver stop...");
                    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_STOP_REQUEST);
                    /*ESP_LOGI(TAG, "Wait for driver to stop...");
                    if (xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000)) & DRIVER_EVENT_RUNNING) {
                        ESP_LOGE(TAG, "Driver stop timeout !!");
                        return false;
                    }*/
                }
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_PAUSED);
            } else if (notif == PLAYER_NOTIFY_PLAY) {
                ESP_LOGI(TAG, "control: play requested");
                if ((xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) == 0) {
                    ESP_LOGI(TAG, "Request driver resume...");
                    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESUME_REQUEST);
                    ESP_LOGI(TAG, "Wait for driver to resume...");
                    if ((xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000)) & DRIVER_EVENT_RUNNING) == 0) {
                        ESP_LOGE(TAG, "Driver resume timeout !!");
                        return;
                    }
                }
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
            } else if (notif == PLAYER_NOTIFY_FASTFORWARD) {
                xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_FASTFORWARD);
            }
        } else { //no incoming notification

        }
        if ((xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) && (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_FINISHED)) { //still running, but driver reached end
            ESP_LOGI(TAG, "Driver finished, starting next track");
            xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
            if (Player_NextTrk(false)) {
                ESP_LOGI(TAG, "next track proceeding");
                if (Player_StartTrack(QueuePlayingFilename) & PLAYER_ERR) {
                    failed = true;
                    failed_plays++;
                }
            } else {
                ESP_LOGI(TAG, "next track failed");
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_RAN_OUT);
            }
        }

        if (failed) {
            ESP_LOGW(TAG, "%d failed plays out of %d in queue", failed_plays, QueueLength);
            //avoid looping and popping up modals forever
            if (failed_plays >= QueueLength) {
                //incomplete handling of stop, just to catch any attempts to do this...
                xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
                QueueLength = 0;
                QueuePosition = 0;
            } else {
                xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_NEXT, eSetValueWithoutOverwrite);
            }
        }
    }
}

bool Player_Setup() {
    ESP_LOGI(TAG, "Setting up");

    Player_Gd3_Title[0] = 0;
    Player_Gd3_Game[0] = 0;
    Player_Gd3_Author[0] = 0;

    ESP_LOGI(TAG, "Creating status event group");
    Player_Status = xEventGroupCreateStatic(&Player_StatusBuf);
    if (Player_Status == NULL) {
        ESP_LOGE(TAG, "Failed !!");
        return false;
    }
    xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
    return true;
}

static tinfl_decompressor decomp;
static tinfl_status Player_Unvgz(char *FilePath, bool ReplaceOriginalFile) {
    FILE *reader;
    FILE *writer;

    char vgmfn[513];
    if (ReplaceOriginalFile) {
        strcpy(vgmfn, FilePath);
        //actually check if last char is z, in case it's a compressed file with .vgm extension
        if (vgmfn[strlen(vgmfn)-1] == 'z' || vgmfn[strlen(vgmfn)-1] == 'Z') {
            vgmfn[strlen(vgmfn)-1] -= 0x0d;
        }
        
        ESP_LOGW(TAG, "Unvgz: Decompressing %s to %s using temp file", FilePath, vgmfn);
    } else {
        ESP_LOGW(TAG, "Unvgz: Decompressing %s to temp file", FilePath);
    }
    reader = fopen(FilePath, "r");
    writer = fopen(unvgztmp, "w");
    if (!reader || !writer) {
        file_error(true);
        if (reader) fclose(reader);
        if (writer) fclose(writer);
        return 0x12345678;
    }
    fseek(writer, 0, SEEK_SET);

    //get compressed size
    fseek(reader, 0, SEEK_END);
    size_t in_remaining = ftell(reader) - 18; //18 = gzip header size. TODO check gzip header for sanity
    fseek(reader, 10, SEEK_SET);

    tinfl_init(&decomp);
    const void *next_in = Driver_CommandStreamBuf;
    void *next_out = Driver_PcmBuf;
    size_t avail_in = 0;
    size_t avail_out = 65536; // >= LZ dict size*2 && <= DACSTREAM_BUF_SIZE*DACSTREAM_PRE_COUNT}
    size_t total_in = 0;
    size_t total_out = 0;
    size_t in_bytes, out_bytes;
    tinfl_status status;
    for (;;) {
        if (!avail_in) {
            size_t rd = (in_remaining<32767)?in_remaining:32767; //power of 2 <= DRIVER_QUEUE_SIZE
            if (fread(Driver_CommandStreamBuf, 1, rd, reader) != rd) {
                ESP_LOGE(TAG, "read fail");
            }
            ESP_LOGD(TAG, "read chunk %d", rd);
            next_in = Driver_CommandStreamBuf;
            avail_in = rd;
            in_remaining -= rd;
        }
        in_bytes = avail_in;
        out_bytes = avail_out;
        ESP_LOGD(TAG, "inb %d outb %d", in_bytes, out_bytes);
        if (ferror(reader)) { //check reader before each block decompress
            file_error(false);
            fclose(reader);
            fclose(writer);
            return 0x12345678;
        }
        status = tinfl_decompress(&decomp, (const mz_uint8 *)next_in, &in_bytes, Driver_PcmBuf, (mz_uint8 *)next_out, &out_bytes, (in_remaining?TINFL_FLAG_HAS_MORE_INPUT:0)/*|TINFL_FLAG_PARSE_ZLIB_HEADER*/);

        avail_in -= in_bytes;
        next_in = (const mz_uint8 *)next_in + in_bytes;
        total_in += in_bytes;
        
        avail_out -= out_bytes;
        next_out = (mz_uint8 *)next_out + out_bytes;
        total_out += out_bytes;

        if ((status <= TINFL_STATUS_DONE) || (!avail_out)) {
            size_t wr = 65536 - avail_out;
            fwrite(Driver_PcmBuf, 1, wr, writer);
            ESP_LOGD(TAG, "wrote chunk %d", wr);
            if (ferror(writer)) { //catch problems with write here
                file_error(true);
                fclose(reader);
                fclose(writer);
                return 0x12345678;
            }
            next_out = Driver_PcmBuf;
            avail_out = 65536;
        }

        if (status <= TINFL_STATUS_DONE) {
            if (status == TINFL_STATUS_DONE) {
                ESP_LOGI(TAG, "decomp ok !!");
                break;
            } else {
                ESP_LOGE(TAG, "decomp fail %d", status);
                break;
            }
        }
    }
    fclose(reader);
    fclose(writer);
    if (status != TINFL_STATUS_DONE) return status;
    if (ReplaceOriginalFile) {
        ESP_LOGW(TAG, "Unvgz: Deleting original file");
        int ret = remove(FilePath);
        if (ret != 0) {
            file_error(true);
            return 0x12345678;
        }
        ESP_LOGW(TAG, "Unvgz: Renaming temp file to %s", vgmfn);
        ret = rename(unvgztmp, vgmfn);
        if (ret != 0) {
            file_error(true);
            return 0x12345678;
        }
    }
    return status;
}

//is_pcm sets whether or not the chip should be considered an "addon" - for example, the sega cd pcm chip
#define CHECK_CLOCK(offset, is_pcm)\
    if (Driver_PcmBuf[offset+3] & 0x40) {\
        clocks_specified += 2;\
        if (is_pcm) clocks_specified_pcm += 2;\
        ESP_LOGI(TAG, "Dual-chip at %08x", offset);\
    } else if (Driver_PcmBuf[offset] || Driver_PcmBuf[offset+1] || Driver_PcmBuf[offset+2] || Driver_PcmBuf[offset+3]) {\
        clocks_specified++;\
        if (is_pcm) clocks_specified_pcm++;\
        ESP_LOGI(TAG, "Chip at %08x", offset);\
    }

static uint32_t Player_StartTrack(char *FilePath) {
    const char *OpenFilePath = FilePath;

    ESP_LOGI(TAG, "Checking file type of %s", FilePath);
    FILE *test = fopen(FilePath, "r");
    //note that errors here could be the user's fault - for example, a playlist specifying non-existent files
    if (!test) {
        if (*(FilePath+(strlen(FilePath)-1)) == 'z' || *(FilePath+(strlen(FilePath)-1)) == 'Z') {
            ESP_LOGW(TAG, "vgz doesn't exist, let's try vgm");
            *(FilePath+(strlen(FilePath)-1)) -= 0x0d;
            test = fopen(FilePath, "r");
            if (!test) {
                ESP_LOGE(TAG, "vgm doesn't exist either");
                return PLAYER_ERR;
            }
        } else {
            ESP_LOGE(TAG, "file doesn't exist");
            return PLAYER_ERR;
        }
    }

    uint16_t magic = 0;
    fseek(test, 0, SEEK_SET);
    fread(&magic, 2, 1, test);
    fclose(test);
    if (magic == 0x8b1f) {
        ESP_LOGI(TAG, "Compressed");
        Ui_StatusBar_SetExtract(true);
        tinfl_status u = Player_Unvgz(FilePath, Player_UnvgzReplaceOriginal);
        if (u != TINFL_STATUS_DONE) {
            //do this silly 0x12345678 thing to avoid blowing away any modal that was popped up in Player_Unvgz. yeah, i don't like it either...
            if (u != 0x12345678) modal_show_simple(TAG, "VGZ Extraction Failed", "An error occurred while extracting this VGZ file. The file may be corrupt.", LV_SYMBOL_OK " OK");

            //get the last track's info off of nowplaying
            Player_Gd3_Title[0] = 0;
            Player_Gd3_Game[0] = 0;
            Player_Gd3_Author[0] = 0;
            Player_Info.TotalSamples = 0;
            Player_Info.LoopOffset = 0;
            Player_Info.LoopSamples = 0;
            Ui_NowPlaying_DataAvail = true;

            Ui_StatusBar_SetExtract(false); //we won't make it to the one below

            return PLAYER_ERR;
        }
        if (Player_UnvgzReplaceOriginal) {
            if (*(FilePath+(strlen(FilePath)-1)) == 'z' || *(FilePath+(strlen(FilePath)-1)) == 'Z') {
                *(FilePath+(strlen(FilePath)-1)) -= 0x0d;
            }
        } else {
            OpenFilePath = unvgztmp;
        }
        Ui_StatusBar_SetExtract(false);
    } else if (magic == 0x6756) {
        ESP_LOGI(TAG, "Uncompressed");
    } else {
        ESP_LOGI(TAG, "Unknown");
        return PLAYER_ERR;
    }
    ESP_LOGI(TAG, "opening files");
    Player_VgmFile = fopen(OpenFilePath, "r");
    Player_PcmFile = fopen(OpenFilePath, "r");
    Player_DsFindFile = fopen(OpenFilePath, "r");
    Player_DsFillFile = fopen(OpenFilePath, "r");
    Driver_Opna_PcmUploadFile = fopen(OpenFilePath, "r");
    if (!Player_VgmFile || !Player_PcmFile || !Player_DsFillFile || !Player_DsFindFile || !Driver_Opna_PcmUploadFile) {
        file_error(false);
        if (Player_VgmFile) fclose(Player_VgmFile);
        if (Player_PcmFile) fclose(Player_PcmFile);
        if (Player_DsFindFile) fclose(Player_DsFindFile);
        if (Player_DsFillFile) fclose(Player_DsFillFile);
        if (Driver_Opna_PcmUploadFile) fclose(Driver_Opna_PcmUploadFile);
        return PLAYER_ERR | PLAYER_ERR_INTERNAL;
    }
    fseek(Player_VgmFile, 0, SEEK_SET);
    fseek(Player_DsFindFile, 0, SEEK_SET);

    ESP_LOGI(TAG, "parsing header");
    VgmParseHeader(Player_VgmFile, &Player_Info);
    if (ferror(Player_VgmFile)) { //good time for a check
        file_error(false);
        return PLAYER_ERR | PLAYER_ERR_INTERNAL;
    }

    ESP_LOGI(TAG, "VGM VER = %d", Player_Info.Version);

    //known bad vgm header checksum
    fseek(Player_VgmFile, 0, SEEK_SET);
    fread(Driver_PcmBuf, 1, Player_Info.DataOffset, Player_VgmFile);
    uint32_t headercrc = 0;
    headercrc = crc32_le(headercrc, Driver_PcmBuf, Player_Info.DataOffset);
    uint32_t eof = Player_Info.EofOffset+4;
    if (Player_Info.Gd3Offset && eof-Player_Info.Gd3Offset <= sizeof(Driver_PcmBuf)) {
        fseek(Player_VgmFile, Player_Info.Gd3Offset, SEEK_SET);
        fread(Driver_PcmBuf, 1, eof-Player_Info.Gd3Offset, Player_VgmFile);
        headercrc = crc32_le(headercrc, Driver_PcmBuf, eof-Player_Info.Gd3Offset);
    }
    ESP_LOGI(TAG, "File header CRC = 0x%08x", headercrc);
    bool bad = false;
    for (uint32_t i=0;i<sizeof(known_bad_testreg_vgms)/sizeof(uint32_t);i++) {
        if (headercrc == known_bad_testreg_vgms[i]) {
            ESP_LOGW(TAG, "Known bad vgm - test register");
            bad = true;
            break;
        }
    }
    for (uint32_t i=0;i<sizeof(known_bad_other_vgms)/sizeof(uint32_t);i++) {
        if (headercrc == known_bad_other_vgms[i]) {
            ESP_LOGW(TAG, "Known bad vgm - other reason");
            modal_show_simple(TAG, "Warning", "This VGM is known to be incorrectly logged or have other playback issues. Please report this issue to the pack author as it is not a MegaGRRL bug.", LV_SYMBOL_OK " OK");
            break;
        }
    }

    if (ferror(Player_VgmFile)) { //good time for a check
        file_error(false);
        return PLAYER_ERR | PLAYER_ERR_INTERNAL;
    }

    Gd3Descriptor_t desc;
    Gd3ParseDescriptor(Player_VgmFile, &Player_Info, &desc);
    if (desc.parsed) {
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_TRACK_EN, &Player_Gd3_Title[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_GAME_EN, &Player_Gd3_Game[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_AUTHOR_EN, &Player_Gd3_Author[0], PLAYER_GD3_FIELD_SIZES);
        if (ferror(Player_VgmFile)) { //better check before telling nowplaying to use garbage gd3 data...
            file_error(false);
            return PLAYER_ERR | PLAYER_ERR_INTERNAL;
        }
    } else {
        Player_Gd3_Title[0] = 0;
        Player_Gd3_Game[0] = 0;
        Player_Gd3_Author[0] = 0;
    }

    Ui_NowPlaying_DataAvail = true;

    /* todo here:
    * set driver clock rate for wait scaling
    */

    ESP_LOGI(TAG, "vgm rate: %d", Player_Info.Rate);

    //read in the first 256 bytes of the file, then go through it and figure out how many chip clocks are specified
    fseek(Player_VgmFile, 0, SEEK_SET);
    fread(Driver_PcmBuf, 1, 256, Player_VgmFile);
    uint8_t clocks_specified = 0;
    uint8_t clocks_specified_pcm = 0;
    uint8_t clocks_used = 0;
    CHECK_CLOCK(0x0c, false); //sn76489
    CHECK_CLOCK(0x10, false); //ym2413
    if (Player_Info.Version >= 110) {
        CHECK_CLOCK(0x2c, false); //opn2
        CHECK_CLOCK(0x30, false); //opm
        if (Player_Info.Version >= 151) {
            CHECK_CLOCK(0x38, true); //spcm
            CHECK_CLOCK(0x40, true); //rf5c68
            CHECK_CLOCK(0x44, false); //opn
            CHECK_CLOCK(0x48, false); //opna
            CHECK_CLOCK(0x4c, false); //opnb
            CHECK_CLOCK(0x50, false); //opl2
            CHECK_CLOCK(0x54, false); //opl
            CHECK_CLOCK(0x58, false); //y8950
            CHECK_CLOCK(0x5c, false); //opl3
            CHECK_CLOCK(0x60, false); //ymf278b
            CHECK_CLOCK(0x64, false); //ymf271
            CHECK_CLOCK(0x68, false); //ymz280b
            CHECK_CLOCK(0x6c, true); //rf5c164
            CHECK_CLOCK(0x70, true); //32x pwm
            CHECK_CLOCK(0x74, false); //ay38910
            if (Player_Info.Version >= 161) {
                CHECK_CLOCK(0x80, false); //dmg
                CHECK_CLOCK(0x84, false); //nes apu
                CHECK_CLOCK(0x88, true); //multipcm
                CHECK_CLOCK(0x8c, true); //upd7759
                CHECK_CLOCK(0x90, true); //msm6258
                CHECK_CLOCK(0x96, true); //msm6295
                CHECK_CLOCK(0x9c, false); //k051649 k052539
                CHECK_CLOCK(0xa0, false); //k054539
                CHECK_CLOCK(0xa4, true); //huc6280
                CHECK_CLOCK(0xa8, true); //c140
                CHECK_CLOCK(0xac, false); //k053260
                CHECK_CLOCK(0xb0, false); //pokey
                CHECK_CLOCK(0xb4, false); //qsound
                if (Player_Info.Version >= 171) {
                    CHECK_CLOCK(0xb8, false); //scsp
                    CHECK_CLOCK(0xc0, false); //wonderswan
                    CHECK_CLOCK(0xc4, false); //virtual boy
                    CHECK_CLOCK(0xc8, false); //saa1099
                    CHECK_CLOCK(0xcc, false); //es5503
                    CHECK_CLOCK(0xd0, false); //es5505 5506
                    CHECK_CLOCK(0xd8, false); //x1-010
                    CHECK_CLOCK(0xdc, false); //c352
                    CHECK_CLOCK(0xe0, false); //ga20
                }
            }
        }
    }
    ESP_LOGI(TAG, "clocks specified: %d", clocks_specified);

    //todo: more graceful handling of dual chip bit 30. really, more graceful handling of this whole thing...
    if (Driver_DetectedMod == MEGAMOD_NONE) {
        ESP_LOGI(TAG, "MegaMod: none");
        uint32_t DcsgClock = 0;
        uint32_t FmClock = 0;
        fseek(Player_VgmFile, 0x0c, SEEK_SET);
        fread(&DcsgClock, 4, 1, Player_VgmFile);
        fseek(Player_VgmFile, 0x2c, SEEK_SET);
        fread(&FmClock, 4, 1, Player_VgmFile);
        FmClock &= ~(1<<31); //3438 bit, ffs...
        if (DcsgClock & (1<<30)) {
            ESP_LOGW(TAG, "Only one DCSG supported! Writes to the second chip will be dropped");
            DcsgClock &= ~(1<<30);
        }
        if (DcsgClock & (1<<31)) {
            ESP_LOGW(TAG, "VGM specifies T6W28!");
            DcsgClock &= ~(1<<31);
        }
        ESP_LOGI(TAG, "Clocks from vgm: dcsg %d, fm %d", DcsgClock, FmClock);

        if (!DcsgClock && !FmClock) {
            if (Player_Info.Version >= 151) {
                ESP_LOGW(TAG, "Missing OPN2 and DCSG clocks, attempting OPN...");
                fseek(Player_VgmFile, 0x44, SEEK_SET);
                fread(&FmClock, 4, 1, Player_VgmFile);
                if (!FmClock) {
                    ESP_LOGW(TAG, "Attempting OPNA...");
                    fread(&FmClock, 4, 1, Player_VgmFile);
                    if (!FmClock) {
                        ESP_LOGW(TAG, "It's not OPNA either");
                    } else {
                        clocks_used++;
                    }
                } else {
                    FmClock <<= 1;
                    clocks_used++;
                }
            } else {
                ESP_LOGW(TAG, "Not checking OPN/OPNA clocks, VGM too old");
            }
        } else {
            clocks_used++;
        }
        if (FmClock & 0x80000000) {
            ESP_LOGE(TAG, "Only one FM chip supported !!");
        }

        if (DcsgClock) clocks_used++;

        //handle (some of) dcsg special flags
        uint8_t dcsg_sr_width = 16;
        uint8_t dcsg_flags = 0;
        if (Player_Info.Version >= 110) {
            fseek(Player_VgmFile, 0x2a, SEEK_SET);
            fread(&dcsg_sr_width, 1, 1, Player_VgmFile);
            if (dcsg_sr_width < 15 || dcsg_sr_width > 17) {
                ESP_LOGE(TAG, "Invalid DCSG SR width (%d)!!! Assuming 16", dcsg_sr_width);
                dcsg_sr_width = 16;
            }
            if (Player_Info.Version >= 151) {
                fread(&dcsg_flags, 1, 1, Player_VgmFile);
            }
        }
        ESP_LOGI(TAG, "DCSG SR width = %d", dcsg_sr_width);
        ESP_LOGI(TAG, "DCSG flags = 0x%02x", dcsg_flags);
        Driver_VgmDcsgSrWidth = dcsg_sr_width;
        Driver_VgmDcsgSpecialFreq0 = (dcsg_flags & (1<<0)) == 0;

        if (DcsgClock == 0) DcsgClock = 3579545;
        else if (DcsgClock < 1000000) DcsgClock = 1000000;
        else if (DcsgClock > 4100000) DcsgClock = 4100000;
        if (FmClock == 0) FmClock = 7670453;
        else if (FmClock < 7000000) FmClock = 7000000;
        else if (FmClock > 8300000) FmClock = 8300000;
        ESP_LOGI(TAG, "Clocks clamped: dcsg %d, fm %d", DcsgClock, FmClock);
        Clk_Set(CLK_FM, FmClock);
        Clk_Set(CLK_DCSG, DcsgClock);
    } else if (Driver_DetectedMod == MEGAMOD_OPLLDCSG) {
        ESP_LOGI(TAG, "MegaMod: OPLL+DCSG");
        Clk_Set(CLK_DCSG, 0);
        uint32_t opll = 0;
        uint32_t dcsg = 0;
        fseek(Player_VgmFile, 0x0c, SEEK_SET);
        fread(&dcsg,4,1,Player_VgmFile);
        fread(&opll,4,1,Player_VgmFile);
        if ((dcsg & 0x40000000) || (opll & 0x40000000)) {
            ESP_LOGW(TAG, "Only one of each chip supported !!");
        } else if (dcsg && opll && (dcsg != opll)) {
            ESP_LOGW(TAG, "Different clocks not supported !!");
        }
        ESP_LOGI(TAG, "Clock from vgm: DCSG: %d, OPLL: %d", dcsg, opll);
        if (dcsg) clocks_used++;
        if (opll) clocks_used++;
        if (dcsg == 0) dcsg = opll;
        if (dcsg < 3000000) dcsg = 3000000;
        else if (dcsg > 4100000) dcsg = 4100000;
        ESP_LOGI(TAG, "Clock clamped: %d", dcsg);
        Clk_Set(CLK_FM, dcsg);
    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
        ESP_LOGI(TAG, "MegaMod: OPNA");
        Clk_Set(CLK_DCSG, 0);
        uint32_t opna = 0;
        uint32_t opn = 0;
        uint32_t ay = 0;
        uint32_t opn2 = 0;
        fseek(Player_VgmFile, 0x2c, SEEK_SET);
        fread(&opn2, 4, 1, Player_VgmFile);
        opn2 &= ~(1<<31); //3438 bit
        if (Player_Info.Version >= 151) {
            fseek(Player_VgmFile, 0x44, SEEK_SET);
            fread(&opn,4,1,Player_VgmFile);
            fread(&opna,4,1,Player_VgmFile);
            fseek(Player_VgmFile, 0x74, SEEK_SET);
            fread(&ay,4,1,Player_VgmFile);
        }
        if (opna & (1<<30) || opn2 & (1<<30) || ay & (1<<30)) {
            ESP_LOGE(TAG, "Only one opna/opn2/ay supported !!");
        }
        if (!opna && opn2) {
            ESP_LOGW(TAG, "OPNA MegaMod: No opna clock, using opn2");
            opna = opn2;
        }
        if (opna) {
            clocks_used++;
            ESP_LOGI(TAG, "Clock from vgm: %d", opna);
            if (opna < 6000000) opna = 6000000;
            else if (opna > 10000000) opna = 10000000;
            ESP_LOGI(TAG, "Clock clamped: %d", opna);
            Clk_Set(CLK_FM, opna);
        } else if (opn) {
            clocks_used++;
            if (opn & (1<<30)) clocks_used++;
            Clk_Set(CLK_FM, (opn&~(1<<30))<<1); //todo clamping
        } else if (ay) {
            clocks_used++;
            ESP_LOGI(TAG, "Clock from vgm: AY %d", ay<<2);
            Clk_Set(CLK_FM, (ay&~(1<<30))<<2);
        }
    } else if (Driver_DetectedMod == MEGAMOD_OPL3) {
        uint32_t opl = 0;
        uint32_t opl2 = 0;
        uint32_t opl3 = 0;
        fseek(Player_VgmFile, 0x50, SEEK_SET);
        fread(&opl2,4,1,Player_VgmFile);
        fread(&opl,4,1,Player_VgmFile);
        fseek(Player_VgmFile, 0x5c, SEEK_SET);
        fread(&opl3,4,1,Player_VgmFile);
        if ((opl & 0x40000000) || (opl2 & 0x40000000) || (opl3 & 0x40000000)) {
            ESP_LOGW(TAG, "Only one of each chip supported FOR NOW !!");
        }
        //todo make this shit work like the above with clamping etc
        if (opl3) {
            clocks_used++;
            ESP_LOGI(TAG, "set %d", opl3);
            Clk_Set(CLK_FM, opl3);
        } else if (opl2) {
            clocks_used++;
            opl2 *= 4;
            ESP_LOGI(TAG, "set %d", opl2);
            Clk_Set(CLK_FM, opl2);
        } else if (opl) {
            clocks_used++;
            opl *= 4;
            ESP_LOGI(TAG, "set %d", opl);
            Clk_Set(CLK_FM, opl);
        }
    } else if (Driver_DetectedMod == MEGAMOD_OPM) {
        ESP_LOGI(TAG, "MegaMod: OPM");
        Clk_Set(CLK_DCSG, 0);
        uint32_t opm = 0;
        fseek(Player_VgmFile, 0x30, SEEK_SET);
        fread(&opm,4,1,Player_VgmFile);
        if (opm & 0x40000000) {
            ESP_LOGW(TAG, "Only one opm supported !!");
        }
        clocks_used++;
        ESP_LOGI(TAG, "Clock from vgm: %d", opm);
        if (opm < 3000000) opm = 3000000;
        else if (opm > 5000000) opm = 5000000;
        ESP_LOGI(TAG, "Clock clamped: %d", opm);
        Clk_Set(CLK_FM, opm);
    }
    if (ferror(Player_VgmFile)) { //final check after all the clock stuff
        file_error(false);
        return PLAYER_ERR | PLAYER_ERR_INTERNAL;
    }

    if (clocks_specified > clocks_used) {
        ESP_LOGW(TAG, "Can't play this vgm correctly - %d clocks specified, %d used", clocks_specified, clocks_used);
        if (Player_SkipUnsupported) {
            ESP_LOGW(TAG, "Not playing as requested by option");
            modal_show_simple(TAG, "Warning", "One or more tracks were skipped because they use sound chips not supported by this hardware. This can be disabled in Settings.", LV_SYMBOL_OK " OK");
            return PLAYER_ERR | PLAYER_UNSUPPORTED_CHIPS;
        }
    }

    ESP_LOGI(TAG, "Signalling driver reset");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);

    ESP_LOGI(TAG, "Starting dacstreams");
    bool ret;
    ret = DacStream_Start(Player_DsFindFile, Player_DsFillFile, &Player_Info);
    if (!ret) {
        ESP_LOGE(TAG, "Dacstreams failed to start !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    ESP_LOGI(TAG, "Starting loader");
    ret = Loader_Start(Player_VgmFile, Player_PcmFile, &Player_Info, bad);
    if (!ret) {
        ESP_LOGE(TAG, "Loader failed to start !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    ESP_LOGI(TAG, "Wait for loader to start...");
    EventBits_t bits;
    bits = xEventGroupWaitBits(Loader_Status, LOADER_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & LOADER_RUNNING) == 0) {
        ESP_LOGE(TAG, "Loader start timeout !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    ESP_LOGI(TAG, "Wait for loader buffer OK...");
    bits = xEventGroupWaitBits(Loader_BufStatus, LOADER_BUF_OK | LOADER_BUF_FULL, false, false, pdMS_TO_TICKS(10000));
    if ((bits & (LOADER_BUF_OK | LOADER_BUF_FULL)) == 0) {
        ESP_LOGE(TAG, "Loader buffer timeout !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    ESP_LOGI(TAG, "Wait for dacstream fill task...");
    bits = xEventGroupWaitBits(DacStream_FillStatus, DACSTREAM_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DACSTREAM_RUNNING) == 0) {
        ESP_LOGE(TAG, "Dacstream fill task start timeout !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    ESP_LOGI(TAG, "Wait for driver to reset...");
    bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RESET_ACK) == 0) {
        ESP_LOGE(TAG, "Driver reset ack timeout !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }
    xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK);

    ESP_LOGI(TAG, "Request driver start...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);

    ESP_LOGI(TAG, "Wait for driver to start...");
    bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RUNNING) == 0) {
        ESP_LOGE(TAG, "Driver start timeout !!");
        return PLAYER_ERR | PLAYER_ERR_SYS;
    }

    Ui_NowPlaying_DriverRunning = true;

    ESP_LOGI(TAG, "Driver started !!");

    return 0;
}

static bool Player_StopTrack() {
    Ui_NowPlaying_DataAvail = false;

    ESP_LOGI(TAG, "Requesting driver stop...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_STOP_REQUEST);

    ESP_LOGI(TAG, "Waiting for driver to stop...");
    while (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) vTaskDelay(pdMS_TO_TICKS(10));

    Ui_NowPlaying_DriverRunning = false;

    ESP_LOGI(TAG, "Signalling driver reset");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);

    ESP_LOGI(TAG, "Requesting loader stop...");
    bool ret = Loader_Stop();
    if (!ret) {
        ESP_LOGE(TAG, "Loader stop timeout !!");
        fclose(Player_VgmFile);
        fclose(Player_PcmFile);
        fclose(Player_DsFindFile);
        fclose(Player_DsFillFile);
        fclose(Driver_Opna_PcmUploadFile);
        return false;
    }

    ESP_LOGI(TAG, "Requesting dacstream stop...");
    ret = DacStream_Stop();
    if (!ret) {
        ESP_LOGE(TAG, "Dacstream stop timeout !!");
        fclose(Player_VgmFile);
        fclose(Player_PcmFile);
        fclose(Player_DsFindFile);
        fclose(Player_DsFillFile);
        fclose(Driver_Opna_PcmUploadFile);
        return false;
    }

    ESP_LOGI(TAG, "Wait for driver to reset...");
    EventBits_t bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RESET_ACK) == 0) {
        ESP_LOGE(TAG, "Driver reset ack timeout !!");
        return false;
    }
    xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK);

    fclose(Player_VgmFile);
    fclose(Player_PcmFile);
    fclose(Player_DsFindFile);
    fclose(Player_DsFillFile);
    fclose(Driver_Opna_PcmUploadFile);

    return true;
}
