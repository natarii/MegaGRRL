
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ioexp.h"
#include "i2c.h"
#include "taskmgr.h"
#include "mallocs.h"
#include <string.h>
#include <math.h>
#include "hal.h"

static const char* TAG = "LedDrv";

volatile uint8_t led_pwm[16];
volatile uint8_t LedDrv_States[16];
volatile uint8_t LedDrv_States_ULatch[3];
volatile uint8_t LedDrv_Brightness = 0x40;

const uint8_t led_curve_lut[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05,
    0x05, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0D, 0x0D, 0x0E, 0x0F, 0x0F, 0x10, 0x11, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x26, 0x27, 0x29, 0x2B, 0x2C,
    0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x43, 0x45, 0x47, 0x4A, 0x4C, 0x4F,
    0x51, 0x54, 0x57, 0x59, 0x5C, 0x5F, 0x62, 0x64, 0x67, 0x6A, 0x6D, 0x70, 0x73, 0x76, 0x79, 0x7C,
    0x7F, 0x82, 0x85, 0x88, 0x8B, 0x8E, 0x91, 0x94, 0x97, 0x9A, 0x9C, 0x9F, 0xA2, 0xA5, 0xA7, 0xAA,
    0xAD, 0xAF, 0xB2, 0xB4, 0xB7, 0xB9, 0xBB, 0xBE, 0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
    0xD0, 0xD2, 0xD3, 0xD5, 0xD7, 0xD8, 0xDA, 0xDB, 0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5,
    0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2,
    0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF8,
    0xF9, 0xF9, 0xF9, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFB, 0xFC,
    0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD,
    0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF
};

#if defined HWVER_PORTABLE
const uint8_t led_channel_assignment[] = {
    //FM 1-6
    7, 5, 0, 2, 3, 8+6,
    //PSG 1-3 + noise
    6, 4, 1, 8+4,
    //FM PCM
    8+5,
    //ABC
    8+3, 8+2, 8+1,
    //unused
    8+0, 8+7,
};
#elif defined HWVER_DESKTOP
const uint8_t led_channel_assignment[] = {
    //FM 1-6
    7, 1, 2, 3, 8+1, 8+2,
    //PSG 1-3 + noise
    0, 6, 5, 4,
    //FM PCM
    8+3,
    //ABC
    8+5, 8+6, 8+7,
    //unused
    8+0, 8+4,
};
#endif

const uint8_t pca9634_addr[2] = {0x40,0x01};

#define PCA9634_ADDR_SWRST 0b0000011

bool LedDrv_Reset() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9634_ADDR_SWRST<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xa5, true);
    i2c_master_write_byte(cmd, 0x5a, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

bool LedDrv_WriteRegister(uint8_t ChipId, uint8_t Register, uint8_t Value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (pca9634_addr[ChipId]<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (Register & 0b00011111), true);
    i2c_master_write_byte(cmd, Value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

bool LedDrv_WritePwmValues(uint8_t ChipId, uint8_t *Values) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (pca9634_addr[ChipId]<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (0x02 & 0b00011111) | 0x80, true); //AI2 = autoincrement for all regs
    for (uint8_t i=0;i<8;i++) {
        i2c_master_write_byte(cmd, Values[i], true);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

bool LedDrv_Update() {
    //we will expect whoever is calling this to take/give the i2c mutex themself.

    for (uint8_t i=0;i<16;i++) {
        if (i>=11 && i<=13) {
            //handle user leds differently
            //don't scale brightness
            //handle pulse stretching "latch"
            led_pwm[led_channel_assignment[i]] = LedDrv_States_ULatch[i-11];
            if (!LedDrv_States[i]) LedDrv_States_ULatch[i-11] = 0;
            continue;
        }
        led_pwm[led_channel_assignment[i]] = led_curve_lut[LedDrv_States[i]];
    }

    bool ret = LedDrv_WritePwmValues(0, (uint8_t *)&led_pwm[0]);
    if (!ret) {
        return false;
    }
    ret = LedDrv_WritePwmValues(1, (uint8_t *)&led_pwm[8]);
    return ret;
}

bool LedDrv_Setup() {
    ESP_LOGI(TAG, "Setting up");

    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }

    memset((void *)&LedDrv_States[0], 0, sizeof(LedDrv_States));

    ESP_LOGI(TAG, "Resetting...");
    if (!LedDrv_Reset()) {
        ESP_LOGE(TAG, "Reset fail !!");
        I2cMgr_Release(false);
        return false;
    }

    ESP_LOGI(TAG, "Send initial config...");
    for (uint8_t i=0;i<2;i++) {
        if (!LedDrv_WriteRegister(i, 0x00, 0b00000000)) {   //MODE1
            I2cMgr_Release(false);
            return false;
        }
        if (!LedDrv_WriteRegister(i, 0x01, 0b00000001)) {   //MODE2
            I2cMgr_Release(false);
            return false;
        }
        if (!LedDrv_WriteRegister(i, 0x0a, LedDrv_Brightness)) {         //GRPPWM
            I2cMgr_Release(false);
            return false;
        }
        if (!LedDrv_WriteRegister(i, 0x0c, 0xff)) {         //LEDOUT0
            I2cMgr_Release(false);
            return false;
        }
        if (!LedDrv_WriteRegister(i, 0x0d, 0xff)) {         //LEDOUT1
            I2cMgr_Release(false);
            return false;
        }
    }
    if (!LedDrv_Update()) {
        ESP_LOGE(TAG, "Update fail !!");
        I2cMgr_Release(false);
        return false;
    }

    I2cMgr_Release(false);

    ESP_LOGI(TAG, "OK !!");
    return true;
}

void LedDrv_UpdateBrightness() {
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return;
    }

    LedDrv_WriteRegister(0, 0x0a, LedDrv_Brightness);
    LedDrv_WriteRegister(1, 0x0a, LedDrv_Brightness);

    I2cMgr_Release(false);
}
