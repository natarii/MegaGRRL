#include "sdcard.h"
#include "mallocs.h"
#include "esp_log.h"
#include <dirent.h>
#include "ui/shuffleall.h"
#include "diskio_impl.h"
#include "driver/sdmmc_host.h"

static const char* TAG = "Sdcard";

static sdmmc_host_t Sdcard_Host = SDMMC_HOST_DEFAULT();
static sdmmc_slot_config_t Sdcard_Slot = SDMMC_SLOT_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t Sdcard_Mount = {
    .format_if_mount_failed = false,
    .max_files = MAX_OPEN_FILES,
    .allocation_unit_size = 1 //irrelevant because we don't format if mount failed right now...
};
static sdmmc_card_t* Sdcard_Card;
static const char Sdcard_BasePath[] = "/sd";
static volatile bool Sdcard_Online = false;

uint8_t Sdcard_Setup() {
    ESP_LOGI(TAG, "Setting up");

    Sdcard_Host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    Sdcard_Slot.width = 4;

    //newer esp32 parts: gpio config goes here...

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(Sdcard_BasePath, &Sdcard_Host, &Sdcard_Slot, &Sdcard_Mount, &Sdcard_Card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc mount fail: %s", esp_err_to_name(ret));
        return 1;
    }
    
    ESP_LOGI(TAG, "Card online !!");

    DIR *test = opendir("/sd/.mega");
    if (!test) {
        ESP_LOGW(TAG, ".mega doesn't exist, creating !!");
        if (mkdir("/sd/.mega", S_IRWXU) != 0) {
            ESP_LOGE(TAG, ".mega mkdir error !!");
            return 3;
        }
    } else {
        closedir(test);
    }

    Sdcard_Online = true;
    return 0;
}

void Sdcard_Invalidate() { //mark card offline and invalidate any cached stuff that depends on card contents
    Sdcard_Online = false;
    Ui_ShuffleAll_Invalidate();
}

void Sdcard_Destroy() {
    esp_vfs_fat_sdcard_unmount(Sdcard_BasePath, Sdcard_Card);
    Sdcard_Invalidate(); //instead of Sdcard_Online = false;
}

bool Sdcard_IsOnline() {
    return Sdcard_Online;
}
