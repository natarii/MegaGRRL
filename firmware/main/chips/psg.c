#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include "psg.h"
#include "natutils.h"
#include "esp_log.h"
#include "../driver.h" //only until board stuff is rewritten

static const char* TAG = "chip_psg";

//TODO proper handling of EG-controlled atten mode. what happens to the atten register?
//expanded mode, per 8930 ds: "Switching modes causes loss of all register data from the previous mode. All registers will be initialized except for the Mode Select code of R15."
//reg write delays - where?

static void psg_common_phys_reset(psg_state_t *state) {

}

static void psg_opn_type_phys_write(psg_state_t *state, uint8_t reg, uint8_t val) {
    Driver_FmOutopnaDEV(0, reg, val);
}

static void psg_common_init(psg_state_t *state, uint8_t hw_slot) {
    memset(state, 0, sizeof(psg_state_t));

    state->hw_slot = hw_slot;
    state->mute_mask = 0xf;

    state->mixer_config = 0; //according to mame
    for (uint8_t i=0;i<3;i++) state->level[i] = 0; //according to mame
}

void psg_opn_type_init(psg_state_t *state, uint8_t hw_slot) {
    psg_common_init(state, hw_slot);
    state->type = PSG_TYPE_OPN_SERIES;
    state->write_func = (void *)psg_opn_type_phys_write;
}

static uint8_t psg_common_generate_mixer_config(psg_state_t *state) {
    uint8_t conf = state->mixer_config;
    uint8_t mask = state->mute_mask;

    if (state->in_pre_period || state->paused || state->slip) mask = 0;

    conf |= (~mask) & 7; //tones
    if (!GETBIT(mask, 3)) conf |= 0b00111000; //noise

    return conf;
}

static void psg_common_handle_mixer_config(psg_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    state->mixer_config = write->out_val;
    write->out_val = psg_common_generate_mixer_config(state);
}

static uint8_t psg_common_apply_fade(psg_state_t *state, uint8_t level) {
    if (!state->fade_pos) return level;
    //todo expanded mode
    int32_t l = level & 0xf;
    l -= map(state->fade_pos, 0, 44100*state->fade_len, 0, 0xf);
    if (l < 0) l = 0;
    return (level & 0xf0) | l; 
}

static void psg_common_dump_levels(psg_state_t *state) {
    for (uint8_t ch=0;ch<3;ch++) {
        state->write_func(state, 0x08+ch, psg_common_apply_fade(state, state->level[ch]));
    }
}

static void psg_common_handle_level_write(psg_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint8_t ch = write->out_reg - 0x08;
    state->level[ch] = write->out_val;

    if (state->in_pre_period || state->paused || state->slip) {
        write->short_circuit = write->drop = true;
        return;
    }

    write->out_val = psg_common_apply_fade(state, write->out_val);
}

void psg_virt_write(psg_state_t *state, chip_write_t *write) {
    WRITE_COPY_IN_OUT(write);
    switch (write->out_reg) {
        case 0x07:
            psg_common_handle_mixer_config(state, write);
            break;
        case 0x08:
        case 0x09:
        case 0x0a:
            psg_common_handle_level_write(state, write);
            break;
    }
    if (!write->drop) state->write_func(state, write->out_reg, write->out_val);
}

static void psg_common_update_fade_data(psg_state_t *state, chip_fade_data_t data) {
    state->fade_pos = data.pos;
    state->fade_len = data.len;
    if (data.pos) psg_common_dump_levels(state);
}

static void psg_common_update_muting_data(psg_type_t type, psg_state_t *state) {
    state->write_func(state, 0x07, psg_common_generate_mixer_config(state));
    psg_common_dump_levels(state);
}

void psg_common_ioctl(psg_type_t type, psg_state_t *state, chip_ioctl_t ioctl, void *data) {
    switch (ioctl) {
        case CHIP_IOCTL_UPDATE_MUTING:
            state->mute_mask = *(uint8_t *)data;
            psg_common_update_muting_data(type, state);
            break;
        case CHIP_IOCTL_UPDATE_FADE:
            psg_common_update_fade_data(state, *(chip_fade_data_t *)data);
            break;
        case CHIP_IOCTL_PRE_PERIOD:
            state->in_pre_period = *(bool *)data;
            psg_common_update_muting_data(type, state);
            break;
        case CHIP_IOCTL_UPDATE_SLIP:
            state->slip = *(bool *)data;
            psg_common_update_muting_data(type, state);
            break;
        case CHIP_IOCTL_UPDATE_PAUSE:
            state->paused = *(bool *)data;
            psg_common_update_muting_data(type, state);
            break;
        default:
            break;
    }
}