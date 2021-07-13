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

EventGroupHandle_t Player_Status;
StaticEventGroup_t Player_StatusBuf;

char Player_Gd3_Title[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Game[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Author[PLAYER_GD3_FIELD_SIZES+1];

const static char* unvgztmp = "/sd/.mega/unvgz.tmp";

static bool Player_StartTrack(char *FilePath);
static bool Player_StopTrack();

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
        if (xTaskNotifyWait(0,0xffffffff, &notif, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (notif == PLAYER_NOTIFY_START_RUNNING) {
                ESP_LOGI(TAG, "control: start requested");
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RAN_OUT);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if ((xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) == 0) {
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING); //do this now, Player_StartTrack could take longer than the timeout of the task waiting for this event.
                    QueueSetupEntry(false, true);
                    Player_StartTrack(&QueuePlayingFilename[0]);
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
                    Player_StartTrack(QueuePlayingFilename);
                } else {
                    ESP_LOGI(TAG, "next track failed");
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RAN_OUT);
                }
            } else if (notif == PLAYER_NOTIFY_PREV) {
                ESP_LOGI(TAG, "control: prev requested");
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if (Driver_FirstWait || (Driver_Sample < 3*44100)) { //actually change track
                    ESP_LOGI(TAG, "within 3 second window");
                    if (Player_PrevTrk(true)) {
                        ESP_LOGI(TAG, "prev track proceeding");
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                        Player_StartTrack(QueuePlayingFilename);
                    } else {
                        ESP_LOGI(TAG, "prev track failed");
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    }
                } else { //just restart
                    ESP_LOGI(TAG, "outside 3 second window, just restarting track");
                    Player_StopTrack();
                    Player_StartTrack(&QueuePlayingFilename[0]);
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
                Player_StartTrack(QueuePlayingFilename);
            } else {
                ESP_LOGI(TAG, "next track failed");
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_RAN_OUT);
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

static bool Player_StartTrack(char *FilePath) {
    const char *OpenFilePath = FilePath;

    ESP_LOGI(TAG, "Checking file type of %s", FilePath);
    FILE *test = fopen(FilePath, "r");
    if (!test) {
        if (*(FilePath+(strlen(FilePath)-1)) == 'z' || *(FilePath+(strlen(FilePath)-1)) == 'Z') {
            ESP_LOGW(TAG, "vgz doesn't exist, let's try vgm");
            *(FilePath+(strlen(FilePath)-1)) -= 0x0d;
            test = fopen(FilePath, "r");
            if (!test) {
                ESP_LOGE(TAG, "vgm doesn't exist either");
                return false;
            }
        } else {
            ESP_LOGE(TAG, "file doesn't exist");
            return false;
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

            return false;
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
        return false;
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
        return false;
    }
    fseek(Player_VgmFile, 0, SEEK_SET);
    fseek(Player_DsFindFile, 0, SEEK_SET);

    ESP_LOGI(TAG, "parsing header");
    VgmParseHeader(Player_VgmFile, &Player_Info);
    if (ferror(Player_VgmFile)) { //good time for a check
        file_error(false);
        return false;
    }

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
        return false;
    }

    Gd3Descriptor_t desc;
    Gd3ParseDescriptor(Player_VgmFile, &Player_Info, &desc);
    if (desc.parsed) {
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_TRACK_EN, &Player_Gd3_Title[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_GAME_EN, &Player_Gd3_Game[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_AUTHOR_EN, &Player_Gd3_Author[0], PLAYER_GD3_FIELD_SIZES);
        if (ferror(Player_VgmFile)) { //better check before telling nowplaying to use garbage gd3 data...
            file_error(false);
            return false;
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

    //todo: improve this, check that the vgm is for the chips we have, more graceful handling of dual chip bit 30
    if (Driver_DetectedMod == MEGAMOD_NONE) {
        ESP_LOGI(TAG, "MegaMod: none");
        uint32_t PsgClock = 0;
        uint32_t FmClock = 0;
        fseek(Player_VgmFile, 0x0c, SEEK_SET);
        fread(&PsgClock, 4, 1, Player_VgmFile);
        fseek(Player_VgmFile, 0x2c, SEEK_SET);
        fread(&FmClock, 4, 1, Player_VgmFile);
        FmClock &= ~(1<<31); //3438 bit, ffs...
        if (PsgClock & (1<<30)) {
            ESP_LOGW(TAG, "Only one PSG supported! Writes to the second chip will be dropped");
            PsgClock &= ~(1<<30);
        }
        ESP_LOGI(TAG, "Clocks from vgm: psg %d, fm %d", PsgClock, FmClock);

        if (!PsgClock && !FmClock) {
            ESP_LOGW(TAG, "Missing OPN2 and PSG clocks, attempting OPN...");
            fseek(Player_VgmFile, 0x44, SEEK_SET);
            fread(&FmClock, 4, 1, Player_VgmFile);
            if (!FmClock) {
                ESP_LOGW(TAG, "Attempting OPNA...");
                fread(&FmClock, 4, 1, Player_VgmFile);
                if (!FmClock) {
                    ESP_LOGW(TAG, "It's not OPNA either");
                }
            } else {
                FmClock <<= 1;
            }
        }
        if (FmClock & 0x80000000) {
            ESP_LOGE(TAG, "Only one FM chip supported !!");
        }

        if (PsgClock == 0) PsgClock = 3579545;
        else if (PsgClock < 1000000) PsgClock = 1000000;
        else if (PsgClock > 4100000) PsgClock = 4100000;
        if (FmClock == 0) FmClock = 7670453;
        else if (FmClock < 7000000) FmClock = 7000000;
        else if (FmClock > 8300000) FmClock = 8300000;
        ESP_LOGI(TAG, "Clocks clamped: psg %d, fm %d", PsgClock, FmClock);
        Clk_Set(CLK_FM, FmClock);
        Clk_Set(CLK_PSG, PsgClock);
    } else if (Driver_DetectedMod == MEGAMOD_OPLLPSG) {
        ESP_LOGI(TAG, "MegaMod: OPLL+PSG");
        Clk_Set(CLK_PSG, 0);
        uint32_t opll = 0;
        uint32_t psg = 0;
        fseek(Player_VgmFile, 0x0c, SEEK_SET);
        fread(&psg,4,1,Player_VgmFile);
        fread(&opll,4,1,Player_VgmFile);
        if ((psg & 0x80000000) || (opll & 0x80000000)) {
            ESP_LOGW(TAG, "Only one of each chip supported !!");
        } else if (psg && opll && (psg != opll)) {
            ESP_LOGW(TAG, "Different clocks not supported !!");
        }
        ESP_LOGI(TAG, "Clock from vgm: PSG: %d, OPLL: %d", psg, opll);
        if (psg == 0) psg = opll;
        if (psg < 3000000) psg = 3000000;
        else if (psg > 4100000) psg = 4100000;
        ESP_LOGI(TAG, "Clock clamped: %d", psg);
        Clk_Set(CLK_FM, psg);
    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
        ESP_LOGI(TAG, "MegaMod: OPNA");
        Clk_Set(CLK_PSG, 0);
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
        if (opna & (1<<30) || opn2 & (1<<30)) {
            ESP_LOGE(TAG, "Only one opna/opn2 supported !!");
        }
        if (!opna && opn2) {
            ESP_LOGW(TAG, "OPNA MegaMod: No opna clock, using opn2");
            opna = opn2;
        }
        if (opna) {
            ESP_LOGI(TAG, "Clock from vgm: %d", opna);
            if (opna < 6000000) opna = 6000000;
            else if (opna > 10000000) opna = 10000000;
            ESP_LOGI(TAG, "Clock clamped: %d", opna);
            Clk_Set(CLK_FM, opna);
        } else if (opn) {
            Clk_Set(CLK_FM, opn<<1); //todo clamping
        } else if (ay) {
            ESP_LOGI(TAG, "Clock from vgm: AY %d", ay<<2);
            Clk_Set(CLK_FM, ay<<2);
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
        if ((opl & 0x80000000) || (opl2 & 0x80000000) || (opl3 & 0x80000000)) {
            ESP_LOGW(TAG, "Only one of each chip supported FOR NOW !!");
        }
        //todo make this shit work like the above with clamping etc
        if (opl3) {
            ESP_LOGI(TAG, "set %d", opl3);
            Clk_Set(CLK_FM, opl3);
        } else if (opl2) {
            opl2 *= 4;
            ESP_LOGI(TAG, "set %d", opl2);
            Clk_Set(CLK_FM, opl2);
        } else if (opl) {
            opl *= 4;
            ESP_LOGI(TAG, "set %d", opl);
            Clk_Set(CLK_FM, opl);
        }
    } else if (Driver_DetectedMod == MEGAMOD_OPM) {
        ESP_LOGI(TAG, "MegaMod: OPM");
        Clk_Set(CLK_PSG, 0);
        uint32_t opm = 0;
        fseek(Player_VgmFile, 0x30, SEEK_SET);
        fread(&opm,4,1,Player_VgmFile);
        if (opm & 0x80000000) {
            ESP_LOGW(TAG, "Only one opm supported !!");
        }
        ESP_LOGI(TAG, "Clock from vgm: %d", opm);
        if (opm < 3000000) opm = 3000000;
        else if (opm > 5000000) opm = 5000000;
        ESP_LOGI(TAG, "Clock clamped: %d", opm);
        Clk_Set(CLK_FM, opm);
    }
    if (ferror(Player_VgmFile)) { //final check after all the clock stuff
        file_error(false);
        return false;
    }

    ESP_LOGI(TAG, "Signalling driver reset");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);

    ESP_LOGI(TAG, "Starting dacstreams");
    bool ret;
    ret = DacStream_Start(Player_DsFindFile, Player_DsFillFile, &Player_Info);
    if (!ret) {
        ESP_LOGE(TAG, "Dacstreams failed to start !!");
        return false;
    }

    ESP_LOGI(TAG, "Starting loader");
    ret = Loader_Start(Player_VgmFile, Player_PcmFile, &Player_Info, bad);
    if (!ret) {
        ESP_LOGE(TAG, "Loader failed to start !!");
        return false;
    }

    ESP_LOGI(TAG, "Wait for loader to start...");
    EventBits_t bits;
    bits = xEventGroupWaitBits(Loader_Status, LOADER_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & LOADER_RUNNING) == 0) {
        ESP_LOGE(TAG, "Loader start timeout !!");
        return false;
    }

    ESP_LOGI(TAG, "Wait for loader buffer OK...");
    bits = xEventGroupWaitBits(Loader_BufStatus, LOADER_BUF_OK | LOADER_BUF_FULL, false, false, pdMS_TO_TICKS(10000));
    if ((bits & (LOADER_BUF_OK | LOADER_BUF_FULL)) == 0) {
        ESP_LOGE(TAG, "Loader buffer timeout !!");
        return false;
    }

    ESP_LOGI(TAG, "Wait for dacstream fill task...");
    bits = xEventGroupWaitBits(DacStream_FillStatus, DACSTREAM_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DACSTREAM_RUNNING) == 0) {
        ESP_LOGE(TAG, "Dacstream fill task start timeout !!");
        return false;
    }

    ESP_LOGI(TAG, "Wait for driver to reset...");
    bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RESET_ACK) == 0) {
        ESP_LOGE(TAG, "Driver reset ack timeout !!");
        return false;
    }
    xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK);

    ESP_LOGI(TAG, "Request driver start...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);

    ESP_LOGI(TAG, "Wait for driver to start...");
    bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RUNNING) == 0) {
        ESP_LOGE(TAG, "Driver start timeout !!");
        return false;
    }

    Ui_NowPlaying_DriverRunning = true;

    ESP_LOGI(TAG, "Driver started !!");

    return true;
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
