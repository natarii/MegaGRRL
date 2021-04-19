#ifndef AGR_UI_SPFM_H
#define AGR_UI_SPFM_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_Spfm_Setup(lv_obj_t *uiscreen);
void Ui_Spfm_Destroy();
void Ui_Spfm_Key(KeyEvent_t event);
void Ui_Spfm_Tick();

#endif