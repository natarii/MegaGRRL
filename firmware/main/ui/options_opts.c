#include "options_cats.h"
#include "options_opts.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../ui.h"
#include "../options.h"
#include "softbar.h"
#include "../player.h" //for repeat mode defs
#include "../userled.h" //for user led source defs
#include "filebrowser.h" //for sort dir defs

static IRAM_ATTR lv_obj_t *container;
lv_style_t containerstyle;

uint8_t Options_OptId = 0;
uint8_t Options_Sel = 0;
uint8_t Options_Off = 0;

IRAM_ATTR lv_obj_t *optionoptlines[5];
IRAM_ATTR lv_obj_t *optionoptlabels[5];
lv_style_t optionoptstyle_normal;
lv_style_t optionoptstyle_sel;
lv_style_t optiondescstyle;

//IRAM_ATTR lv_obj_t *optiontitle;
IRAM_ATTR lv_obj_t *optiondesc;
IRAM_ATTR lv_obj_t *optionvalue;
IRAM_ATTR lv_obj_t *optiondefault;

static bool editing = false;
static uint8_t oldval = 0;

static char *currentvalue = "Current: ";
static char *defaultvalue = "Default: ";
static char currentvalue_buf[32] = "";
static char defaultvalue_buf[32] = "";



void redrawopts();
void redrawopt();
static void displayvalue(char *buf, bool def) {
    if (!def && Options[Options_OptId].var == NULL) return;
    uint8_t val = def?Options[Options_OptId].defaultval:*(uint8_t*)Options[Options_OptId].var;
    char lbuf[16];
    switch (Options[Options_OptId].type) {
        case OPTION_TYPE_NUMERIC:
            sprintf(&lbuf, "%d", val);
            strcat(buf, &lbuf);
            break;
        case OPTION_TYPE_LOOPS:
            if (val == 255) {
                strcat(buf, "Infinite");
            } else {
                sprintf(&lbuf, "%d", val);
                strcat(buf, &lbuf);
            }
            break;
        case OPTION_TYPE_BOOL:
            if (val) {
                strcat(buf, "True");
            } else {
                strcat(buf, "False");
            }
            break;
        case OPTION_TYPE_STEREOMONO:
            if (val) {
                strcat(buf, "Force Mono");
            } else {
                strcat(buf, "Stereo");
            }
            break;
        case OPTION_TYPE_PLAYMODE:
            switch ((RepeatMode_t)val) {
                case REPEAT_NONE:
                    strcat(buf, "Repeat None");
                    break;
                case REPEAT_ONE:
                    strcat(buf, "Repeat One");
                    break;
                case REPEAT_ALL:
                    strcat(buf, "Repeat All");
                    break;
                default:
                    break;
            }
            break;
        case OPTION_TYPE_USERLED:
            switch ((UserLedSource_t)val) {
                case USERLED_SRC_NONE:
                    strcat(buf, "Disabled");
                    break;
                case USERLED_SRC_PLAYPAUSE:
                    strcat(buf, "Play/Pause");
                    break;
                case USERLED_SRC_DISK_ALL:
                    strcat(buf, "SD Read");
                    break;
                case USERLED_SRC_DISK_VGM:
                    strcat(buf, "SD Read (VGM)");
                    break;
                case USERLED_SRC_DISK_DSALL:
                    strcat(buf, "SD Read (DACStream)");
                    break;
                case USERLED_SRC_DISK_DSFILL:
                    strcat(buf, "SD Read (DS Fill)");
                    break;
                case USERLED_SRC_DISK_DSFIND:
                    strcat(buf, "SD Read (DS Find)");
                    break;
                case USERLED_SRC_DRIVERCPU:
                    strcat(buf, "Driver CPU");
                    break;
                default:
                    break;
            }
            break;
        case OPTION_TYPE_SORTDIR:
            switch ((SortDirection_t)val) {
                case SORT_ASCENDING:
                    strcat(buf, "Ascending");
                    break;
                case SORT_DESCENDING:
                    strcat(buf, "Descending");
                    break;
                default:
                    break;
            };
        default:
            break;
    }
}

static void changevalue(bool inc) {
    uint8_t *var = (uint8_t*)Options[Options_OptId].var;
    switch (Options[Options_OptId].type) {
        case OPTION_TYPE_NUMERIC:
            if (inc) {
                if (*var < 255) {
                    *var += 1;
                }
            } else {
                if (*var > 0) {
                    *var -= 1;
                }
            }
            break;
        case OPTION_TYPE_USERLED:
            if (inc) {
                if (*var < USERLED_SRC_COUNT-1) {
                    *var += 1;
                }
            } else {
                if (*var > 0) {
                    *var -= 1;
                }
            }
            break;
        case OPTION_TYPE_LOOPS:
            if (inc) {
                if (*var < 10) {
                    *var += 1;
                } else {
                    *var = 255;
                }
            } else {
                if (*var == 255) {
                    *var = 10;
                } else if (*var > 1) {
                    *var -= 1;
                }
            }
            break;
        case OPTION_TYPE_PLAYMODE:
            if (inc) {
                if (*var < REPEAT_COUNT-1) *var += 1;
            } else {
                if (*var > 0) *var -= 1;
            }
            break;
        case OPTION_TYPE_BOOL:
        case OPTION_TYPE_STEREOMONO:
            *var = (uint8_t)inc;
            break;
        case OPTION_TYPE_SORTDIR:
            if (inc) {
                *var = SORT_DESCENDING;
            } else {
                *var = SORT_ASCENDING;
            }
        default:
            break;
    }
}

