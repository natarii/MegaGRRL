#ifndef AGR_UI_OPTIONS_OPTS_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_Options_Opts_Setup(lv_obj_t *uiscreen);
void Ui_Options_Opts_Destroy();
void Ui_Options_Opts_Key(KeyEvent_t event);
//void Ui_Options_Tick();

#endif