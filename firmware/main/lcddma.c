#include "lcddma.h"
#include "pins.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "mallocs.h"
#include "taskmgr.h"

#define LV_VDB_SIZE 4094/2
#define LV_VDB_SIZE_IN_BYTES (LV_VDB_SIZE*2)

static const char* TAG = "LcdDma";

volatile bool LcdDma_AltMode = true;
volatile bool LcdDma_Screenshot = false;

StaticSemaphore_t LcdDma_MutexBuffer;
SemaphoreHandle_t LcdDma_Mutex = NULL;

DRAM_ATTR static uint8_t LcdDma_Lvgl_Buf[LV_VDB_SIZE_IN_BYTES];
DRAM_ATTR static uint8_t LcdDma_Lvgl_Buf2[LV_VDB_SIZE_IN_BYTES];
static lv_disp_buf_t disp_buf;
static volatile IRAM_ATTR lv_disp_t *disp;

static uint8_t ss_id = 0;
static FILE *ss_file;

void LcdDma_PostTransferCallback(spi_transaction_t *t) {
    if (/*!LcdDma_AltMode && */(uint8_t)(uintptr_t)t->user & 1<<1) lv_disp_flush_ready((lv_disp_drv_t *)disp);
}

void LcdDma_PreTransferCallback(spi_transaction_t *t) {
    gpio_set_level(PIN_DISP_DC, (uint8_t)(uintptr_t)t->user); //no need to mask off the end of sequence bit - gpio driver just does "if (level)"
}

spi_device_interface_config_t LcdDma_SpiDeviceConfig = {
    .clock_speed_hz = 40000000,
    .mode = 0,
    .spics_io_num = PIN_DISP_CS,
    .queue_size = 6,
    .pre_cb = LcdDma_PreTransferCallback,
    .post_cb = LcdDma_PostTransferCallback,
    .flags = SPI_DEVICE_NO_DUMMY
};
spi_device_handle_t LcdDma_SpiDevice;

//this is awful and based on loboris library init format
static DRAM_ATTR uint8_t ILI9341_init[] = {
  24,                   					        // 24 commands in list
  TFT_CMD_SWRESET, TFT_CMD_DELAY,					//  1: Software reset, no args, w/delay
  120,	                                            //5ms until next command, 120 required until sleep out. the overhead of sending all the other params probably accounts for a good chunk of the 120...
  TFT_CMD_POWERA, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  TFT_CMD_POWERB, 3, 0x00, 0XC1, 0X30,
  0xEF, 3, 0x03, 0x80, 0x02,
  TFT_CMD_DTCA, 3, 0x85, 0x00, 0x78,
  TFT_CMD_DTCB, 2, 0x00, 0x00,
  TFT_CMD_POWER_SEQ, 4, 0x64, 0x03, 0X12, 0X81,
  TFT_CMD_PRC, 1, 0x20,
  TFT_CMD_PWCTR1, 1, 0x23,							//Power control VRH[5:0]
  TFT_CMD_PWCTR2, 1, 0x10,							//Power control SAP[2:0];BT[3:0]
  TFT_CMD_VMCTR1, 2, 0x3e, 0x28,					//VCM control
  TFT_CMD_VMCTR2, 1, 0x86,							//VCM control2
  TFT_MADCTL, 1,									// Memory Access Control (orientation)
  (0b1011100),
  TFT_CMD_PIXFMT, 1, 0x55,
  TFT_INVOFF, 0,
  TFT_CMD_FRMCTR1, 2, 0x00, 0x18,
  TFT_CMD_DFUNCTR, 4, 0x08, 0x82, 0x27, 0x00,		// Display Function Control
  TFT_PTLAR, 4, 0x00, 0x00, 0x01, 0x3F,
  TFT_CMD_SLPOUT, TFT_CMD_DELAY,					//  Sleep out
  15,			 									//datasheet specifies only 5ms required, but this is long enough to hide the initial draw
  TFT_CMD_3GAMMA_EN, 1, 0x00,						// 3Gamma Function: Disable (0x02), Enable (0x03)
  TFT_CMD_GAMMASET, 1, 0x01,						//Gamma curve selected (0x01, 0x02, 0x04, 0x08)
  TFT_CMD_GMCTRP1, 15,   							//Positive Gamma Correction
  0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  TFT_CMD_GMCTRN1, 15,   							//Negative Gamma Correction
  0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  TFT_DISPON, 0
};

void LcdDma_Cmd(uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t,0,sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0;
    ret = spi_device_transmit(LcdDma_SpiDevice, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cmd transmit fail !! 0x%x", ret);
    }
}

