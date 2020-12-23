#include "freertos/FreeRTOS.h"
#include "statusbar.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../battery.h"
#include "../ioexp.h"
#include "../hal.h"
#include "freertos/event_groups.h"
#include "../driver.h"
#include "../player.h"
#include "../queue.h"

//static const char* TAG = "Ui_StatusBar";

static IRAM_ATTR lv_obj_t *container;
#if defined HWVER_PORTABLE
static IRAM_ATTR lv_obj_t *charge;
static IRAM_ATTR lv_obj_t *battery;
#endif

typedef enum {
    ICON_PLAYING,
    ICON_PAUSED,
    ICON_STOPPED,
    ICON_CHANNELMUTED,
    ICON_MONO,
    ICON_STEREO,
    ICON_SOUND,
    ICON_EXTRACT,
    ICON_BLANK,
    ICON_REPEAT_ONE,
    ICON_REPEAT_ALL,
    ICON_SHUFFLE,
    ICON_COUNT
} StatusbarIcon_t;

typedef struct {
    const char *c;
    int8_t xoffset;
    int8_t yoffset;
} StatusbarIconDef_t;

const StatusbarIconDef_t icondefs[] = {
    {LV_SYMBOL_PLAY, 0, 0},
    {LV_SYMBOL_PAUSE, 0, 0},
    {LV_SYMBOL_STOP, 0, 0},
    {LV_SYMBOL_WARNING, -2, 0},
    {"MO", -3, 0},
    {"ST", 0, 0},
    {LV_SYMBOL_VOLUME_MAX, 5, 0},
    {LV_SYMBOL_UPLOAD, 0, 0},
    {"", 0, 0},
    {"1", 0, 0},
    {"A", 0, 0},
    {LV_SYMBOL_SHUFFLE, 0, 0},
};

static IRAM_ATTR lv_obj_t *iconlabels[6];

void Ui_StatusBar_DrawIcon(StatusbarIcon_t icon, lv_obj_t *label, int16_t x, int16_t y) {
    lv_label_set_static_text(label, icondefs[icon].c);
    lv_obj_set_pos(label, x+icondefs[icon].xoffset, y+icondefs[icon].yoffset);
}

bool Ui_StatusBar_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    static lv_style_t statusstyle;
    lv_style_copy(&statusstyle, &lv_style_plain);
    statusstyle.body.main_color = LV_COLOR_MAKE(0,0,64);
    statusstyle.body.grad_color = LV_COLOR_MAKE(0,0,127);
    statusstyle.body.radius = 0;
    statusstyle.text.font = &lv_font_dejavu_18;
    statusstyle.text.color = LV_COLOR_MAKE(255,255,255);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &statusstyle);
    lv_obj_set_height(container, 34);
    lv_obj_set_width(container, 240);
    //lv_cont_set_fit(container, false, false);

    for (uint8_t i=0;i<6;i++) {
        iconlabels[i] = lv_label_create(container, NULL);
        lv_label_set_static_text(iconlabels[i], "");
    }

    #if defined HWVER_PORTABLE
    charge = lv_label_create(container, NULL);
    lv_obj_set_pos(charge, 195, 6);
    lv_label_set_static_text(charge, "");
    lv_obj_set_width(charge, 24);

    battery = lv_label_create(container, NULL);
    lv_obj_set_pos(battery, 210, 6);
    lv_label_set_static_text(battery, "");
    lv_obj_set_width(charge, 24);
    #endif

    LcdDma_Mutex_Give();
    return true;
}

