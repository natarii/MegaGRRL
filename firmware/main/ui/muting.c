#include "muting.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../ui.h"
#include "softbar.h"
#include "../driver.h"

static IRAM_ATTR lv_obj_t *container;
const char *chnames[11] = {
    "FM 1",
    "FM 2",
    "FM 3",
    "FM 4",
    "FM 5",
    "FM 6 (FM Mode)",
    "FM 6 (DAC Mode)",
    "PSG 1",
    "PSG 2",
    "PSG 3",
    "PSG Noise"
};
const char *chnames_opna[11] = {
    "FM 1",
    "FM 2",
    "FM 3",
    "FM 4",
    "FM 5",
    "FM 6",
    "PCM",
    "SSG 1",
    "SSG 2",
    "SSG 3",
    "SSG Noise"
};
const char titletext[] = "Channel Muting Setup";
const char mute[] = "Mute";
const char unmute[] = "Unmute";
const char muted[] = LV_SYMBOL_MUTE;
const char unmuted[] = LV_SYMBOL_VOLUME_MAX;

IRAM_ATTR lv_obj_t *ch_label[11];
IRAM_ATTR lv_obj_t *ch_status[11];
IRAM_ATTR lv_obj_t *done_label;
lv_style_t ch_label_style;
lv_style_t ch_label_style_sel;
lv_style_t ch_on;
lv_style_t ch_off;
uint8_t ch_sel = 0;
IRAM_ATTR lv_obj_t *title;
lv_style_t title_style;



void Ui_Muting_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

bool ch_en(uint8_t ch) {
    if (ch <= 6) {
        return (Driver_FmMask & (1<<ch)) > 0;
    } else {
        return (Driver_PsgMask & (1<<(ch-7))) > 0;
    }
}

void ch_set(uint8_t ch, bool en) {
    if (ch <= 6) {
        Driver_FmMask = ((Driver_FmMask&~(1<<ch)) | ((en?1:0)<<ch));
    } else {
        ch -= 7;
        Driver_PsgMask = ((Driver_PsgMask&~(1<<ch)) | ((en?1:0)<<ch));
    }
}

void drawlist() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    for (uint8_t i=0;i<11;i++) {
        lv_label_set_style(ch_label[i], LV_LABEL_STYLE_MAIN, (ch_sel==i)?&ch_label_style_sel:&ch_label_style);
        lv_label_set_style(ch_status[i], LV_LABEL_STYLE_MAIN, ch_en(i)?&ch_on:&ch_off);
        lv_label_set_static_text(ch_status[i], ch_en(i)?unmuted:muted);
    }
    
    Ui_SoftBar_Update(2, true, (char *)(ch_en(ch_sel)?mute:unmute), false);

    LcdDma_Mutex_Give();
}

void Ui_Muting_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    lv_style_copy(&ch_label_style, &lv_style_plain);
    ch_label_style.text.color = LV_COLOR_MAKE(200,200,200);
    ch_label_style.text.font = &lv_font_dejavu_18;
    lv_style_copy(&ch_label_style_sel, &ch_label_style);
    ch_label_style_sel.text.color = LV_COLOR_MAKE(255,255,0);
    lv_style_copy(&title_style, &ch_label_style);
    title_style.text.color = LV_COLOR_MAKE(255,255,255);
    lv_style_copy(&ch_on, &ch_label_style);
    ch_on.text.color = LV_COLOR_MAKE(0,255,0);
    lv_style_copy(&ch_off, &ch_label_style);
    ch_off.text.color = LV_COLOR_MAKE(255,0,0);

    //ch_sel = 0;

    for (uint8_t i=0;i<11;i++) {
        ch_label[i] = lv_label_create(container, NULL);
        lv_obj_set_pos(ch_label[i], 42, 25+(20*i));
        //this is going to have to change a lot to support other mods without the same channel count:
        if (Driver_DetectedMod == MEGAMOD_NONE) {
            lv_label_set_static_text(ch_label[i], chnames[i]);
        } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
            lv_label_set_static_text(ch_label[i], chnames_opna[i]);
        }

        ch_status[i] = lv_label_create(container, NULL);
        lv_obj_set_pos(ch_status[i], 20, 24+(20*i));
    }

    title = lv_label_create(container, NULL);
    lv_label_set_style(title, LV_LABEL_STYLE_MAIN, &title_style);
    lv_label_set_static_text(title, titletext);
    lv_obj_set_pos(title, 10, 5);

    Ui_SoftBar_Update(0, true, "Togg.All", false);
    Ui_SoftBar_Update(1, true, LV_SYMBOL_AUDIO"Player", false);

    LcdDma_Mutex_Give();

    drawlist();
}

void Ui_Muting_ToggleAll() {
    bool anyon = false;
    for (uint8_t i=0;i<11;i++) {
        if (ch_en(i)) {
            anyon = true;
            break;
        }
    }
    for (uint8_t i=0;i<11;i++) {
        ch_set(i, !anyon);
    }
    if (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
    drawlist();
}

void Ui_Muting_Key(KeyEvent_t event) {
    switch (event.Key) {
        case KEY_A:
            if (event.State == KEY_EVENT_PRESS) Ui_Muting_ToggleAll();
            break;
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_C:
            if (event.State == KEY_EVENT_PRESS) {
                ch_set(ch_sel, !ch_en(ch_sel));
                if (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING) xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
                drawlist();
            }
            break;
        case KEY_B:
            if (event.State == KEY_EVENT_PRESS) {
                KeyMgr_Consume(KEY_B);
                Ui_Screen = UISCREEN_NOWPLAYING;
            }
            break;
        case KEY_UP:
            if (event.State & KEY_EVENT_PRESS) {
                if (ch_sel) {
                    ch_sel--;
                    drawlist();
                }
            }
            break;
        case KEY_DOWN:
            if (event.State & KEY_EVENT_PRESS) {
                if (ch_sel < 10) {
                    ch_sel++;
                    drawlist();
                }
            }
            break;
        default:
            break;
    };
}
