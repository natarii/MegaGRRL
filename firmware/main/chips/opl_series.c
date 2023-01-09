#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include "opl_series.h"
#include "natutils.h"
#include "esp_log.h"
#include "../driver.h" //only until board stuff is rewritten

static const char* TAG = "chip_opl_series";

//todo look into what happens in ch keyed on and then perc enabled. perc gets overridden???

//todo
//mute
//remap CD->AB
//force mono
//op/algo/ch determination cleanup

#define COMMON_PORT_REG_TO_CHAN(port, reg) ((port*9)+LONIB(reg))

static const uint8_t op_lut_4op[18*4] = {
    0,1,2,255,255,255,12,13,14,18,19,20,255,255,255,30,31,32,
    3,4,5,255,255,255,15,16,17,21,22,23,255,255,255,33,34,35,
    6,7,8,255,255,255,255,255,255,24,25,26,255,255,255,255,255,255,
    9,10,11,255,255,255,255,255,255,27,28,29,255,255,255,255,255,255
};

static const bool output_lut[6*4] = {
    //X = op 0~3, Y = algo 0~5
    0,1,0,0,
    1,1,0,0,
    0,0,0,1,
    1,0,0,1,
    0,1,0,1,
    1,0,1,1,
};

static const uint8_t op_to_reg[18] = {0,1,2,3,4,5,8,9,0xa,0xb,0xc,0xd,0x10,0x11,0x12,0x13,0x14,0x15};
static const uint8_t reg_to_op[22] = {0,1,2,3,4,5,0xff,0xff,6,7,8,9,10,11,0xff,0xff,12,13,14,15,16,17};

static bool opl3_ch_is_4op(opl_series_state_t *state, uint8_t ch) {
    if (ch > 14) return false;
    switch (ch) {
        case 0:
        case 3:
            return GETBIT(state->fourop_en, 0);
            break;
        case 1:
        case 4:
            return GETBIT(state->fourop_en, 1);
            break;
        case 2:
        case 5:
            return GETBIT(state->fourop_en, 2);
            break;
        case 9:
        case 12:
            return GETBIT(state->fourop_en, 3);
            break;
        case 10:
        case 13:
            return GETBIT(state->fourop_en, 4);
            break;
        case 11:
        case 14:
            return GETBIT(state->fourop_en, 5);
            break;
        default:
            return false;
            break;
    }
}

static bool opl3_4op_ch_is_low(uint8_t ch) {
    switch (ch) {
        case 0:
        case 1:
        case 2:
        case 9:
        case 10:
        case 11:
            return true;
            break;
        default:
            return false;
            break;
    }
}



static void opl3_phys_write(opl_series_state_t *state, uint8_t port, uint8_t reg, uint8_t val) {
    Driver_FmOutopl3(port, reg, val);
}

static void opl_common_init(opl_series_state_t *state, uint8_t hw_slot) {
    memset(state, 0, sizeof(opl_series_state_t));

    state->hw_slot = hw_slot;

    for (uint8_t i=0;i<18;i++) {
        state->fm_pan[i] = 0b00110000; //according to Nuked (not verified on hardware)
    }

    state->mute_mask = 0xffffffff;
}

static uint8_t opl_common_apply_fade(opl_series_state_t *state, uint8_t tl) {
    if (!state->fade_pos) return tl;
    uint8_t scaled = tl & 0x3f;
    scaled += map(state->fade_pos, 0, 44100*state->fade_len, 0, 0x20);
    if (scaled > 0x3f) scaled = 0x3f;
    return (tl & 0xc0) | scaled;
}

static uint8_t opl3_get_ch_theoretical_algo(opl_series_state_t *state, uint8_t ch) {
    if (opl3_ch_is_4op(state, ch)) {
        //channels in 4op mode require special handling
        if (!opl3_4op_ch_is_low(ch)) ch -= 3;
        return 2 + (state->fm_conn_bit[ch+3]<<1) + state->fm_conn_bit[ch];
    } else {
        return state->fm_conn_bit[ch];
    }
}

