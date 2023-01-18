#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include "opn_series.h"
#include "psg.h"
#include "natutils.h"
#include "esp_log.h"
#include "../driver.h" //only until board stuff is rewritten

static const char* TAG = "chip_opn_series";

//todo pretty much all opna
//todo fixup in/out, pmuch all should be out
//todo force mono
//todo verify opna start/stop regs don't affect currently playing sample

//untested etc etc:
//mute
//forcemono
//opn2 write when opna mod detected

//currently not planned to be implemented:
//vgm_trim mitigation for opna adpcm/rhythm

static const uint8_t opn_op_map[4] = {0,2,1,3};

#define OPNA_SET_RAM_CONNECTION(val) val &= 0b11111100; //hw wiring dependent
#define OPNA_BAD_RAM_CONNECTION(val) (val & 3) //hw wiring dependent
#define COMMON_FORCE_MONO(var) if (var & 0b11000000) var |= 0b11000000; //if either L/R is enabled, turn on both
#define COMMON_PORT_REG_TO_CHAN(port, reg) ((port*3)+(reg&3))
#define COMMON_CHAN_TO_PORT(ch) (ch/3)
#define COMMON_CHAN_TO_REG(base, ch) (base+(ch%3))
#define COMMON_OP_ALGO_IS_SLOT(op, algo) !((op == 0 && algo <= 6) || (op == 1 && algo <= 3) || (op == 2 && algo <= 4))

static void opn_common_phys_reset(opn_series_state_t *state) {

}

static void opn2_phys_write(opn_series_state_t *state, uint8_t port, uint8_t reg, uint8_t val) {
    //ESP_LOGI(TAG, "opn2 wr %d %02x %02x", port, reg, val);
    Driver_FmOutDEV(port, reg, val);
}

static void opna_phys_write(opn_series_state_t *state, uint8_t port, uint8_t reg, uint8_t val) {
    Driver_FmOutopnaDEV(port, reg, val);
}

static void opn_common_init(opn_series_state_t *state, uint8_t hw_slot) {
    memset(state, 0, sizeof(opn_series_state_t));

    state->hw_slot = hw_slot;

    for (uint8_t i=0;i<6;i++) state->fm_pan[i] = 0b11000000;
    state->mute_mask = 0xff;
}

void opn2_init(opn_series_state_t *state, uint8_t hw_slot) {
    opn_common_init(state, hw_slot);
    state->type = OPN_TYPE_OPN2;
    state->write_func = (void *)opn2_phys_write;
}

void opna_init(opn_series_state_t *state, uint8_t hw_slot) {
    opn_common_init(state, hw_slot);
    state->type = OPN_TYPE_OPNA;
    state->write_func = (void *)opna_phys_write;

    state->opna_psg_state = calloc(1, sizeof(psg_state_t));
    //todo handle fail
    psg_opn_type_init(state->opna_psg_state, hw_slot);

    state->opna_adpcm_config.config = 0b11000000; //needs hw verify
    for (uint8_t i=0;i<6;i++) state->opna_rhythm_config.config[i] = 0b11000000; //needs hw verify
    //also needs hw verify: rhythm & adpcm level reset states
}

static uint8_t opn_common_filter_pan(opn_series_state_t *state, uint8_t ch, uint8_t pan) {
    //special behavior for opn2 dac
    if (ch == 5 && state->type == OPN_TYPE_OPN2 && state->ch6_is_dac) ch++; //test the seventh slot instead

    //mute tests
    if (!GETBIT(state->mute_mask, ch)) { //ch is muted
        pan &= 0b00111111;
    } else if (state->in_pre_period) { //muted during wait period
        pan &= 0b00111111;
    } else if (state->slip) { //muted during slip
        pan &= 0b00111111;
    } else if (state->paused) {
        pan &= 0b00111111;
    }

    //force mono test. if either L/R is enabled, turn on both
    if (state->force_mono) COMMON_FORCE_MONO(pan);

    return pan;
}

static void opn_common_dump_pans(opn_series_state_t *state) {
    //this handles fm and opn2 dac
    for (uint8_t ch=0;ch<6;ch++) {
        uint8_t pan = opn_common_filter_pan(state, ch, state->fm_pan[ch]);

        state->write_func(state, COMMON_CHAN_TO_PORT(ch), COMMON_CHAN_TO_REG(0xb4, ch), pan);
    }
}

static inline void opn2_filter_dedup(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint16_t idx = (write->out_port<<8)+write->out_reg;
    if ((write->out_reg >> 4) != 0xa) {
        if (state->dedup[idx] == write->out_val) {
            write->drop = true;
            return;
        }
        state->dedup[idx] = write->out_val;
    }
}

