#include "volume.h"
#include "esp_log.h"
#include "i2c.h"

static const char* TAG = "VolumeMgr";

bool Volume_Setup() {
    ESP_LOGI(TAG, "Setting up");

    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }

    ESP_LOGI(TAG, "Enabling zero-cross detection");
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0b0101000<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0b10111101, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to enable zero-cross detection !! %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Muting");
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0b0101000<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0b10101111, true);
    i2c_master_write_byte(cmd, 0x40, true); //mute bit
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to mute !! %d", ret);
        return false;
    }

    I2cMgr_Release(false);
    return true;
}

bool Volume_SetVolume(uint8_t Left, uint8_t Right) {
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0b0101000<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0b10101001, true);
    i2c_master_write_byte(cmd, Left, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to set left volume !! %d", ret);
        return false;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0b0101000<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0b10101010, true);
    i2c_master_write_byte(cmd, Left, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to set right volume !! %d", ret);
        return false;
    }

    I2cMgr_Release(false);
    return true;
}