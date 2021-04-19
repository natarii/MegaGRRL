#ifndef AGR_TUTORIAL_H
#define AGR_TUTORIAL_H

#include "freertos/FreeRTOS.h"

extern volatile uint8_t tutorial_completion;

#define TUT_CMPL_MAINMENU (1<<0)
#define TUT_CMPL_FILEBROWSER (1<<1)
#define TUT_CMPL_NOWPLAYING (1<<2)

#define TUT_CMPL_ALL (TUT_CMPL_MAINMENU | TUT_CMPL_FILEBROWSER | TUT_CMPL_NOWPLAYING)

bool tutorial_section_is_complete(uint8_t section);
void tutorial_section_mark_complete(uint8_t section);
void tutorial_show_if_not_complete(uint8_t section, bool mark_complete, uint8_t extra_data);

#endif