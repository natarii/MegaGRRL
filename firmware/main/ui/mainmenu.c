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
#include "../ver.h"
#include <stdio.h>
#include <dirent.h>
#include <math.h>

#include "softbar.h"

//static const char* TAG = "Ui_MainMenu";

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
LV_IMG_DECLARE(img_shuffleall);
LV_IMG_DECLARE(img_shuffleall_half);
LV_IMG_DECLARE(img_mainmenu0); LV_IMG_DECLARE(img_mainmenu1); LV_IMG_DECLARE(img_mainmenu2); LV_IMG_DECLARE(img_mainmenu3); LV_IMG_DECLARE(img_mainmenu4); LV_IMG_DECLARE(img_mainmenu5); LV_IMG_DECLARE(img_mainmenu6); LV_IMG_DECLARE(img_mainmenu7); LV_IMG_DECLARE(img_mainmenu8); LV_IMG_DECLARE(img_mainmenu9); LV_IMG_DECLARE(img_mainmenu10); LV_IMG_DECLARE(img_mainmenu11); LV_IMG_DECLARE(img_mainmenu12); LV_IMG_DECLARE(img_mainmenu13); LV_IMG_DECLARE(img_mainmenu14); LV_IMG_DECLARE(img_mainmenu15); LV_IMG_DECLARE(img_mainmenu16); LV_IMG_DECLARE(img_mainmenu17); LV_IMG_DECLARE(img_mainmenu18); LV_IMG_DECLARE(img_mainmenu19); LV_IMG_DECLARE(img_mainmenu20); LV_IMG_DECLARE(img_mainmenu21); LV_IMG_DECLARE(img_mainmenu22); LV_IMG_DECLARE(img_mainmenu23); LV_IMG_DECLARE(img_mainmenu24); LV_IMG_DECLARE(img_mainmenu25); LV_IMG_DECLARE(img_mainmenu26); LV_IMG_DECLARE(img_mainmenu27); LV_IMG_DECLARE(img_mainmenu28); LV_IMG_DECLARE(img_mainmenu29); LV_IMG_DECLARE(img_mainmenu30); LV_IMG_DECLARE(img_mainmenu31); LV_IMG_DECLARE(img_mainmenu32); LV_IMG_DECLARE(img_mainmenu33); LV_IMG_DECLARE(img_mainmenu34); LV_IMG_DECLARE(img_mainmenu35); LV_IMG_DECLARE(img_mainmenu36); LV_IMG_DECLARE(img_mainmenu37); LV_IMG_DECLARE(img_mainmenu38); LV_IMG_DECLARE(img_mainmenu39); LV_IMG_DECLARE(img_mainmenu40); LV_IMG_DECLARE(img_mainmenu41); LV_IMG_DECLARE(img_mainmenu42); LV_IMG_DECLARE(img_mainmenu43); LV_IMG_DECLARE(img_mainmenu44); LV_IMG_DECLARE(img_mainmenu45); LV_IMG_DECLARE(img_mainmenu46); LV_IMG_DECLARE(img_mainmenu47); LV_IMG_DECLARE(img_mainmenu48); LV_IMG_DECLARE(img_mainmenu49); LV_IMG_DECLARE(img_mainmenu50); LV_IMG_DECLARE(img_mainmenu51); LV_IMG_DECLARE(img_mainmenu52); LV_IMG_DECLARE(img_mainmenu53); LV_IMG_DECLARE(img_mainmenu54); LV_IMG_DECLARE(img_mainmenu55); LV_IMG_DECLARE(img_mainmenu56); LV_IMG_DECLARE(img_mainmenu57); LV_IMG_DECLARE(img_mainmenu58); LV_IMG_DECLARE(img_mainmenu59); LV_IMG_DECLARE(img_mainmenu60); LV_IMG_DECLARE(img_mainmenu61); LV_IMG_DECLARE(img_mainmenu62); LV_IMG_DECLARE(img_mainmenu63); 
static const lv_img_dsc_t *img_mainmenu[64] = {&img_mainmenu0, &img_mainmenu1, &img_mainmenu2, &img_mainmenu3, &img_mainmenu4, &img_mainmenu5, &img_mainmenu6, &img_mainmenu7, &img_mainmenu8, &img_mainmenu9, &img_mainmenu10, &img_mainmenu11, &img_mainmenu12, &img_mainmenu13, &img_mainmenu14, &img_mainmenu15, &img_mainmenu16, &img_mainmenu17, &img_mainmenu18, &img_mainmenu19, &img_mainmenu20, &img_mainmenu21, &img_mainmenu22, &img_mainmenu23, &img_mainmenu24, &img_mainmenu25, &img_mainmenu26, &img_mainmenu27, &img_mainmenu28, &img_mainmenu29, &img_mainmenu30, &img_mainmenu31, &img_mainmenu32, &img_mainmenu33, &img_mainmenu34, &img_mainmenu35, &img_mainmenu36, &img_mainmenu37, &img_mainmenu38, &img_mainmenu39, &img_mainmenu40, &img_mainmenu41, &img_mainmenu42, &img_mainmenu43, &img_mainmenu44, &img_mainmenu45, &img_mainmenu46, &img_mainmenu47, &img_mainmenu48, &img_mainmenu49, &img_mainmenu50, &img_mainmenu51, &img_mainmenu52, &img_mainmenu53, &img_mainmenu54, &img_mainmenu55, &img_mainmenu56, &img_mainmenu57, &img_mainmenu58, &img_mainmenu59, &img_mainmenu60, &img_mainmenu61, &img_mainmenu62, &img_mainmenu63, };

