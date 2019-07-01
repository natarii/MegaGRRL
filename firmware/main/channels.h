#ifndef AGR_CHANNELS_H
#define AGR_CHANNELS_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

enum ChannelStates {
    CHSTATE_KON = 0x01,
    CHSTATE_PARAM = 0x02,
};

extern volatile uint8_t ChannelMgr_States[7+4];
extern uint8_t ChannelMgr_States_Old[7+4];
extern volatile uint32_t ChannelMgr_PcmAccu;
extern volatile uint32_t ChannelMgr_PcmCount;

bool ChannelMgr_Setup();
void ChannelMgr_Main();

#endif