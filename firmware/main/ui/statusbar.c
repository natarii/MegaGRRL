#include "freertos/FreeRTOS.h"
#include "statusbar.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../battery.h"
#include "../ioexp.h"
#include "../hal.h"

static const char* TAG = "Ui_StatusBar";

lv_obj_t *container;
#if defined HWVER_PORTABLE
lv_obj_t *charge;
lv_obj_t *battery;
lv_obj_t *voltage;
#endif
char tempbuf[10];

bool Ui_StatusBar_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    static lv_style_t style;
    lv_style_copy(&style, &lv_style_plain);
    style.body.main_color = LV_COLOR_MAKE(0,0,64);
    style.body.grad_color = LV_COLOR_MAKE(0,0,127);
    style.body.radius = 0;
    style.text.font = &lv_font_dejavu_20;
    style.text.color = LV_COLOR_MAKE(255,255,255);
    lv_cont_set_style(container, &style);
    lv_obj_set_height(container, 34);
    lv_obj_set_width(container, 240);
    lv_cont_set_fit(container, false, false);

    #if defined HWVER_PORTABLE
    charge = lv_label_create(container, NULL);
    lv_obj_set_pos(charge, 2, 5);
    lv_label_set_text(charge, SYMBOL_CHARGE);
    lv_obj_set_width(charge, 24);

    battery = lv_label_create(container, NULL);
    lv_obj_set_pos(battery, 2+24+2, 5);
    lv_label_set_text(battery, SYMBOL_BATTERY_3);
    lv_obj_set_width(charge, 24);

    voltage = lv_label_create(container, NULL);
    lv_obj_set_pos(voltage, 2+24+2+24+2+5/*?*/, 5);
    lv_label_set_text(voltage, "????mV");
    lv_obj_set_width(voltage, 100);
    #endif

    LcdDma_Mutex_Give();
    return true;
}

uint32_t last = 0;
void Ui_StatusBar_Tick() {
    if (xthal_get_ccount() - last >= 240000000) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        #if defined HWVER_PORTABLE
        if (IoExp_ChargeStatus()) {
            lv_label_set_text(charge, SYMBOL_CHARGE);
        } else {
            lv_label_set_text(charge, "");
        }
        if (BatteryMgr_Voltage >= 4000) {
            lv_label_set_text(battery, SYMBOL_BATTERY_FULL);
        } else if (BatteryMgr_Voltage >= 3600) {
            lv_label_set_text(battery, SYMBOL_BATTERY_3);
        } else if (BatteryMgr_Voltage >= 3400) {
            lv_label_set_text(battery, SYMBOL_BATTERY_2);
        } else if (BatteryMgr_Voltage >= 3300) {
            lv_label_set_text(battery, SYMBOL_BATTERY_1);
        } else {
            lv_label_set_text(battery, SYMBOL_BATTERY_EMPTY);
        }
        sprintf(tempbuf, "%dmV", BatteryMgr_Voltage);
        lv_label_set_text(voltage, tempbuf);
        #endif
        LcdDma_Mutex_Give();
        last = xthal_get_ccount();
    }
}