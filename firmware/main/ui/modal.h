#ifndef AGR_UI_MODAL_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

void Ui_Modal_Setup(lv_obj_t *uiscreen);
void Ui_Modal_Key(KeyEvent_t event);
void Ui_Modal_Destroy();
void modal_show_simple(const char *CALLER_TAG, char *title, char *text, char *button);
bool modal_is_updated();
bool modal_is_visible();
void modal_update(bool HandleMutex);

#endif