void Ui_Options_Opts_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0, 0, 0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&optionoptstyle_normal, &lv_style_plain);
    optionoptstyle_normal.text.font = &lv_font_dejavu_18;
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
        lv_label_set_long_mode(optionoptlabels[i], (i==0xff)?LV_LABEL_LONG_SROLL:LV_LABEL_LONG_DOT); //todo
        lv_obj_set_width(optionoptlabels[i], 240);
    }

    lv_style_copy(&optiondescstyle, &lv_style_plain);
    optiondescstyle.text.font = &lv_font_dejavu_18;
    optiondescstyle.text.color = LV_COLOR_MAKE(127,127,127);

    optiondesc = lv_label_create(container, NULL);
    lv_obj_set_style(optiondesc, &optiondescstyle);
    lv_obj_set_pos(optiondesc, 10, 135);
    lv_label_set_long_mode(optiondesc, LV_LABEL_LONG_SROLL);
    lv_obj_set_width(optiondesc, 220);
    lv_label_set_anim_speed(optiondesc, 75);

    optionvalue = lv_label_create(container, NULL);
    lv_obj_set_style(optionvalue, &optionoptstyle_normal);
    lv_obj_set_pos(optionvalue, 10, 175);

    optiondefault = lv_label_create(container, NULL);
    lv_obj_set_style(optiondefault, &optiondescstyle);
    lv_obj_set_pos(optiondefault, 10, 155);

    Ui_SoftBar_Update(0, true, LV_SYMBOL_HOME"Home", false);
    Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT"Edit", false);

    LcdDma_Mutex_Give();

    Options_OptId = 0;
    Options_Sel = 0;
    Options_Off = 0;
    editing = false;

    redrawopts();
    redrawopt();
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
            Options_OptId = opt;
            lv_obj_set_style(optionoptlines[idx], &optionoptstyle_sel);
            lv_obj_set_style(optionoptlabels[idx], &optionoptstyle_sel);
            lv_label_set_long_mode(optionoptlabels[idx], LV_LABEL_LONG_SROLL);
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

    strcpy(&defaultvalue_buf, defaultvalue);
    displayvalue(&defaultvalue_buf, true);
    lv_label_set_static_text(optiondefault, &defaultvalue_buf);

    LcdDma_Mutex_Give();
}

void redrawopt() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    strcpy(&currentvalue_buf, currentvalue);
    displayvalue(&currentvalue_buf, false);
    lv_label_set_static_text(optionvalue, &currentvalue_buf);
    LcdDma_Mutex_Give();
}

void Ui_Options_Opts_Key(KeyEvent_t event) {
    if (event.State & KEY_EVENT_PRESS) {
        if (event.Key == KEY_UP && !editing) {
            if (Options_Sel) {
                Options_Sel--;
                redrawopts();
                redrawopt();
            }
        } else if (event.Key == KEY_DOWN && !editing) {
            uint8_t max = 0;
            for (uint8_t opt=0;opt<OPTION_COUNT;opt++) {
                if (Options[opt].category == Options_Cat) max++;
            }
            if (Options_Sel < max-1) { //need to actually see how many things are in the category to know where to clamp this
                Options_Sel++;
                redrawopts();
                redrawopt();
            }
        } else if (event.Key == KEY_A) {
            Ui_Screen = UISCREEN_MAINMENU;
            if (editing) {
                *(uint8_t*)Options[Options_OptId].var = oldval;
                if (Options[Options_OptId].cb != NULL) Options[Options_OptId].cb();
            }
        } else if (event.Key == KEY_B) {
            if (editing) {
                *(uint8_t*)Options[Options_OptId].var = oldval;
                if (Options[Options_OptId].cb != NULL) Options[Options_OptId].cb();
                editing = false;
                redrawopt();
                Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT" Edit", true);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", true);
            } else {
                Ui_Screen = UISCREEN_OPTIONS_CATS;
            }
        } else if (event.Key == KEY_C) {
            if (!editing) {
                Ui_SoftBar_Update(2, true, LV_SYMBOL_SAVE" Save", true);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_CLOSE"Cancel", true);
                oldval = *(uint8_t*)Options[Options_OptId].var;
            } else {
                Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT" Edit", true);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", true);
                OptionsMgr_Touch();
            }
            editing = !editing;
        } else if (editing && event.Key == KEY_LEFT) {
            changevalue(false);
            redrawopt();
            if (Options[Options_OptId].cb != NULL) Options[Options_OptId].cb();
            
        } else if (editing && event.Key == KEY_RIGHT) {
            changevalue(true);
            redrawopt();
            if (Options[Options_OptId].cb != NULL) Options[Options_OptId].cb();
        }
    }
}