static inline void opn2_filter_test_reg(opn_series_state_t *state, chip_write_t *write) {
    if (state->test_reg_blocked && (write->out_reg == 0x21 || write->out_reg == 0x2c)) {
        WRITE_CHECK_SHORT_CIRCUIT(write);
        write->drop = true;
    }
}

static inline void opn2_filter_dac_pre(opn_series_state_t *state, chip_write_t *write) {
    if (state->in_pre_period || state->slip) {
        WRITE_CHECK_SHORT_CIRCUIT(write);
        state->opn2_dac_touched = true;
        state->opn2_dac_last = write->out_val;
        write->drop = true;
    }
}

static void opn2_fix_dac(opn_series_state_t *state) {
    if (!state->in_pre_period && state->opn2_dac_touched)
        state->write_func(state, 0, 0x2a, state->opn2_dac_last);
}

static void opn_common_filter_pan_write(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint8_t ch = COMMON_PORT_REG_TO_CHAN(write->out_port, write->out_reg);
    state->fm_pan[ch] = write->out_val;
    write->out_val = opn_common_filter_pan(state, ch, write->out_val);
}

static uint8_t opn_common_apply_fade(opn_series_state_t *state, uint8_t tl) {
    if (!state->fade_pos) return tl;
    uint32_t scaled = tl & 0x7f;
    scaled += map(state->fade_pos, 0, 44100*state->fade_len, 0, 0x30);
    if (scaled > 0x7f) scaled = 0x7f;
    return scaled;
}

static uint8_t opna_adpcm_apply_fade(opn_series_state_t *state, uint8_t level) {
    if (!state->fade_pos) return level;
    int32_t scaled = level;
    scaled -= map(state->fade_pos, 0, 44100*state->fade_len, 0, 0xaf); //TODO determine val
    if (scaled < 0) scaled = 0;
    return scaled;
}

static uint8_t opna_rhythm_apply_fade(opn_series_state_t *state, uint8_t tl) {
    if (!state->fade_pos) return tl;
    int32_t scaled = tl & 0x3f;
    scaled -= map(state->fade_pos, 0, 44100*state->fade_len, 0, 0x3f); //TODO determine val
    if (scaled < 0) scaled = 0;
    return scaled;
}

static void opn_common_dump_tls(opn_series_state_t *state) {
    for (uint8_t ch=0;ch<6;ch++) {
        uint8_t algo = state->fm_algo[ch];
        for (uint8_t op=0;op<4;op++) {
            uint8_t tl = state->fm_tl[(ch*4)+op];
            if (COMMON_OP_ALGO_IS_SLOT(op, algo)) tl = opn_common_apply_fade(state, tl);
            //on old driver this is only done for slot ops:
            state->write_func(state, COMMON_CHAN_TO_PORT(ch), COMMON_CHAN_TO_REG(0x40, ch)+(4*opn_op_map[op]), tl);
        }
    }
}

static void opn_common_filter_tl_write(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint8_t op = opn_op_map[(write->out_reg-0x40)/4];
    uint8_t ch = (3*write->out_port)+((write->out_reg-0x40)-(opn_op_map[op]*4));
    if (op > 3 || ch > 5) return; //drop write?
    uint8_t idx = (ch*4)+op;
    uint8_t algo = state->fm_algo[ch];
    state->fm_tl[idx] = write->out_val & 0x7f;
    if (COMMON_OP_ALGO_IS_SLOT(op, algo)) write->out_val = opn_common_apply_fade(state, state->fm_tl[idx]);
}

static void opn_common_handle_algo_write(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint8_t ch = COMMON_PORT_REG_TO_CHAN(write->out_port, write->out_reg);
    uint8_t old = state->fm_algo[ch];
    uint8_t new = write->out_val & 7;

    if (old != new) {
        //don't use opn_common_dump_tls here, only need to do it for the current ch
        for (uint8_t op=0;op<4;op++) {
            uint8_t savedtl = state->fm_tl[(ch*4)+op];
            if (COMMON_OP_ALGO_IS_SLOT(op, new)) savedtl = opn_common_apply_fade(state, savedtl);
            state->write_func(state, COMMON_CHAN_TO_PORT(ch), COMMON_CHAN_TO_REG(0x40, ch)+(4*opn_op_map[op]), savedtl);
        }
    }

    state->fm_algo[ch] = new;
}

static void opn_common_virt_write_parts(opn_series_state_t *state, chip_write_t *write) {
    if (INRANGE(write->out_reg, 0xb4, 0xb6)) { //pan, fms, ams
        opn_common_filter_pan_write(state, write);
    } else if (INRANGE(write->out_reg, 0xb0, 0xb2)) { //algo
        opn_common_handle_algo_write(state, write);
    } else if (INRANGE(write->out_reg, 0x40, 0x42) || INRANGE(write->out_reg, 0x48, 0x4a) || INRANGE(write->out_reg, 0x44, 0x46) || INRANGE(write->out_reg, 0x4c, 0x4e)) { //tl
        opn_common_filter_tl_write(state, write);
    }
}