static IRAM_ATTR lv_obj_t *mm_icon;
static IRAM_ATTR lv_obj_t *mm_iconL;
static IRAM_ATTR lv_obj_t *mm_iconR;
static IRAM_ATTR lv_obj_t *mm_icontext;
static lv_style_t mm_icontext_style;
static IRAM_ATTR lv_obj_t *mm_icondesc;
static lv_style_t mm_icondesc_style;

static IRAM_ATTR lv_obj_t *verlabel;
static lv_style_t verlabel_style;

static IRAM_ATTR lv_obj_t *container;
static lv_style_t containerstyle;

static IRAM_ATTR lv_obj_t *bgtiles[64];

typedef struct {
    const char *text;
    const char *desc;
    const void *img;
    const void *img_half;
    const UiScreen_t newscreen;
} mm_icon_t;

static uint8_t mm_curicon = 1;

static const mm_icon_t mm_icontable[] = {
    {"Music Player", "View and control the currently playing track.", &img_player, &img_player_half, UISCREEN_NOWPLAYING},
    {"File Browser", "Browse files stored on the SD card.", &img_library, &img_library_half, UISCREEN_FILEBROWSER},
    {"Shuffle All", "Shuffle and play all tracks on the SD card.", &img_shuffleall, &img_shuffleall_half, UISCREEN_SHUFFLEALL},
    {"Settings", "Adjust system settings.", &img_settings, &img_settings_half, UISCREEN_OPTIONS_CATS},
    #if 0
    {"SPFM Mode", "Enter SPFM emulation mode.", &img_spfm, &img_spfm_half, UISCREEN_MAINMENU},
    #endif
    {"About", "View information, credits, acknowledgements.", &img_about, &img_about_half, UISCREEN_ABOUT},
};
#define MM_ICON_COUNT (sizeof(mm_icontable)/sizeof(mm_icon_t))

static void mm_updateicons();

void Ui_MainMenu_Setup(lv_obj_t *uiscreen) {
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
        lv_img_set_src(bgtiles[i], img_mainmenu[i]);
    }

    mm_icon = lv_img_create(container, NULL);
    lv_img_set_src(mm_icon, &img_library);
    lv_obj_set_pos(mm_icon, (240/2)-(100/2)+1, (250/2)-(100/2)-18);

    mm_iconL = lv_img_create(container, NULL);
    lv_img_set_src(mm_iconL, &img_library);
    lv_obj_set_pos(mm_iconL, -(50/2)+10, (250/2)-(50/2)-18);
    mm_iconR = lv_img_create(container, NULL);
    lv_img_set_src(mm_iconR, &img_library);
    lv_obj_set_pos(mm_iconR, 240-(50/2)-10, (250/2)-(50/2)-18);

    lv_style_copy(&mm_icontext_style, &lv_style_plain);
    mm_icontext_style.text.color = LV_COLOR_MAKE(255,255,255);
    mm_icontext_style.text.font = &lv_font_dejavu_30;

    mm_icontext = lv_label_create(container, NULL);
    lv_label_set_long_mode(mm_icontext, LV_LABEL_LONG_CROP);
    lv_obj_set_pos(mm_icontext, 1, 168);
    lv_obj_set_size(mm_icontext, 239, 50);
    lv_label_set_style(mm_icontext, LV_LABEL_STYLE_MAIN, &mm_icontext_style);
    lv_label_set_align(mm_icontext, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(mm_icontext, "");

    lv_style_copy(&mm_icondesc_style, &lv_style_plain);
    mm_icondesc_style.text.color = LV_COLOR_MAKE(255,255,255);
    mm_icondesc_style.text.font = &lv_font_dejavu_18;

    mm_icondesc = lv_label_create(container, NULL);
    lv_label_set_long_mode(mm_icondesc, Ui_GetScrollType());
    lv_obj_set_pos(mm_icondesc, 4, 199);
    lv_obj_set_size(mm_icondesc, 232, 50);
    lv_label_set_style(mm_icondesc, LV_LABEL_STYLE_MAIN, &mm_icondesc_style);
    lv_label_set_align(mm_icondesc, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(mm_icondesc, "");
    lv_label_set_anim_speed(mm_icondesc, 50);

    lv_style_copy(&verlabel_style, &lv_style_plain);
    verlabel_style.text.font = &lv_font_dejavu_10;
    verlabel_style.text.color = LV_COLOR_MAKE(0,0,0);
    verlabel = lv_label_create(container, NULL);
    lv_label_set_long_mode(verlabel, LV_LABEL_LONG_BREAK);
    lv_obj_set_pos(verlabel, 0, 235);
    lv_obj_set_size(verlabel, 237, 10);
    lv_label_set_style(verlabel, LV_LABEL_STYLE_MAIN, &verlabel_style);
    lv_label_set_align(verlabel, LV_LABEL_ALIGN_RIGHT);
    lv_label_set_static_text(verlabel, FWVER);

    #ifdef HWVER_PORTABLE
    Ui_SoftBar_Update(0, true, "PwrOff", false);
    #elif defined HWVER_DESKTOP
    Ui_SoftBar_Update(0, false, "", false);
    #endif
    Ui_SoftBar_Update(1, false, "", false);
    Ui_SoftBar_Update(2, true, "Select", false);
    LcdDma_Mutex_Give();

    mm_updateicons();
}

void Ui_MainMenu_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void mm_updateicons() {
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
