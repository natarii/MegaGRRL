#include "sdcard.h"
#include "mallocs.h"
#include "esp_log.h"
#include <dirent.h>

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
static FATFS* Sdcard_Fs = NULL;
volatile bool Sdcard_Online = false;
static uint8_t pdrv = 0xff;
static char drv[3] = {'x',':',0};

static bool state_mounted_vol = false;
static bool state_inited_host = false;
static bool state_inited_slot = false;
static bool state_inited_card = false;
static bool state_registered_diskio = false;
static bool state_registered_vfs = false;
static bool state_mounted_fs = false;

uint8_t Sdcard_Setup() {
    ESP_LOGI(TAG, "Setting up");

    Sdcard_Host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == 0xff) {
        ESP_LOGE(TAG, "Can't mount any more volumes !!");
        return 0xff;
    }
    state_mounted_vol = true;

    Sdcard_Card = malloc(sizeof(sdmmc_card_t));
    if (Sdcard_Card == NULL) {
        ESP_LOGE(TAG, "Couldn't malloc card object !!");
        return 0xff;
    }

    esp_err_t err;
    err = Sdcard_Host.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Host init fail !! 0x%x", err);
        return 0xff;
    }
    state_inited_host = true;

    err = sdmmc_host_init_slot(Sdcard_Host.slot, &Sdcard_Slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Slot init fail !! 0x%x", err);
        return 0xff;
    }
    state_inited_slot = true;

    err = sdmmc_card_init(&Sdcard_Host, Sdcard_Card);
    //probably no card - 0x107 seen
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Card init fail !! 0x%x", err);
        return 1;
    }
    state_inited_card = true;

    ff_diskio_register_sdmmc(pdrv, Sdcard_Card);
    state_registered_diskio = true;

    drv[0] = (char)('0'+pdrv);
    err = esp_vfs_fat_register(Sdcard_BasePath, drv, Sdcard_Mount.max_files, &Sdcard_Fs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fail to register with vfs !! 0x%x", err);
        return 0xff;
    }
    state_registered_vfs = true;

    FRESULT res = f_mount(Sdcard_Fs, drv, 1);
    //0xd = FR_NO_FILESYSTEM - GPT, msdos with no partition, msdos with wrong partition...
    if (res != FR_OK) {
        ESP_LOGE(TAG, "Failed to mount !! 0x%x", res);
        return 2;
    }
    state_mounted_fs = true;

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

void Sdcard_Destroy() {
    if (state_mounted_fs) f_unmount(drv);
    if (state_registered_vfs) {
        esp_vfs_fat_sdmmc_unmount();
        esp_vfs_fat_unregister(); //deprecated?
    }
    if (state_registered_diskio) ff_diskio_unregister(pdrv);
    if (state_inited_host) sdmmc_host_deinit();
    //Sdcard_Host.deinit();

    state_mounted_vol = false;
    state_inited_host = false;
    state_inited_slot = false;
    state_inited_card = false;
    state_registered_diskio = false;
    state_registered_vfs = false;
    state_mounted_fs = false;
    Sdcard_Online = false;
}