void LcdDma_Data(uint8_t *data, uint16_t len) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t,0,sizeof(t));
    t.length = 8*len;
    t.tx_buffer = data;
    t.user = (void*)1;
    ret = spi_device_transmit(LcdDma_SpiDevice, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data transmit fail !! 0x%x", ret);
    }
}

spi_transaction_t LcdDma_Flush_Txs[6];
static uint8_t ss_buf[512];
static void LcdDma_Lvgl_Flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    //don't bother checking the size of the data given to this, we should never receive more than the maximum dma transfer size, as ensured by the size of LV_VDB_SIZE.

    int32_t x1, y1, x2, y2;
    x1 = area->x1;
    y1 = area->y1;
    x2 = area->x2;
    y2 = area->y2;
    uint32_t pix = ((x2-x1)+1)*((y2-y1)+1);

    //todo: maybe redo this, by adding another lvgl display device, and switching it to output to that when we want a screenshot
    //this would produce broken images if lvgl ever rendered the screen out of order, but since we invalidate the whole screen and only render once, that should never happen
    if (LcdDma_Screenshot) {
        //GLLBBBBBRRRRRGHH (this is the byteswapped RGB565 bit layout, not me making random growling noises >_>)
        uint16_t waiting = 0;
        while (pix) {
            uint16_t p = color_p->full;
            uint8_t r = p & 0b11111000;
            uint8_t g = ((p&0b111)<<5) | (p>>11);
            uint8_t b = (p>>5) & 0b11111000;
            pix--;
            color_p++;
            ss_buf[waiting++] = b;
            ss_buf[waiting++] = g;
            ss_buf[waiting++] = r;
            if (waiting >= 509) {
                fwrite(ss_buf, 1, waiting, ss_file);
                waiting = 0;
            }
        }
        if (waiting) {
            fwrite(ss_buf, 1, waiting, ss_file);
        }
        lv_disp_flush_ready((lv_disp_drv_t *)disp);
        return;
    }

    for (uint8_t i=0;i<6;i++) {
        memset(&LcdDma_Flush_Txs[i], 0, sizeof(spi_transaction_t));
        if (i & 1) {
            LcdDma_Flush_Txs[i].length = 32;
            LcdDma_Flush_Txs[i].user = (void*)1;
        } else {
            LcdDma_Flush_Txs[i].length = 8;
            LcdDma_Flush_Txs[i].user = (void*)0;
        }
        if (i < 5) LcdDma_Flush_Txs[i].flags = SPI_TRANS_USE_TXDATA;
    }

    //X
    LcdDma_Flush_Txs[0].tx_data[0] = 0x2a;
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[0], portMAX_DELAY);
    LcdDma_Flush_Txs[1].tx_data[0] = (x1>>8)&0xff;
    LcdDma_Flush_Txs[1].tx_data[1] = x1&0xff;
    LcdDma_Flush_Txs[1].tx_data[2] = (x2>>8)&0xff;
    LcdDma_Flush_Txs[1].tx_data[3] = x2&0xff;
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[1], portMAX_DELAY);

    //Y
    LcdDma_Flush_Txs[2].tx_data[0] = 0x2b;
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[2], portMAX_DELAY);
    LcdDma_Flush_Txs[3].tx_data[0] = (y1>>8)&0xff;
    LcdDma_Flush_Txs[3].tx_data[1] = y1&0xff;
    LcdDma_Flush_Txs[3].tx_data[2] = (y2>>8)&0xff;
    LcdDma_Flush_Txs[3].tx_data[3] = y2&0xff;
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[3], portMAX_DELAY);

    //data
    LcdDma_Flush_Txs[4].tx_data[0] = 0x2c;
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[4], portMAX_DELAY);
    LcdDma_Flush_Txs[5].tx_buffer = (uint8_t *)color_p;
    LcdDma_Flush_Txs[5].length = pix<<4;
    LcdDma_Flush_Txs[5].user = (void *)0b11; //bit 1 used as end of sequence flag, checked in post-transfer callback
    spi_device_queue_trans(LcdDma_SpiDevice, &LcdDma_Flush_Txs[5], portMAX_DELAY);

    ESP_LOGD(TAG, "Tx buf%d %d bytes", ((void *)color_p >= (void *)&LcdDma_Lvgl_Buf[0] && (void *)color_p < (void *)&LcdDma_Lvgl_Buf[LV_VDB_SIZE_IN_BYTES])?1:2, pix<<1);
    
    //alt mode. i guess another way to do this would be set a flag in the interrupt and check it here...
    return;
    if (LcdDma_AltMode) {
        spi_transaction_t *check;
        esp_err_t err;
        do {
            err = spi_device_get_trans_result(LcdDma_SpiDevice, &check, pdMS_TO_TICKS(1000));
        } while (err != ESP_OK || ((uint8_t)(uintptr_t)check->user & 1<<1) == 0);
        lv_disp_flush_ready((lv_disp_drv_t *)disp);
        ESP_LOGD(TAG, "Alt mode flush successful");
    }
}

