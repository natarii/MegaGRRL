#pragma once
#include <stdint.h>
#include <stdbool.h>
//todo make fade len in samples
//todo missing short circuit checks everywhere
//todo clock stuff
//todo change ioctls to not use defines so they can be referred to by using fn ptrs? each variant should have its own fn and virtwrite prolly <- bruh what

#define WRITE_COPY_IN_OUT(write) write->out_addr = write->in_addr; write->out_reg = write->in_reg; write->out_val = write->in_val;
#define WRITE_CHECK_SHORT_CIRCUIT(write) if (write->drop || write->short_circuit) return;

/*typedef enum {
    CHIP_NONE,
    CHIP_OPL,
    CHIP_OPL2,
    CHIP_OPL3,
    CHIP_OPLL,
    CHIP_OPN,
    CHIP_OPN2,
    CHIP_OPNA,
    CHIP_PSG,
    CHIP_DCSG,
} chip_t;*/

typedef struct {
    uint8_t in_addr;
    uint8_t in_reg;
    uint8_t in_val;
    bool direct;
    uint8_t out_addr;
    uint8_t out_reg;
    uint8_t out_val;
    bool drop;
    bool short_circuit;
    uint8_t hw_slot;
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
    CHIP_IOCTL_SPECIAL_RESET,
} chip_ioctl_t;

typedef uint32_t chip_muting_data_t;

typedef struct {
    uint32_t pos;
    uint32_t len;
} chip_fade_data_t;