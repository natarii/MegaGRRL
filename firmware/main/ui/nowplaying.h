#ifndef AGR_UI_NOWPLAYING_H
#define AGR_UI_NOWPLAYING_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

extern volatile bool Ui_NowPlaying_DataAvail;
extern volatile bool Ui_NowPlaying_DriverRunning;

void Ui_NowPlaying_Setup(lv_obj_t *uiscreen);
void Ui_NowPlaying_Key(KeyEvent_t event);
void Ui_NowPlaying_Tick();
void Ui_NowPlaying_Destroy();

#endif