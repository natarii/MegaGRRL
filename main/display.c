#include "display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pins.h"
#include "tftspi.h"
#include "tft.h"

static const char* TAG = "DisplayMgr";

spi_lobo_device_handle_t DisplayMgr_Spi;

bool DisplayMgr_Setup() {
    ESP_LOGI(TAG, "Setting up");

    ESP_LOGI(TAG, "Setting up GPIO");
    /*gpio_pad_select_gpio(PIN_DISP_CS);
    gpio_pad_select_gpio(PIN_DISP_MISO);
    gpio_pad_select_gpio(PIN_DISP_MOSI);
    gpio_pad_select_gpio(PIN_DISP_SCK);
    gpio_pad_select_gpio(PIN_DISP_DC);
    gpio_set_direction(PIN_DISP_MISO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_DISP_MISO, GPIO_PULLUP_ONLY);
    gpio_set_direction(PIN_DISP_CS, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DISP_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DISP_SCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DISP_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_DISP_DC, 0);*/
    TFT_PinsInit();

    ESP_LOGI(TAG, "Setting up HSPI");
    spi_lobo_device_handle_t spi;
    spi_lobo_bus_config_t bus = {
        .miso_io_num = PIN_DISP_MISO,
        .mosi_io_num = PIN_DISP_MOSI,
        .sclk_io_num = PIN_DISP_SCK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t inter = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = -1,
        .spics_ext_io_num = PIN_DISP_CS,
        .flags = LB_SPI_DEVICE_HALFDUPLEX,
    };

    esp_err_t ret;
    ESP_LOGI(TAG, "add dev");
    ret = spi_lobo_bus_add_device(TFT_HSPI_HOST, &bus, &inter, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "err");
        return false;
    }
    disp_spi = spi; //did i ever mention i hate this library?

    //loboris example does these and checks ret
    spi_lobo_device_select(spi, 1);
    spi_lobo_device_deselect(spi);

    ESP_LOGI(TAG, "disp init");
    TFT_display_init();

    spi_lobo_set_speed(spi, 25000000);

    /*TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
    TFT_setRotation(LANDSCAPE_FLIP);
    TFT_setFont(DEFAULT_FONT, NULL);
    TFT_resetclipwin();
    TFT_print("girls !!!!!!!", 50, 50);*/

    return true;
}