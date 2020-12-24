#ifndef AGR_QUEUE_H
#define AGR_QUEUE_H

#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "vgm.h"

enum {
    QUEUE_SOURCE_UNDEFINED,
    QUEUE_SOURCE_MEGAGRRL,
    QUEUE_SOURCE_M3U,
};

extern char QueueM3uFilename[256];
extern char QueuePlayingFilename[512];
extern IRAM_ATTR uint32_t QueuePosition;
extern IRAM_ATTR uint32_t QueueLength;
extern volatile bool Queue_Shuffle;

void QueueLoadM3u(const char *M3uPath, const char *M3uFilename, uint32_t pos, bool CountComments, bool ShufflePreserveCurrentEntry);
bool QueueNext();
bool QueuePrev();
void QueueSetupEntry(bool ReturnComments, bool ProcessShuffle);

#endif
