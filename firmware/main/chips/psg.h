#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef enum {
    PSG_TYPE_AY_3_8910,
    PSG_TYPE_YM2149,
    PSG_TYPE_OPN_SERIES,
} psg_type_t;

typedef struct psg_state_t {
    uint8_t hw_slot;
    uint32_t clock;
    void (*write_func)(struct psg_state_t *state, uint8_t port, uint8_t reg, uint8_t val);
    psg_type_t type;


} psg_state_t;

/*void opn2_init(opn_series_state_t *state, uint8_t hw_slot);
void opna_init(opn_series_state_t *state, uint8_t hw_slot);
void opn2_virt_write(opn_series_state_t *state, chip_write_t *write);
void opna_virt_write(opn_series_state_t *state, chip_write_t *write);
#define opn2_ioctl(state, ioctl, data) opn_common_ioctl(OPN_TYPE_OPN2, state, ioctl, data)
#define opna_ioctl(state, ioctl, data) opn_common_ioctl(OPN_TYPE_OPNA, state, ioctl, data)
void opn_common_ioctl(opn_series_type_t type, opn_series_state_t *state, chip_ioctl_t ioctl, void *data);
*/