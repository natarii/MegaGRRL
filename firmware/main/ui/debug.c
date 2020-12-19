#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../ui.h"
#include "../key.h"
#include "../lcddma.h"
#include "freertos/task.h"
#include "debug.h"
#include "softbar.h"
#include "../taskmgr.h"
#include "../driver.h"
#include "../mallocs.h"
#include "../dacstream.h"

static const char* TAG = "Ui_Debug";

static IRAM_ATTR lv_obj_t *container;
lv_style_t containerstyle;
static IRAM_ATTR lv_obj_t *tasklabel;
static lv_style_t tasklabel_style;
static IRAM_ATTR lv_obj_t *driverbuflabel;
static IRAM_ATTR lv_obj_t *heaplabel;
static IRAM_ATTR lv_obj_t *dslabel;
static lv_style_t bar_style;
static lv_style_t bar_style_idle;
static IRAM_ATTR lv_obj_t *driverbuf;
static IRAM_ATTR lv_obj_t *driverbufpcm;
static IRAM_ATTR lv_obj_t *ds[DACSTREAM_PRE_COUNT];
static char tasklabel_buf[(50*TASK_COUNT)+50];
static TaskStatus_t taskstatus[TASK_COUNT+6]; //6 = IDLE0, IDLE1, Tmr Svc, ipc0, ipc1, esp_timer
static const char *taskblacklist[6] = { "IDLE0", "IDLE1", "Tmr Svc", "ipc0", "ipc1", "esp_timer" };
static IRAM_ATTR uint32_t timer = 0;
static IRAM_ATTR uint32_t taskstimer = 0;
static IRAM_ATTR uint32_t tasklastruntime[TASK_COUNT+6];
static IRAM_ATTR uint32_t tasklasttrt = 0;
static char heapbuf[100] = "";
static char drvbuf[100] = "";
static char samplebuf1[50] = "";
static char samplebuf2[75] = "";
static IRAM_ATTR lv_obj_t *samplelabel1;
static IRAM_ATTR lv_obj_t *samplelabel2;
static bool fast = false;

static int comp(const void *a, const void *b) {
    const TaskStatus_t *tsa = a, *tsb = b;
    return strcmp(tsa->pcTaskName, tsb->pcTaskName);
}

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void draw() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    uint16_t d = MegaStream_Used(&Driver_CommandStream);
    uint16_t p = MegaStream_Used(&Driver_PcmStream);
    sprintf(drvbuf, "#00007f DrvBuf# %5d/%5d #00007f PCM# %5d/%5d", d, DRIVER_QUEUE_SIZE, p, DRIVER_QUEUE_SIZE);
    lv_label_set_static_text(driverbuflabel, drvbuf);
    lv_obj_set_size(driverbuf, map(d,0,DRIVER_QUEUE_SIZE,0,240), 1);
    lv_label_set_static_text(driverbuflabel, drvbuf);
    lv_obj_set_size(driverbufpcm, map(p,0,DRIVER_QUEUE_SIZE,0,240), 1);
    sprintf(samplebuf1, "#00007f Driver cur sample:# %d", Driver_Sample);
    lv_label_set_static_text(samplelabel1, samplebuf1);
    uint32_t s = Driver_Sample;
    uint32_t ns = Driver_NextSample+1;
    if (!Driver_FirstWait) {
        if (s>ns) {
            sprintf(samplebuf2, "#00007f Driver lag:# #ff1f1f %06d samples# ;_;", s-ns);
        } else {
            strcpy(samplebuf2, "#00007f Driver lag:# 000000 samples ^_^");
        }
    } else {
        sprintf(samplebuf2, "#00007f Driver lag:# #00ff00 %06d samples# >_>", (s>ns)?(s-ns):0);
    }
    lv_label_set_static_text(samplelabel2, samplebuf2);

    for (uint8_t i=0;i<DACSTREAM_PRE_COUNT;i++) {
        lv_obj_set_style(ds[i], (i==DacStreamId)?&bar_style:&bar_style_idle);
        lv_obj_set_size(ds[i], map(MegaStream_Used((MegaStreamContext_t *)&DacStreamEntries[i].Stream), 0, DACSTREAM_BUF_SIZE, 0, 240), 1);
    }

    LcdDma_Mutex_Give();
}

static void drawtasks() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    char *bp = &tasklabel_buf[0];
    strcpy(bp, "#00007f Task   Core MinStack CPU%#\n");
    bp += strlen(&tasklabel_buf[0]);

    uint32_t trt;
    uxTaskGetSystemState(&taskstatus[0], TASK_COUNT+6, &trt);
    qsort(taskstatus, TASK_COUNT+6, sizeof(TaskStatus_t), comp);
    for (uint8_t i=0;i<TASK_COUNT+6;i++) {
        uint8_t blacklisted = false;
        for (uint8_t q=0;q<6;q++) {
            if (strcmp(taskstatus[i].pcTaskName, taskblacklist[q]) == 0) {
                blacklisted = true;
                break;
            };
        }
        if (blacklisted) continue;
        uint32_t cpu = (taskstatus[i].ulRunTimeCounter-tasklastruntime[i]) / ((trt-tasklasttrt)/100);
        bp += sprintf(bp, "%s %4d %8d %3d%%\n", taskstatus[i].pcTaskName, (taskstatus[i].xHandle==Taskmgr_Handles[TASK_DRIVER])?1:0, taskstatus[i].usStackHighWaterMark, cpu);
        tasklastruntime[i] = taskstatus[i].ulRunTimeCounter;
    }
    tasklasttrt = trt;

    lv_label_set_static_text(tasklabel, &tasklabel_buf[0]);

    sprintf(heapbuf, "#00007f FreeHeap# %5d #00007f ctg# %5d #00007f min# %5d", xPortGetFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), xPortGetMinimumEverFreeHeapSize());
    lv_label_set_static_text(heaplabel, heapbuf);
    
    LcdDma_Mutex_Give();
}

