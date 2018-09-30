#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ioexp.h"
#include "i2c.h"
#include "taskmgr.h"
#include "mallocs.h"

static const char* TAG = "IoExpander";
uint8_t IoExp_OLATB = 0b00010001; //hack for display backlight and amp remove me
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
    0x15,0b00010001,    //OLATB //hack for display backlight remove me
};

static uint8_t PORTAQueueBuf[IOEXP_PORTA_QUEUE_SIZE * sizeof(PORTAQueueItem_t)];
QueueHandle_t IoExp_PORTAQueue = NULL;
static StaticQueue_t IoExp_PORTAStaticQueue;

bool IoExp_WriteRegister(uint8_t Register, uint8_t Value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, Register, true);
    i2c_master_write_byte(cmd, Value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

uint8_t IoExp_ReadRegister(uint8_t Register) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, Register, true);
    i2c_master_stop(cmd);
    esp_err_t ret;
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register %02x read command fail !!", Register);
        return 0;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_IOEXP<<1)|I2C_MASTER_READ, true);
    uint8_t buf;
    i2c_master_read_byte(cmd, &buf, 1); //1 = NACK
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register %02x read data fail !!", Register);
        return 0;
    }
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

    ESP_LOGI(TAG, "Setting up PORTA queue...");
    IoExp_PORTAQueue = xQueueCreateStatic(IOEXP_PORTA_QUEUE_SIZE, sizeof(PORTAQueueItem_t), &PORTAQueueBuf[0], &IoExp_PORTAStaticQueue);
    if (IoExp_PORTAQueue == NULL) {
        ESP_LOGE(TAG, "PORTA queue create fail !!");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); //settle

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
        bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) > 0; //wait for notification, and poll periodically when no notifs
        if (notified) ESP_LOGE(TAG, "notified !!");
        if (!I2cMgr_Seize(false, pdMS_TO_TICKS(100))) {
            ESP_LOGW(TAG, "Couldn't seize bus !!");
            continue;
        }
        
        uint8_t intcapa = IoExp_ReadRegister(0x10); //INTCAPA
        uint8_t reg = IoExp_ReadRegister(0x12);     //GPIOA
        if (notified) reg |= intcapa;

        I2cMgr_Release(false);

        PORTAQueueItem_t event;
        event.Timestamp = xTaskGetTickCount();
        event.PORTA = reg;

        if (xQueueSend(IoExp_PORTAQueue, &event, 0) != pdTRUE) {
            ESP_LOGE(TAG, "PORTA queue over !!");
        }
    }
}

bool IoExp_WriteLed(uint8_t LedNo, bool LedStatus) {
    uint8_t n = IoExp_OLATB;
    if (LedStatus) {
        n |= (1<<(5+LedNo));
    } else {
        n &= ~(1<<(5+LedNo));
    }
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    if (!IoExp_WriteRegister(0x15, n)) {
        ESP_LOGE(TAG, "OLATB write fail !!");
        return false;
    }
    I2cMgr_Release(false);
    IoExp_OLATB = n;
    return true;
}

bool IoExp_WriteLeds(bool FlashlightStatus, bool Led1Status, bool Led2Status) {
    uint8_t l = (FlashlightStatus<<5) | (Led1Status<<6) | (Led2Status<<7);
    uint8_t n = (l & 0xe0) | (IoExp_OLATB & 0x1f);
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    if (!IoExp_WriteRegister(0x15, n)) {
        ESP_LOGE(TAG, "OLATB write fail !!");
        return false;
    }
    I2cMgr_Release(false);
    IoExp_OLATB = n;
    return true;
}

bool IoExp_WriteLedsBin(uint8_t ThreeBits) {
    return IoExp_WriteLeds(ThreeBits&1, (ThreeBits>>1)&1, (ThreeBits>>2)&1);
}

bool IoExp_PowerControl(bool HoldPower) {
    uint8_t n = (HoldPower<<1) | (IoExp_OLATB & 0xfd);
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    if (!IoExp_WriteRegister(0x15, n)) {
        ESP_LOGE(TAG, "OLATB write fail !!");
        return false;
    }
    I2cMgr_Release(false);
    IoExp_OLATB = n;
    return true;
}

bool IoExp_AmpControl(bool AmpPower) {
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    uint8_t n = IoExp_OLATB;
    if (AmpPower) {
        n &= 0b11111110;
    } else {
        n |= 1;
    }
    if (!IoExp_WriteRegister(0x15, n)) {
        ESP_LOGE(TAG, "OLATB write fail !!");
        return false;
    }
    I2cMgr_Release(false);
    IoExp_OLATB = n;
    return true;
}

bool IoExp_ChargeStatus() {
    if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
        ESP_LOGE(TAG, "Couldn't seize bus !!");
        return false;
    }
    uint8_t r = IoExp_ReadRegister(0x13);
    I2cMgr_Release(false);
    return (r & 0b100) == 0;
}