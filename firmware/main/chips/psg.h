#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    PSG_TYPE_8910,
    PSG_TYPE_8930,
    PSG_TYPE_YM2149,
    PSG_TYPE_OPN_SERIES,
} psg_type_t;

typedef struct psg_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct psg_state_t *state, uint8_t reg, uint8_t val);
    psg_type_t type;

    bool in_pre_period;
    bool slip;
    bool paused;
    uint8_t mute_mask;
    uint32_t fade_pos;
    uint32_t fade_len;

    uint8_t mixer_config;
    bool expanded_mode;
    uint8_t level[3];
} psg_state_t;

void psg_opn_type_init(psg_state_t *state, uint8_t hw_slot);
void psg_virt_write(psg_state_t *state, chip_write_t *write);
#define psg_ioctl(state, ioctl, data) psg_common_ioctl(0, state, ioctl, data) //todo fixme
void psg_common_ioctl(psg_type_t type, psg_state_t *state, chip_ioctl_t ioctl, void *data);