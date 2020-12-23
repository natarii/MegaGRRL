#ifndef AGR_UI_EASTEREGG_H
#define AGR_UI_EASTEREGG_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_EasterEgg_Setup(lv_obj_t *uiscreen);
void Ui_EasterEgg_Destroy();
void Ui_EasterEgg_Key(KeyEvent_t event);

#endif