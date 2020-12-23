#include "freertos/FreeRTOS.h"
#include "mainmenu.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "freertos/task.h"
#include "softbar.h"

LV_IMG_DECLARE(img_madison0); LV_IMG_DECLARE(img_madison1); LV_IMG_DECLARE(img_madison2); LV_IMG_DECLARE(img_madison3); LV_IMG_DECLARE(img_madison4); LV_IMG_DECLARE(img_madison5); LV_IMG_DECLARE(img_madison6); LV_IMG_DECLARE(img_madison7); LV_IMG_DECLARE(img_madison8); LV_IMG_DECLARE(img_madison9); LV_IMG_DECLARE(img_madison10); LV_IMG_DECLARE(img_madison11); LV_IMG_DECLARE(img_madison12); LV_IMG_DECLARE(img_madison13); LV_IMG_DECLARE(img_madison14); LV_IMG_DECLARE(img_madison15); LV_IMG_DECLARE(img_madison16); LV_IMG_DECLARE(img_madison17); LV_IMG_DECLARE(img_madison18); LV_IMG_DECLARE(img_madison19); LV_IMG_DECLARE(img_madison20); LV_IMG_DECLARE(img_madison21); LV_IMG_DECLARE(img_madison22); LV_IMG_DECLARE(img_madison23); LV_IMG_DECLARE(img_madison24); LV_IMG_DECLARE(img_madison25); LV_IMG_DECLARE(img_madison26); LV_IMG_DECLARE(img_madison27); LV_IMG_DECLARE(img_madison28); LV_IMG_DECLARE(img_madison29); LV_IMG_DECLARE(img_madison30); LV_IMG_DECLARE(img_madison31); LV_IMG_DECLARE(img_madison32); LV_IMG_DECLARE(img_madison33); LV_IMG_DECLARE(img_madison34); LV_IMG_DECLARE(img_madison35); LV_IMG_DECLARE(img_madison36); LV_IMG_DECLARE(img_madison37); LV_IMG_DECLARE(img_madison38); LV_IMG_DECLARE(img_madison39); LV_IMG_DECLARE(img_madison40); LV_IMG_DECLARE(img_madison41); LV_IMG_DECLARE(img_madison42); LV_IMG_DECLARE(img_madison43); LV_IMG_DECLARE(img_madison44); LV_IMG_DECLARE(img_madison45); LV_IMG_DECLARE(img_madison46); LV_IMG_DECLARE(img_madison47); LV_IMG_DECLARE(img_madison48); LV_IMG_DECLARE(img_madison49); LV_IMG_DECLARE(img_madison50); LV_IMG_DECLARE(img_madison51); LV_IMG_DECLARE(img_madison52); LV_IMG_DECLARE(img_madison53); LV_IMG_DECLARE(img_madison54); LV_IMG_DECLARE(img_madison55); LV_IMG_DECLARE(img_madison56); LV_IMG_DECLARE(img_madison57); LV_IMG_DECLARE(img_madison58); LV_IMG_DECLARE(img_madison59); LV_IMG_DECLARE(img_madison60); LV_IMG_DECLARE(img_madison61); LV_IMG_DECLARE(img_madison62); LV_IMG_DECLARE(img_madison63); 
static const lv_img_dsc_t *img_madison[64] = {&img_madison0, &img_madison1, &img_madison2, &img_madison3, &img_madison4, &img_madison5, &img_madison6, &img_madison7, &img_madison8, &img_madison9, &img_madison10, &img_madison11, &img_madison12, &img_madison13, &img_madison14, &img_madison15, &img_madison16, &img_madison17, &img_madison18, &img_madison19, &img_madison20, &img_madison21, &img_madison22, &img_madison23, &img_madison24, &img_madison25, &img_madison26, &img_madison27, &img_madison28, &img_madison29, &img_madison30, &img_madison31, &img_madison32, &img_madison33, &img_madison34, &img_madison35, &img_madison36, &img_madison37, &img_madison38, &img_madison39, &img_madison40, &img_madison41, &img_madison42, &img_madison43, &img_madison44, &img_madison45, &img_madison46, &img_madison47, &img_madison48, &img_madison49, &img_madison50, &img_madison51, &img_madison52, &img_madison53, &img_madison54, &img_madison55, &img_madison56, &img_madison57, &img_madison58, &img_madison59, &img_madison60, &img_madison61, &img_madison62, &img_madison63, };

static IRAM_ATTR lv_obj_t *container;
static lv_style_t containerstyle;
static IRAM_ATTR lv_obj_t *bgtiles[64];
static UiScreen_t lastscreen = UISCREEN_MAINMENU;

void Ui_EasterEgg_Setup(lv_obj_t *uiscreen) {
    lastscreen = Ui_Screen_Last;
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);

    for (uint8_t i=0;i<64;i++) {
        bgtiles[i] = lv_img_create(container, NULL);
        lv_obj_set_pos(bgtiles[i], (i%8)*32, (i/8)*32);
        lv_img_set_src(bgtiles[i], img_madison[i]);
    }

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, false, "", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_OK" Done", false);

    LcdDma_Mutex_Give();
}

void Ui_EasterEgg_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void Ui_EasterEgg_Key(KeyEvent_t event) {
    if (event.State == KEY_EVENT_PRESS) {
        switch (event.Key) {
            case KEY_C:
                KeyMgr_Consume(KEY_C);
                Ui_Screen = lastscreen;
                break;
            default:
                break;
        }
    }
}