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
#include "../driver.h" //to know what megamod we have
#include <string.h>

static IRAM_ATTR lv_obj_t *container;

static uint8_t Options_OptId = 0;
static uint8_t Options_Sel = 0;
static uint8_t Options_Off = 0;

static IRAM_ATTR lv_obj_t *optionoptlines[5];
static IRAM_ATTR lv_obj_t *optionoptlabels[5];
static lv_style_t optionoptstyle_normal;
static lv_style_t optionoptstyle_sel;
static lv_style_t optiondescstyle;
static lv_style_t optionvalstyle;

//IRAM_ATTR lv_obj_t *optiontitle;
static IRAM_ATTR lv_obj_t *optiondesc;
static IRAM_ATTR lv_obj_t *optionvalue;
static IRAM_ATTR lv_obj_t *optiondefault;
static IRAM_ATTR lv_obj_t *optionvalue_val;
static IRAM_ATTR lv_obj_t *optiondefault_val;

static bool editing = false;
static uint8_t oldval = 0;

static const char *currentvalue = "Current:";
static const char *defaultvalue = "Default:";
static char currentvalue_buf[32] = "";
static char defaultvalue_buf[32] = "";

static lv_style_t headerstyle;
static IRAM_ATTR lv_obj_t *header;
static char headertext[64] = "Settings "LV_SYMBOL_RIGHT" ";
static uint8_t pages = 0;

static lv_style_t linestyle;
static IRAM_ATTR lv_obj_t *listdivs[2];

static const char *help1 = LV_SYMBOL_UP" "LV_SYMBOL_DOWN" to select setting";
static const char *help2 = LV_SYMBOL_LEFT" "LV_SYMBOL_RIGHT" to adjust value";
static IRAM_ATTR lv_obj_t *help;
static lv_style_t helpstyle;