static bool opl_common_op_slot_is_output(opl_series_state_t *state, uint8_t opslot) {
    //if drums are enabled, we always know op slots 12~17 are drums (therefore, output)
    if (state->drum_en && INRANGE(opslot, 12, 17)) {
        //TODO verify bassdrum behavior - are both operators output? sounds like it
        //it's a drum, we should scale all of these
        return true;
    }

    //now do 4op checks
    uint8_t ch = 0xff;
    bool ch4op = false;
    uint8_t chop = 0xff;
    for (uint8_t i=0;i<18*4;i++) {
        if (op_lut_4op[i] == opslot) {
            //located this op slot in the 4op lut. so which channel is it on?
            ch = i%18;
            //if the channel isn't in 4op mode, then bail and find it in 2op mode
            ch4op = opl3_ch_is_4op(state, ch);
            if (!ch4op) break;
            //if we get here, ch is in 4op mode. which op on the channel (as in, 1 through 4) is it?
            chop = i/18;
            break;
        }
    }
    //if the found channel wasn't 4op, then it can't be the right one
    if (!ch4op) {
        uint8_t base = opslot/6;
        base *= 3; //can't just do opslot/2 - relying on the implicit "floor"
        ch = base + (opslot%3);
        chop = (opslot%6)>=3;
    }
    assert(ch<18);
    assert(chop<4);
    uint8_t ta = opl3_get_ch_theoretical_algo(state, ch);
    assert(ta<6);
    return output_lut[(ta*4)+chop];
    
    //ESP_LOGW(TAG, "op slot %d is ch %d (4op %d) op %d ta %d output %d", opslot, ch, ch4op, chop, ta, isoutput);
}

static void opl_common_filter_tl_write(opl_series_state_t *state, chip_write_t *write) {
    //find which op this is (step 1)
    uint8_t opslot = reg_to_op[write->out_reg-0x40];
    //op slots are not laid out linearly in the address space; there are gaps in the mapping. so make sure this is actually a valid position
    if (opslot == 0xff) {
        //should be safe to just drop this write. it's noop or UB anyway (todo: confirm on hw?)
        write->drop = true;
        return;
    }
    //finally, apply offset for the second bank (step 2)
    if (write->out_port) opslot += 18;
    assert(opslot<36);

    //save the unmodified value
    state->fm_tl[opslot] = write->out_val;

    if (opl_common_op_slot_is_output(state, opslot)) write->out_val = opl_common_apply_fade(state, write->out_val);
}

static void opl_common_dump_tls(opl_series_state_t *state) {
    uint8_t banks = (state->type==OPL_TYPE_OPL3)?2:1;
    for (uint8_t port=0;port<banks;port++) {
        for (uint8_t off=0;off<18;off++) {
            uint8_t opslot = (port*18)+off;
            uint8_t reg = 0x40+op_to_reg[off];
            if (opl_common_op_slot_is_output(state, opslot)) state->write_func(state, port, reg, opl_common_apply_fade(state, state->fm_tl[opslot]));
        }
    }
}

static void opl_common_filter_drum_write(opl_series_state_t *state, chip_write_t *write) {
    bool new = GETBIT(write->out_val, 5);
    if (state->drum_en != new) {
        //todo dump out all TLs because drums change ch<->op mappings...
        opl_common_dump_tls(state);
    }
    state->drum_en = new;
}

static void opl_common_filter_conn_write(opl_series_state_t *state, chip_write_t *write) {
    uint8_t ch = COMMON_PORT_REG_TO_CHAN(write->out_port, write->out_reg);
    uint8_t oldalgo = opl3_get_ch_theoretical_algo(state, ch);
    state->fm_conn_bit[ch] = GETBIT(write->out_val, 0);
    uint8_t newalgo = opl3_get_ch_theoretical_algo(state, ch);
    if (oldalgo != newalgo) {
        //algo change, we need to dump out TLs for this ch (and its sister ch if 4op)
        //todo: just doing all for now, probably ok?
        opl_common_dump_tls(state);
    }
}

void opl3_init(opl_series_state_t *state, uint8_t hw_slot) {
    opl_common_init(state, hw_slot);
    state->type = OPL_TYPE_OPL3;
    state->write_func = (void *)opl3_phys_write;
}

static void opl_common_apply_opl_compat_hacks(opl_series_state_t *state, chip_write_t *write) {
    //hacks for opl playback on opl2 and opl3

    WRITE_CHECK_SHORT_CIRCUIT(write);

    //opl doesn't have waveform select - fix any erroneous writes to the test reg
    if (write->out_reg == 0x01) CLEARBIT(write->out_val, 5);

    //and since there's no waveform select, don't allow it to touch the wave select reg at all
    if (INRANGE(write->out_reg, 0xe0, 0xf5)) write->drop = true;

    //if we're on opl3, further hacks will be applied in the opl2 hack func
}

static void opl_common_apply_opl2_compat_hacks(opl_series_state_t *state, chip_write_t *write) {
    //hacks for opl2 playback on opl3

    WRITE_CHECK_SHORT_CIRCUIT(write);

    //upper bank of registers does not exist on opl2
    //an opl < 3 tune is not capable of writing to the upper bank, so skip this check
    //if (write->out_port) write->drop = true;

    //opl2 has no pan bits at all - force LR on
    if (INRANGE(write->out_reg, 0xc0, 0xc8)) write->out_val |= 0b00110000;

    //opl2 doesn't have the extra four waveforms that opl3 does
    if (INRANGE(write->out_reg, 0xe0, 0xf5)) CLEARBIT(write->out_val, 2);
}

