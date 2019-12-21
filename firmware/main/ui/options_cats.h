#ifndef AGR_UI_OPTIONS_CATS_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

extern uint8_t Options_Cat;

void Ui_Options_Cats_Setup(lv_obj_t *uiscreen);
void Ui_Options_Cats_Destroy();
void Ui_Options_Cats_Key(KeyEvent_t event);
//void Ui_Options_Tick();

#endif