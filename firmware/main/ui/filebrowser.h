#ifndef AGR_UI_FILEBROWSER_H
#define AGR_UI_FILEBROWSER_H

#include "esp_system.h"
#include "lvgl.h"
#include "../key.h"

typedef enum {
    SORT_ASCENDING,
    SORT_DESCENDING,
    SORT_COUNT
} SortDirection_t;

extern volatile SortDirection_t Ui_FileBrowser_SortDir;
extern volatile bool Ui_FileBrowser_Sort;
extern volatile bool Ui_FileBrowser_DirsBeforeFiles;
extern volatile bool Ui_FileBrowser_Narrow;

void Ui_FileBrowser_Setup();
bool Ui_FileBrowser_Activate(lv_obj_t *uiscreen);
void Ui_FileBrowser_Destroy();
void Ui_FileBrowser_Key(KeyEvent_t event);
void Ui_FileBrowser_Tick();
void Ui_FileBrowser_InvalidateDirEntry();

#endif