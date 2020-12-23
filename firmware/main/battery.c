#include "battery.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "pins.h"
#include "ioexp.h"
#include "hal.h"

static const char* TAG = "BatteryMgr";
#if defined HWVER_PORTABLE
static esp_adc_cal_characteristics_t *BatteryMgr_AdcChars;
#endif
uint16_t BatteryMgr_Voltage = 0;

bool BatteryMgr_Setup() {
    #if defined HWVER_PORTABLE
    ESP_LOGI(TAG, "Setting up ADC");
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_6, ADC_ATTEN_DB_11);
    BatteryMgr_AdcChars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, BatteryMgr_AdcChars);
    #endif

    return true;
}

uint16_t BatteryMgr_Sample() {
    IoExp_BatSenseControl(true);
    vTaskDelay(pdMS_TO_TICKS(5));
    uint32_t accu = 0;
    for (uint8_t i=0;i<100;i++) {
        accu += adc1_get_raw(ADC_CHANNEL_6);
    }
    IoExp_BatSenseControl(false);
    return accu/100;
}

void BatteryMgr_Main() {
    ESP_LOGI(TAG, "Task start");
    while (1) {
        #if defined HWVER_PORTABLE
        uint32_t v = esp_adc_cal_raw_to_voltage(BatteryMgr_Sample(), BatteryMgr_AdcChars);
        v *= 2; //voltage divider on input
        BatteryMgr_Voltage = v;
        ESP_LOGI(TAG, "bat %dmV", v);
        if (v < 3200) {
            //do shutdown stuff
            IoExp_PowerControl(false);
            vTaskDelay(pdMS_TO_TICKS(500));
            //if we're still alive, we must be on usb power...
            ESP_LOGE(TAG, "Reset by holding back !!");
            esp_restart();
        }
        #endif
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
