#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"
#include "psg.h"

typedef enum {
    //OPN_TYPE_OPN,
    OPN_TYPE_OPN2,
    OPN_TYPE_OPNA,
    //OPN_TYPE_OPNB,
} opn_series_type_t;

typedef struct {
    uint8_t config;
    uint8_t level;
    uint16_t start_addr;
    uint16_t stop_addr;
} opna_adpcm_config_t;

typedef struct {
    uint8_t config[6];
    uint8_t tl;
} opna_rhythm_config_t;

typedef struct opn_series_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct opn_series_state_t *state, uint8_t addr, uint8_t reg, uint8_t val);
    opn_series_type_t type;
    
    uint8_t fm_pan[6];
    uint8_t fm_algo[6];
    uint8_t fm_tl[4*6];
    uint8_t dedup[512];
    uint8_t mute_mask;
    uint8_t ch6_is_dac;
    bool force_mono;
    bool in_pre_period;
    uint32_t fade_pos;
    uint32_t fade_len;
    bool test_reg_blocked;
    uint8_t opn2_dac_last;
    bool opn2_dac_touched;
    bool slip;
    bool paused;

    psg_state_t *opna_psg_state; //todo rename - used for opn as well, should we ever support it
    opna_adpcm_config_t opna_adpcm_config;
    opna_rhythm_config_t opna_rhythm_config;
} opn_series_state_t;

void opn2_init(opn_series_state_t *state, uint8_t hw_slot);
void opna_init(opn_series_state_t *state, uint8_t hw_slot);
void opn2_virt_write(opn_series_state_t *state, chip_write_t *write);
void opna_virt_write(opn_series_state_t *state, chip_write_t *write);
#define opn2_ioctl(state, ioctl, data) opn_common_ioctl(OPN_TYPE_OPN2, state, ioctl, data)
void opna_ioctl(opn_series_state_t *state, chip_ioctl_t ioctl, void *data);
void opn_common_ioctl(opn_series_type_t type, opn_series_state_t *state, chip_ioctl_t ioctl, void *data);