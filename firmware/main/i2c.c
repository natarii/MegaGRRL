#include "i2c.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char* TAG = "I2cMgr";

StaticSemaphore_t I2cMgr_MutexBuffer;
SemaphoreHandle_t I2cMgr_Sync = NULL;

bool I2cMgr_Setup() {
    ESP_LOGI(TAG, "Setting up");

    ESP_LOGI(TAG, "Setting up peripheral");
    i2c_config_t config;
    config.mode = I2C_MODE_MASTER;
    config.scl_io_num = PIN_I2C_CLK;
    config.sda_io_num = PIN_I2C_DATA;
    config.scl_pullup_en = GPIO_PULLUP_DISABLE;
    config.sda_pullup_en = GPIO_PULLUP_DISABLE;
    config.master.clk_speed = 400000;
    esp_err_t ret;
    ret = i2c_param_config(I2C_NUM_0, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C peripheral config failed !!");
        return false;
    }
    ret = i2c_driver_install(I2C_NUM_0, config.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C peripheral install failed !!");
        return false;
    }

    ESP_LOGI(TAG, "Creating mutex");
    I2cMgr_Sync = xSemaphoreCreateMutexStatic(&I2cMgr_MutexBuffer);
    if (I2cMgr_Sync == NULL) {
        ESP_LOGE(TAG, "Mutex creation failed !!");
        return false;
    }

    ESP_LOGI(TAG, "I2C up !!");

    return true;
}

bool I2cMgr_Seize(bool IsIsr, TickType_t Ticks) {
    if (I2cMgr_Sync == NULL) {
        ESP_LOGE(TAG, "Trying to seize I2C bus before it is set up !!");
        return false;
    }
    if (IsIsr) {
        return xSemaphoreTakeFromISR(I2cMgr_Sync, false) == pdTRUE; //can't wait in ISR
    } else {
        return xSemaphoreTake(I2cMgr_Sync, Ticks) == pdTRUE;
    }
}

bool I2cMgr_Release(bool IsIsr) {
    bool ok;
    if (IsIsr) {
        ok = xSemaphoreGiveFromISR(I2cMgr_Sync, false) == pdTRUE;
    } else {
        ok = xSemaphoreGive(I2cMgr_Sync) == pdTRUE;
    }
    if (!ok) {
        ESP_LOGE(TAG, "Error releasing I2C bus !!");
        return false;
    }
    return true;
}

uint8_t I2cMgr_Clear() {
    gpio_config_t cfg;
    cfg.intr_type = GPIO_PIN_INTR_DISABLE;
    cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    cfg.pin_bit_mask = (1<<PIN_I2C_CLK) | (1<<PIN_I2C_DATA);
    cfg.pull_down_en = 0;
    cfg.pull_up_en = 0;
    gpio_config(&cfg);
    gpio_set_level(PIN_I2C_DATA, 1);
    gpio_set_level(PIN_I2C_CLK, 1);
    ets_delay_us(10000);
    for (uint8_t i=0;i<9;i++) {
        gpio_set_level(PIN_I2C_CLK, 0);
        ets_delay_us(4);
        gpio_set_level(PIN_I2C_CLK, 1);
        ets_delay_us(4);
    }
    ets_delay_us(10000);
    if (!gpio_get_level(PIN_I2C_DATA)) {
        ESP_LOGE(TAG, "I2C SDA appears to be stuck low");
        return 1;
    }
    if (!gpio_get_level(PIN_I2C_CLK)) {
        ESP_LOGE(TAG, "I2C SCL appears to be stuck low");
        return 2;
    }
    gpio_set_level(PIN_I2C_CLK, 0);
    ets_delay_us(10000);
    if (!gpio_get_level(PIN_I2C_DATA)) {
        ESP_LOGE(TAG, "I2C SCL & SDA appear to be shorted");
        return 3;
    }
    return 0;
}
