#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    DCSG_TYPE_SN76489,
} dcsg_type_t;

typedef enum {
    DCSG_FLAG_ASSUME_SEGA = (1<<0),
    DCSG_FLAG_SPECIAL_FREQ0 = (1<<1),
} dcsg_fix_flags_t;

typedef struct dcsg_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct dcsg_state_t *state, uint8_t data);
    dcsg_type_t type;

    uint8_t atten[4];
    uint16_t freq[3];
    uint8_t latched_ch;
    bool noise_is_periodic;
    bool noise_source_ch3;

    uint8_t tune_sr_width;
    uint8_t mute_mask;
    bool in_pre_period;
    uint32_t fade_pos;
    uint32_t fade_len;
    bool slip;
    bool paused;
    dcsg_fix_flags_t fix_flags;
} dcsg_state_t;

void dcsg_init(dcsg_state_t *state, uint8_t hw_slot);
void dcsg_virt_write(dcsg_state_t *state, chip_write_t *write);
void dcsg_ioctl(dcsg_state_t *state, chip_ioctl_t ioctl, void *data);