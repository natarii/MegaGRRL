#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    OPL_TYPE_OPL,
    OPL_TYPE_OPL2,
    OPL_TYPE_OPL3,
    //OPL_TYPE_OPL4,
    OPL_TYPE_OPLL,
    //OPL_TYPE_OPLLP, //YMF281
    //OPL_TYPE_OPLLX, //YM2423
    //OPL_TYPE_VRC7,
} opl_series_type_t;

typedef struct opl_series_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct opl_series_state_t *state, uint8_t port, uint8_t reg, uint8_t val);
    opl_series_type_t type;
    opl_series_type_t virt_type;
    
    uint8_t fm_pan[18];
    uint8_t fm_conn_bit[18];
    uint8_t fm_tl[36];
    uint32_t mute_mask;
    bool force_mono;
    bool in_pre_period;
    uint32_t fade_pos;
    uint32_t fade_len;
    bool slip;
    bool paused;
    bool sum_to_ab;
    bool is_opl3_mode;
    uint8_t fourop_en;
    bool drum_en;
} opl_series_state_t;

void opl3_init(opl_series_state_t *state, uint8_t hw_slot);
void opl3_virt_write(opl_series_state_t *state, chip_write_t *write);
#define opl3_ioctl(state, ioctl, data) opl_common_ioctl(OPL_TYPE_OPL3, state, ioctl, data)
void opl_common_ioctl(opl_series_type_t type, opl_series_state_t *state, chip_ioctl_t ioctl, void *data);