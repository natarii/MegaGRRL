#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ui.h"
#include "lvgl.h"
#include "key.h"
#include "lcddma.h"
#include "mallocs.h"
#include "taskmgr.h"

#include "ui/statusbar.h"
#include "ui/softbar.h"
#include "ui/filebrowser.h"
#include "ui/nowplaying.h"
#include "ui/mainmenu.h"
#include "ui/muting.h"
#include "ui/about.h"
#include "ui/options_cats.h"
#include "ui/options_opts.h"
#include "ui/fwupdate.h"
#include "ui/debug.h"

static const char* TAG = "Ui";

QueueHandle_t Ui_KeyQueue = NULL;
StaticQueue_t Ui_KeyStaticQueue;
static uint8_t Ui_KeyQueueBuf[UI_KEYQUEUE_SIZE*sizeof(KeyEvent_t)];

volatile UiScreen_t Ui_Screen;
UiScreen_t Ui_Screen_Last;

lv_obj_t *uiscreen;
lv_style_t uiscreenstyle;
lv_obj_t *hlines[2];
lv_style_t hlinestyle;
static lv_point_t hpoints[4];

bool Ui_EarlySetup() {
    ESP_LOGI(TAG, "Doing early setup...");

    ESP_LOGI(TAG, "Creating key queue");
    Ui_KeyQueue = xQueueCreateStatic(UI_KEYQUEUE_SIZE, sizeof(KeyEvent_t), &Ui_KeyQueueBuf[0], &Ui_KeyStaticQueue);
    if (Ui_KeyQueue == NULL) {
        ESP_LOGE(TAG, "Key queue create failed !!");
        return false;
    }

    ESP_LOGI(TAG, "Creating main UI screen");
    lv_style_copy(&uiscreenstyle, &lv_style_plain);
    uiscreenstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    uiscreenstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    uiscreen = lv_obj_create(NULL, NULL);
    lv_obj_set_style(uiscreen, &uiscreenstyle);
    lv_scr_load(uiscreen);
    LcdDma_Mutex_Give();

    Ui_Screen_Last = Ui_Screen = UISCREEN_INIT;

    return true;
}

bool Ui_Setup() {
    ESP_LOGI(TAG, "Setting up...");

    KeyMgr_TargetQueue = Ui_KeyQueue;

    Ui_SoftBar_Setup(uiscreen);
    Ui_StatusBar_Setup(uiscreen);
    Ui_FileBrowser_Setup();

    lv_style_copy(&hlinestyle, &lv_style_plain);
    hlinestyle.line.color = LV_COLOR_MAKE(127,127,127);
    hlinestyle.line.width = 1;
    hpoints[0].x = 0;
    hpoints[0].y = 34;
    hpoints[1].x = 239;
    hpoints[1].y = 34;
    hpoints[2].x = 0;
    hpoints[2].y = 320-34;
    hpoints[3].x = 239;
    hpoints[3].y = 320-34;
    for (uint8_t i=0;i<2;i++) {
        hlines[i] = lv_line_create(uiscreen, NULL);
        lv_line_set_style(hlines[i], &hlinestyle);
        lv_line_set_points(hlines[i], &hpoints[2*i], 2);
    }

    Ui_Screen = UISCREEN_MAINMENU;

    return true;
}

void Ui_Main() {
    ESP_LOGI(TAG, "Task start");
    KeyMgr_TargetTask = Taskmgr_Handles[TASK_UI]; //can't do this in early setup, because handle isn't known yet
    while (1) {
        xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(20));
        while (uxQueueMessagesWaiting(Ui_KeyQueue)) {
            KeyEvent_t event;
            xQueueReceive(Ui_KeyQueue, &event, 0);
            switch (Ui_Screen) {
                case UISCREEN_FILEBROWSER:
                    Ui_FileBrowser_Key(event);
                    break;
                case UISCREEN_NOWPLAYING:
                    Ui_NowPlaying_Key(event);
                    break;
                case UISCREEN_MAINMENU:
                    Ui_MainMenu_Key(event);
                    break;
                case UISCREEN_MUTING:
                    Ui_Muting_Key(event);
                    break;
                case UISCREEN_ABOUT:
                    Ui_About_Key(event);
                    break;
                case UISCREEN_OPTIONS_CATS:
                    Ui_Options_Cats_Key(event);
                    break;
                case UISCREEN_OPTIONS_OPTS:
                    Ui_Options_Opts_Key(event);
                    break;
                case UISCREEN_FWUPDATE:
                    Ui_Fwupdate_Key(event);
                    break;
                case UISCREEN_DEBUG:
                    Ui_Debug_Key(event);
                    break;
                default:
                    break;
            }
            if (Ui_Screen != Ui_Screen_Last) break;
        }
        if (Ui_Screen != Ui_Screen_Last) {
            switch (Ui_Screen_Last) {
                case UISCREEN_INIT:
                    break;
                case UISCREEN_FILEBROWSER:
                    Ui_FileBrowser_Destroy();
                    break;
                case UISCREEN_MAINMENU:
                    Ui_MainMenu_Destroy();
                    break;
                case UISCREEN_NOWPLAYING:
                    Ui_NowPlaying_Destroy();
                    break;
                case UISCREEN_MUTING:
                    Ui_Muting_Destroy();
                    break;
                case UISCREEN_ABOUT:
                    Ui_About_Destroy();
                    break;
                case UISCREEN_OPTIONS_CATS:
                    Ui_Options_Cats_Destroy();
                    break;
                case UISCREEN_OPTIONS_OPTS:
                    Ui_Options_Opts_Destroy();
                    break;
                case UISCREEN_FWUPDATE:
                    Ui_Fwupdate_Destroy();
                    break;
                case UISCREEN_DEBUG:
                    Ui_Debug_Destroy();
                    break;
                default:
                    break;
            }
            switch (Ui_Screen) {
                case UISCREEN_FILEBROWSER:
                    Ui_FileBrowser_Activate(uiscreen);
                    break;
                case UISCREEN_NOWPLAYING:
                    Ui_NowPlaying_Setup(uiscreen);
                    break;
                case UISCREEN_MAINMENU:
                    Ui_MainMenu_Setup(uiscreen);
                    break;
                case UISCREEN_MUTING:
                    Ui_Muting_Setup(uiscreen);
                    break;
                case UISCREEN_ABOUT:
                    Ui_About_Setup(uiscreen);
                    break;
                case UISCREEN_OPTIONS_CATS:
                    Ui_Options_Cats_Setup(uiscreen);
                    break;
                case UISCREEN_OPTIONS_OPTS:
                    Ui_Options_Opts_Setup(uiscreen);
                    break;
                case UISCREEN_FWUPDATE:
                    Ui_Fwupdate_Setup(uiscreen);
                    break;
                case UISCREEN_DEBUG:
                    Ui_Debug_Setup(uiscreen);
                    break;
                default:
                    break;
            }
            Ui_Screen_Last = Ui_Screen;
        }
        Ui_StatusBar_Tick();
        Ui_FileBrowser_Tick();
        switch (Ui_Screen) {
            case UISCREEN_NOWPLAYING:
                Ui_NowPlaying_Tick();
                break;
            /*case UISCREEN_MAINMENU:
                Ui_MainMenu_Tick();
                break;*/
            case UISCREEN_ABOUT:
                Ui_About_Tick();
                break;
            case UISCREEN_DEBUG:
                Ui_Debug_Tick();
                break;
            default:
                break;
        }
    }
}