#ifndef AGR_UI_ABOUT_H
#define AGR_UI_ABOUT_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_About_Setup(lv_obj_t *uiscreen);
void Ui_About_Destroy();
void Ui_About_Key(KeyEvent_t event);
void Ui_About_Tick();

#endif