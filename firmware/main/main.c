/*
    MegaGRRL source code

    Copyright (c) 2018-2019, kunoichi labs / natalie null
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

#include "driver.h"
#include "taskmgr.h"
#include "i2c.h"
#include "ioexp.h"
#include "battery.h"
#include "key.h"
#include "sdcard.h"
#include "dacstream.h"
#include "lcddma.h"
#include "loader.h"
#include "queue.h"
#include "player.h"
#include "leddrv.h"
#include "channels.h"
#include "lvgl.h"
#include "ui.h"
#include "clk.h"
#include "userled.h"

#include <stdio.h>
#include <dirent.h>

static const char* TAG = "Main";

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

lv_obj_t * textarea;
lv_style_t textarea_style;

void crash() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    textarea_style.body.main_color = LV_COLOR_MAKE(255,0,0);
    textarea_style.body.grad_color = LV_COLOR_MAKE(255,0,0);
    textarea_style.text.color = LV_COLOR_MAKE(255,255,255);
    lv_obj_report_style_mod(&textarea_style);
    LcdDma_Mutex_Give();
    ESP_LOGE(TAG, "CRASHING !!");
    for (uint8_t i=0;i<5;i++) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
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

void app_main(void)
{
    uint8_t r = rtc_get_reset_reason(0);

    ESP_LOGI(TAG, "MegaGRRL boot");
    ESP_LOGI(TAG, "Copyright (c) 2018-2019, kunoichi labs / natalie null");
    ESP_LOGI(TAG, "https://kunoichilabs.dev");
    ESP_LOGI(TAG, "Compiled on %s at %s", __DATE__, __TIME__);
        
    bool setup_ret = false;

    //this is only in here because the chips use more power when not being clocked
    ESP_LOGI(TAG, "Bring up clocks...");
    Clk_Set(CLK_FM, 7670453);
    Clk_Set(CLK_PSG, 3579545);

    ESP_LOGI(TAG, "Setting up driver. If this fails, something is really wrong!!");
    Driver_Setup();

    ESP_LOGI(TAG, "Clear I2C...");
    I2cMgr_Clear();

    ESP_LOGI(TAG, "Init I2C...");
    bool I2cUp = I2cMgr_Setup();
    
    ESP_LOGI(TAG, "Init IoExp...");
    bool IoUp = IoExp_Setup();

    //turn off our own power. if we crashed previously, let's not just auto-reboot...
    ESP_LOGI(TAG, "Try poweroff...");
    IoExp_PowerControl(false);
    vTaskDelay(pdMS_TO_TICKS(250));
    //if we're still running at this point, either the user is holding the power button, or usb power is connected, so continue with booting...
    //it's also possible the ioexpander is very borked
    IoExp_PowerControl(true);

    IoExp_BacklightControl(true);

    ESP_LOGI(TAG, "Early LcdDma setup... let's hope this doesn't fail !!");
    LcdDma_Setup();

    ESP_LOGI(TAG, "Early UI setup... let's hope this doesn't fail !!");
    Ui_EarlySetup();

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    lv_style_copy(&textarea_style, &lv_style_plain);
    textarea_style.text.color = LV_COLOR_MAKE(0,0,0);
    textarea_style.body.main_color = LV_COLOR_MAKE(0x74,0xd7,0xec);
    textarea_style.body.grad_color = LV_COLOR_MAKE(0xff,0xaf,0xc7);
    textarea_style.text.font = &lv_font_monospace_8;

    textarea = lv_ta_create(lv_layer_top(), NULL);
    lv_obj_set_size(textarea, 240, 320);
    lv_obj_set_pos(textarea, 0, 0);
    lv_ta_set_style(textarea, LV_TA_STYLE_BG, &textarea_style);
    lv_ta_set_cursor_type(textarea, LV_CURSOR_NONE);
    lv_ta_set_cursor_pos(textarea, 0);

    lv_ta_set_text(textarea, "MegaGRRL\nby kunoichi labs\n\nBoot\n");

    LcdDma_Mutex_Give();

    if (!I2cUp) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "I2C init failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }
    if (!IoUp) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "IO init failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up Sdcard... ");
    LcdDma_Mutex_Give();
    setup_ret = Sdcard_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up KeyMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = KeyMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up LedDrv... ");
    LcdDma_Mutex_Give();
    setup_ret = LedDrv_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        //crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up ChannelMgr... ");
    LcdDma_Mutex_Give();
    setup_ret = ChannelMgr_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
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
    } else {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "failed !!\n");
        LcdDma_Mutex_Give();
        crash();
    }

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_ta_add_text(textarea, "Setting up UI... ");
    LcdDma_Mutex_Give();
    setup_ret = Ui_Setup();
    if (setup_ret) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_ta_add_text(textarea, "ok\n");
        LcdDma_Mutex_Give();
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
    vTaskDelay(pdMS_TO_TICKS(250));
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(textarea);
    LcdDma_Mutex_Give();
}