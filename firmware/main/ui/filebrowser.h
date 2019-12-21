#ifndef AGR_UI_FILEBROWSER_H
#define AGR_UI_FILEBROWSER_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_FileBrowser_Setup();
bool Ui_FileBrowser_Activate(lv_obj_t *uiscreen);
void Ui_FileBrowser_Destroy();
void Ui_FileBrowser_Key(KeyEvent_t event);
void Ui_FileBrowser_Tick();

#endif