void redrawopts();
void redrawopt();
static void displayvalue(char *buf, bool def) {
    if (!def && Options[Options_OptId].var == NULL) return;
    uint8_t val = def?Options[Options_OptId].defaultval:*(uint8_t*)Options[Options_OptId].var;
    switch (Options[Options_OptId].type) {
        case OPTION_TYPE_NUMERIC:
            switch (Options[Options_OptId].subtype) {
                case OPTION_SUBTYPE_NONE:
                case OPTION_SUBTYPE_BRIGHTNESS: //this one will go away as soon as the LUT exists
                    sprintf(buf, "%d", val);
                    break;
                case OPTION_SUBTYPE_LOOPS:
                    if (val == 255) {
                        strcpy(buf, "Infinite");
                    } else {
                        sprintf(buf, "%d", val);
                    }
                    break;
                case OPTION_SUBTYPE_FADELENGTH:
                    if (val > 1) {
                        sprintf(buf, "%d seconds", val);
                    } else {
                        strcpy(buf, "1 second");
                    }
                    break;
                case OPTION_SUBTYPE_PLAYMODE:
                    switch ((RepeatMode_t)val) {
                        case REPEAT_NONE:
                            strcpy(buf, "Repeat None");
                            break;
                        case REPEAT_ONE:
                            strcpy(buf, "Repeat One");
                            break;
                        case REPEAT_ALL:
                            strcpy(buf, "Repeat All");
                            break;
                        default:
                            break;
                    }
                    break;
                case OPTION_SUBTYPE_USERLED:
                    switch ((UserLedSource_t)val) {
                        case USERLED_SRC_NONE:
                            strcpy(buf, "Disabled");
                            break;
                        case USERLED_SRC_PLAYPAUSE:
                            strcpy(buf, "Play/Pause");
                            break;
                        case USERLED_SRC_DISK_ALL:
                            strcpy(buf, "SD Read");
                            break;
                        case USERLED_SRC_DISK_VGM:
                            strcpy(buf, "SD Read (VGM)");
                            break;
                        case USERLED_SRC_DISK_DSALL:
                            strcpy(buf, "SD Read (DS)");
                            break;
                        case USERLED_SRC_DISK_DSFILL:
                            strcpy(buf, "SD Read (DS Fill)");
                            break;
                        case USERLED_SRC_DISK_DSFIND:
                            strcpy(buf, "SD Read (DS Find)");
                            break;
                        case USERLED_SRC_DRIVERCPU:
                            strcpy(buf, "Driver CPU");
                            break;
                        default:
                            break;
                    }
                    break;
                case OPTION_SUBTYPE_SORTDIR:
                    switch ((SortDirection_t)val) {
                        case SORT_ASCENDING:
                            strcpy(buf, "Ascending");
                            break;
                        case SORT_DESCENDING:
                            strcpy(buf, "Descending");
                            break;
                        default:
                            break;
                    }
                    break;
                case OPTION_SUBTYPE_SCROLLTYPE:
                    switch ((ScrollType_t)val) {
                        case SCROLLTYPE_PINGPONG:
                            strcpy(buf, "Ping-Pong");
                            break;
                        case SCROLLTYPE_CIRCULAR:
                            strcpy(buf, "Circular");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    strcpy(buf, "OPTION_TYPE_NUMERIC");
                    break;
            }
            break;
        case OPTION_TYPE_BOOL:
            switch (Options[Options_OptId].subtype) {
                case OPTION_SUBTYPE_NONE:
                    if (val) {
                        strcpy(buf, "True");
                    } else {
                        strcpy(buf, "False");
                    }
                    break;
                case OPTION_SUBTYPE_STEREOMONO:
                    if (val) {
                        strcpy(buf, "Force Mono");
                    } else {
                        strcpy(buf, "Stereo");
                    }
                    break;
                case OPTION_SUBTYPE_SHUFFLE:
                    if (val) {
                        strcpy(buf, "On");
                    } else {
                        strcpy(buf, "Off");
                    }
                    break;
                default:
                    strcpy(buf, "OPTION_TYPE_BOOL");
                    break;
            }
            break;
        default:
            strcpy(buf, "Unknown option type");
            break;
    }
}

static void changevalue(bool inc) {
    uint8_t *var = (uint8_t*)Options[Options_OptId].var;
    switch (Options[Options_OptId].type) {
        case OPTION_TYPE_NUMERIC:
            switch (Options[Options_OptId].subtype) {
                case OPTION_SUBTYPE_NONE:
                case OPTION_SUBTYPE_BRIGHTNESS:
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
                case OPTION_SUBTYPE_FADELENGTH:
                    if (inc) {
                        if (*var < 10) {
                            *var += 1;
                        }
                    } else {
                        if (*var > 1) {
                            *var -= 1;
                        }
                    }
                    break;
                case OPTION_SUBTYPE_USERLED:
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
                case OPTION_SUBTYPE_LOOPS:
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
                case OPTION_SUBTYPE_PLAYMODE:
                    if (inc) {
                        if (*var < REPEAT_COUNT-1) *var += 1;
                    } else {
                        if (*var > 0) *var -= 1;
                    }
                    break;
                case OPTION_SUBTYPE_SORTDIR:
                    if (inc) {
                        *var = SORT_DESCENDING;
                    } else {
                        *var = SORT_ASCENDING;
                    }
                    break;
                case OPTION_SUBTYPE_SCROLLTYPE:
                    if (inc) {
                        if (*var < SCROLLTYPE_COUNT-1) *var += 1;
                    } else {
                        if (*var > 0) *var -= 1;
                    }
                    //update it so they can see it now
                    lv_label_set_long_mode(optiondesc, Ui_GetScrollType());
                    lv_obj_set_width(optiondesc, 220);
                    break;
                default:
                    //uh oh
                    break;
            }
            break;
        case OPTION_TYPE_BOOL:
            //subtype doesn't matter for changing a bool
            *var = (uint8_t)inc;
            break;
        default:
            break;
    }
}

void Ui_Options_Opts_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&optionoptstyle_normal, &lv_style_plain);
    optionoptstyle_normal.text.font = &lv_font_dejavu_18;
    optionoptstyle_normal.body.main_color = LV_COLOR_MAKE(0,0,0);
    optionoptstyle_normal.body.grad_color = LV_COLOR_MAKE(0,0,0);
    optionoptstyle_normal.text.color = LV_COLOR_MAKE(220,220,220);

    lv_style_copy(&optionvalstyle, &lv_style_plain);
    optionvalstyle.text.font = &lv_font_dejavu_14_2bpp;
    optionvalstyle.body.opa = 0;
    optionvalstyle.text.color = LV_COLOR_MAKE(220,220,220);

    lv_style_copy(&optionoptstyle_sel, &optionoptstyle_normal);
    optionoptstyle_sel.body.main_color = LV_COLOR_MAKE(0,0,100);
    optionoptstyle_sel.body.grad_color = LV_COLOR_MAKE(0,0,100);
    optionoptstyle_sel.body.radius = 8;

    optionoptstyle_normal.body.opa = 0; //must be after the copies

    int16_t y = 4;

    lv_style_copy(&headerstyle, &lv_style_plain);
    headerstyle.text.font = &lv_font_dejavu_14_2bpp;
    headerstyle.text.color = LV_COLOR_MAKE(127,127,127);
    header = lv_label_create(container, NULL);
    lv_obj_set_pos(header, 4, y);
    y += 20;
    lv_label_set_style(header, LV_LABEL_STYLE_MAIN, &headerstyle);
    uint8_t items = 0;
    for (uint8_t i=0;i<OPTION_COUNT;i++) {
        if (Driver_DetectedMod != MEGAMOD_OPNA && Options[i].uid == 0x0010) continue;
        if (Options[i].category != Options_Cat) continue;
        items++;
    }
    pages = (items/5) + ((items%5)?1:0);
    sprintf(&headertext[13], "%s (Pg. 1/%d)", OptionCatNames[Options_Cat], pages);
    lv_label_set_static_text(header, headertext);

    lv_style_copy(&linestyle, &lv_style_plain);
    linestyle.line.color = LV_COLOR_MAKE(255,255,255);
    linestyle.line.width = 1;
    listdivs[0] = lv_line_create(container, NULL);
    lv_line_set_style(listdivs[0], LV_LINE_STYLE_MAIN, &linestyle);
    static const lv_point_t divpoints[2] = {{0,0},{240,0}};
    lv_line_set_points(listdivs[0], divpoints, 2);
    lv_obj_set_pos(listdivs[0], 0, y);
    y += 1;

    for (uint8_t i=0;i<5;i++) {
        optionoptlines[i] = lv_cont_create(container, NULL);
        lv_obj_set_style(optionoptlines[i], &optionoptstyle_sel);
        lv_obj_set_height(optionoptlines[i], 25);
        lv_obj_set_width(optionoptlines[i],240);
        lv_obj_set_pos(optionoptlines[i], 0, y);
        y += 25;

        optionoptlabels[i] = lv_label_create(optionoptlines[i], NULL);
        lv_obj_set_pos(optionoptlabels[i], 2, 2);
        lv_label_set_text(optionoptlabels[i], "");
        lv_label_set_long_mode(optionoptlabels[i], (i==0xff)?Ui_GetScrollType():LV_LABEL_LONG_DOT); //todo
        lv_obj_set_width(optionoptlabels[i], 240);
    }

    listdivs[1] = lv_line_create(container, NULL);
    lv_line_set_style(listdivs[1], LV_LINE_STYLE_MAIN, &linestyle);
    lv_line_set_points(listdivs[1], divpoints, 2);
    lv_obj_set_pos(listdivs[1], 0, y);
    y += 1;

    lv_style_copy(&optiondescstyle, &lv_style_plain);
    optiondescstyle.text.font = &lv_font_dejavu_18;
    optiondescstyle.text.color = LV_COLOR_MAKE(127,127,127);

    optiondesc = lv_label_create(container, NULL);
    lv_obj_set_style(optiondesc, &optiondescstyle);
    y += 2; //
    lv_obj_set_pos(optiondesc, 10, y);
    y += 20;
    lv_label_set_long_mode(optiondesc, Ui_GetScrollType());
    lv_obj_set_width(optiondesc, 220);
    lv_label_set_anim_speed(optiondesc, 75);

    optiondefault = lv_label_create(container, NULL);
    lv_obj_set_style(optiondefault, &optiondescstyle);
    lv_obj_set_pos(optiondefault, 10, y);
    lv_label_set_static_text(optiondefault, defaultvalue);
    optiondefault_val = lv_label_create(container, NULL);
    lv_obj_set_style(optiondefault_val, &optionvalstyle);
    lv_obj_set_pos(optiondefault_val, 90, y+4);
    y += 20;

    optionvalue = lv_label_create(container, NULL);
    lv_obj_set_style(optionvalue, &optionoptstyle_normal);
    lv_obj_set_pos(optionvalue, 10, y);
    lv_label_set_static_text(optionvalue, currentvalue);
    optionvalue_val = lv_label_create(container, NULL);
    lv_obj_set_style(optionvalue_val, &optionvalstyle);
    lv_obj_set_pos(optionvalue_val, 90, y+4);
    y += 20;


    lv_style_copy(&helpstyle, &lv_style_plain);
    helpstyle.text.font = &lv_font_dejavu_14_2bpp;
    helpstyle.text.color = LV_COLOR_MAKE(255,255,0);
    help = lv_label_create(container, NULL);
    lv_label_set_long_mode(help, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(help, 240);
    lv_label_set_align(help, LV_LABEL_ALIGN_CENTER);
    lv_label_set_style(help, LV_LABEL_STYLE_MAIN, &helpstyle);
    lv_label_set_static_text(help, help1);
    lv_obj_set_pos(help, 0, 227);



    Ui_SoftBar_Update(0, true, LV_SYMBOL_HOME"Home", false);
    Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT"Edit", false);

    Options_OptId = 0;
    Options_Sel = 0;
    Options_Off = 0;
    editing = false;

    redrawopts();
    redrawopt();

    LcdDma_Mutex_Give();

}

