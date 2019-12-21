#include "options_cats.h"
#include "options_opts.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../ui.h"
#include "../options.h"
#include "softbar.h"

lv_obj_t *container;
lv_style_t containerstyle;

uint8_t Options_OptId = 0;
uint8_t Options_Sel = 0;
uint8_t Options_Off = 0;

lv_obj_t *optionoptlines[5];
lv_obj_t *optionoptlabels[5];
lv_style_t optionoptstyle_normal;
lv_style_t optionoptstyle_sel;
lv_style_t optiondescstyle;

//lv_obj_t *optiontitle;
lv_obj_t *optiondesc;
lv_obj_t *optionvalue;

void redrawopts();

void Ui_Options_Opts_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0, 0, 0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    lv_cont_set_style(container, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    lv_cont_set_fit(container, false, false);

    lv_style_copy(&optionoptstyle_normal, &lv_style_plain);
    optionoptstyle_normal.text.font = &lv_font_dejavu_20;
    optionoptstyle_normal.body.main_color = LV_COLOR_MAKE(0,0,0);
    optionoptstyle_normal.body.grad_color = LV_COLOR_MAKE(0,0,0);
    optionoptstyle_normal.text.color = LV_COLOR_MAKE(220,220,220);

    lv_style_copy(&optionoptstyle_sel, &optionoptstyle_normal);
    optionoptstyle_sel.body.main_color = LV_COLOR_MAKE(0,0,100);
    optionoptstyle_sel.body.grad_color = LV_COLOR_MAKE(0,0,100);
    optionoptstyle_sel.body.radius = 8;

    for (uint8_t i=0;i<5;i++) {
        optionoptlines[i] = lv_cont_create(container, NULL);
        lv_obj_set_style(optionoptlines[i], &optionoptstyle_sel);
        lv_obj_set_height(optionoptlines[i], 25);
        lv_obj_set_width(optionoptlines[i],240);
        lv_obj_set_pos(optionoptlines[i], 0, 25*i);

        optionoptlabels[i] = lv_label_create(optionoptlines[i], NULL);
        lv_obj_set_pos(optionoptlabels[i], 2, 2);
        lv_label_set_text(optionoptlabels[i], "");
        lv_label_set_long_mode(optionoptlabels[i], (i==0xff)?LV_LABEL_LONG_ROLL:LV_LABEL_LONG_DOT); //todo
        lv_obj_set_width(optionoptlabels[i], 240);
    }

    lv_style_copy(&optiondescstyle, &lv_style_plain);
    optiondescstyle.text.font = &lv_font_dejavu_20;
    optiondescstyle.text.color = LV_COLOR_MAKE(127,127,127);

    optiondesc = lv_label_create(container, NULL);
    lv_obj_set_style(optiondesc, &optiondescstyle);
    lv_obj_set_pos(optiondesc, 10, 135);
    lv_label_set_long_mode(optiondesc, LV_LABEL_LONG_ROLL);
    lv_obj_set_width(optiondesc, 220);
    lv_label_set_anim_speed(optiondesc, 75);

    Ui_SoftBar_Update(0, true, SYMBOL_HOME"Home", false);
    Ui_SoftBar_Update(1, true, "Back", false);
    Ui_SoftBar_Update(2, true, SYMBOL_EDIT"Edit", false);

    LcdDma_Mutex_Give();

    redrawopts();
}

void Ui_Options_Opts_Destroy() {
    lv_obj_del(container);
}

void redrawopts() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    uint8_t idx = 0;
    uint8_t skip = 0;
    uint8_t off = (Options_Sel/5)*5;
    for (uint8_t opt=0;opt<OPTION_COUNT;opt++) {
        if (Options[opt].category != Options_Cat) continue;
        if (skip++ < off) continue;
        lv_label_set_text(optionoptlabels[idx], Options[opt].name);
        if (Options_Sel != off+idx) {
            lv_obj_set_style(optionoptlines[idx], &optionoptstyle_normal);
            lv_obj_set_style(optionoptlabels[idx], &optionoptstyle_normal);
            lv_label_set_long_mode(optionoptlabels[idx], LV_LABEL_LONG_DOT);
        } else {
            lv_obj_set_style(optionoptlines[idx], &optionoptstyle_sel);
            lv_obj_set_style(optionoptlabels[idx], &optionoptstyle_sel);
            lv_label_set_long_mode(optionoptlabels[idx], LV_LABEL_LONG_ROLL);
            lv_label_set_text(optiondesc, Options[opt].description);
        }
        idx++;
        if (idx == 5) break;
    }
    
    //blank left over ones
    for (;idx<5;idx++) {
        lv_obj_set_style(optionoptlines[idx], &optionoptstyle_normal);
        lv_obj_set_style(optionoptlabels[idx], &optionoptstyle_normal);
        lv_label_set_static_text(optionoptlabels[idx], "");
    }
    LcdDma_Mutex_Give();
}

void redrawopt() {
    
}

void Ui_Options_Opts_Key(KeyEvent_t event) {
    if (event.State & KEY_EVENT_PRESS) {
        if (event.Key == KEY_UP) {
            if (Options_Sel) {
                Options_Sel--;
                redrawopts();
            }
        } else if (event.Key == KEY_DOWN) {
            uint8_t max = 0;
            for (uint8_t opt=0;opt<OPTION_COUNT;opt++) {
                if (Options[opt].category == Options_Cat) max++;
            }
            if (Options_Sel < max-1) { //need to actually see how many things are in the category to know where to clamp this
                Options_Sel++;
                redrawopts();
            }
        } else if (event.Key == KEY_A) {
            Ui_Screen = UISCREEN_MAINMENU;
        } else if (event.Key == KEY_B) {
            //todo stuff if editing
            Ui_Screen = UISCREEN_OPTIONS_CATS;
        }
    }
}