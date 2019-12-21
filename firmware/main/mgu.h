#ifndef AGR_MGU_H
#define AGR_MGU_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"

typedef struct {
    uint32_t magic;
    char hardware_version;
    uint8_t ver_length;
    uint32_t app_size;
    uint32_t updater_size;
    uint32_t app_sum;
    uint32_t updater_sum;
} mgu_header_t;

#endif