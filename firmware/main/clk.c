#include "clk.h"
#include "driver/ledc.h"
#include "pins.h"
#include "esp_log.h"

static const char* TAG = "ClkMgr";

static const uint8_t Clk_GPIOs[2] = {PIN_CLK_FM,PIN_CLK_PSG};
static bool Clk_ffinstalled = false;
static uint32_t clk[2] = {0,0};
static int16_t mult[2] = {0,0};
volatile bool Clk_Unlock = false;

//TODO fix races on running adjustmult from another task

void Clk_Set(uint8_t ch, uint32_t freq) {
    clk[ch] = freq;
    if (freq == 0) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, ch, 0);
        return;
    }
    uint64_t f = freq;                                                                                                              
    f *= (1000LL + mult[ch]);                                                                                                       
    f /= 1000;
    ledc_timer_config_t config = {
        .duty_resolution = LEDC_TIMER_1_BIT,
        .freq_hz = f,
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
}

uint32_t Clk_GetCh1() {
    uint64_t f = clk[1];
    f *= (1000LL + mult[1]);
    f /= 1000;
    return f;
}

void fastupdate(uint8_t ch) {
    uint64_t f = clk[ch];
    f *= (1000LL + mult[ch]);
    f /= 1000;
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, ch, f);
}

void Clk_AdjustMult(uint8_t ch, int16_t newmult) {
    mult[ch] = newmult;
    if (clk[ch]) {
        fastupdate(ch);
    }
}