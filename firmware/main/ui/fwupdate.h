#ifndef AGR_UI_FWUPDATE_H
#define AGR_UI_FWUPDATE_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

extern char *fwupdate_file;

void Ui_Fwupdate_Setup(lv_obj_t *uiscreen);
void Ui_Fwupdate_Destroy();
void Ui_Fwupdate_Key(KeyEvent_t event);
void Ui_Fwupdate_Tick();

#endif