#include "freertos/FreeRTOS.h"
#include "channels.h"
#include "leddrv.h"
#include "freertos/task.h"
#include "i2c.h"
#include "esp_log.h"
#include "driver.h"

static const char* TAG = "ChannelMgr";

volatile uint8_t ChannelMgr_States[6+4];
ChannelLedState_t ChannelMgr_LedStates[6+4];
IRAM_ATTR uint32_t ChannelMgr_BrTime[6+4];
uint8_t ChannelMgr_States_Old[6+4];
volatile IRAM_ATTR uint32_t ChannelMgr_PcmAccu;
volatile IRAM_ATTR uint32_t ChannelMgr_PcmCount;

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
        uint8_t newstates[10];
        for (uint8_t i=0;i<6+4;i++) { //fm, dcsg
            uint8_t chstate = ChannelMgr_States[i];

            if (ChannelMgr_LedStates[i] == LEDSTATE_BRIGHT && xthal_get_ccount() - ChannelMgr_BrTime[i] >= 50*240000) {
                ChannelMgr_LedStates[i] = LEDSTATE_ON;
            }
            if (chstate & CHSTATE_KON_PS) { //kon rising edge
                ChannelMgr_BrTime[i] = xthal_get_ccount();
                ChannelMgr_LedStates[i] = LEDSTATE_BRIGHT;
            } else if ((chstate & CHSTATE_PARAM) && (chstate & CHSTATE_KON)) { //param rising edge
                ChannelMgr_BrTime[i] = xthal_get_ccount();
                ChannelMgr_LedStates[i] = LEDSTATE_BRIGHT;
            } else if (chstate & CHSTATE_FIXED_ON) {
                ChannelMgr_LedStates[i] = LEDSTATE_PERMABRIGHT;
            } else if ((chstate & CHSTATE_KON) == 0) {
                ChannelMgr_LedStates[i] = LEDSTATE_OFF;
            }

            //dac override
            if (i == 5) {
                if (chstate & CHSTATE_DAC) {
                    ChannelMgr_LedStates[5] = LEDSTATE_ON;
                }
            }

            ChannelMgr_States[i] &= ~(CHSTATE_PARAM|CHSTATE_KON_PS); //write back

            uint8_t ch6mask = (ChannelMgr_States[5] & CHSTATE_DAC)?(1<<6):(1<<5);
            
            if (i < 5 && (Driver_FmMask & (1<<i)) == 0) {
                newstates[i] = 0;
            } else if (i == 5 && (Driver_FmMask & ch6mask) == 0) {
                newstates[5] = 0;
            } else {
                switch (ChannelMgr_LedStates[i]) {
                    case LEDSTATE_PERMABRIGHT:
                    case LEDSTATE_BRIGHT:
                        newstates[i] = 255;
                        break;
                    case LEDSTATE_ON:
                        newstates[i] = 96;
                        break;
                    case LEDSTATE_OFF:
                        newstates[i] = 0;
                        break;
                    default:
                        break;
                }
            }

            ChannelMgr_States_Old[i] = ChannelMgr_States[i];
        }
        uint32_t avg = 0;
        if (ChannelMgr_PcmCount && (Driver_FmMask & (1<<6))) {
            avg = ChannelMgr_PcmAccu / ChannelMgr_PcmCount;
            ChannelMgr_PcmAccu = ChannelMgr_PcmCount = 0;
            avg <<= 1;
            if (avg > 255) avg = 255;
        }
        if (!I2cMgr_Seize(false, pdMS_TO_TICKS(1000))) {
            ESP_LOGE(TAG, "Couldn't seize bus !!");
            return;
        }
        LedDrv_States[6+4] = avg;
        for (uint8_t i=0;i<10;i++) {
            LedDrv_States[i] = newstates[i];
        }
        LedDrv_Update();
        I2cMgr_Release(false);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
