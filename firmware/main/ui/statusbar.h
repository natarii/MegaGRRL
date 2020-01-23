#ifndef AGR_UI_STATUSBAR_H
#define AGR_UI_STATUSBAR_H

#include "esp_system.h"
#include "lvgl.h"

bool Ui_StatusBar_Setup(lv_obj_t *uiscreen);
void Ui_StatusBar_Tick();
void Ui_StatusBar_SetExtract(bool extracting);

#endif