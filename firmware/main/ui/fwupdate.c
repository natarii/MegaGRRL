#include "../ver.h"
#include "fwupdate.h"
#include "../hal.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "freertos/task.h"
#include "fwupdate.h"
#include "softbar.h"
#include "../mgu.h"
#include <rom/crc.h>
#include "rom/rtc.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

static const char* TAG = "Ui_Fwupdate";

#ifdef HWVER_PORTABLE
LV_IMG_DECLARE(img_frame);
#elif defined HWVER_DESKTOP
LV_IMG_DECLARE(img_desktopframe);
#endif
LV_IMG_DECLARE(img_lcdhenohenomoheji);
LV_IMG_DECLARE(img_lcdsad);
LV_IMG_DECLARE(img_lcdhappy);

static IRAM_ATTR lv_obj_t *container;
lv_style_t containerstyle;
char *fwupdate_file;
IRAM_ATTR lv_obj_t *fwupdate_ta;
lv_style_t fwupdate_ta_style;
mgu_header_t newfirmware;
bool fwupdate_valid = false;
uint8_t *fwupdate_buf = NULL;

IRAM_ATTR lv_obj_t *fwupdate_frame;
IRAM_ATTR lv_obj_t *fwupdate_lcd;

void Ui_Fwupdate_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    LcdDma_AltMode = true;
    vTaskDelay(pdMS_TO_TICKS(500)); //bad hack to let any prior draws finish, because altmode is kinda broken in lcddma atm

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);

    fwupdate_frame = lv_img_create(container, NULL);
    #ifdef HWVER_PORTABLE
    lv_img_set_src(fwupdate_frame, &img_frame);
    lv_obj_set_pos(fwupdate_frame, 95, 15);
    #elif defined HWVER_DESKTOP
    lv_img_set_src(fwupdate_frame, &img_desktopframe);
    lv_obj_set_pos(fwupdate_frame, 86, 15);
    #endif

    fwupdate_lcd = lv_img_create(container, NULL);
    lv_img_set_src(fwupdate_lcd, &img_lcdhenohenomoheji);
    #ifdef HWVER_PORTABLE
    lv_obj_set_pos(fwupdate_lcd, 95+21, 15+8);
    #elif defined HWVER_DESKTOP
    lv_obj_set_pos(fwupdate_lcd, 86+22, 15+5);
    #endif

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, true, "Back", false);
    Ui_SoftBar_Update(2, false, LV_SYMBOL_CHARGE" Flash", false);

    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&fwupdate_ta_style, &lv_style_plain);
    fwupdate_ta_style.text.color = LV_COLOR_MAKE(0xff,0xff,0xff);
    fwupdate_ta_style.text.line_space = 2;
    fwupdate_ta_style.body.main_color = LV_COLOR_MAKE(0,0,0);
    fwupdate_ta_style.body.grad_color = LV_COLOR_MAKE(0,0,0);
    fwupdate_ta_style.text.font = &lv_font_unscii_8;
    fwupdate_ta_style.text.line_space = 0;
    fwupdate_ta_style.text.letter_space = -1;

    fwupdate_ta = lv_ta_create(container, NULL);
    lv_obj_set_style(fwupdate_ta, &fwupdate_ta_style);
    lv_obj_set_size(fwupdate_ta, 240, 150);
    lv_obj_set_pos(fwupdate_ta, 0, 100);
    lv_ta_set_cursor_type(fwupdate_ta, LV_CURSOR_NONE);
    lv_ta_set_cursor_pos(fwupdate_ta, 0);
    lv_ta_set_text(fwupdate_ta, "MegaGRRL OS Updater\n\nChecking update file...\n");

    LcdDma_Mutex_Give();


    FILE *f = fopen(fwupdate_file, "r");
    fread(&newfirmware.magic, 4, 1, f);
    fread(&newfirmware.hardware_version, 1, 1, f);
    fread(&newfirmware.ver_length, 1, 1, f);
    fread(&newfirmware.app_size, 4, 1, f);
    fread(&newfirmware.updater_size, 4, 1, f);
    fread(&newfirmware.app_sum, 4, 1, f);
    fread(&newfirmware.updater_sum, 4, 1, f);
    if (newfirmware.magic != 0x6167656d) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Invalid update file");
        LcdDma_Mutex_Give();
        fwupdate_valid = false;
        fclose(f);
        return;
    }
    #ifdef HWVER_DESKTOP
    if (newfirmware.hardware_version != 'd') {
    #elif defined HWVER_PORTABLE
    if (newfirmware.hardware_version != 'p') {
    #endif
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Wrong hardware version\n\nCannot flash.\nPress Back to return");
        LcdDma_Mutex_Give();
        fwupdate_valid = false;
        fclose(f);
        return;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Hardware version ok\n");
        LcdDma_Mutex_Give();
    }
    char *nv = calloc(newfirmware.ver_length+1, 1);
    fread(nv, 1, newfirmware.ver_length, f);
    uint32_t remaining = newfirmware.app_size;
    uint32_t crc = 0;
    if (remaining) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "File contains new OS\nChecking OS...");
        LcdDma_Mutex_Give();
        fwupdate_buf = malloc(1024);
        while (remaining) {
            uint16_t s = (remaining>1024)?1024:remaining;
            fread(fwupdate_buf, 1, s, f);
            crc = crc32_le(crc, fwupdate_buf, s);
            remaining -= s;
        }
        free(fwupdate_buf);
        if (crc != newfirmware.app_sum) {
            LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
            lv_ta_add_text(fwupdate_ta, " failed\n\nUpdate file corrupt\nPress Back to return");
            lv_img_set_src(fwupdate_lcd, &img_lcdsad);
            LcdDma_Mutex_Give();
            fwupdate_valid = false;
            fclose(f);
            return;
        }
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, " ok\n");
        LcdDma_Mutex_Give();
    }
    remaining = newfirmware.updater_size;
    if (remaining) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "File contains new updater\nChecking updater...");
        LcdDma_Mutex_Give();
        fwupdate_buf = malloc(1024);
        crc = 0;
        while (remaining) {
            uint16_t s = (remaining>1024)?1024:remaining;
            fread(fwupdate_buf, 1, s, f);
            crc = crc32_le(crc, fwupdate_buf, s);
            remaining -= s;
        }
        free(fwupdate_buf);
        if (crc != newfirmware.updater_sum) {
            LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
            lv_ta_add_text(fwupdate_ta, " failed\n\nUpdate file corrupt\nPress Back to return");
            lv_img_set_src(fwupdate_lcd, &img_lcdsad);
            LcdDma_Mutex_Give();
            fwupdate_valid = false;
            fclose(f);
            return;
        }
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, " ok\n");
        LcdDma_Mutex_Give();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(fwupdate_ta, "\n\n\nCurrent version: ");
    lv_ta_add_text(fwupdate_ta, FWVER);
    lv_ta_add_text(fwupdate_ta, "\nNew version:     ");
    lv_ta_add_text(fwupdate_ta, nv);
    lv_ta_add_text(fwupdate_ta, "\n");
    free(nv);
    LcdDma_Mutex_Give();

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(fwupdate_ta, "*** Hold Flash to install ***");
    Ui_SoftBar_Update(2, true, LV_SYMBOL_CHARGE" Flash", false);
    LcdDma_Mutex_Give();

    fwupdate_valid = true;

    fclose(f);
}