static inline void opn2_filter_dac_fade(opn_series_state_t *state, chip_write_t *write) {
    if (!state->fade_pos) return;
    WRITE_CHECK_SHORT_CIRCUIT(write);
    int32_t sgn = write->out_val;
    sgn -= 0x7f;
    sgn *= 1000;
    int32_t sf = map(state->fade_pos, 0, 44100*state->fade_len, 0, 155);
    sf *= sf;
    sf += 1000;
    sgn /= sf;
    sgn += 0x7f;
    write->out_val = sgn;
}

void opn2_virt_write(opn_series_state_t *state, chip_write_t *write) {
    WRITE_COPY_IN_OUT(write);
    if (write->out_reg == 0x2b) { //dac en
        bool new = write->out_val & (1<<7);
        if (new != state->ch6_is_dac) {
            state->ch6_is_dac = new;
            opn_common_dump_pans(state);
        }
    } else if (write->out_reg == 0x2a) { //dac data
        opn2_filter_dac_pre(state, write);
        opn2_filter_dac_fade(state, write);
    }
    opn2_filter_test_reg(state, write);
    opn_common_virt_write_parts(state, write);
    opn2_filter_dedup(state, write);
    if (!write->drop) state->write_func(state, write->out_port, write->out_reg, write->out_val);
}

static uint8_t opna_adpcm_apply_muting(opn_series_state_t *state, uint8_t val) {
    if (!GETBIT(state->mute_mask, 7)) val &= 0b00111111;
    if (state->force_mono) COMMON_FORCE_MONO(val);
    return val;
}

static void opna_handle_adpcm_config(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    state->opna_adpcm_config.config = write->out_val;
    OPNA_SET_RAM_CONNECTION(write->out_val);
    write->out_val = opna_adpcm_apply_muting(state, write->out_val);
}

static void opna_handle_adpcm_start(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    if (!GETBIT(write->out_val, 7)) return;
    uint16_t wr_start = state->opna_adpcm_config.start_addr;
    uint16_t wr_stop = state->opna_adpcm_config.stop_addr;
    if (OPNA_BAD_RAM_CONNECTION(write->out_val)) {
        wr_start *= 8;
        wr_stop *= 8;
    }
    state->write_func(state, 1, 0x02, wr_start&0xff);
    state->write_func(state, 1, 0x03, wr_start>>8);
    state->write_func(state, 1, 0x04, wr_stop&0xff);
    state->write_func(state, 1, 0x05, wr_stop>>8);
}

static void opna_handle_adpcm_addr(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    switch (write->out_reg) {
        case 0x02:
            state->opna_adpcm_config.start_addr &= 0xff00;
            state->opna_adpcm_config.start_addr |= write->out_val;
            break;
        case 0x03:
            state->opna_adpcm_config.start_addr &= 0x00ff;
            state->opna_adpcm_config.start_addr |= (write->out_val<<8);
            break;
        case 0x04:
            state->opna_adpcm_config.stop_addr &= 0xff00;
            state->opna_adpcm_config.stop_addr |= write->out_val;
            break;
        case 0x05:
            state->opna_adpcm_config.stop_addr &= 0x00ff;
            state->opna_adpcm_config.stop_addr |= (write->out_val<<8);
            break;
        default:
            break;
    }
    write->drop = true;
}

static void opna_handle_adpcm_level(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    state->opna_adpcm_config.level = write->out_val;
    write->out_val = opna_adpcm_apply_fade(state, write->out_val);
}

static void opna_handle_rhythm_tl(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    state->opna_rhythm_config.tl = write->out_val & 0x3f; //mask not strictly necessary...
    write->out_val = opna_rhythm_apply_fade(state, write->out_val);
}

static uint8_t opna_rhythm_apply_muting(opn_series_state_t *state, uint8_t reg) {
    if (!GETBIT(state->mute_mask, 6)) reg &= 0b00111111;
    if (state->force_mono) COMMON_FORCE_MONO(reg);
    return reg;
}

static void opna_handle_rhythm_il(opn_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);
    uint8_t ch = write->out_reg-0x18;
    state->opna_rhythm_config.config[ch] = write->out_val;
    write->out_val = opna_rhythm_apply_muting(state, write->out_val);
}

