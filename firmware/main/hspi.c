#include "hspi.h"
#include "pins.h"
#include "esp_log.h"
#include "driver/spi_master.h"

static const char* TAG = "Hspi";

static spi_bus_config_t HspiBusConfig = {
    .miso_io_num = -1,
    .mosi_io_num = PIN_DISP_MOSI,
    .sclk_io_num = PIN_DISP_SCK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4094,
};

void Hspi_Setup() {
    esp_err_t ret = spi_bus_initialize(HSPI_HOST, &HspiBusConfig, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Spi bus init fail !! 0x%x", ret);
        return;
    }
}
