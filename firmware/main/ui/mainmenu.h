#ifndef AGR_UI_MAINMENU_H
#define AGR_UI_MAINMENU_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_MainMenu_Setup(lv_obj_t *uiscreen);
void Ui_MainMenu_Destroy();
void Ui_MainMenu_Key(KeyEvent_t event);

#endif