void Ui_Fwupdate_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    LcdDma_AltMode = false;
    lv_obj_del(container);
    LcdDma_Mutex_Give();
    //if (fwupdate_buf != NULL) free(fwupdate_buf);
}
void fwupdate_flash() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, false, "Back", false);
    Ui_SoftBar_Update(2, false, "Flashing", false);
    lv_ta_set_text(fwupdate_ta, "Installing update...\n\nDO NOT REMOVE SD CARD\nDO NOT POWER OFF\n\n");
    LcdDma_Mutex_Give();
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (newfirmware.updater_size) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Backing up...\n");
        LcdDma_Mutex_Give();
        const esp_partition_t *partition;
        partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        if (partition == NULL) ESP_LOGE(TAG, "no part found !!");
        uint8_t *upd;
        spi_flash_mmap_handle_t mmaphandle;
        esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, (const void**)&upd, &mmaphandle);
        FILE *f = fopen("/sd/.mega/backup_0.bin", "w");
        fwrite(upd, partition->size, 1, f);
        fclose(f);
        spi_flash_munmap(mmaphandle);
        vTaskDelay(pdMS_TO_TICKS(500));
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Installing new updater...\n");
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_ota_handle_t updatehandle = 0;
        esp_err_t ret = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &updatehandle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ota begin fail %s", esp_err_to_name(ret));
        }
        if (!updatehandle) ESP_LOGE(TAG, "update handle bad");
        f = fopen(fwupdate_file, "r");
        fseek(f,22+newfirmware.ver_length+newfirmware.app_size, SEEK_SET);
        uint32_t remaining = newfirmware.updater_size;
        fwupdate_buf = malloc(1024);
        while (remaining) {
            uint16_t s = (remaining>1024)?1024:remaining;
            fread(fwupdate_buf, 1, s, f);
            esp_ota_write(updatehandle, fwupdate_buf, s);
            remaining -= s;
        }
        fclose(f);
        free(fwupdate_buf);
        esp_ota_end(updatehandle); //this causes bizarre issues
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (newfirmware.app_size) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Preparing new OS...\n");
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(500));
        FILE *f = fopen(fwupdate_file, "r");
        FILE *df = fopen("/sd/.mega/firmware.mgf", "w");
        fseek(f,22+newfirmware.ver_length, SEEK_SET);
        uint32_t remaining = newfirmware.app_size;
        fwupdate_buf = malloc(1024);
        while (remaining) {
            uint16_t s = (remaining>1024)?1024:remaining;
            fread(fwupdate_buf, 1, s, f);
            fwrite(fwupdate_buf, 1, s, df);
            remaining -= s;
        }
        fclose(f);
        fclose(df);
        free(fwupdate_buf);
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Rebooting into updater!");
        lv_img_set_src(fwupdate_lcd, &img_lcdhappy);
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(3000));
        const esp_partition_t *factory;
        factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        esp_ota_set_boot_partition(factory);
        esp_restart();
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(fwupdate_ta, "Update done !!");
        lv_img_set_src(fwupdate_lcd, &img_lcdhappy);
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(2000));
        Ui_Screen = UISCREEN_FILEBROWSER;
    }
}

void Ui_Fwupdate_Key(KeyEvent_t event) {
    if (event.Key == KEY_B) {
        Ui_Screen = UISCREEN_FILEBROWSER;
    } else if (event.Key == KEY_C && event.State == KEY_EVENT_HOLD) {
        if (fwupdate_valid) fwupdate_flash();
    }
}
