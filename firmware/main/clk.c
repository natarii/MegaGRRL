#include "clk.h"
#include "driver/ledc.h"
#include "pins.h"
#include "esp_log.h"
#include "freertos/semphr.h"

static const char* TAG = "ClkMgr";

static const uint8_t Clk_GPIOs[2] = {PIN_CLK_FM,PIN_CLK_PSG};
static bool Clk_ffinstalled = false;
static IRAM_ATTR uint32_t clk[2] = {0,0};
static int16_t mult[2] = {0,0};
static bool init_ledc[2] = {false, false};
static bool init_i2s = false;
volatile bool Clk_Unlock = false;
volatile bool lockout[2] = {false, false};
static StaticSemaphore_t mutexbuffer;
static SemaphoreHandle_t mutex = NULL;

#include "soc/efuse_reg.h"
#include "soc/rtc.h"
#include "driver/i2s.h"
#include "freertos/task.h"
#define APLL_MIN_FREQ                     (250000000)
#define APLL_MAX_FREQ                     (500000000)
#define APLL_I2S_MIN_RATE                 (10675)
#include "soc/dport_reg.h"
static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};
//these two functions are ripped out of the espressif i2s driver:
static float i2s_apll_get_fi2s(int bits_per_sample, int sdm0, int sdm1, int sdm2, int odir)
{
    int f_xtal = (int)rtc_clk_xtal_freq_get() * 1000000;
    uint32_t is_rev0 = (GET_PERI_REG_BITS2(EFUSE_BLK0_RDATA3_REG, 1, 15) == 0);
    if (is_rev0) {
        sdm0 = 0;
        sdm1 = 0;
    }
    float fout = f_xtal * (sdm2 + sdm1 / 256.0f + sdm0 / 65536.0f + 4);
    if (fout < APLL_MIN_FREQ || fout > APLL_MAX_FREQ) {
        return APLL_MAX_FREQ;
    }
    float fpll = fout / (2 * (odir+2)); //== fi2s (N=1, b=0, a=1)
    return fpll/2;
}

static esp_err_t i2s_apll_calculate_fi2s(int rate, int bits_per_sample, int *sdm0, int *sdm1, int *sdm2, int *odir)
{
    int _odir, _sdm0, _sdm1, _sdm2;
    float avg;
    float min_rate, max_rate, min_diff;
    if (rate/bits_per_sample/2/8 < APLL_I2S_MIN_RATE) {
        return ESP_ERR_INVALID_ARG;
    }

    *sdm0 = 0;
    *sdm1 = 0;
    *sdm2 = 0;
    *odir = 0;
    min_diff = APLL_MAX_FREQ;

    for (_sdm2 = 4; _sdm2 < 9; _sdm2 ++) {
        max_rate = i2s_apll_get_fi2s(bits_per_sample, 255, 255, _sdm2, 0);
        min_rate = i2s_apll_get_fi2s(bits_per_sample, 0, 0, _sdm2, 31);
        avg = (max_rate + min_rate)/2;
        if (abs(avg - rate) < min_diff) {
            min_diff = abs(avg - rate);
            *sdm2 = _sdm2;
        }
    }
    min_diff = APLL_MAX_FREQ;
    for (_odir = 0; _odir < 32; _odir ++) {
        max_rate = i2s_apll_get_fi2s(bits_per_sample, 255, 255, *sdm2, _odir);
        min_rate = i2s_apll_get_fi2s(bits_per_sample, 0, 0, *sdm2, _odir);
        avg = (max_rate + min_rate)/2;
        if (abs(avg - rate) < min_diff) {
            min_diff = abs(avg - rate);
            *odir = _odir;
        }
    }
    min_diff = APLL_MAX_FREQ;
    for (_sdm2 = 4; _sdm2 < 9; _sdm2 ++) {
        max_rate = i2s_apll_get_fi2s(bits_per_sample, 255, 255, _sdm2, *odir);
        min_rate = i2s_apll_get_fi2s(bits_per_sample, 0, 0, _sdm2, *odir);
        avg = (max_rate + min_rate)/2;
        if (abs(avg - rate) < min_diff) {
            min_diff = abs(avg - rate);
            *sdm2 = _sdm2;
        }
    }

    min_diff = APLL_MAX_FREQ;
    for (_sdm1 = 0; _sdm1 < 256; _sdm1 ++) {
        max_rate = i2s_apll_get_fi2s(bits_per_sample, 255, _sdm1, *sdm2, *odir);
        min_rate = i2s_apll_get_fi2s(bits_per_sample, 0, _sdm1, *sdm2, *odir);
        avg = (max_rate + min_rate)/2;
        if (abs(avg - rate) < min_diff) {
            min_diff = abs(avg - rate);
            *sdm1 = _sdm1;
        }
    }

    min_diff = APLL_MAX_FREQ;
    for (_sdm0 = 0; _sdm0 < 256; _sdm0 ++) {
        avg = i2s_apll_get_fi2s(bits_per_sample, _sdm0, *sdm1, *sdm2, *odir);
        if (abs(avg - rate) < min_diff) {
            min_diff = abs(avg - rate);
            *sdm0 = _sdm0;
        }
    }

    return ESP_OK;
}