void Ui_StatusBar_RedrawIcons() {
    StatusbarIcon_t i;
    EventBits_t playerevents = xEventGroupGetBits(Player_Status);
    if (playerevents & PLAYER_STATUS_PAUSED) {
        i = ICON_PAUSED;
    } else if (playerevents & PLAYER_STATUS_RUNNING) {
        i = ICON_PLAYING;
    } else {
        i = ICON_STOPPED;
    }
    Ui_StatusBar_DrawIcon(i, iconlabels[0], 8, 7);

    Ui_StatusBar_DrawIcon(Driver_ForceMono?ICON_MONO:ICON_STEREO, iconlabels[1], 30, 7);

    if (Driver_FmMask == 0b01111111 && Driver_PsgMask == 0b00001111) {
        Ui_StatusBar_DrawIcon(ICON_SOUND, iconlabels[2], 54, 7);
    } else {
        Ui_StatusBar_DrawIcon(ICON_CHANNELMUTED, iconlabels[2], 60, 7);
    }

    switch (Player_RepeatMode) {
        case REPEAT_NONE:
            Ui_StatusBar_DrawIcon(ICON_BLANK, iconlabels[4], 83, 7); //0px wide...
            break;
        case REPEAT_ONE:
            Ui_StatusBar_DrawIcon(ICON_REPEAT_ONE, iconlabels[4], 83, 7);
            break;
        case REPEAT_ALL:
            Ui_StatusBar_DrawIcon(ICON_REPEAT_ALL, iconlabels[4], 83, 7);
            break;
        default:
            //oh god oh fuck
            break;
    }

    uint16_t nextx = 83;
    if (Player_RepeatMode != REPEAT_NONE) nextx += 17;
    Ui_StatusBar_DrawIcon(Queue_Shuffle?ICON_SHUFFLE:ICON_BLANK, iconlabels[5], nextx, 7);
}

void Ui_StatusBar_SetExtract(bool extracting) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    if (extracting) {
        uint16_t nextx = 83;
        if (Player_RepeatMode != REPEAT_NONE) nextx += 17;
        if (Queue_Shuffle) nextx += 22;
        Ui_StatusBar_DrawIcon(ICON_EXTRACT, iconlabels[3], nextx, 7);
    } else {
        lv_label_set_static_text(iconlabels[3], "");
    }
    LcdDma_Mutex_Give();
}

IRAM_ATTR uint32_t last = 0;
EventBits_t lastplayerevents = 0xff;
bool lastforcemono = true;
uint8_t statusbarmasks[2] = {0xff, 0xff};
uint8_t lastrepeatmode = 0xff;
bool lastshuffle = false;
void Ui_StatusBar_Tick() {
    if (xthal_get_ccount() - last >= 24000000) {
        EventBits_t playerevents = xEventGroupGetBits(Player_Status);
        bool redraw = false;
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        if (playerevents != lastplayerevents) {
            redraw = true;
        }
        if (Driver_ForceMono != lastforcemono) {
            redraw = true;
        }
        if (Driver_FmMask != statusbarmasks[0] || Driver_PsgMask != statusbarmasks[1]) {
            redraw = true;
        }
        if (lastrepeatmode != Player_RepeatMode) {
            redraw = true;
        }
        if (lastshuffle != Queue_Shuffle) {
            redraw = true;
        }
        if (redraw) Ui_StatusBar_RedrawIcons();
        #if defined HWVER_PORTABLE
        if (IoExp_ChargeStatus()) {
            lv_label_set_text(charge, LV_SYMBOL_CHARGE);
        } else {
            lv_label_set_static_text(charge, "");
        }
        if (BatteryMgr_Voltage >= 3900) {
            lv_label_set_text(battery, LV_SYMBOL_BATTERY_FULL);
        } else if (BatteryMgr_Voltage >= 3600) {
            lv_label_set_text(battery, LV_SYMBOL_BATTERY_3);
        } else if (BatteryMgr_Voltage >= 3400) {
            lv_label_set_text(battery, LV_SYMBOL_BATTERY_2);
        } else if (BatteryMgr_Voltage >= 3300) {
            lv_label_set_text(battery, LV_SYMBOL_BATTERY_1);
        } else {
            lv_label_set_text(battery, LV_SYMBOL_BATTERY_EMPTY);
        }
        #endif
        LcdDma_Mutex_Give();
        lastplayerevents = playerevents;
        lastforcemono = Driver_ForceMono;
        statusbarmasks[0] = Driver_FmMask;
        statusbarmasks[1] = Driver_PsgMask;
        lastrepeatmode = Player_RepeatMode;
        lastshuffle = Queue_Shuffle;
        last = xthal_get_ccount();
    }
}
