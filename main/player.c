#include "player.h"
#include "esp_log.h"
#include "unistd.h"
#include "driver.h"
#include "ioexp.h"
#include "loader.h"
#include "volume.h"
#include "dacstream.h"

static const char* TAG = "Player";

FILE *Player_VgmFile;
FILE *Player_PcmFile;
FILE *Player_DsFindFile;
FILE *Player_DsFillFile;
VgmInfoStruct_t Player_Info;
uint32_t notif = 0;
bool stopped = true;

void Player_Main() {
    ESP_LOGI(TAG, "Task start");

    while (1) {
        if (xTaskNotifyWait(0,0xffffffff, &notif, pdMS_TO_TICKS(3000)) == pdTRUE) {
            if (notif == PLAYER_NOTIFY_PLAY) {
                
            }
        }
    }
}

bool Player_StartTrack(char *FilePath) {
    Player_VgmFile = fopen(FilePath, "r");
    Player_PcmFile = fopen(FilePath, "r");
    Player_DsFindFile = fopen(FilePath, "r");
    Player_DsFillFile = fopen(FilePath, "r");
    fseek(Player_VgmFile, 0, SEEK_SET);
    fseek(Player_DsFindFile, 0, SEEK_SET);
    VgmParseHeader(Player_VgmFile, &Player_Info);

    /* todo here:
    * set driver clock rate for wait scaling
    * manage clkgens, add clamping
    */

    ESP_LOGI(TAG, "Signalling driver reset");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);

    ESP_LOGI(TAG, "Amp powerup");
    IoExp_AmpControl(true); //hw takes maybe 500ms to power up, so do it now
    TickType_t Time_AmpOn = xTaskGetTickCount();


    ESP_LOGI(TAG, "Starting loader");
    bool ret;
    ret = Loader_Start(Player_VgmFile, Player_PcmFile, &Player_Info);
    if (!ret) {
        ESP_LOGE(TAG, "Loader failed to start !!");
        return false;
    }

    ESP_LOGI(TAG, "Starting dacstreams");
    ret = DacStream_Start(Player_DsFindFile, Player_DsFillFile, &Player_Info);
    if (!ret) {
        ESP_LOGE(TAG, "Dacstreams failed to start !!");
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

    ESP_LOGI(TAG, "Volume up");
    Volume_SetVolume(Volume_SystemVolume,Volume_SystemVolume);
    TickType_t Time_VolUp = xTaskGetTickCount();

    ESP_LOGI(TAG, "Wait for hw timeouts...");
    TickType_t Time_Cur;
    do {
        vTaskDelay(pdMS_TO_TICKS(10));
        Time_Cur = xTaskGetTickCount();
    } while (Time_Cur - Time_VolUp < pdMS_TO_TICKS(60) || Time_Cur - Time_AmpOn < pdMS_TO_TICKS(250));

    ESP_LOGI(TAG, "Request driver start...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);

    ESP_LOGI(TAG, "Wait for driver to start...");
    bits = xEventGroupWaitBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING, false, false, pdMS_TO_TICKS(3000));
    if ((bits & DRIVER_EVENT_RUNNING) == 0) {
        ESP_LOGE(TAG, "Driver start timeout !!");
        return false;
    }

    return true;
}

bool Player_StopTrack() {
    ESP_LOGI(TAG, "Requesting driver stop...");
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_STOP_REQUEST);

    ESP_LOGI(TAG, "Waiting for driver to stop...");
    while (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "Requesting loader stop...");
    bool ret = Loader_Stop();
    if (!ret) {
        ESP_LOGE(TAG, "Loader stop timeout !!");
        return false;
    }

    ESP_LOGI(TAG, "Requesting dacstream stop...");
    ret = DacStream_Stop();
    if (!ret) {
        ESP_LOGE(TAG, "Dacstream stop timeout !!");
        return false;
    }

    fclose(Player_VgmFile);
    fclose(Player_PcmFile);
    fclose(Player_DsFindFile);
    fclose(Player_DsFillFile);

    return true;
}