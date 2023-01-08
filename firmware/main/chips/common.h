#pragma once

//todo make fade len in samples

#define WRITE_COPY_IN_OUT(write) write->out_port = write->in_port; write->out_reg = write->in_reg; write->out_val = write->in_val;
#define CHECK_SHORT_CIRCUIT if (write->short_circuit) return;

typedef struct {
    uint8_t in_port;
    uint8_t in_reg;
    uint8_t in_val;
    bool direct;
    uint8_t out_port;
    uint8_t out_reg;
    uint8_t out_val;
    bool drop;
    bool short_circuit;
} chip_write_t;

typedef enum {
    CHIP_IOCTL_UPDATE_MUTING,
    CHIP_IOCTL_UPDATE_FADE,
    CHIP_IOCTL_OPN2_BLOCK_TEST_REG,
    CHIP_IOCTL_PRE_PERIOD,
    CHIP_IOCTL_UPDATE_SLIP,
    CHIP_IOCTL_UPDATE_PAUSE,
    CHIP_IOCTL_DCSG_SET_SR_WIDTH,
    CHIP_IOCTL_DCSG_SET_FIX_FLAGS,
} chip_ioctl_t;

typedef uint32_t chip_muting_data_t;

typedef struct {
    uint32_t pos;
    uint32_t len;
} chip_fade_data_t;