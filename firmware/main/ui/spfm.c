#include "spfm.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "softbar.h"
#include "../clk.h"

static const char* TAG = "Ui_Spfm";

lv_obj_t *container;
lv_style_t containerstyle;
static lv_obj_t *ta;
static lv_style_t ta_style;

static lv_obj_t *frame;
static lv_obj_t *lcd;

#ifdef HWVER_PORTABLE
LV_IMG_DECLARE(img_frame);
#elif defined HWVER_DESKTOP
LV_IMG_DECLARE(img_desktopframe);
#endif
LV_IMG_DECLARE(img_lcdhenohenomoheji);
LV_IMG_DECLARE(img_lcdsad);
LV_IMG_DECLARE(img_lcdhappy);

void Ui_Spfm_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);

    frame = lv_img_create(container, NULL);
    #ifdef HWVER_PORTABLE
    lv_img_set_src(frame, &img_frame);
    lv_obj_set_pos(frame, 95, 15);
    #elif defined HWVER_DESKTOP
    lv_img_set_src(frame, &img_desktopframe);
    lv_obj_set_pos(frame, 86, 15);
    #endif

    lcd = lv_img_create(container, NULL);
    lv_img_set_src(lcd, &img_lcdhenohenomoheji);
    #ifdef HWVER_PORTABLE
    lv_obj_set_pos(lcd, 95+21, 15+8);
    #elif defined HWVER_DESKTOP
    lv_obj_set_pos(lcd, 86+22, 15+5);
    #endif

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, true, "Back", false);
    Ui_SoftBar_Update(2, false, "", false);

    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&ta_style, &lv_style_plain);
    ta_style.text.color = LV_COLOR_MAKE(0xff,0xff,0xff);
    ta_style.text.line_space = 2;
    ta_style.body.main_color = LV_COLOR_MAKE(0,0,0);
    ta_style.body.grad_color = LV_COLOR_MAKE(0,0,0);
    //ta_style.text.font = &lv_font_monospace_8;

    ta = lv_ta_create(container, NULL);
    lv_obj_set_style(ta, &ta_style);
    lv_obj_set_size(ta, 240, 150);
    lv_obj_set_pos(ta, 0, 100);
    lv_ta_set_cursor_type(ta, LV_CURSOR_NONE);
    lv_ta_set_cursor_pos(ta, 0);
    lv_ta_set_text(ta, "MegaGRRL\nSPFM Emulation Mode");

    //Clk_Set(CLK_FM, 7987200);

    LcdDma_Mutex_Give();
}

void Ui_Spfm_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void Ui_Spfm_Key(KeyEvent_t event) {
    if (event.Key == KEY_B) {
        Ui_Screen = UISCREEN_MAINMENU;
    }
}