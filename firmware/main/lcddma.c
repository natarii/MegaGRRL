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

static const char* TAG = "LcdDma";

StaticSemaphore_t LcdDma_MutexBuffer;
SemaphoreHandle_t LcdDma_Mutex = NULL;

void LcdDma_PreTransferCallback(spi_transaction_t *t) {
    uint8_t dc = (uint8_t)t->user;
    gpio_set_level(PIN_DISP_DC, dc);
}

spi_bus_config_t LcdDma_SpiBusConfig = {
    .miso_io_num = -1,
    .mosi_io_num = PIN_DISP_MOSI,
    .sclk_io_num = PIN_DISP_SCK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 3840,   
};
spi_device_interface_config_t LcdDma_SpiDeviceConfig = {
    .clock_speed_hz = 40000000,
    .mode = 0,
    .spics_io_num = PIN_DISP_CS,
    .queue_size = 1,
    .pre_cb = LcdDma_PreTransferCallback,
    .flags = SPI_DEVICE_NO_DUMMY,
};
spi_device_handle_t LcdDma_SpiDevice;

//this is awful and based on loboris library init format
static DRAM_ATTR uint8_t ILI9341_init[] = {
  2324,                   					        // 24 commands in list
  TFT_CMD_SWRESET, TFT_CMD_DELAY,					//  1: Software reset, no args, w/delay
  250,	
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
  // *** INTERFACE PIXEL FORMAT: 0x66 -> 18 bit; 0x55 -> 16 bit
  TFT_CMD_PIXFMT, 1, 0x55,
  TFT_INVOFF, 0,
  TFT_CMD_FRMCTR1, 2, 0x00, 0x18,
  TFT_CMD_DFUNCTR, 4, 0x08, 0x82, 0x27, 0x00,		// Display Function Control
  TFT_PTLAR, 4, 0x00, 0x00, 0x01, 0x3F,
  TFT_CMD_3GAMMA_EN, 1, 0x00,						// 3Gamma Function: Disable (0x02), Enable (0x03)
  TFT_CMD_GAMMASET, 1, 0x01,						//Gamma curve selected (0x01, 0x02, 0x04, 0x08)
  TFT_CMD_GMCTRP1, 15,   							//Positive Gamma Correction
  0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  TFT_CMD_GMCTRN1, 15,   							//Negative Gamma Correction
  0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  TFT_CMD_SLPOUT, TFT_CMD_DELAY,					//  Sleep out
  200,			 									//  120 ms delay
  TFT_DISPON, 0/*TFT_CMD_DELAY, 200,*/
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

static void LcdDma_Lvgl_Flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t * color_p) {
    ESP_LOGD(TAG, "send pos %d,%d %d,%d", x1, y1, x2, y2);
    uint32_t pixels = ((x2-x1)+1)*((y2-y1)+1);
    uint8_t set[4];
    set[0] = (x1>>8)&0xff;
    set[1] = x1&0xff;
    set[2] = (x2>>8)&0xff;
    set[3] = x2&0xff;
    LcdDma_Cmd(0x2a);
    LcdDma_Data(&set,4);
    set[0] = (y1>>8)&0xff;
    set[1] = y1&0xff;
    set[2] = (y2>>8)&0xff;
    set[3] = y2&0xff;
    LcdDma_Cmd(0x2b);
    LcdDma_Data(&set,4);

    LcdDma_Cmd(0x2c);
    while (pixels > 240*8) { //max we can fit in a dma transfer
        LcdDma_Data((uint8_t*)color_p, 240*8*2);
        pixels -= 240*8;
        color_p += 240*8;
    }
    LcdDma_Data((uint8_t*)color_p, pixels*2);

    lv_flush_ready();
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
    ret = spi_bus_initialize(HSPI_HOST, &LcdDma_SpiBusConfig, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Spi bus init fail !! 0x%x", ret);
        return false;
    }
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
    ESP_LOGI(TAG, "Lvgl display driver init...");
    lv_disp_drv_t disp;
	lv_disp_drv_init(&disp);
	disp.disp_flush = LcdDma_Lvgl_Flush;
    ESP_LOGI(TAG, "Lvgl display driver register...");
	lv_disp_drv_register(&disp);
    
    ESP_LOGI(TAG, "Start lvgl tick task");
    xTaskCreatePinnedToCore(LcdDma_Main, "LCD DMA", 8192, NULL, 3, &Taskmgr_Handles[TASK_LCDDMA], 0);

    ESP_LOGI(TAG, "Done");
    return true;
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
        lv_task_handler();
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