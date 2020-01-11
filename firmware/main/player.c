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

static const char* TAG = "Player";

FILE *Player_VgmFile;
FILE *Player_PcmFile;
FILE *Player_DsFindFile;
FILE *Player_DsFillFile;
VgmInfoStruct_t Player_Info;
uint32_t notif = 0;

volatile uint8_t Player_SetLoopCount = 2;
volatile uint8_t Player_LoopCount = 2;
volatile RepeatMode_t Player_RepeatMode = REPEAT_ALL;

EventGroupHandle_t Player_Status;
StaticEventGroup_t Player_StatusBuf;

char Player_Gd3_Title[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Game[PLAYER_GD3_FIELD_SIZES+1];
char Player_Gd3_Author[PLAYER_GD3_FIELD_SIZES+1];

bool stopped = true;

bool Player_NextTrk(bool UserSpecified) { //returns true if there is now a track playing
    Player_StopTrack();
    if (!UserSpecified && Player_RepeatMode == REPEAT_ONE) {
        //nothing to do - just start the same track again
    } else {
        bool end = (QueuePosition == QueueLength-1);
        if (end) {
            if (Player_RepeatMode == REPEAT_NONE) {
                return false; //nothing more to play
            } else if (Player_RepeatMode == REPEAT_ALL) {
                QueuePosition = 0;
                QueueSetupEntry(false);
            } else if (Player_RepeatMode == REPEAT_ONE) { //if it's repeat one and they manually pressed next
                return false; //nothing more to play
            }
        } else { //not the end of the queue, just load the next track
            QueueNext();
            QueueSetupEntry(false);
        }
    }
    Player_StartTrack(&QueuePlayingFilename[0]);
    return true;
}

bool Player_PrevTrk(bool UserSpecified) { //returns true if there is now a track playing
    Player_StopTrack();
    if (!UserSpecified && Player_RepeatMode == REPEAT_ONE) {
        //nothing to do - just start the same track again
    } else {
        bool end = (QueuePosition == 0);
        if (end) {
            if (Player_RepeatMode == REPEAT_NONE) {
                return false; //nothing more to play
            } else if (Player_RepeatMode == REPEAT_ALL) {
                QueuePosition = QueueLength-1;
                QueueSetupEntry(false);
            } else if (Player_RepeatMode == REPEAT_ONE) { //if it's repeat one and they manually pressed prev
                return false; //nothing more to play
            }
        } else { //not the end of the queue, just load the prev track
            QueuePrev();
            QueueSetupEntry(false);
        }
    }
    Player_StartTrack(&QueuePlayingFilename[0]);
    return true;
}

void Player_Main() {
    ESP_LOGI(TAG, "Task start");

    while (1) {
        if (xTaskNotifyWait(0,0xffffffff, &notif, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (notif == PLAYER_NOTIFY_START_RUNNING) {
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if ((xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) == 0) {
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING); //do this now, Player_StartTrack could take longer than the timeout of the task waiting for this event.
                    QueueSetupEntry(false);
                    Player_StartTrack(&QueuePlayingFilename[0]);
                } else {
                    //already running. yikes!
                }
            } else if (notif == PLAYER_NOTIFY_STOP_RUNNING) {
                if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) {
                    Player_StopTrack();
                }
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
            } else if (notif == PLAYER_NOTIFY_NEXT) {
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if (Player_NextTrk(true)) {
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                } else {
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                }
            } else if (notif == PLAYER_NOTIFY_PREV) {
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
                if (Driver_Sample < 3*44100) { //actually change track
                    if (Player_PrevTrk(true)) {
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    } else {
                        xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                        xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                    }
                } else { //just restart
                    Player_StopTrack();
                    Player_StartTrack(&QueuePlayingFilename[0]);
                    xEventGroupSetBits(Player_Status, PLAYER_STATUS_RUNNING);
                    xEventGroupClearBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
                }
            } else if (notif == PLAYER_NOTIFY_PAUSE) {
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
                if ((xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) == 0) {
                    ESP_LOGI(TAG, "Request driver resume...");
                    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESUME_REQUEST);
                    ESP_LOGI(TAG, "Wait for driver to resume...");
                    if ((xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000)) & DRIVER_EVENT_RUNNING) == 0) {
                        ESP_LOGE(TAG, "Driver resume timeout !!");
                        return false;
                    }
                }
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
            }
        } else { //no incoming notification

        }
        if ((xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) && (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_FINISHED)) { //still running, but driver reached end
            ESP_LOGI(TAG, "Driver finished, starting next track");
            xEventGroupClearBits(Player_Status, PLAYER_STATUS_PAUSED);
            if (Player_NextTrk(false)) {
                    //nothing to do, i don't think...
            } else {
                xEventGroupClearBits(Player_Status, PLAYER_STATUS_RUNNING);
                xEventGroupSetBits(Player_Status, PLAYER_STATUS_NOT_RUNNING);
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

bool Player_StartTrack(char *FilePath) {
    Player_VgmFile = fopen(FilePath, "r");
    Player_PcmFile = fopen(FilePath, "r");
    Player_DsFindFile = fopen(FilePath, "r");
    Player_DsFillFile = fopen(FilePath, "r");
    fseek(Player_VgmFile, 0, SEEK_SET);
    fseek(Player_DsFindFile, 0, SEEK_SET);
    VgmParseHeader(Player_VgmFile, &Player_Info);

    Gd3Descriptor_t desc;
    Gd3ParseDescriptor(Player_VgmFile, &Player_Info, &desc);
    if (desc.parsed) {
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_TRACK_EN, &Player_Gd3_Title[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_GAME_EN, &Player_Gd3_Game[0], PLAYER_GD3_FIELD_SIZES);
        Gd3GetStringChars(Player_VgmFile, &desc, GD3STRING_AUTHOR_EN, &Player_Gd3_Author[0], PLAYER_GD3_FIELD_SIZES);
    } else {
        Player_Gd3_Title[0] = 0;
        Player_Gd3_Game[0] = 0;
        Player_Gd3_Author[0] = 0;
    }

    Ui_NowPlaying_DataAvail = true;
    Ui_NowPlaying_NewTrack = true;

    /* todo here:
    * set driver clock rate for wait scaling
    */

    ESP_LOGI(TAG, "vgm rate: %d", Player_Info.Rate);

    uint32_t PsgClock = 0;
    uint32_t FmClock = 0;
    fseek(Player_VgmFile, 0x0c, SEEK_SET);
    fread(&PsgClock, 4, 1, Player_VgmFile);
    fseek(Player_VgmFile, 0x2c, SEEK_SET);
    fread(&FmClock, 4, 1, Player_VgmFile);
    ESP_LOGI(TAG, "Clocks from vgm: psg %d, fm %d", PsgClock, FmClock);

    if (PsgClock == 0) PsgClock = 3579545;
    else if (PsgClock < 3000000) PsgClock = 3000000;
    else if (PsgClock > 4100000) PsgClock = 4100000;
    if (FmClock == 0) FmClock = 7670453;
    else if (FmClock < 7000000) FmClock = 7000000;
    else if (FmClock > 8300000) FmClock = 8300000;
    ESP_LOGI(TAG, "Clocks clamped: psg %d, fm %d", PsgClock, FmClock);
    ESP_LOGI(TAG, "Enabling clocks");
    Clk_Set(CLK_FM, FmClock);
    Clk_Set(CLK_PSG, PsgClock);

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
    ret = Loader_Start(Player_VgmFile, Player_PcmFile, &Player_Info);
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

    ESP_LOGI(TAG, "Driver started !!");

    return true;
}

bool Player_StopTrack() {
    Ui_NowPlaying_DataAvail = false;

    ESP_LOGI(TAG, "Requesting driver stop...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_STOP_REQUEST);

    ESP_LOGI(TAG, "Waiting for driver to stop...");
    while (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) vTaskDelay(pdMS_TO_TICKS(10));

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

    return true;
}