bool LcdDma_Setup() {
    gpio_set_direction(PIN_DISP_DC, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Creating mutex");
    LcdDma_Mutex = xSemaphoreCreateMutexStatic(&LcdDma_MutexBuffer);
    if (LcdDma_Mutex == NULL) {
        ESP_LOGE(TAG, "Mutex creation failed !!");
        return false;
    }

    esp_err_t ret;
    ret = spi_bus_add_device(HSPI_HOST, &LcdDma_SpiDeviceConfig, &LcdDma_SpiDevice);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Spi bus device add fail !! 0x%x", ret);
        return false;
    }

    //lcd init
    uint16_t idx = 1;
    while (idx < sizeof(ILI9341_init)) {
        uint8_t cmd = ILI9341_init[idx++];
        uint8_t params = ILI9341_init[idx++];
        uint8_t params2 = params & 0b01111111;
        ESP_LOGD(TAG, "send command %02x, %d params, delay %d", cmd, params2, (params >> 7));
        LcdDma_Cmd(cmd);
        if (params2 > 0) {
            LcdDma_Data(&ILI9341_init[idx], params2);
            idx += params2;
        }
        if (params & TFT_CMD_DELAY) {
            vTaskDelay(pdMS_TO_TICKS(ILI9341_init[idx++]));
        } else {
            //vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    //set up lvgl stuff
    ESP_LOGI(TAG, "Init lvgl...");
    lv_init();
    lv_tick_inc(20);
    //lv_vdb_set_adr(&LcdDma_Lvgl_Buf[0], &LcdDma_Lvgl_Buf2[0]);
    lv_disp_buf_init(&disp_buf, LcdDma_Lvgl_Buf, LcdDma_Lvgl_Buf2, LV_VDB_SIZE);
    ESP_LOGI(TAG, "Lvgl display driver init...");
    static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.flush_cb = LcdDma_Lvgl_Flush;
    disp_drv.buffer = &disp_buf;
    ESP_LOGI(TAG, "Lvgl display driver register...");
	disp = lv_disp_drv_register(&disp_drv);
    
    ESP_LOGI(TAG, "Start lvgl tick task");
    xTaskCreatePinnedToCore(LcdDma_Main, "LcdDma", 4096, NULL, 3, &Taskmgr_Handles[TASK_LCDDMA], 0);

    ESP_LOGI(TAG, "Done");
    return true;
}
static const uint8_t ss_tgaheader[18] = {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x40, 0x01, 24, 0b00100000}; //240x320, 24bit RGB, top-down
static void ss_start() {
    char fn[32];
    strcpy(fn, "/sd/.mega/screen");
    sprintf(&fn[strlen(fn)], "%02x", ss_id++);
    strcat(fn, ".tga");
    ss_file = fopen(fn, "w");
    fwrite(ss_tgaheader, 18, 1, ss_file);
}
static void ss_finish() {
    fclose(ss_file);
}

void LcdDma_Main() {
    ESP_LOGI(TAG, "Task start");
    uint32_t last = 0;
    uint32_t cur = 0;
    while (1) {
        cur = xTaskGetTickCount();
        lv_tick_inc(xTaskGetTickCount() - last);
        last = cur;
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        if (LcdDma_Screenshot) {
            //these two draws should be identical, since there is no lv_tick_inc in between
            LcdDma_Screenshot = false;
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now((lv_disp_t *)disp);
            LcdDma_Screenshot = true;
            ss_start();
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now((lv_disp_t *)disp);
            ss_finish();
            LcdDma_Screenshot = false;
        } else {
            lv_task_handler();
        }
        LcdDma_Mutex_Give();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool LcdDma_Mutex_Take(TickType_t Ticks) {
    if (LcdDma_Mutex == NULL) {
        ESP_LOGE(TAG, "Trying to take LcdDma mutex before it's set up !!");
        return false;
    }
    return xSemaphoreTake(LcdDma_Mutex, Ticks) == pdTRUE;
}

bool LcdDma_Mutex_Give() {
    bool ok;
    ok = xSemaphoreGive(LcdDma_Mutex) == pdTRUE;
    if (!ok) {
        ESP_LOGE(TAG, "Error releasing LcdDma mutex !!");
        return false;
    }
    return true;
}
