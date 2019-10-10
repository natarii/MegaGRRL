#ifndef AGR_UI_MUTING_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_Muting_Setup(lv_obj_t *uiscreen);
void Ui_Muting_Key(KeyEvent_t event);
void Ui_Muting_Destroy();

#endif