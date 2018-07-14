#include "esp_log.h"
#include "ioexp.h"
#include "i2c.h"

static const char* TAG = "IoExpander";
static const uint8_t MCP23017_Config[] = {
    0x0a,0b01100010,    /* banking off, since that's the default and we don't know if this is POR or just esp reset
                         * INT pin mirroring on
                         * SEQOP disabled
                         * slew rate control enabled
                         * HAEN: don't care
                         * INT pin active drive enable
                         * INT pin active high
                         * don't care */

    0x00,0b11111111,    //IODIRA all input
    0x02,0b11111111,    //IPOLA invert all
    0x04,0b01111111,    //GPINTENA for all bits except 7
    0x06,0x00,          //DEFVALA doesn't actually matter
    0x08,0b00000000,    //INTCONA compare against previous value
    0x0c,0b00000000,    //GPPUA all disabled

    0x01,0b00001100,    //IODIRB
    0x03,0b00001000,    //IPOLB
    0x05,0b00000000,    //GPINTENB
    0x07,0x00,          //DEFVALB
    0x09,0x00,          //INTCONB
    0x0d,0b00000000,    //GPPUB
};

bool IoExp_WriteRegister(uint8_t Register, uint8_t Value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_WRITE, true);
    uint16_t w = (Register<<8) | Value;
    i2c_master_write_byte(cmd, Register, true);
    i2c_master_write_byte(cmd, Value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

uint8_t IoExp_ReadRegister(uint8_t Register) {
    //TODO: handle errors from i2c_master_cmd_begin
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, Register, true);
    i2c_master_stop(cmd);
    esp_err_t ret;
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_READ, true);
    uint8_t buf;
    i2c_master_read_byte(cmd, &buf, 1); //1 = NACK
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return buf;
}

bool IoExp_Setup() {
    ESP_LOGI(TAG, "Setting up");

    ESP_LOGI(TAG, "Configuring MCP23017...");
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    for (uint8_t i=0;i<sizeof(MCP23017_Config);i+=2) {
        if (!IoExp_WriteRegister(MCP23017_Config[i], MCP23017_Config[i+1])) {
            ESP_LOGE(TAG, "Register %02x write failed !!", MCP23017_Config[i]);
            return false;
        }
        //vTaskDelay(pdMS_TO_TICKS(100));
        uint8_t v = IoExp_ReadRegister(MCP23017_Config[i]);
        if (v != MCP23017_Config[i+1]) {
            ESP_LOGE(TAG, "Register %02x verify failed !!\nWrote %02x, read %02x", MCP23017_Config[i], MCP23017_Config[i+1], v);
            return false;
        }
        //vTaskDelay(pdMS_TO_TICKS(100));
    }
    I2cMgr_Release(false);
    ESP_LOGI(TAG, "MCP23017 configured !!");

    return true;
}