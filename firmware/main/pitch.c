#include "pitch.h"
#include "clk.h"
#include "driver.h"
#include "esp_log.h"

static const char* TAG = "Pitch";

static int16_t mult = 0;

int16_t Pitch_Adjust(int16_t newmult) {
    ESP_LOGI(TAG, "New pitch requested: %f", (float)newmult/10.0f);
    if (newmult < -200) {
        newmult = -200;
    } else {
        if (Clk_Unlock && newmult > 200) {
            newmult = 200;
        } else if (!Clk_Unlock && newmult > 50) {
            newmult = 50;
        }
    }
    mult = newmult;
    Driver_SpeedMult = mult;
    Clk_AdjustMult(0, mult);
    Clk_AdjustMult(1, mult);
    ESP_LOGI(TAG, "New pitch set: %f", (float)mult/10.0f);
    return mult;
}

int16_t Pitch_Get() {
    return mult;
}