#ifndef AGR_UI_H
#define AGR_UI_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "lvgl.h"

typedef enum {
    UISCREEN_INIT,
    UISCREEN_MAINMENU,
    UISCREEN_FILEBROWSER,
    UISCREEN_NOWPLAYING,
    UISCREEN_OPTIONS_CATS,
    UISCREEN_OPTIONS_OPTS,
    UISCREEN_MUTING,
    UISCREEN_ABOUT,
    UISCREEN_FWUPDATE,
    UISCREEN_DEBUG,

    UISCREEN_COUNT
} UiScreen_t;

extern volatile UiScreen_t Ui_Screen;
extern UiScreen_t Ui_Screen_Last;

bool Ui_EarlySetup();
bool Ui_Setup();
void Ui_Main();

#endif