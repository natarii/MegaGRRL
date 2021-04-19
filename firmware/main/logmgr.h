#ifndef AGR_LOGMGR_H
#define AGR_LOGMGR_H

#include "freertos/FreeRTOS.h"

extern volatile uint8_t logmgr_loglevel;

void logmgr_update_loglevel();

#endif