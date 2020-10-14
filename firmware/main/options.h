#ifndef AGR_OPTIONS_H
#define AGR_OPTIONS_H
#include "esp_system.h"
#include "hal.h"

typedef enum {
    OPTION_TYPE_NUMERIC,
    OPTION_TYPE_BOOL,
    OPTION_TYPE_COUNT
} optiontype_t;

typedef enum {
    OPTION_SUBTYPE_NONE,
    OPTION_SUBTYPE_BRIGHTNESS,
    OPTION_SUBTYPE_STEREOMONO,
    OPTION_SUBTYPE_USERLED,
    OPTION_SUBTYPE_SORTDIR,
    OPTION_SUBTYPE_PLAYMODE,
    OPTION_SUBTYPE_LOOPS,
    OPTION_SUBTYPE_FADELENGTH,
    OPTION_SUBTYPE_SCROLLTYPE,
    OPTION_SUBTYPE_SHUFFLE,
    OPTION_SUBTYPE_COUNT
} optionsubtype_t;

typedef enum {
    OPTION_CATEGORY_GENERAL,
    OPTION_CATEGORY_FILEBROWSER,
    OPTION_CATEGORY_LEDS,
    OPTION_CATEGORY_PLAYBACK,
    OPTION_CATEGORY_ADVANCED,
    //OPTION_CATEGORY_SCREEN, //disabling for now, portable only
    OPTION_CATEGORY_COUNT
} optioncategory_t;

typedef struct {
    uint16_t uid;
    const char *name;
    const char *description;
    optioncategory_t category;
    optiontype_t type;
    optionsubtype_t subtype;
    //uint8_t width; //make them all 8bit lmaooooo
    volatile void *var;
    uint8_t defaultval;
    void (*cb)();
    void (*cb_initial)();
} option_t;

#define OPTION_COUNT 19

//options version. this was 0xA0 before the crc was added, so we will support 0xA0 files.
#define OPTIONS_VER 0xA1

extern const option_t Options[];
extern const char *OptionCatNames[];

extern volatile bool OptionsMgr_Unsaved;

void OptionsMgr_Setup();
void OptionsMgr_Main();
void OptionsMgr_Touch();

#endif