void Ui_Debug_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);
    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(127,0,127);
    containerstyle.body.grad_color = LV_COLOR_MAKE(127,0,127);

    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    uint16_t y = 0;

    lv_style_copy(&tasklabel_style, &lv_style_plain);
    tasklabel_style.body.main_color = LV_COLOR_MAKE(127,0,127);
    tasklabel_style.body.grad_color = LV_COLOR_MAKE(127,0,127);
    tasklabel_style.text.color = LV_COLOR_MAKE(255,255,255);
    tasklabel_style.text.font = &lv_font_unscii_8;
    tasklabel_style.text.line_space = 0;
    tasklabel_style.text.letter_space = -1;

    tasklabel = lv_label_create(container, NULL);
    lv_obj_set_pos(tasklabel, 1, 2);
    y = 2 + (TASK_COUNT+1)*9;
    lv_label_set_style(tasklabel, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(tasklabel, true);

    heaplabel = lv_label_create(container, NULL);
    lv_obj_set_pos(heaplabel, 1, y);
    y += 9;
    lv_label_set_style(heaplabel, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(heaplabel, true);

    driverbuflabel = lv_label_create(container, NULL);
    lv_obj_set_pos(driverbuflabel, 1, y);
    y += 9;
    lv_label_set_style(driverbuflabel, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(driverbuflabel, true);

    lv_style_copy(&bar_style, &lv_style_plain);
    bar_style.body.main_color = LV_COLOR_MAKE(255,255,255);
    bar_style.body.grad_color = LV_COLOR_MAKE(255,255,255);
    lv_style_copy(&bar_style_idle, &bar_style);
    bar_style_idle.body.main_color = LV_COLOR_MAKE(0,255,0);
    bar_style_idle.body.grad_color = LV_COLOR_MAKE(0,255,0);

    driverbuf = lv_obj_create(container, NULL);
    lv_obj_set_pos(driverbuf, 0, y-1);
    y += 1;
    lv_obj_set_size(driverbuf, 240, 1);
    lv_obj_set_style(driverbuf, &bar_style);

    y += 1; //spacer

    driverbufpcm = lv_obj_create(container, NULL);
    lv_obj_set_pos(driverbufpcm, 0, y-1);
    y += 1;
    lv_obj_set_size(driverbufpcm, 240, 1);
    lv_obj_set_style(driverbufpcm, &bar_style);

    samplelabel1 = lv_label_create(container, NULL);
    lv_obj_set_pos(samplelabel1, 1, y);
    y += 9;
    lv_label_set_style(samplelabel1, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(samplelabel1, true);

    samplelabel2 = lv_label_create(container, NULL);
    lv_obj_set_pos(samplelabel2, 1, y);
    y += 9;
    lv_label_set_style(samplelabel2, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(samplelabel2, true);

    dslabel = lv_label_create(container, NULL);
    lv_obj_set_pos(dslabel, 1, y);
    y += 9;
    lv_label_set_style(dslabel, LV_LABEL_STYLE_MAIN, &tasklabel_style);
    lv_label_set_recolor(dslabel, true);
    lv_label_set_static_text(dslabel, "#00007f DAC Streams#");

    for (uint8_t i=0;i<DACSTREAM_PRE_COUNT;i++) {
        ds[i] = lv_obj_create(container, NULL);
        lv_obj_set_pos(ds[i], 0, y-1);
        y += 2;
        lv_obj_set_size(ds[i], 240, 1);
        lv_obj_set_style(ds[i], &bar_style);
    }

    Ui_SoftBar_Update(0, false, "", false);
    Ui_SoftBar_Update(1, false, "", false);
    Ui_SoftBar_Update(2, true, LV_SYMBOL_OK" Done", false);

    LcdDma_Mutex_Give();

    timer = xthal_get_ccount();

    tasklabel_buf[0] = 0;

    drawtasks();
    draw();
}

void Ui_Debug_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

void Ui_Debug_Key(KeyEvent_t event) {
    if (event.Key == KEY_C && event.State == KEY_EVENT_PRESS) {
        KeyMgr_Consume(KEY_C);
        Ui_Screen = UISCREEN_ABOUT;
    }
    if (event.Key == KEY_A && event.State & KEY_EVENT_HOLD) {
        ESP_LOGI(TAG, "Enabling screenshot key");
        Ui_ScreenshotEnabled = true;
    }
    if (event.Key == KEY_B && event.State == KEY_EVENT_PRESS) {
        fast = !fast;
    }
}

void Ui_Debug_Tick() {
    uint32_t t = xthal_get_ccount();
    if (t - timer >= 240000000/(fast?30:15)) {
        draw();
        timer = xthal_get_ccount();
    }
    if (t - taskstimer >= 240000000/2) {
        drawtasks();
        taskstimer = xthal_get_ccount();
    }
}
