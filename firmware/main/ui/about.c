#include "../hal.h"
#include "../ver.h"
const char *about_text = 
#ifdef HWVER_PORTABLE
"#ffafff MegaGRRL#"
#elif defined HWVER_DESKTOP
"#ffafff MegaGRRL Desktop#"
#endif
"\nFirmware ver: "FWVER_D
"\n#462dff by# #ffffff kunoichi labs#\n"
"#462dff //# #ffafff natalie null#\n"
"#00ff00 Copyright 2018-2020#\n"
"#ffff00 https://kunoichilabs.dev#\n\n"
"Some icons by Icons8\n"
"#ffff00 http://icons8.com#\n\n"
"MegaGRRL uses ESP-IDF and LittlevGL\n\n\n"
"#462dff Special Thanks#\n"
"Aidan\n\n"
"Anna\n\n"
"nia\n\n"
"Jorin\n\n"
"Allison\n\n"
"The other Allison\n\n"
"Cassandra\n\n"
"caela\n\n"
"akira\n\n"
"astra\n\n"
"mirai\n\n"
"LUN-4\n\n"
"Amy\n\n"
"ash\n\n"
"Maliki\n\n"
"Lindsay\n\n"
"Pure\n\n"
"0x3F\n\n"
"The writers at Hackaday and Hackster\n\n"
"Plutiedev\n\n"
"McQueen8601\n\n"
"Project2612.org\n\n"
"Retro.Live\n\n"
"Everyone who had nice things to say on Fedi, YouTube, Twitter, and Hackaday.io\n\n"
"You\n\n\n"
"\nThanks to Nichibutsu for getting me into FM synth\n\n"
"\n\nLater!\n-natalie\n#ff3030 <3#"
;

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "freertos/task.h"
#include "about.h"
#include "softbar.h"

//static const char* TAG = "Ui_About";

int32_t about_map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static IRAM_ATTR lv_obj_t *container;
IRAM_ATTR lv_obj_t *about_ta;
lv_style_t about_ta_style;
IRAM_ATTR uint32_t about_time = 0;
IRAM_ATTR uint32_t about_last_time = 0;
uint16_t about_h = 0;



void Ui_About_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    about_ta = lv_label_create(container, NULL);
    lv_style_copy(&about_ta_style, &lv_style_plain);
    about_ta_style.text.color = LV_COLOR_MAKE(255,255,255);
    about_ta_style.text.font = &lv_font_dejavu_18;
    lv_label_set_long_mode(about_ta, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(about_ta, 240);
    lv_label_set_align(about_ta, LV_LABEL_ALIGN_CENTER);
    lv_label_set_style(about_ta, LV_LABEL_STYLE_MAIN, &about_ta_style);
    lv_label_set_recolor(about_ta, true);
    lv_label_set_static_text(about_ta, about_text);
    lv_obj_set_pos(about_ta, 0, 250);
    about_h = lv_obj_get_height(about_ta);

    about_time = 0;
    about_last_time = xthal_get_ccount();

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, true, LV_SYMBOL_PLUS"Debug", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_OK" Done", false);

    LcdDma_Mutex_Give();
}

void Ui_About_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void Ui_About_Key(KeyEvent_t event) {
    if (event.Key == KEY_C && event.State == KEY_EVENT_PRESS) {
        KeyMgr_Consume(KEY_C);
        Ui_Screen = UISCREEN_MAINMENU;
    } else if (event.Key == KEY_B && event.State == KEY_EVENT_PRESS) {
        KeyMgr_Consume(KEY_B);
        Ui_Screen = UISCREEN_DEBUG;
    }
}

void Ui_About_Tick() {
    uint32_t t = xthal_get_ccount();
    about_time += (t - about_last_time)/240000; //millis
    about_last_time = t;
    if (about_time >= 120000) about_time = 0;

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    uint16_t y = about_map(about_time, 0, 120000, 0, about_h+250);
    lv_obj_set_pos(about_ta, 0, 250-y);
    
    LcdDma_Mutex_Give();
}
