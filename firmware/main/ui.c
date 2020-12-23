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
#include "ui/shuffleall.h"
#include "ui/modal.h"
#include "ui/easteregg.h"

static const char* TAG = "Ui";

QueueHandle_t Ui_KeyQueue = NULL;
StaticQueue_t Ui_KeyStaticQueue;
static uint8_t Ui_KeyQueueBuf[UI_KEYQUEUE_SIZE*sizeof(KeyEvent_t)];

volatile bool Ui_ScreenshotEnabled = false;

volatile ScrollType_t Ui_ScrollType = SCROLLTYPE_PINGPONG;

volatile UiScreen_t Ui_Screen;
UiScreen_t Ui_Screen_Last;

lv_obj_t *uiscreen;
lv_style_t uiscreenstyle;
lv_obj_t *hlines[2];
lv_style_t hlinestyle;
static lv_point_t hpoints[4];

static const lv_label_long_mode_t scrolltypeLUT[SCROLLTYPE_COUNT] = {LV_LABEL_LONG_SROLL, LV_LABEL_LONG_SROLL_CIRC, LV_LABEL_LONG_DOT};

static bool modal_visible = false;

lv_label_long_mode_t Ui_GetScrollType() {
    return scrolltypeLUT[Ui_ScrollType];
}

static uint8_t key_hist[10] = {0,0,0,0,0,0,0,0,0,0};
static uint8_t key_hist_idx = 0;
static const uint8_t key_easteregg[10] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_B, KEY_A};

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
    hlinestyle.line.color = LV_COLOR_MAKE(255,255,255);
    hlinestyle.line.width = 1;
    hpoints[0].x = 0;
    hpoints[0].y = 34;
    hpoints[1].x = 239;
    hpoints[1].y = 34;
    hpoints[2].x = 0;
    hpoints[2].y = 319-34;
    hpoints[3].x = 239;
    hpoints[3].y = 319-34;
    for (uint8_t i=0;i<2;i++) {
        hlines[i] = lv_line_create(uiscreen, NULL);
        lv_line_set_style(hlines[i], LV_LINE_STYLE_MAIN, &hlinestyle);
        lv_line_set_points(hlines[i], &hpoints[2*i], 2);
    }

    Ui_Screen = UISCREEN_MAINMENU;

    return true;
}

void Ui_Main() {
    //todo: clean up this disaster
    ESP_LOGI(TAG, "Task start");
    KeyMgr_TargetTask = Taskmgr_Handles[TASK_UI]; //can't do this in early setup, because handle isn't known yet
    while (1) {
        xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(20));
        while (uxQueueMessagesWaiting(Ui_KeyQueue)) {
            KeyEvent_t event;
            xQueueReceive(Ui_KeyQueue, &event, 0);
            if (event.State == KEY_EVENT_PRESS) {
                key_hist[key_hist_idx++] = event.Key;
                if (key_hist_idx == 10) key_hist_idx = 0;
            }
            if (Ui_ScreenshotEnabled && event.Key == KEY_A) {
                if (event.State & KEY_EVENT_DOWN) {
                    ESP_LOGI(TAG, "Taking screenshot");
                    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                    LcdDma_Screenshot = true;
                    LcdDma_Mutex_Give();
                    vTaskDelay(pdMS_TO_TICKS(500)); //kinda hacky...
                } else if (event.State & KEY_EVENT_HOLD) {
                    KeyMgr_Consume(KEY_A);
                    ESP_LOGI(TAG, "Disabling screenshot key");
                    Ui_ScreenshotEnabled = false;
                }
            } else if (modal_visible) {
                Ui_Modal_Key(event);
            } else {
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
                    case UISCREEN_SHUFFLEALL:
                        Ui_ShuffleAll_Key(event);
                        break;
                    case UISCREEN_EASTEREGG:
                        Ui_EasterEgg_Key(event);
                        break;
                    default:
                        break;
                }
                if (Ui_Screen != Ui_Screen_Last) break;
            }
        }

        bool new_modal_visible = modal_is_visible();
        if (!modal_visible) {
            if (new_modal_visible) {
                ESP_LOGI(TAG, "Creating modal window");
                Ui_Modal_Setup(uiscreen);
            }
        } else {
            if (!new_modal_visible) {
                ESP_LOGI(TAG, "Destroying modal window");
                Ui_Modal_Destroy();
            } else if (modal_is_updated()) {
                ESP_LOGI(TAG, "Redrawing modal window");
                modal_update(true);
            }
        }
        modal_visible = new_modal_visible;

        bool matched_egg = true;
        for (uint8_t i=0;i<10;i++) {
            uint8_t check = (key_hist_idx+i)%10;
            if (key_hist[check] != key_easteregg[i]) matched_egg = false;
        }

        if (matched_egg) {
            memset(key_hist, 0, 10);
            Ui_Screen = UISCREEN_EASTEREGG;
        }

        if (Ui_Screen != Ui_Screen_Last) {
            UiScreen_t scr = Ui_Screen;
            ESP_LOGI(TAG, "screen %d -> %d", Ui_Screen_Last, Ui_Screen);
            ESP_LOGI(TAG, "destroying old");
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
                case UISCREEN_SHUFFLEALL:
                    Ui_ShuffleAll_Destroy();
                    break;
                case UISCREEN_EASTEREGG:
                    Ui_EasterEgg_Destroy();
                    break;
                default:
                    break;
            }
            ESP_LOGI(TAG, "creating new");
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
                case UISCREEN_SHUFFLEALL:
                    Ui_ShuffleAll_Setup(uiscreen);
                    break;
                case UISCREEN_EASTEREGG:
                    Ui_EasterEgg_Setup(uiscreen);
                    break;
                default:
                    break;
            }
            Ui_Screen_Last = scr; //one of the setups might have changed the actual Ui_Screen var, so use this backup. we should really have a function to change screen, not just setting a var...
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