#include <string.h>
#include "dcsg.h"
#include "common.h"
#include "natutils.h"
#include "esp_log.h"
#include "../driver.h" //only until board stuff is rewritten

static const char* TAG = "chip_dcsg";

//todo other branches in runcommand

#define FIX_FREQ0 ((state->fix_flags & DCSG_FLAG_SPECIAL_FREQ0) || (state->fix_flags & DCSG_FLAG_ASSUME_SEGA))
#define GET_CH(data) ((data>>5)&3)

static void dcsg_phys_write(dcsg_state_t *state, uint8_t data) {
    //ESP_LOGW(TAG, "wr %02x", data);
    Driver_DcsgOutDEV(data);
}

void dcsg_init(dcsg_state_t *state, uint8_t hw_slot) {
    memset(state, 0, sizeof(dcsg_state_t));
    state->write_func = (void *)dcsg_phys_write;
    for (uint8_t i=0;i<4;i++)
        state->atten[i] = 0b10011111 | (i<<5);
    state->mute_mask = 0xf;
    state->fix_flags = DCSG_FLAG_ASSUME_SEGA | DCSG_FLAG_SPECIAL_FREQ0;
}

static void dcsg_write_freq(dcsg_state_t *state, uint8_t ch, uint16_t freq) {
    if (freq == 0 && FIX_FREQ0) freq = 1;

    //low half
    uint8_t out = 0x80 | (ch << 5) | LONIB(freq);
    state->write_func(state, out);

    //high half
    out = (freq >> 4) & 0x3f;
    state->write_func(state, out);
}

static void dcsg_dump_ch3(dcsg_state_t *state) {
    //get current ch3 freq
    uint32_t freq = state->freq[2];

    //should we modify it?
    //todo: this will need further abstraction if different dcsg hardware is ever supported
    bool wants_fix = (state->fix_flags & DCSG_FLAG_ASSUME_SEGA) || (state->tune_sr_width != 15);
    if (wants_fix && state->noise_is_periodic && state->noise_source_ch3 && LONIB(state->atten[2]) == 0xf) {
        if (state->tune_sr_width == 16 || (state->fix_flags & DCSG_FLAG_ASSUME_SEGA)) {
            //6.25% fudge
            freq *= 10625;
            freq /= 10000;
        } else if(state->tune_sr_width == 17) {
            //13.33% fudge
            freq *= 11333;
            freq /= 10000;
        }
        //currently, values other than 15~17 are clamped in Player
    }

    //write to the chip
    dcsg_write_freq(state, 2, freq);
}

static void dcsg_filter_freq_hi(dcsg_state_t *state, chip_write_t *write) {
    //build the new frequency value
    uint16_t freq = LONIB(state->freq[state->latched_ch]) | ((write->out_val & 0x3f) << 4);
    state->freq[state->latched_ch] = freq;

    //write to the chip
    if (state->latched_ch == 2) {
        //special behavior for ch3
        dcsg_dump_ch3(state);
    } else {
        dcsg_write_freq(state, state->latched_ch, state->freq[state->latched_ch]);
    }
}

static uint8_t dcsg_apply_fade(dcsg_state_t *state, uint8_t atten) {
    if (!state->fade_pos) return atten;

    //mask off everything except the actual attenuation value. this way, we can pass a full chip write through this func
    uint32_t scaled = atten & 0xf;
    
    //do the deed
    scaled += map(state->fade_pos, 0, 44100*state->fade_len, 0, 0xf);
    if (scaled > 0xf) scaled = 0xf;

    //build the output value, subbing in the new atten
    return (atten & 0xf0) | scaled;
}

static uint8_t dcsg_filter_atten(dcsg_state_t *state, uint8_t ch, uint8_t atten) {
    //apply fade
    atten = dcsg_apply_fade(state, atten);
    
    //is chan muted by user?
    if (!GETBIT(state->mute_mask, ch)) atten |= 0xf;

    //does system want it muted?
    if (state->in_pre_period || state->slip || state->paused) atten |= 0xf;

    return atten;
}

static void dcsg_handle_atten_write(dcsg_state_t *state, chip_write_t *write) {
    uint8_t ch = GET_CH(write->out_val);

    //backup the value from the tune
    state->atten[ch] = write->out_val;

    //apply filters
    write->out_val = dcsg_filter_atten(state, ch, write->out_val);

    //if ch3 atten is updated, we need to write its frequency again due to periodic noise fixes
    //todo: only if it transitions in/out of full atten
    if (ch == 2) dcsg_dump_ch3(state);
}

static void dcsg_dump_attens(dcsg_state_t *state) {
    dcsg_dump_ch3(state);
    for (uint8_t i=0;i<4;i++) state->write_func(state, 0x80 | (i<<5) | (1<<4) | dcsg_filter_atten(state, i, state->atten[i]));
}

static void dcsg_filter_noise_ctrl(dcsg_state_t *state, chip_write_t *write) {
    state->noise_is_periodic = !GETBIT(write->out_val, 2);
    state->noise_source_ch3 = (write->out_val & 3) == 3;

    //update ch3 frequency
    //todo: only if the above values have actually changed
    dcsg_dump_ch3(state);
}

void dcsg_virt_write(dcsg_state_t *state, chip_write_t *write) {
    WRITE_COPY_IN_OUT(write);
    if (!GETBIT(write->out_val, 7)) { //freq hi byte
        dcsg_filter_freq_hi(state, write);
    } else {
        if (!GETBIT(write->out_val, 4) && GET_CH(write->out_val) != 3) { //ch1~3 freq lo byte
            state->latched_ch = GET_CH(write->out_val);
            state->freq[state->latched_ch] = (state->freq[state->latched_ch] & 0x3f0) | LONIB(write->out_val);
            dcsg_write_freq(state, state->latched_ch, state->freq[state->latched_ch]);
        } else { //atten, noise control
            if (GETBIT(write->out_val, 4)) { //atten
                dcsg_handle_atten_write(state, write);
            } else if (HINIB(write->out_val) == 0xe0) { //noise ctrl
                dcsg_filter_noise_ctrl(state, write);
            }
        }
    }
    if (!write->drop) state->write_func(state, write->out_val);
}

void dcsg_update_fade_data(dcsg_state_t *state, chip_fade_data_t data) {
    state->fade_pos = data.pos;
    state->fade_len = data.len;
    if (data.pos) dcsg_dump_attens(state);
}

void dcsg_ioctl(dcsg_state_t *state, chip_ioctl_t ioctl, void *data) {
    switch (ioctl) {
        case CHIP_IOCTL_UPDATE_MUTING:
            state->mute_mask = *(uint8_t *)data;
            dcsg_dump_attens(state);
            break;
        case CHIP_IOCTL_UPDATE_FADE:
            dcsg_update_fade_data(state, *(chip_fade_data_t *)data);
            break;
        case CHIP_IOCTL_PRE_PERIOD:
            state->in_pre_period = *(bool *)data;
            dcsg_dump_attens(state);
            break;
        case CHIP_IOCTL_UPDATE_SLIP:
            state->slip = *(bool *)data;
            dcsg_dump_attens(state);
            break;
        case CHIP_IOCTL_UPDATE_PAUSE:
            state->paused = *(bool *)data;
            dcsg_dump_attens(state);
            break;
        case CHIP_IOCTL_DCSG_SET_FIX_FLAGS:
            state->fix_flags = *(dcsg_fix_flags_t *)data;
            break;
        case CHIP_IOCTL_DCSG_SET_SR_WIDTH:
            state->tune_sr_width = *(uint8_t *)data;
            break;
        default:
            break;
    }
}