void opna_virt_write(opn_series_state_t *state, chip_write_t *write) {
    WRITE_COPY_IN_OUT(write);
    if (write->out_port == 0 && write->out_reg <= 0x0d) {
        psg_virt_write(state->opna_psg_state, write);
        write->short_circuit = write->drop = true;
    } else if (write->out_port == 0 && write->out_reg == 0x11) {
        opna_handle_rhythm_tl(state, write);
    } else if (write->out_port == 0 && INRANGE(write->out_reg, 0x18, 0x1d)) {
        opna_handle_rhythm_il(state, write);
    } else if (write->out_port == 1 && write->out_reg == 0x01) {
        opna_handle_adpcm_config(state, write);
    } else if (write->out_port == 1 && write->out_reg == 0x00) {
        opna_handle_adpcm_start(state, write);
    } else if (write->out_port == 1 && INRANGE(write->out_reg, 0x02, 0x05)) {
        opna_handle_adpcm_addr(state, write);
    } else if (write->out_port == 1 && INRANGE(write->out_reg, 0x0c, 0x0d)) {
        //TODO LIMIT
        write->drop = true;
    } else if (write->out_port == 1 && write->out_reg == 0x0b) {
        opna_handle_adpcm_level(state, write);
    }
    opn_common_virt_write_parts(state, write);
    if (!write->drop) state->write_func(state, write->out_port, write->out_reg, write->out_val);
}

static void opna_dump_other_pans(opn_series_state_t *state) {
    //adpcm
    state->write_func(state, 1, 1, opna_adpcm_apply_muting(state, state->opna_adpcm_config.config));

    //rhythm
    for (uint8_t ch=0;ch<6;ch++) {
        state->write_func(state, 0, 0x18+ch, opna_rhythm_apply_muting(state, state->opna_rhythm_config.config[ch]));
    }
}

static void opn_common_dump_muting_data(opn_series_state_t *state) {
    opn_common_dump_pans(state);
}

static void opna_dump_muting_data(opn_series_state_t *state) {
    opna_dump_other_pans(state);
    //todo also call out to update ssg muting data here
}

static void opn_common_update_muting_data(opn_series_state_t *state, chip_muting_data_t data) {
    state->mute_mask = data & 0x7f;
    opn_common_dump_muting_data(state);
}

static void opna_update_muting_data(opn_series_state_t *state, chip_muting_data_t data) {
    opna_dump_muting_data(state);
}

static void opn_common_update_fade_data(opn_series_state_t *state, chip_fade_data_t data) {
    state->fade_pos = data.pos;
    state->fade_len = data.len;
    if (data.pos) opn_common_dump_tls(state);
}

static inline void opn_common_handle_sysmute_ioctl(opn_series_type_t type, opn_series_state_t *state) {
    opn_common_dump_muting_data(state);
    if (type == OPN_TYPE_OPNA) opna_dump_muting_data(state);
    if (type == OPN_TYPE_OPN2) opn2_fix_dac(state);
}

static void opna_dump_other_levels(opn_series_state_t *state) {
    state->write_func(state, 0, 0x11, opna_rhythm_apply_fade(state, state->opna_rhythm_config.tl));
    state->write_func(state, 1, 0x0b, opna_adpcm_apply_fade(state, state->opna_adpcm_config.level));
    //todo psg
}

//todo move opna specific into opna_ioctl
void opn_common_ioctl(opn_series_type_t type, opn_series_state_t *state, chip_ioctl_t ioctl, void *data) {
    switch (ioctl) {
        case CHIP_IOCTL_UPDATE_MUTING:
            opn_common_update_muting_data(state, *(chip_muting_data_t *)data);
            if (type == OPN_TYPE_OPNA) opna_update_muting_data(state, *(chip_muting_data_t *)data);
            break;
        case CHIP_IOCTL_UPDATE_FADE:
            opn_common_update_fade_data(state, *(chip_fade_data_t *)data);
            if (type == OPN_TYPE_OPNA) opna_dump_other_levels(state);
            break;
        case CHIP_IOCTL_PRE_PERIOD:
            state->in_pre_period = *(bool *)data;
            opn_common_handle_sysmute_ioctl(type, state);
            break;
        case CHIP_IOCTL_UPDATE_SLIP:
            state->slip = *(bool *)data;
            opn_common_handle_sysmute_ioctl(type, state);
            break;
        case CHIP_IOCTL_UPDATE_PAUSE:
            state->paused = *(bool *)data;
            opn_common_handle_sysmute_ioctl(type, state);
            break;
        case CHIP_IOCTL_OPN2_BLOCK_TEST_REG:
            state->test_reg_blocked = *(bool *)data;
            break;
        default:
            break;
    }
}

void opna_ioctl(opn_series_state_t *state, chip_ioctl_t ioctl, void *data) {
    opn_common_ioctl(OPN_TYPE_OPNA, state, ioctl, data);
    if (ioctl == CHIP_IOCTL_UPDATE_MUTING) {
        //psg_ioctl(state->opna_psg_state, CHIP_IOCTL_UPDATE_MUTING, /*get second byte*/); //TODO
        return;
    }
    psg_ioctl(state->opna_psg_state, ioctl, data);
}