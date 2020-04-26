//icon accent 462dff

#include "freertos/FreeRTOS.h"
#include "mainmenu.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "freertos/task.h"
#include "../driver.h"
#include <stdio.h>
#include <dirent.h>
#include <math.h>

#include "softbar.h"

static const char* TAG = "Ui_MainMenu";

LV_IMG_DECLARE(img_megagrrlhdr);
LV_IMG_DECLARE(img_library);
LV_IMG_DECLARE(img_player);
LV_IMG_DECLARE(img_settings);
LV_IMG_DECLARE(img_about);
LV_IMG_DECLARE(img_library_half);
LV_IMG_DECLARE(img_player_half);
LV_IMG_DECLARE(img_settings_half);
LV_IMG_DECLARE(img_about_half);
LV_IMG_DECLARE(img_spfm);
LV_IMG_DECLARE(img_spfm_half);
LV_IMG_DECLARE(img_blank);
//LV_IMG_DECLARE(img_kunoichilabs_smol);
IRAM_ATTR lv_obj_t *mm_logo;
//IRAM_ATTR lv_obj_t *mm_logo_kl;
IRAM_ATTR lv_obj_t *mm_icon;
IRAM_ATTR lv_obj_t *mm_iconL;
IRAM_ATTR lv_obj_t *mm_iconR;
IRAM_ATTR lv_obj_t *mm_icontext;
lv_style_t mm_icontext_style;
IRAM_ATTR lv_obj_t *mm_icondesc;
lv_style_t mm_icondesc_style;

static IRAM_ATTR lv_obj_t *container;
lv_style_t containerstyle;
lv_style_t linestyle;
lv_point_t points[2*7];
IRAM_ATTR lv_obj_t *lines[7];
lv_point_t pointsL[2*7];
IRAM_ATTR lv_obj_t *linesL[7];
lv_point_t pointsX[2*14];
IRAM_ATTR lv_obj_t *linesX[14];

typedef struct {
    const char *text;
    const char *desc;
    const void *img;
    const void *img_half;
    const UiScreen_t newscreen;
} mm_icon_t;

uint8_t mm_curicon = 1;

const mm_icon_t mm_icontable[] = {
    {"Music Player", "View and control the currently playing track.", &img_player, &img_player_half, UISCREEN_NOWPLAYING},
    {"File Browser", "Browse files stored on the SD card.", &img_library, &img_library_half, UISCREEN_FILEBROWSER},
    {"Settings", "Adjust system settings.", &img_settings, &img_settings_half, UISCREEN_OPTIONS_CATS},
    #if 0
    {"SPFM Mode", "Enter SPFM emulation mode.", &img_spfm, &img_spfm_half, UISCREEN_MAINMENU},
    #endif
    {"About", "View information, credits, acknowledgements.", &img_about, &img_about_half, UISCREEN_ABOUT},
};
#define MM_ICON_COUNT (sizeof(mm_icontable)/sizeof(mm_icon_t))

void mm_updateicons();