bool Clk_CreateMutex() {
    ESP_LOGI(TAG, "Creating mutex");
    mutex = xSemaphoreCreateMutexStatic(&mutexbuffer);
    if (mutex == NULL) {
        ESP_LOGE(TAG, "Mutex creation failed !!");
        return false;
    }
    ESP_LOGI(TAG, "Mutex created");
    return true;
}

static bool takemutex() {
    return xSemaphoreTake(mutex, pdMS_TO_TICKS(10000)) == pdTRUE;
}

static bool givemutex() {
    return xSemaphoreGive(mutex) == pdTRUE;
}

static void setup_i2s(uint32_t startup_f) {
    periph_module_enable(PERIPH_I2S0_MODULE);
    I2S[0]->conf.tx_reset = 1;
    I2S[0]->conf.tx_reset = 0;
    I2S[0]->conf.rx_reset = 1;
    I2S[0]->conf.rx_reset = 0;
    I2S[0]->lc_conf.in_rst = 1;
    I2S[0]->lc_conf.in_rst = 0;
    I2S[0]->lc_conf.out_rst = 1;
    I2S[0]->lc_conf.out_rst = 0;
    I2S[0]->lc_conf.check_owner = 0;
    I2S[0]->lc_conf.out_loop_test = 0;
    I2S[0]->lc_conf.out_auto_wrback = 0;
    I2S[0]->lc_conf.out_data_burst_en = 0;
    I2S[0]->lc_conf.outdscr_burst_en = 0;
    I2S[0]->lc_conf.out_no_restart_clr = 0;
    I2S[0]->lc_conf.indscr_burst_en = 0;
    I2S[0]->lc_conf.out_eof_mode = 1;
    I2S[0]->conf2.lcd_en = 0;
    I2S[0]->conf2.camera_en = 0;
    I2S[0]->pdm_conf.pcm2pdm_conv_en = 0;
    I2S[0]->pdm_conf.pdm2pcm_conv_en = 0;
    I2S[0]->fifo_conf.dscr_en = 0;
    I2S[0]->conf_chan.tx_chan_mod = 0;
    I2S[0]->fifo_conf.tx_fifo_mod = 0;
    I2S[0]->conf.tx_mono = 0;
    I2S[0]->conf_chan.rx_chan_mod = 0;
    I2S[0]->fifo_conf.rx_fifo_mod = 0;
    I2S[0]->conf.rx_mono = 0;
    I2S[0]->fifo_conf.dscr_en = 0;
    I2S[0]->conf.tx_start = 0;
    I2S[0]->conf.rx_start = 0;
    I2S[0]->conf.tx_msb_right = 0;
    I2S[0]->conf.tx_right_first = 0;
    I2S[0]->conf.tx_slave_mod = 0;
    I2S[0]->fifo_conf.tx_fifo_mod_force_en = 0;
    I2S[0]->clkm_conf.clkm_div_num = 1;
    I2S[0]->clkm_conf.clkm_div_b = 0;
    I2S[0]->clkm_conf.clkm_div_a = 1;
    I2S[0]->clkm_conf.clka_en = 1;
    I2S[0]->sample_rate_conf.tx_bits_mod = 0;
    I2S[0]->sample_rate_conf.tx_bck_div_num = 1;
    I2S[0]->sample_rate_conf.rx_bck_div_num = 1;
    I2S[0]->int_ena.out_eof = 0;
    I2S[0]->int_ena.out_dscr_err = 0;

    int sdm0=0, sdm1=0, sdm2=0, odir=0;
    i2s_apll_calculate_fi2s(startup_f*2, 1, &sdm0, &sdm1, &sdm2, &odir);
    //double fi2s_rate = i2s_apll_get_fi2s(1, sdm0, sdm1, sdm2, odir);

    //in case it wasn't already set by LEDC:
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[PIN_CLK_FM], PIN_FUNC_GPIO);
    gpio_set_direction(PIN_CLK_FM, GPIO_MODE_DEF_OUTPUT);

    rtc_clk_apll_enable(true, sdm0, sdm1, sdm2, odir);

    I2S[0]->out_link.start = 1;
    I2S[0]->conf.tx_start = 1;
}

