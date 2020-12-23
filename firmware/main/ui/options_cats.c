#include "options_cats.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../ui.h"
#include "../options.h"
#include "softbar.h"

static IRAM_ATTR lv_obj_t *container;

uint8_t Options_Cat = 0;

static IRAM_ATTR lv_obj_t *optioncatlines[OPTION_CATEGORY_COUNT];
static IRAM_ATTR lv_obj_t *optioncatlabels[OPTION_CATEGORY_COUNT];
static lv_style_t optioncatstyle_normal;
static lv_style_t optioncatstyle_sel;
static lv_style_t headerstyle;
static IRAM_ATTR lv_obj_t *header;

static UiScreen_t lastscreen = UISCREEN_MAINMENU;

static lv_style_t linestyle;
static IRAM_ATTR lv_obj_t *listdiv;

void redrawoptcats();

void Ui_Options_Cats_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&optioncatstyle_normal, &lv_style_plain);
    optioncatstyle_normal.text.font = &lv_font_dejavu_18;
    optioncatstyle_normal.body.main_color = LV_COLOR_MAKE(0,0,0);
    optioncatstyle_normal.body.grad_color = LV_COLOR_MAKE(0,0,0);
    optioncatstyle_normal.text.color = LV_COLOR_MAKE(220,220,220);

    lv_style_copy(&optioncatstyle_sel, &optioncatstyle_normal);
    optioncatstyle_sel.body.main_color = LV_COLOR_MAKE(0,0,100);
    optioncatstyle_sel.body.grad_color = LV_COLOR_MAKE(0,0,100);
    optioncatstyle_sel.body.radius = 8;

    optioncatstyle_normal.body.opa = 0; //must be after the copies

    int16_t y = 4;

    lv_style_copy(&headerstyle, &lv_style_plain);
    headerstyle.text.font = &lv_font_dejavu_14_2bpp;
    headerstyle.text.color = LV_COLOR_MAKE(127,127,127);
    header = lv_label_create(container, NULL);
    lv_obj_set_pos(header, 4, y);
    y += 20;
    lv_label_set_style(header, LV_LABEL_STYLE_MAIN, &headerstyle);
    lv_label_set_static_text(header, "Settings...");

    lv_style_copy(&linestyle, &lv_style_plain);
    linestyle.line.color = LV_COLOR_MAKE(255,255,255);
    linestyle.line.width = 1;
    listdiv = lv_line_create(container, NULL);
    lv_line_set_style(listdiv, LV_LINE_STYLE_MAIN, &linestyle);
    static const lv_point_t divpoints[2] = {{0,0},{240,0}};
    lv_line_set_points(listdiv, divpoints, 2);
    lv_obj_set_pos(listdiv, 0, y);
    y += 1;

    for (uint8_t i=0;i<OPTION_CATEGORY_COUNT;i++) {
        optioncatlines[i] = lv_cont_create(container, NULL);
        lv_obj_set_style(optioncatlines[i], &optioncatstyle_sel);
        lv_obj_set_height(optioncatlines[i], 25);
        lv_obj_set_width(optioncatlines[i],240);
        lv_obj_set_pos(optioncatlines[i], 0, y);
        y += 25;

        optioncatlabels[i] = lv_label_create(optioncatlines[i], NULL);
        lv_obj_set_pos(optioncatlabels[i], 2, 2);
        lv_label_set_text(optioncatlabels[i], "");
        lv_label_set_long_mode(optioncatlabels[i], (i==0xff)?Ui_GetScrollType():LV_LABEL_LONG_DOT);
        lv_obj_set_width(optioncatlabels[i], 240);
    }

    Ui_SoftBar_Update(0, true, LV_SYMBOL_HOME"Home", false);
    Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_RIGHT" Open", false);

    LcdDma_Mutex_Give();

    redrawoptcats();

    if (Ui_Screen_Last != UISCREEN_OPTIONS_OPTS) lastscreen = Ui_Screen_Last;
}

void Ui_Options_Cats_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void redrawoptcats() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    for (uint8_t i=0;i<OPTION_CATEGORY_COUNT;i++) {
        if (Options_Cat != i) {
            lv_obj_set_style(optioncatlines[i], &optioncatstyle_normal);
            lv_obj_set_style(optioncatlabels[i], &optioncatstyle_normal);
        } else {
            lv_obj_set_style(optioncatlines[i], &optioncatstyle_sel);
            lv_obj_set_style(optioncatlabels[i], &optioncatstyle_sel);
        }
        lv_label_set_static_text(optioncatlabels[i], OptionCatNames[i]);
    }
    LcdDma_Mutex_Give();
}

void Ui_Options_Cats_Key(KeyEvent_t event) {
    if (event.State & KEY_EVENT_PRESS) {
        if (event.Key == KEY_UP) {
            if (Options_Cat) {
                Options_Cat--;
                redrawoptcats();
            }
        } else if (event.Key == KEY_DOWN) {
            if (Options_Cat < OPTION_CATEGORY_COUNT-1) {
                Options_Cat++;
                redrawoptcats();
            }
        } else if (event.Key == KEY_C) {
            Ui_Screen = UISCREEN_OPTIONS_OPTS;
        } else if (event.Key == KEY_A) {
            Ui_Screen = UISCREEN_MAINMENU;
        } else if (event.Key == KEY_B) {
            Ui_Screen = lastscreen;
        }
    }
}
