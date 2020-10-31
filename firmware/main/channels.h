#ifndef AGR_CHANNELS_H
#define AGR_CHANNELS_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

enum ChannelStates {
    CHSTATE_KON = 0x01,
    CHSTATE_PARAM = 0x02,
    CHSTATE_DAC = 0x04,
    CHSTATE_KON_PS = 0x08,
};

typedef enum {
    LEDSTATE_OFF,
    LEDSTATE_BRIGHT,
    LEDSTATE_ON,
    LEDSTATE_COUNT
} ChannelLedState_t;

extern volatile uint8_t ChannelMgr_States[6+4];
extern uint8_t ChannelMgr_States_Old[6+4];
extern volatile IRAM_ATTR uint32_t ChannelMgr_PcmAccu;
extern volatile IRAM_ATTR uint32_t ChannelMgr_PcmCount;

bool ChannelMgr_Setup();
void ChannelMgr_Main();

#endif