static uint8_t opl_common_filter_pan(opl_series_state_t *state, uint8_t ch, uint8_t pan) {
    if (!GETBIT(state->mute_mask, ch)) {
        pan &= 0xf;
    } else if (state->in_pre_period || state->slip || state->paused) {
        pan &= 0xf;
    }

    //todo force mono

    return pan;
}

static void opl_common_dump_pans(opl_series_state_t *state) {
    uint8_t banks = (state->type==OPL_TYPE_OPL3)?2:1;
    for (uint8_t port=0;port<banks;port++) {
        for (uint8_t off=0;off<9;off++) {
            uint8_t ch = COMMON_PORT_REG_TO_CHAN(port, off);
            state->write_func(state, port, 0xc0+off, opl_common_filter_pan(state, ch, state->fm_pan[ch]));
        }
    }
}

static inline void opl_common_dump_muting_data(opl_series_state_t *state) {
    opl_common_dump_pans(state);
}

static void opl_common_filter_pan_write(opl_series_state_t *state, chip_write_t *write) {
    WRITE_CHECK_SHORT_CIRCUIT(write);

    uint8_t ch = COMMON_PORT_REG_TO_CHAN(write->out_port, write->out_reg);
    state->fm_pan[ch] = write->out_val;

    write->out_val = opl_common_filter_pan(state, ch, write->out_val);
}

static void opl_common_virt_write(opl_series_state_t *state, chip_write_t *write) {
    if (write->out_port && write->out_reg == 0x05) {
        state->is_opl3_mode = GETBIT(write->out_val, 0);
        //todo update whatever
    } else if (write->out_port && write->out_reg == 0x04) {
        state->fourop_en = write->out_val;
        //todo update whatever
    } else if (INRANGE(write->out_reg, 0xc0, 0xc8)) {
        opl_common_filter_conn_write(state, write);
        opl_common_filter_pan_write(state, write);
    } else if (INRANGE(write->out_reg, 0x40, 0x55)) {
        opl_common_filter_tl_write(state, write);
    } else if (write->out_port == 0 && write->out_reg == 0xbd) {
        opl_common_filter_drum_write(state, write);
    }

    if (!write->drop) state->write_func(state, write->out_port, write->out_reg, write->out_val);
}

void opl3_virt_write(opl_series_state_t *state, chip_write_t *write) {
    WRITE_COPY_IN_OUT(write);

    //opl3 specific hacks
    switch (state->virt_type) {
        case OPL_TYPE_OPL:
            opl_common_apply_opl_compat_hacks(state, write);
        case OPL_TYPE_OPL2:
            opl_common_apply_opl2_compat_hacks(state, write);
            break;
        default:
            break;
    }

    opl_common_virt_write(state, write);
}

void opl_common_setup_virt_type(opl_series_type_t type, opl_series_state_t *state, opl_series_type_t virt_type) {
    state->virt_type = virt_type;
    if (state->type == OPL_TYPE_OPL3) {
        if (virt_type != OPL_TYPE_OPL3) {
            //playing opl/opl2 tune on opl3

            //inject a write to set "NEW" mode, so we have access to panning
            state->write_func(state, 1, 0x05, 1);

            opl_common_dump_muting_data(state);
        }
    }
}

static void opl_common_update_muting_data(opl_series_state_t *state, chip_muting_data_t data) {
    state->mute_mask = data;
    opl_common_dump_muting_data(state);
}

static void opl_common_update_fade_data(opl_series_state_t *state, chip_fade_data_t data) {
    state->fade_pos = data.pos;
    state->fade_len = data.len;
    if (data.pos) opl_common_dump_tls(state);
}

static inline void opl_common_handle_sysmute_ioctl(opl_series_state_t *state) {
    opl_common_dump_muting_data(state);
}

void opl_common_ioctl(opl_series_type_t type, opl_series_state_t *state, chip_ioctl_t ioctl, void *data) {
    switch (ioctl) {
        case CHIP_IOCTL_SET_VIRT_TYPE:
            opl_common_setup_virt_type(type, state, *(opl_series_type_t *)data);
            break;
        case CHIP_IOCTL_UPDATE_MUTING:
            opl_common_update_muting_data(state, *(chip_muting_data_t *)data);
            break;
        case CHIP_IOCTL_UPDATE_FADE:
            opl_common_update_fade_data(state, *(chip_fade_data_t *)data);
            break;
        case CHIP_IOCTL_PRE_PERIOD:
            state->in_pre_period = *(bool *)data;
            opl_common_handle_sysmute_ioctl(state);
            break;
        case CHIP_IOCTL_UPDATE_SLIP:
            state->slip = *(bool *)data;
            opl_common_handle_sysmute_ioctl(state);
            break;
        case CHIP_IOCTL_UPDATE_PAUSE:
            state->paused = *(bool *)data;
            opl_common_handle_sysmute_ioctl(state);
            break;
        default:
            break;
    }
}