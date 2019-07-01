#include "freertos/FreeRTOS.h"
#include "channels.h"
#include "leddrv.h"
#include "freertos/task.h"
#include "i2c.h"
#include "esp_log.h"

static const char* TAG = "ChannelMgr";

volatile uint8_t ChannelMgr_States[7+4];
uint32_t ChannelMgr_BrTime[7+4];
uint8_t ChannelMgr_States_Old[7+4];
volatile uint32_t ChannelMgr_PcmAccu;
volatile uint32_t ChannelMgr_PcmCount;

void ChannelMgr_Reset() {
    for (uint8_t i=0;i<6+4;i++) {
        ChannelMgr_States[i] = 0;
        ChannelMgr_States_Old[i] = 0;
    }
}

bool ChannelMgr_Setup() {
    ESP_LOGI(TAG, "Setting up !!");
    ChannelMgr_Reset();
    return true;
}

void ChannelMgr_Main() {
    while (1) {
        for (uint8_t i=0;i<7+4;i++) { //fm, psg
            if ((ChannelMgr_States_Old[i] & CHSTATE_KON) == 0 && (ChannelMgr_States[i] & CHSTATE_KON)) { //kon rising edge
                ChannelMgr_BrTime[i] = xthal_get_ccount();
            } else if (/*((ChannelMgr_States_Old[i] & CHSTATE_PARAM) == 0) && */(ChannelMgr_States[i] & CHSTATE_PARAM) && (ChannelMgr_States[i] & CHSTATE_KON)) { //param rising edge
                ChannelMgr_BrTime[i] = xthal_get_ccount();
            }

            ChannelMgr_States[i] &= ~CHSTATE_PARAM;
            
            uint8_t br = 0;
            if (ChannelMgr_States[i] & CHSTATE_KON) {
                br = 96;
                if (xthal_get_ccount() - ChannelMgr_BrTime[i] <= 12000000) br = 255;
            } else {
            }
            LedDrv_States[i] = br;

            ChannelMgr_States_Old[i] = ChannelMgr_States[i];
        }
        uint32_t avg = 0;
        if (ChannelMgr_PcmCount) {
            avg = ChannelMgr_PcmAccu / ChannelMgr_PcmCount;
            ChannelMgr_PcmAccu = ChannelMgr_PcmCount = 0;
            avg <<= 1;
            if (avg > 255) avg = 255;
        }
        LedDrv_States[6] = avg;
        if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
            ESP_LOGE(TAG, "Couldn't seize bus !!");
            return false;
        }
        LedDrv_Update();
        I2cMgr_Release(false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
