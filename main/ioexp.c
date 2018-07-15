#include "esp_log.h"
#include "ioexp.h"
#include "i2c.h"
#include "taskmgr.h"

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
    0x0c,0b10000000,    //GPPUA enable for bit 7 to avoid false triggering

    0x01,0b00001100,    //IODIRB
    0x03,0b00001000,    //IPOLB
    0x05,0b00000000,    //GPINTENB
    0x07,0x00,          //DEFVALB
    0x09,0x00,          //INTCONB
    0x0d,0b00000000,    //GPPUB
    0x15,0b00000000,    //OLATB
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
        uint8_t v = IoExp_ReadRegister(MCP23017_Config[i]);
        if (v != MCP23017_Config[i+1]) {
            ESP_LOGE(TAG, "Register %02x verify failed !!\nWrote %02x, read %02x", MCP23017_Config[i], MCP23017_Config[i+1], v);
            return false;
        }
    }
    I2cMgr_Release(false);
    ESP_LOGI(TAG, "MCP23017 configured !!");

    ESP_LOGI(TAG, "Configuring interrupt...");
    gpio_config_t intconf;
    intconf.intr_type = GPIO_PIN_INTR_POSEDGE;
    intconf.mode = GPIO_MODE_INPUT;
    intconf.pull_down_en = 0;
    intconf.pull_up_en = 0;
    intconf.pin_bit_mask = 1ULL<<PIN_IOEXP_INT;
    gpio_config(&intconf);

    ESP_LOGI(TAG, "OK !!");
    return true;
}

static void IRAM_ATTR IoExp_Isr(void* arg) {
    vTaskNotifyGiveFromISR(Taskmgr_Handles[TASK_IOEXP], NULL);
}

void IoExp_Main() {
    ESP_LOGI(TAG, "Task start");
    ESP_LOGI(TAG, "Adding ISR handler...");
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_IOEXP_INT, IoExp_Isr, (void*)PIN_IOEXP_INT);

    ESP_LOGI(TAG, "Starting polling...");
    while (1) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250)); //wait for notification, and poll every 250ms when no notifs
        if (!I2cMgr_Seize(false, pdMS_TO_TICKS(100))) {
            ESP_LOGE(TAG, "Couldn't seize bus !!");
            continue;
        }
        uint8_t intcapa = IoExp_ReadRegister(0x10); //INTCAPA
        intcapa |= IoExp_ReadRegister(0x12); //GPIOA
        I2cMgr_Release(false);
        if (intcapa > 0) {
            I2cMgr_Seize(false, pdMS_TO_TICKS(100));
            for (uint32_t b=0;b<100;b++) {
                uint8_t c = b/10;
                IoExp_WriteRegister(0x15, 0b11100000); //OLATB
                vTaskDelay(pdMS_TO_TICKS(c));
                IoExp_WriteRegister(0x15, 0b10000000); //OLATB
                vTaskDelay(pdMS_TO_TICKS(10-c));
            }
            I2cMgr_Release(false);
        }
        ESP_LOGI(TAG, "polled %02x", intcapa);
    }
}