#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../lcddma.h"
#include "../key.h"
#include "../queue.h"
#include "../player.h"
#include "../taskmgr.h"
#include "../options.h"
#include "../driver.h" //for buffer space
#include "softbar.h"
#include <stdio.h>
#include <dirent.h>

static const char* TAG = "Ui_ShuffleAll";

static IRAM_ATTR lv_obj_t *container;
static lv_style_t containerstyle;

static char thumbsdb[] = "Thumbs.db";
static char sysvolinfo[] = "System Volume Information";
static char recyclebin[] = "$Recycle.Bin";
#define BROWSER_IGNORE if (ent->d_name[0] == '.' || strcasecmp(ent->d_name, thumbsdb) == 0 || strcmp(ent->d_name, sysvolinfo) == 0 || strcmp(ent->d_name, recyclebin) == 0) continue;

static char *path;
static char *temppath;
static FILE *f;
static IRAM_ATTR lv_obj_t *fn;
static IRAM_ATTR lv_obj_t *cnt;
static IRAM_ATTR lv_obj_t *warn;
static IRAM_ATTR lv_obj_t *title;
static lv_style_t fnstyle;
static lv_style_t titlestyle;
static IRAM_ATTR uint32_t lastdraw = 0;
static IRAM_ATTR lv_obj_t *preload;
static bool already = false;
static char cntbuf[16] = "0 tracks";
static IRAM_ATTR uint32_t trackcount = 0;
static IRAM_ATTR uint32_t bufused = 0;

static void trimdir() {
    uint16_t l = 0xffff;
    for (uint16_t i=0;i<strlen(path);i++) {
        if (path[i] == '/') l = i;
    }
    path[l] = 0;
}

static void dumppls() {
    ESP_LOGD(TAG, "starting %s", path);
    DIR *dir = opendir(path);
    struct dirent *ent;
    while ((ent=readdir(dir))!=NULL) {
        BROWSER_IGNORE;
        if (ent->d_type == DT_DIR) {
            strcat(path, "/");
            strcat(path, ent->d_name);
            if (esp_timer_get_time() - lastdraw >= 500000) {
                LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                lv_label_set_text(fn, ent->d_name);
                sprintf(cntbuf, "%d tracks", trackcount);
                lv_label_set_static_text(cnt, cntbuf);
                LcdDma_Mutex_Give();
                ESP_LOGI(TAG, "progress: %s", ent->d_name);
                lastdraw = esp_timer_get_time();
            }
            dumppls();
            trimdir();
        } else if (ent->d_type == DT_REG) {
            uint32_t namelen = strlen(ent->d_name);
            if ((strcasecmp(&ent->d_name[namelen-4], ".vgm") == 0) || (strcasecmp(&ent->d_name[namelen-4], ".vgz") == 0)) {
                memcpy(&Driver_PcmBuf[bufused], path, strlen(path));
                bufused += strlen(path);
                Driver_PcmBuf[bufused++] = '/';
                memcpy(&Driver_PcmBuf[bufused], ent->d_name, strlen(ent->d_name));
                bufused += strlen(ent->d_name);
                Driver_PcmBuf[bufused++] = 0x0a;
                trackcount++;

                if (bufused >= 79500) {
                    ESP_LOGI(TAG, "Flushing");
                    fwrite(Driver_PcmBuf, 1, bufused, f);
                    bufused = 0;
                }
            }
        }
    }
    closedir(dir);
}

void Ui_ShuffleAll_Setup(lv_obj_t *uiscreen) {
    if (!Queue_Shuffle) {
        Queue_Shuffle = true;
        OptionsMgr_Touch();
    }

    ESP_LOGI(TAG, "request player stop");
    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
    ESP_LOGI(TAG, "wait player stop");
    xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
    
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, false, "", false);
    Ui_SoftBar_Update(2, false, "", false);

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);
    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);

    preload = lv_preload_create(container, NULL);
    lv_obj_set_pos(preload, 95, 100);
    lv_obj_set_size(preload, 50, 50);

    fn = lv_label_create(container, NULL);
    lv_style_copy(&fnstyle, &lv_style_plain);
    fnstyle.text.font = &lv_font_unscii_8;
    fnstyle.text.color = LV_COLOR_MAKE(255,255,255);
    fnstyle.text.line_space = 0;
    fnstyle.text.letter_space = -1;
    lv_label_set_style(fn, LV_LABEL_STYLE_MAIN, &fnstyle);
    lv_label_set_long_mode(fn, LV_LABEL_LONG_DOT);
    lv_label_set_align(fn, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_pos(fn, 4, 155);
    lv_obj_set_size(fn, 232, 10);
    lv_label_set_static_text(fn, "Loading...");

    cnt = lv_label_create(container, NULL);
    lv_label_set_style(cnt, LV_LABEL_STYLE_MAIN, &fnstyle);
    lv_label_set_long_mode(cnt, LV_LABEL_LONG_BREAK);
    lv_label_set_align(cnt, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_pos(cnt, 4, 165);
    lv_obj_set_size(cnt, 232, 10);
    lv_label_set_static_text(cnt, "0 tracks");
    lv_obj_set_hidden(cnt, true);

    title = lv_label_create(container, NULL);
    lv_style_copy(&titlestyle, &lv_style_plain);
    titlestyle.text.font = &lv_font_dejavu_18;
    titlestyle.text.color = LV_COLOR_MAKE(255,255,255);
    lv_label_set_style(title, LV_LABEL_STYLE_MAIN, &titlestyle);
    lv_label_set_long_mode(title, LV_LABEL_LONG_BREAK);
    lv_label_set_align(title, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_pos(title, 4, 79);
    lv_obj_set_size(title, 232, 17);
    lv_label_set_static_text(title, "Shuffling All Tracks...");

    warn = lv_label_create(container, NULL);
    lv_label_set_style(warn, LV_LABEL_STYLE_MAIN, &fnstyle);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_BREAK);
    lv_label_set_align(warn, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_pos(warn, 0, 240);
    lv_obj_set_size(warn, 240, 10);
    lv_label_set_static_text(warn, "Please do not power off!");

    lastdraw = esp_timer_get_time();

    LcdDma_Mutex_Give();

    if (!already) {
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_obj_set_hidden(cnt, false);
        LcdDma_Mutex_Give();

        f = fopen("/sd/.mega/fullcard.m3u", "w");

        path = malloc(264);
        temppath = malloc(264);
        if (path == NULL || temppath == NULL) {
            ESP_LOGE(TAG, "out of heap in shuffle all");
            return;
        }
        strcpy(path, "/sd");
        ESP_LOGI(TAG, "start dumping playlist");
        dumppls();
        
        if (bufused) {
            ESP_LOGI(TAG, "Flushing");
            fwrite(Driver_PcmBuf, 1, bufused, f);
        }

        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_obj_set_hidden(cnt, true);
        lv_label_set_static_text(fn, "Loading...");
        LcdDma_Mutex_Give();

        fclose(f);

        free(path);
        free(temppath);

        already = true;
    }

    ESP_LOGI(TAG, "load m3u");
    QueueLoadM3u("/sd/.mega", "/sd/.mega/fullcard.m3u", 0, false, false);
    QueuePosition = 0; //hacky
    ESP_LOGI(TAG, "request start");
    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_START_RUNNING, eSetValueWithoutOverwrite);
    Ui_Screen = UISCREEN_NOWPLAYING;
    ESP_LOGI(TAG, "ok");
}

void Ui_ShuffleAll_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void Ui_ShuffleAll_Key(KeyEvent_t event) {

}
