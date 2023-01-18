#pragma once

//todo make fade len in samples
//todo missing short circuit checks everywhere
//todo clock stuff
//todo change ioctls to not use defines so they can be referred to by using fn ptrs? each variant should have its own fn and virtwrite prolly

#define WRITE_COPY_IN_OUT(write) write->out_port = write->in_port; write->out_reg = write->in_reg; write->out_val = write->in_val;
#define WRITE_CHECK_SHORT_CIRCUIT(write) if (write->drop || write->short_circuit) return;

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
    CHIP_IOCTL_SET_VIRT_TYPE,
} chip_ioctl_t;

typedef uint32_t chip_muting_data_t;

typedef struct {
    uint32_t pos;
    uint32_t len;
} chip_fade_data_t;