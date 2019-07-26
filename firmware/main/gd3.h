#ifndef AGR_GD3_H
#define AGR_GD3_H

#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "vgm.h"

typedef struct {
    uint32_t off;
    uint32_t len;
} Gd3StringDescriptor_t;

typedef enum {
    GD3STRING_TRACK_EN,
    GD3STRING_TRACK_JP,
    GD3STRING_GAME_EN,
    GD3STRING_GAME_JP,
    GD3STRING_SYSTEM_EN,
    GD3STRING_SYSTEM_JP,
    GD3STRING_AUTHOR_EN,
    GD3STRING_AUTHOR_JP,
    GD3STRING_DATE,
    GD3STRING_VGMAUTHOR,
    GD3STRING_NOTES,
    GD3STRING_COUNT
} Gd3String_t;

typedef struct {
    bool parsed;
    uint32_t version;
    Gd3StringDescriptor_t strings[GD3STRING_COUNT];
} Gd3Descriptor_t;

void Gd3ParseDescriptor(FILE *f, VgmInfoStruct_t *info, Gd3Descriptor_t *desc);
void Gd3GetStringChars(FILE *f, Gd3Descriptor_t *desc, Gd3String_t stringid, char buf[], uint32_t max);

#endif