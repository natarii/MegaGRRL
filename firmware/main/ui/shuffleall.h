#ifndef AGR_UI_SHUFFLEALL_H
#define AGR_UI_SHUFFLEALL_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_ShuffleAll_Setup(lv_obj_t *uiscreen);
void Ui_ShuffleAll_Destroy();
void Ui_ShuffleAll_Key(KeyEvent_t event);
void Ui_ShuffleAll_Tick();

#endif