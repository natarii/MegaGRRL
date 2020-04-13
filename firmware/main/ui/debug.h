#ifndef AGR_UI_DEBUG_H
#define AGR_UI_DEBUG_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_Debug_Setup(lv_obj_t *uiscreen);
void Ui_Debug_Destroy();
void Ui_Debug_Key(KeyEvent_t event);
void Ui_Debug_Tick();

#endif