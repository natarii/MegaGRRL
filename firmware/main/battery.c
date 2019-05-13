#include "battery.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "pins.h"

static const char* TAG = "BatteryMgr";
static esp_adc_cal_characteristics_t *BatteryMgr_AdcChars;

bool BatteryMgr_Setup() {
    ESP_LOGI(TAG, "Setting up ADC");
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_6, ADC_ATTEN_DB_11);
    BatteryMgr_AdcChars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    /*esp_adc_cal_value_t type = */esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, BatteryMgr_AdcChars);
    
    ESP_LOGI(TAG, "Setting up sense control");
    gpio_set_direction(PIN_BAT_SENSE_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BAT_SENSE_EN, 0);

    return true;
}

uint16_t BatteryMgr_Sample() {
    gpio_set_level(PIN_BAT_SENSE_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    uint32_t accu = 0;
    for (uint8_t i=0;i<100;i++) {
        accu += adc1_get_raw(ADC_CHANNEL_6);
    }
    gpio_set_level(PIN_BAT_SENSE_EN, 0);
    return accu/100;
}

void BatteryMgr_Main() {
    ESP_LOGI(TAG, "Task start");
    while (1) {
        uint32_t v = esp_adc_cal_raw_to_voltage(BatteryMgr_Sample(), BatteryMgr_AdcChars);
        v *= 2; //voltage divider on input
        ESP_LOGI(TAG, "bat %dmV", v);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}