#include "clk.h"
#include "driver/ledc.h"
#include "pins.h"

uint8_t Clk_GPIOs[2] = {PIN_CLK_FM,PIN_CLK_PSG};
bool Clk_ffinstalled = false;
uint32_t Clk_Last[2] = {0,0};

void Clk_Set(uint8_t ch, uint32_t freq) {
    if (freq == 0) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, ch, 0);
        return;
    }
    if (Clk_Last[ch] == freq) return;
    ledc_timer_config_t config = {
        .duty_resolution = LEDC_TIMER_1_BIT,
        .freq_hz = freq,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = ch,
    };
    ledc_channel_config_t chc = {
        .channel = ch,
        .duty = 0,
        .gpio_num = Clk_GPIOs[ch],
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel = ch,
    };
    if (!Clk_ffinstalled) {
        ledc_fade_func_install(0);
        Clk_ffinstalled = true;
    }
    ledc_timer_config(&config);
    ledc_channel_config(&chc);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, ch, 1, 1);
    Clk_Last[ch] = freq;
}