static void setup_ledc(uint8_t ch, uint32_t startup_f) {
    ledc_timer_config_t config = {
        .duty_resolution = LEDC_TIMER_1_BIT,
        .freq_hz = startup_f,
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

static void set_freq_i2s(uint32_t f) {
    int sdm0=0, sdm1=0, sdm2=0, odir=0;
    i2s_apll_calculate_fi2s(f*2, 1, &sdm0, &sdm1, &sdm2, &odir);
    rtc_clk_apll_enable(true, sdm0, sdm1, sdm2, odir);
}

static void set_freq_ledc(uint8_t ch, uint32_t f) {
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, ch, f);
}

static void ch0_output_i2s() {
    gpio_matrix_out(PIN_CLK_FM, I2S0O_BCK_OUT_IDX, 0, 0);
}

static void ch0_output_ledc() {
    gpio_matrix_out(PIN_CLK_FM, LEDC_HS_SIG_OUT0_IDX, 0, 0);
}

static bool test_i2s_freq(uint32_t f) {
    if (GET_PERI_REG_BITS2(EFUSE_BLK0_RDATA3_REG, 1, 15) == 0) {
        ESP_LOGW(TAG, "i2s clock not allowed on rev.0 silicon!");
        return false;
    }
    int sdm0=0, sdm1=0, sdm2=0, odir=0;
    i2s_apll_calculate_fi2s(f*2, 1, &sdm0, &sdm1, &sdm2, &odir);
    double fi2s_rate = i2s_apll_get_fi2s(1, sdm0, sdm1, sdm2, odir);
    uint32_t r = (uint32_t)fi2s_rate;
    r /= 2;
    int32_t d = f - r;
    if (d < 0) d = -d;
    return (d < 10);
}

static void set_freq(uint8_t ch, uint32_t f) {
    if (f == 0) return;
    if (ch == 0) {
        //see if i2s is suitable for this freq
        bool ok = test_i2s_freq(f);
        if (ok) { //i2s is close enough
            ESP_LOGI(TAG, "CH0 freq is fine for i2s");
            if (!init_i2s) { //need to init i2s, this will also set the freq without anything further
                ESP_LOGI(TAG, "CH0 i2s init");
                setup_i2s(f);
                init_i2s = true;
            } else { //already done, just update freq
                ESP_LOGI(TAG, "CH0 i2s set freq");
                set_freq_i2s(f);
            }
            ch0_output_i2s(); //setup_i2s doesn't update the output
        } else { //i2s too far off - need to use ledc
            ESP_LOGW(TAG, "CH0 cannot use i2s");
            if (!init_ledc[0]) { //need to init i2s, this will handle setting freq too
                ESP_LOGI(TAG, "CH0 ledc init");
                setup_ledc(0, f);
                init_ledc[0] = true;
            } else { //just update
                ESP_LOGI(TAG, "CH0 ledc set freq");
                set_freq_ledc(0, f);
                ch0_output_ledc(); //in case we were on i2s
            }
        }
    } else {
        if (!init_ledc[1]) {
            ESP_LOGI(TAG, "CH1 ledc init");
            setup_ledc(1, f);
            init_ledc[1] = true;
        } else {
            ESP_LOGI(TAG, "CH1 ledc set freq");
            set_freq_ledc(1, f);
        }
    }
}

void Clk_Set(uint8_t ch, uint32_t freq) {
    takemutex();

    clk[ch] = freq;

    uint64_t f = freq;
    f *= (1000LL + mult[ch]);
    f /= 1000;

    set_freq(ch, f);

    givemutex();
}

void Clk_TempSet(uint8_t ch, uint32_t freq) { //NO MULT
    takemutex();

    lockout[ch] = true;

    set_freq(ch, freq);

    givemutex();
}

void Clk_Restore(uint8_t ch) {
    lockout[ch] = false;
    Clk_Set(ch, clk[ch]);
}

uint32_t Clk_GetCh(uint8_t ch) {
    //don't take a mutex here even though we technically should. this gets called super fast in driver
    uint64_t f = clk[ch];
    if (f == 0) return 0;
    f *= (1000LL + mult[ch]);
    f /= 1000;
    return f;
}

void fastupdate(uint8_t ch) {
    uint64_t f = clk[ch];
    if (f == 0) return;
    f *= (1000LL + mult[ch]);
    f /= 1000;

    set_freq(ch, f);
}

void Clk_AdjustMult(uint8_t ch, int16_t newmult) {
    takemutex();

    mult[ch] = newmult;
    if (clk[ch] && !lockout[ch]) {
        fastupdate(ch);
    }

    givemutex();
}