void Ui_MainMenu_Setup(lv_obj_t *uiscreen) {
//ESP_LOGE(TAG, "main setup start");
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250+1); //covering up the bottom row of bg pixels makes the grid look better
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    mm_logo = lv_img_create(container, NULL);
    lv_img_set_src(mm_logo, &img_megagrrlhdr);
    lv_obj_set_pos(mm_logo, 0, (50/2)-(24/2)+1);//-6);

    /*mm_logo_kl = lv_img_create(container, NULL);
    lv_img_set_src(mm_logo_kl, &img_kunoichilabs_smol);
    lv_obj_set_pos(mm_logo_kl, 77, 31);*/

    lv_style_copy(&linestyle, &lv_style_plain);
    linestyle.line.color = LV_COLOR_MAKE(0x7f,0,0xa0);
    linestyle.line.width = 2;
    linestyle.line.rounded = 0;

    for (uint8_t i=0;i<7;i++) {
        lines[i] = lv_line_create(container, NULL);
        linesL[i] = lv_line_create(container, NULL);
        uint16_t x1 = (i+1)*15;
        uint16_t x2 = (i+1)*45;
        points[(i*2)+0].x = x1+120;
        points[(i*2)+0].y = 54;
        points[(i*2)+1].x = x2+120;
        points[(i*2)+1].y = 250;
        lv_line_set_points(lines[i], &points[i*2], 2);
        lv_line_set_style(lines[i], LV_LINE_STYLE_MAIN, &linestyle);
        pointsL[(i*2)+0].x = 120-x1;
        pointsL[(i*2)+0].y = 54;
        pointsL[(i*2)+1].x = 120-x2;
        pointsL[(i*2)+1].y = 250;
        lv_line_set_points(linesL[i], &pointsL[i*2], 2);
        lv_line_set_style(linesL[i], LV_LINE_STYLE_MAIN, &linestyle);
    }

    static IRAM_ATTR lv_obj_t *mline;
    mline = lv_line_create(container, NULL);
    static lv_point_t mpoints[2] = {{120, 53},{120, 250}};
    lv_line_set_points(mline, mpoints, 2);
    lv_line_set_style(mline, LV_LINE_STYLE_MAIN, &linestyle);

    for (uint8_t i=0;i<14;i++) {
        linesX[i] = lv_line_create(container, NULL);
        pointsX[(i*2)+0].x = 0;
        pointsX[(i*2)+0].y = 53 + (i*14) + pow((double)1.5F, (double)i);
        pointsX[(i*2)+1].x = 240;
        pointsX[(i*2)+1].y = 53 + (i*14) + pow((double)1.5F, (double)i);
        lv_line_set_points(linesX[i], &pointsX[i*2], 2);
        lv_line_set_style(linesX[i], LV_LINE_STYLE_MAIN, &linestyle);
    }

    mm_icon = lv_img_create(container, NULL);
    lv_img_set_src(mm_icon, &img_library);
    lv_obj_set_pos(mm_icon, (240/2)-(100/2)+1, (250/2)-(100/2)-15);

    mm_iconL = lv_img_create(container, NULL);
    lv_img_set_src(mm_iconL, &img_library);
    lv_obj_set_pos(mm_iconL, -(50/2)+10, (250/2)-(50/2)-15);
    mm_iconR = lv_img_create(container, NULL);
    lv_img_set_src(mm_iconR, &img_library);
    lv_obj_set_pos(mm_iconR, 240-(50/2)-10, (250/2)-(50/2)-15);

    lv_style_copy(&mm_icontext_style, &lv_style_plain);
    mm_icontext_style.text.color = LV_COLOR_MAKE(255,255,255);
    mm_icontext_style.text.font = &lv_font_dejavu_30;

    mm_icontext = lv_label_create(container, NULL);
    lv_label_set_long_mode(mm_icontext, LV_LABEL_LONG_CROP);
    lv_obj_set_pos(mm_icontext, 1, (250/2)+(100/2)-3-15);
    lv_obj_set_size(mm_icontext, 239, 50);
    lv_label_set_style(mm_icontext, LV_LABEL_STYLE_MAIN, &mm_icontext_style);
    lv_label_set_align(mm_icontext, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(mm_icontext, "");

    lv_style_copy(&mm_icondesc_style, &lv_style_plain);
    mm_icondesc_style.text.color = LV_COLOR_MAKE(255,255,255);
    mm_icondesc_style.text.font = &lv_font_dejavu_18;

    mm_icondesc = lv_label_create(container, NULL);
    lv_label_set_long_mode(mm_icondesc, LV_LABEL_LONG_SROLL);
    lv_obj_set_pos(mm_icondesc, 10, (250/2)+(100/2)+32-15);
    lv_obj_set_size(mm_icondesc, 220, 50);
    lv_label_set_style(mm_icondesc, LV_LABEL_STYLE_MAIN, &mm_icondesc_style);
    lv_label_set_align(mm_icondesc, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(mm_icondesc, "");
    lv_label_set_anim_speed(mm_icondesc, 50);

    #ifdef HWVER_PORTABLE
    Ui_SoftBar_Update(0, true, "PwrOff", false);
    #elif defined HWVER_DESKTOP
    Ui_SoftBar_Update(0, false, "", false);
    #endif
    Ui_SoftBar_Update(1, false, "", false);
    Ui_SoftBar_Update(2, true, "Select", false);
    LcdDma_Mutex_Give();

//ESP_LOGE(TAG, "main setup end");
    mm_updateicons();
}

void Ui_MainMenu_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void mm_updateicons() {
//ESP_LOGE(TAG, "main update start");
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    if (mm_curicon == 0) {
        lv_img_set_src(mm_iconL, &img_blank);
    } else {
        lv_img_set_src(mm_iconL, mm_icontable[mm_curicon-1].img_half);
    }
    lv_img_set_src(mm_icon, mm_icontable[mm_curicon].img);
    lv_label_set_static_text(mm_icontext, mm_icontable[mm_curicon].text);
    lv_label_set_static_text(mm_icondesc, mm_icontable[mm_curicon].desc);
    if (mm_curicon == MM_ICON_COUNT-1) {
        lv_img_set_src(mm_iconR, &img_blank);
    } else {
        lv_img_set_src(mm_iconR, mm_icontable[mm_curicon+1].img_half);
    }
    LcdDma_Mutex_Give();
//ESP_LOGE(TAG, "main update end");
}

void Ui_MainMenu_Key(KeyEvent_t event) {
    if (event.State == KEY_EVENT_PRESS) {
        switch (event.Key) {
            case KEY_LEFT:
                if (mm_curicon) {
                    mm_curicon--;
                    mm_updateicons();
                }
                break;
            case KEY_RIGHT:
                if (mm_curicon < MM_ICON_COUNT-1) {
                    mm_curicon++;
                    mm_updateicons();
                }
                break;
            case KEY_B:
                break;
            case KEY_C:
                KeyMgr_Consume(KEY_C);
                Ui_Screen = mm_icontable[mm_curicon].newscreen;
                break;
            default:
                break;
        }
    }
}