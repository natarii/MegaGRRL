#include "clkgen.h"
#include "esp_log.h"
#include "i2c.h"
#include "math.h"

static const char* TAG = "ClkgenMgr";

bool Clkgen_Setup() {
    ESP_LOGI(TAG, "Setting up");
    return true;
}

bool Clkgen_SetFrequency(uint8_t Clkgen, uint32_t Frequency) {
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }

    uint16_t oct;
    uint16_t dac;
    float t;
    t = 3.322f * log10((float)Frequency / 1039.0f);
    oct =  floor(t);
    t = 2048.0f - (2078.0f*(float)pow(2,10+oct))/(float)Frequency;
    dac = round(t);
    uint16_t out = 0b10; //noninverting on, inverting off
    out |= dac << 2;
    out |= oct << 12;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ((0x16+Clkgen)<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, out>>8, true);
    i2c_master_write_byte(cmd, out & 0xff, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device %02x write fail !! %d", 0x16+Clkgen, ret);
        return false;
    }

    I2cMgr_Release(false);
    return true;
}