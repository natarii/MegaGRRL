#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    //OPL_TYPE_OPL,
    //OPL_TYPE_OPL2,
    OPL_TYPE_OPL3,
    //OPL_TYPE_OPL4,
} opl_series_type_t;

typedef struct opl_series_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct opl_series_state_t *state, uint8_t port, uint8_t reg, uint8_t val);
    opl_series_type_t type;
    
    uint8_t fm_pan[6];
    uint8_t fm_algo[6];
    uint8_t fm_tl[4*6];
    uint8_t mute_mask;
    bool force_mono;
    bool in_pre_period;
    uint32_t fade_pos;
    uint32_t fade_len;
    bool slip;
    bool paused;
} opl_series_state_t;

void opl3_init(opl_series_state_t *state, uint8_t hw_slot);
void opl3_virt_write(opl_series_state_t *state, chip_write_t *write);
#define opl3_ioctl(state, ioctl, data) opl_common_ioctl(OPL_TYPE_OPL3, state, ioctl, data)
void opl_common_ioctl(opl_series_type_t type, opl_series_state_t *state, chip_ioctl_t ioctl, void *data);