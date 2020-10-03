/*
    MegaGRRL source code

    Copyright (c) 2018-2020, kunoichi labs / natalie null
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/rtc.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#include "driver.h"
#include "taskmgr.h"
#include "clk.h"
#include "sdcard.h"
#include "i2c.h"
#include "ioexp.h"
#include "lvgl.h"
#include "lcddma.h"
#include "ui.h"
#include "hal.h"
#include "leddrv.h"
#include "hspi.h"
#include "ver.h"
#include "key.h"
#ifndef FWUPDATE
#include "battery.h"
#include "dacstream.h"
#include "loader.h"
#include "queue.h"
#include "player.h"
#include "channels.h"
#include "userled.h"
#include "options.h"
#endif

#ifdef HWVER_PORTABLE
LV_IMG_DECLARE(img_frame);
#elif defined HWVER_DESKTOP
LV_IMG_DECLARE(img_desktopframe);
#endif
LV_IMG_DECLARE(img_blank);
LV_IMG_DECLARE(img_lcdsd);
LV_IMG_DECLARE(img_lcdsdq);
LV_IMG_DECLARE(img_lcdhappy);
LV_IMG_DECLARE(img_lcdsad);
LV_IMG_DECLARE(img_kunoichilabs_smol);

#include <stdio.h>
#include <dirent.h>

static const char* TAG = "Main";

//LV_FONT_DECLARE(lv_font_monospace_8);

lv_obj_t *frame;
lv_obj_t *progress;
lv_style_t progressstyle;
lv_obj_t *lcd;
#define MAIN_PROGRESS_MAX 14
uint8_t progressval = 0;
#define MAIN_PROGRESS_UPDATE LcdDma_Mutex_Take(pdMS_TO_TICKS(1000)); lv_obj_set_width(progress, main_map(++progressval,0,MAIN_PROGRESS_MAX,0,50)); LcdDma_Mutex_Give(); vTaskDelay(2);
lv_obj_t * textarea;
lv_style_t textarea_style;
static IRAM_ATTR lv_obj_t *mainbg;
static lv_style_t mainbg_style;

#ifdef FWUPDATE
uint8_t buf[10240];
#endif

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}


uint32_t main_map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void crash() {
    ESP_LOGE(TAG, "CRASHING !!");
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    textarea_style.body.main_color = LV_COLOR_MAKE(255,0,0);
    textarea_style.body.grad_color = LV_COLOR_MAKE(255,0,0);
    textarea_style.text.color = LV_COLOR_MAKE(255,255,255);
    lv_obj_report_style_mod(&textarea_style);
    lv_obj_set_hidden(progress, true);
    lv_img_set_src(lcd, &img_lcdsad);
    LcdDma_Mutex_Give();
    for (uint8_t i=0;i<5;i++) vTaskDelay(pdMS_TO_TICKS(1000));
    for (uint8_t i=0;i<5;i++) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_obj_set_hidden(textarea, false);
        lv_ta_add_text(textarea, ".");
        LcdDma_Mutex_Give();
        uint32_t s = xthal_get_ccount();
        while (xthal_get_ccount() - s <= 1000*240000) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    IoExp_PowerControl(false);
    vTaskDelay(pdMS_TO_TICKS(500));
    //if we get here, either usb power is present or the user is holding the power button. either way, reboot...
    esp_restart();
}

void crash_sd(uint8_t reason) {
    lv_obj_t *rlabel;
    lv_style_t rlabel_style;
    lv_style_copy(&rlabel_style, &lv_style_plain);
    rlabel_style.text.color = LV_COLOR_MAKE(255,255,255);
    //rlabel_style.text.font = &lv_font_monospace_8;
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_set_hidden(progress, true);
    rlabel = lv_label_create(mainbg, NULL);
    lv_label_set_long_mode(rlabel, LV_LABEL_LONG_SROLL);
    lv_obj_set_width(rlabel, 240);
    lv_label_set_align(rlabel, LV_LABEL_ALIGN_CENTER);
    lv_label_set_style(rlabel, LV_LABEL_STYLE_MAIN, &rlabel_style);
    if (reason == 1) {
        lv_label_set_static_text(rlabel, "No card inserted");
    } else if (reason == 2) {
        lv_label_set_static_text(rlabel, "No FAT32 filesystem found");
    } else if (reason == 3) {
        lv_label_set_static_text(rlabel, "Card is write-protected");
    } else {
        lv_label_set_static_text(rlabel, "Unknown card error");
    }
    lv_obj_set_pos(rlabel, 0, 126+67+10);
    LcdDma_Mutex_Give();
    for (uint8_t i=0;i<10;i++) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_img_set_src(lcd, &img_lcdsdq);
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(500));
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_img_set_src(lcd, &img_lcdsd);
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    IoExp_PowerControl(false);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

void checkcore() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    LcdDma_AltMode = true;
    LcdDma_Mutex_Give();
    ESP_LOGI(TAG, "looking for core...");
    esp_partition_t *corepart;
    corepart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (corepart) {
        uint8_t *core;
        spi_flash_mmap_handle_t handle;
        esp_err_t err;
        err = esp_partition_mmap(corepart, 0, corepart->size, SPI_FLASH_MMAP_DATA, (const void**)&core, &handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "core mmap ok");
            for (uint32_t i=0;i<corepart->size;i++) {
                if (*(core+i) != 0xff) {
                    ESP_LOGI(TAG, "there's a dump here, first byte at %d, copying it to sdcard", i);
                    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                    lv_obj_set_hidden(textarea, false);
                    lv_ta_add_text(textarea, "Core dump found!\nCopying core to card...\n");
                    LcdDma_Mutex_Give();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    FILE *f;
                    f = fopen("/sd/.mega/core.bin", "w");
                    fwrite(core, corepart->size, 1, f);
                    fclose(f);
                    ESP_LOGI(TAG, "erasing core part");
                    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                    lv_ta_add_text(textarea, "Removing core from flash...\n");
                    LcdDma_Mutex_Give();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    spi_flash_munmap(handle);
                    err = esp_partition_erase_range(corepart, 0, corepart->size);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "core part erase failed %d", err);
                        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                        lv_ta_add_text(textarea, "Flash erase failed !!\n");
                        LcdDma_Mutex_Give();
                        vTaskDelay(pdMS_TO_TICKS(3000));
                    }
                    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                    lv_obj_set_hidden(textarea, true);
                    LcdDma_AltMode = false;
                    LcdDma_Mutex_Give();
                    return;
                }
            }
            ESP_LOGI(TAG, "no core found");
        } else {
            ESP_LOGI(TAG, "failed to mmap core !!");
        }
    } else {
        ESP_LOGI(TAG, "no core partition found");
    }
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    LcdDma_AltMode = false;
    LcdDma_Mutex_Give();
}

void app_main(void)
{
    uint8_t r = rtc_get_reset_reason(0);

    #ifdef FWUPDATE
    ESP_LOGI(TAG, "MegaGRRL OS Updater");
    #else
    ESP_LOGI(TAG, "MegaGRRL boot");
    #endif
    ESP_LOGI(TAG, "Copyright (c) 2018-2019, kunoichi labs / natalie null");
    ESP_LOGI(TAG, "https://kunoichilabs.dev");
    ESP_LOGI(TAG, "Compiled on %s at %s", __DATE__, __TIME__);
    #if defined HWVER_PORTABLE
    ESP_LOGI(TAG, "For hardware version: Portable");
    #elif defined HWVER_DESKTOP
    ESP_LOGI(TAG, "For hardware version: Desktop");
    #endif
        
    bool setup_ret = false;

    //this is only in here because the chips use more power when not being clocked
    ESP_LOGI(TAG, "Bring up FM clock...");
    Clk_Set(CLK_FM, 7670453);

    ESP_LOGI(TAG, "Setting up driver...");
    Driver_Setup();

    ESP_LOGI(TAG, "Detecting mods...");
    Driver_ModDetect();
    switch (Driver_DetectedMod) {
        case MEGAMOD_NONE:
            ESP_LOGI(TAG, "No mod detected, assuming PSG");
            Clk_Set(CLK_PSG, 3579545);
            break;
        case MEGAMOD_OPLLPSG:
            ESP_LOGI(TAG, "OPLL+PSG detected, switching FM clock to PSG");
            Clk_Set(CLK_FM, 3579545);
            break;
        case MEGAMOD_OPL3:
            ESP_LOGI(TAG, "OPL3 detected, switching FM clock to OPL3");
            Clk_Set(CLK_FM, 14318180);
            break;
        case MEGAMOD_OPNA:
            ESP_LOGI(TAG, "OPNA detected, switching FM clock to OPNA");
            Clk_Set(CLK_FM, 8000000);
            break;
        case MEGAMOD_OPM:
            ESP_LOGI(TAG, "OPM detected, switching FM clock to OPM");
            Clk_Set(CLK_FM, 3579545);
            break;
        default:
            ESP_LOGE(TAG, "Unsupported mod (ID %d)", Driver_DetectedMod);
            break;
    }

    ESP_LOGI(TAG, "Driver reset no.2");
    Driver_ResetChips();

    ESP_LOGI(TAG, "Clear I2C...");
    uint8_t i2cclearresult = I2cMgr_Clear();

    ESP_LOGI(TAG, "Init I2C...");
    bool I2cUp = I2cMgr_Setup();

    ESP_LOGI(TAG, "Init IoExp...");
    bool IoUp = IoExp_Setup();

    ESP_LOGI(TAG, "Init hspi...");
    Hspi_Setup();

    #ifndef FWUPDATE
    #ifdef HWVER_PORTABLE
    //turn off our own power. if we crashed previously, let's not just auto-reboot...
    ESP_LOGI(TAG, "Try poweroff...");
    IoExp_PowerControl(false);
    vTaskDelay(pdMS_TO_TICKS(250));
    //if we're still running at this point, either the user is holding the power button, or usb power is connected, so continue with booting...
    //it's also possible the ioexpander is very borked
    #endif
    #endif

    ESP_LOGI(TAG, "Early LcdDma setup... let's hope this doesn't fail !!");
    LcdDma_Setup();

    ESP_LOGI(TAG, "Early UI setup... let's hope this doesn't fail !!");
    Ui_EarlySetup();

    #ifdef HWVER_PORTABLE
    IoExp_PowerControl(true);
    IoExp_BacklightControl(true);
    #endif

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    lv_style_copy(&mainbg_style, &lv_style_plain);
    mainbg_style.body.main_color = LV_COLOR_MAKE(0,0,0);
    mainbg_style.body.grad_color = LV_COLOR_MAKE(0,0,0);
    mainbg = lv_obj_create(lv_layer_top(), NULL);
    lv_obj_set_style(mainbg, &mainbg_style);
    lv_obj_set_pos(mainbg, 0, 0);
    lv_obj_set_size(mainbg, 240, 320);

    frame = lv_img_create(mainbg, NULL);
    #ifdef HWVER_PORTABLE
    lv_img_set_src(frame, &img_frame);
    lv_obj_set_pos(frame, 95, 126);
    #elif defined HWVER_DESKTOP
    lv_img_set_src(frame, &img_desktopframe);
    lv_obj_set_pos(frame, 86, 126);
    #endif

    lcd = lv_img_create(mainbg, NULL);
    lv_img_set_src(lcd, &img_blank);
    #ifdef HWVER_PORTABLE
    lv_obj_set_pos(lcd, 95+21, 126+8);
    #elif defined HWVER_DESKTOP
    lv_obj_set_pos(lcd, 86+22, 126+5);
    #endif

    static lv_obj_t *kl;
    kl = lv_img_create(mainbg, NULL);
    lv_img_set_src(kl, &img_kunoichilabs_smol);
    lv_obj_set_pos(kl, 120 - (157/2), 250);

    lv_style_copy(&progressstyle, &lv_style_plain);
    progressstyle.body.main_color = LV_COLOR_MAKE(255,255,255);
    progressstyle.body.grad_color = LV_COLOR_MAKE(255,255,255);

    progress = lv_obj_create(mainbg, NULL);
    lv_obj_set_height(progress, 10);
    lv_obj_set_width(progress, 0);
    lv_obj_set_pos(progress, 95, 126+67+10);
    lv_obj_set_style(progress, &progressstyle);

    lv_style_copy(&textarea_style, &lv_style_plain);
    textarea_style.text.color = LV_COLOR_MAKE(0,0,0);
    textarea_style.body.main_color = LV_COLOR_MAKE(0x74,0xd7,0xec);
    textarea_style.body.grad_color = LV_COLOR_MAKE(0xff,0xaf,0xc7);
    //textarea_style.text.font = &lv_font_monospace_8;

    textarea = lv_ta_create(mainbg, NULL);
    lv_obj_set_size(textarea, 240, 320);
    lv_obj_set_pos(textarea, 0, 0);
    lv_ta_set_style(textarea, LV_TA_STYLE_BG, &textarea_style);
    lv_ta_set_cursor_type(textarea, LV_CURSOR_NONE);
    lv_ta_set_cursor_pos(textarea, 0);
    if ((IoExp_PowerOnKeys & (1<<KEY_B)) == 0) lv_obj_set_hidden(textarea, true);

    #ifdef FWUPDATE
    lv_ta_set_text(textarea, "MegaGRRL\nby kunoichi labs\n\nOS Updater\n\n");
    #else
    lv_ta_set_text(textarea, "MegaGRRL\nby kunoichi labs\n\nBoot\n");
    #endif

    #if defined HWVER_PORTABLE
    lv_ta_add_text(textarea, "Hw VER: Portable");
    #elif defined HWVER_DESKTOP
    lv_ta_add_text(textarea, "Hw VER: Desktop");
    #endif
    lv_ta_add_text(textarea, "\nFw VER: ");
    lv_ta_add_text(textarea, FWVER);
    lv_ta_add_text(textarea, "\n");

    LcdDma_Mutex_Give();

    if (!I2cUp) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "I2C init failed !!\n");
        if (i2cclearresult == 1) {
            lv_ta_add_text(textarea, "SDA line stuck low?\n");
        } else if (i2cclearresult == 2) {
            lv_ta_add_text(textarea, "SCL line stuck low?\n");
        } else if (i2cclearresult == 3) {
            lv_ta_add_text(textarea, "SCL and SDA lines shorted?\n");
        }
        LcdDma_Mutex_Give();
        crash();
    }
    MAIN_PROGRESS_UPDATE;
    if (!IoUp) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "IO init failed !!\n");
        if (i2cclearresult == 1) {
            lv_ta_add_text(textarea, "SDA line stuck low?\n");
        } else if (i2cclearresult == 2) {
            lv_ta_add_text(textarea, "SCL line stuck low?\n");
        } else if (i2cclearresult == 3) {
            lv_ta_add_text(textarea, "SCL and SDA lines shorted?\n");
        }
        LcdDma_Mutex_Give();
        crash();
    }
    MAIN_PROGRESS_UPDATE;

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up LedDrv... ");
    LcdDma_Mutex_Give();
    setup_ret = LedDrv_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        //crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up Sdcard... ");
    LcdDma_Mutex_Give();
    uint8_t card_setup_ret = Sdcard_Setup();
    if (card_setup_ret == 0) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash_sd(card_setup_ret);
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Checking for core...\n");
    LcdDma_Mutex_Give();
    checkcore();
    MAIN_PROGRESS_UPDATE;

    #ifdef FWUPDATE
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(500));
    LcdDma_AltMode = true;
    lv_obj_set_hidden(textarea, false);
    lv_obj_set_hidden(frame, true);
    lv_obj_set_hidden(lcd, true);
    lv_obj_set_hidden(progress, true);
    lv_ta_add_text(textarea, "Looking for update file...\n");
    LcdDma_Mutex_Give();
    FILE *fw;
    fw = fopen("/sd/recovery.mgf", "r");
    if (fw == NULL) fw = fopen("/sd/.mega/firmware.mgf", "r");
    if (fw == NULL) fw = fopen("/sd/factory.mgf", "r");
    if (fw == NULL) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "No update file found!");
        LcdDma_Mutex_Give();
        crash();
    }

    //ready to flash
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "\nDO NOT REMOVE SD CARD\nDO NOT POWER OFF\n\n");
    LcdDma_Mutex_Give();
    vTaskDelay(pdMS_TO_TICKS(5000));
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Locating OS partition\n");
    LcdDma_Mutex_Give();
    esp_partition_t *partition;
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    uint8_t *app;
    spi_flash_mmap_handle_t mmaphandle;
    vTaskDelay(pdMS_TO_TICKS(500));
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Backing up...\n");
    LcdDma_Mutex_Give();
    esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, (const void**)&app, &mmaphandle);
    FILE *f = fopen("/sd/.mega/backup_1.bin", "w");
    fwrite(app, partition->size, 1, f);
    fclose(f);
    spi_flash_munmap(mmaphandle);
    esp_ota_handle_t update = 0;
    vTaskDelay(pdMS_TO_TICKS(500));
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Starting update\n");
    LcdDma_Mutex_Give();
    esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &update);
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Flashing");
    LcdDma_Mutex_Give();
    while (!feof(fw)) {
        uint16_t read;
        read = fread(&buf[0], sizeof(uint8_t), sizeof(buf), fw);
        esp_ota_write(update, &buf[0], read);
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, ".");
        LcdDma_Mutex_Give();
        vTaskDelay(2);
    }
    fclose(fw);

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "\nRemoving update file\n");
    LcdDma_Mutex_Give();
    vTaskDelay(2);
    rename("/sd/.mega/firmware.mgf", "/sd/.mega/backup_n.mgf");
    rename("/sd/.mega/factory.mgf", "/sd/.mega/backup_f.mgf");
    rename("/sd/.mega/recovery.mgf", "/sd/.mega/backup_r.mgf");

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Finishing update\n");
    LcdDma_Mutex_Give();
    vTaskDelay(2);
    esp_ota_end(update);

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting boot partition\n");
    LcdDma_Mutex_Give();
    vTaskDelay(2);
    esp_ota_set_boot_partition(partition);

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    //lv_ta_set_text(textarea, "Rebooting");
    lv_obj_set_hidden(textarea, true);
    lv_obj_set_hidden(frame, false);
    lv_obj_set_hidden(lcd, false);
    lv_img_set_src(lcd, &img_lcdhappy);
    LcdDma_Mutex_Give();

    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_restart();
    #endif

    #ifndef FWUPDATE

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up KeyMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = KeyMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up OptionsMgr...");
    LcdDma_Mutex_Give();
    OptionsMgr_Setup();
    MAIN_PROGRESS_UPDATE;

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up ChannelMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = ChannelMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up UserLedMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = UserLedMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up BatteryMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = BatteryMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up DacStream... ");
    LcdDma_Mutex_Give();
    setup_ret = DacStream_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up Loader... ");
    LcdDma_Mutex_Give();
    setup_ret = Loader_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up Player... ");
    LcdDma_Mutex_Give();
    setup_ret = Player_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_set_hidden(progress, true);
    lv_img_set_src(lcd, &img_lcdhappy);
    LcdDma_Mutex_Give();
    vTaskDelay(pdMS_TO_TICKS(500));
    lv_obj_t *mmlabel = NULL;
    if (Driver_DetectedMod != MEGAMOD_NONE) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_style_t mmlabel_style;
        lv_style_copy(&mmlabel_style, &lv_style_plain);
        mmlabel_style.text.color = LV_COLOR_MAKE(255,255,255);
        mmlabel_style.text.font = &lv_font_dejavu_18;
        mmlabel = lv_label_create(mainbg, NULL);
        lv_label_set_long_mode(mmlabel, LV_LABEL_LONG_SROLL);
        lv_obj_set_width(mmlabel, 240);
        lv_obj_set_height(mmlabel, 30);
        lv_label_set_align(mmlabel, LV_LABEL_ALIGN_CENTER);
        lv_label_set_style(mmlabel, LV_LABEL_STYLE_MAIN, &mmlabel_style);
        switch (Driver_DetectedMod) {
            case MEGAMOD_OPL3:
                lv_label_set_static_text(mmlabel, "OPL3 MegaMod");
                break;
            case MEGAMOD_OPNA:
                lv_label_set_static_text(mmlabel, "OPNA MegaMod");
                break;
            case MEGAMOD_OPLLPSG:
                lv_label_set_static_text(mmlabel, "OPLL+PSG MegaMod");
                break;
            case MEGAMOD_OPM:
                lv_label_set_static_text(mmlabel, "OPM MegaMod");
                break;
            default:
                lv_label_set_static_text(mmlabel, "ERROR: Unknown MegaMod");
                break;
        }
        lv_obj_set_pos(mmlabel, 0, 126+67+10);
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up UI... ");
    LcdDma_Mutex_Give();
    setup_ret = Ui_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
        MAIN_PROGRESS_UPDATE;
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }
    ESP_LOGI(TAG, "Starting tasks !!");
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Starting tasks!");
    LcdDma_Mutex_Give();
    Taskmgr_CreateTasks();
    
    if ((IoExp_PowerOnKeys & (1<<KEY_B))) vTaskDelay(pdMS_TO_TICKS(5000));

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(mainbg);
    LcdDma_Mutex_Give();

    #endif
}