void Ui_Options_Opts_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void redrawopts() {
    uint8_t idx = 0;
    uint8_t skip = 0;
    uint8_t off = (Options_Sel/5)*5;
    for (uint8_t opt=0;opt<OPTION_COUNT;opt++) {
        if (Driver_DetectedMod != MEGAMOD_OPNA && Options[opt].uid == 0x0010) continue;
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
            lv_label_set_long_mode(optionoptlabels[idx], Ui_GetScrollType());
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

    sprintf(&headertext[13], "%s (Pg. %d/%d)", OptionCatNames[Options_Cat], (Options_Sel/5)+1, pages);
    lv_label_set_static_text(header, headertext);

    displayvalue((char *)&defaultvalue_buf, true);
    lv_label_set_static_text(optiondefault_val, (char *)&defaultvalue_buf);
}

void redrawopt() {
    displayvalue((char *)&currentvalue_buf, false);
    lv_label_set_static_text(optionvalue_val, (char *)&currentvalue_buf);
}

void Ui_Options_Opts_Key(KeyEvent_t event) {
    if (event.State & KEY_EVENT_PRESS) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        if (event.Key == KEY_UP && !editing) {
            if (Options_Sel) {
                Options_Sel--;
                redrawopts();
                redrawopt();
            }
        } else if (event.Key == KEY_DOWN && !editing) {
            uint8_t max = 0;
            for (uint8_t opt=0;opt<OPTION_COUNT;opt++) {
                if (Driver_DetectedMod != MEGAMOD_OPNA && Options[opt].uid == 0x0010) continue;
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
                Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT" Edit", false);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
                lv_label_set_static_text(help, help1);
            } else {
                Ui_Screen = UISCREEN_OPTIONS_CATS;
            }
        } else if (event.Key == KEY_C) {
            if (!editing) {
                Ui_SoftBar_Update(2, true, LV_SYMBOL_SAVE" Save", false);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_CLOSE"Cancel", false);
                oldval = *(uint8_t*)Options[Options_OptId].var;
                lv_label_set_static_text(help, help2);
            } else {
                Ui_SoftBar_Update(2, true, LV_SYMBOL_EDIT" Edit", false);
                Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
                lv_label_set_static_text(help, help1);
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
        LcdDma_Mutex_Give();
    }
}
