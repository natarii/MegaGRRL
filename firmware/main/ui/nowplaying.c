#include "freertos/FreeRTOS.h"
#include "statusbar.h"
#include "esp_log.h"
#include "lvgl.h"
#include "../lcddma.h"
#include "../ui.h"
#include "../key.h"
#include "../player.h"
#include "../taskmgr.h"
#include "../driver.h"
#include "../queue.h"
#include "../options.h"

#include "softbar.h"

static const char* TAG = "Ui_NowPlaying";

bool Ui_NowPlaying_DataAvail = false;
bool Ui_NowPlaying_NewTrack = false;
bool Ui_NowPlaying_FirstDraw = true;
bool Ui_NowPlaying_HadData = false;

lv_obj_t *container;
lv_style_t containerstyle;
lv_style_t labelstyle;
lv_style_t textstyle;
lv_style_t labelstyle_sm;
lv_style_t textstyle_sm;
lv_obj_t *label_title;
lv_obj_t *label_author;
lv_obj_t *label_game;
lv_obj_t *label_time;
lv_obj_t *label_loop;
lv_obj_t *label_playlist;
lv_obj_t *text_title;
lv_obj_t *text_author;
lv_obj_t *text_game;
lv_obj_t *text_time;
lv_obj_t *text_loop;
lv_obj_t *text_playlist;
lv_obj_t *bar_track;
lv_obj_t *bar_trackloop;
lv_obj_t *bar_scrub;
lv_style_t style_bar_track;
lv_style_t style_bar_trackloop;
lv_style_t style_bar_scrub;
lv_obj_t *bar_div;
lv_style_t divstyle;
lv_obj_t *dpad[4];
lv_style_t style_dpad;
lv_obj_t *dpadtext[4];
lv_style_t coverstyle;
lv_obj_t *cover;
lv_obj_t *label_opt_loops;
lv_obj_t *text_opt_loops;
lv_obj_t *label_opt_playmode;
lv_obj_t *text_opt_playmode;
lv_obj_t *text_opt_more;
lv_obj_t *text_opt_muting;
lv_obj_t *broken_vgm_time_warning;
uint8_t selectedopt = 0;
bool optionsopen = false;
lv_style_t textstyle_sm_sel;
char loopcountbuf[3] = {0,0,0};
const char *loading = "Nothing playing...";
const char *broken_vgm_time_warning_text = " Playback time unavailable  due to broken VGM file";

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

LV_FONT_DECLARE(dejavu16);

void Ui_NowPlaying_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

bool Ui_NowPlaying_Setup(lv_obj_t *uiscreen) {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    lv_style_copy(&containerstyle, &lv_style_plain);
    containerstyle.body.main_color = LV_COLOR_MAKE(0,0,0);
    containerstyle.body.grad_color = LV_COLOR_MAKE(0,0,0);

    lv_style_copy(&labelstyle, &lv_style_plain);
    labelstyle.text.color = LV_COLOR_MAKE(100, 100, 230); //127,127,127);
    labelstyle.text.font = &lv_font_dejavu_20;

    lv_style_copy(&textstyle, &labelstyle);
    textstyle.text.color = LV_COLOR_MAKE(220, 220, 220); //255,255,255);

    lv_style_copy(&labelstyle_sm, &labelstyle);
    labelstyle_sm.text.font = &dejavu16;

    lv_style_copy(&textstyle_sm, &textstyle);
    textstyle_sm.text.font = &dejavu16;

    lv_cont_set_style(container, &containerstyle);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    lv_cont_set_fit(container, false, false);

    label_title = lv_label_create(container, NULL);
    lv_label_set_style(label_title, &labelstyle);
    lv_obj_set_pos(label_title, 5, 5);
    lv_label_set_text(label_title, "Title");

    label_author = lv_label_create(container, NULL);
    lv_label_set_style(label_author, &labelstyle);
    lv_obj_set_pos(label_author, 5, 45);
    lv_label_set_text(label_author, "Authors");

    label_game = lv_label_create(container, NULL);
    lv_label_set_style(label_game, &labelstyle);
    lv_obj_set_pos(label_game, 5, 85);
    lv_label_set_text(label_game, "Game");

    text_title = lv_label_create(container, NULL);
    lv_label_set_style(text_title, &textstyle);
    lv_obj_set_pos(text_title, 5, 25);
    lv_label_set_static_text(text_title, loading);

    text_author = lv_label_create(container, NULL);
    lv_label_set_style(text_author, &textstyle);
    lv_obj_set_pos(text_author, 5, 65);
    lv_label_set_static_text(text_author, loading);

    text_game = lv_label_create(container, NULL);
    lv_label_set_style(text_game, &textstyle);
    lv_obj_set_pos(text_game, 5, 105);
    lv_label_set_static_text(text_game, loading);



    label_playlist = lv_label_create(container, NULL);
    lv_label_set_style(label_playlist, &labelstyle_sm);
    lv_obj_set_pos(label_playlist, 125, 145);
    lv_label_set_static_text(label_playlist, "Track No.");

    text_playlist = lv_label_create(container, NULL);
    lv_label_set_style(text_playlist, &textstyle_sm);
    lv_obj_set_pos(text_playlist, 125, 160);
    lv_label_set_text(text_playlist, "0 / 0");

    label_time = lv_label_create(container, NULL);
    lv_label_set_style(label_time, &labelstyle_sm);
    lv_obj_set_pos(label_time, 125, 175);
    lv_label_set_text(label_time, "Time");

    text_time = lv_label_create(container, NULL);
    lv_label_set_style(text_time, &textstyle_sm);
    lv_obj_set_pos(text_time, 125, 190);
    lv_label_set_text(text_time, "00:00 / 00:00");

    label_loop = lv_label_create(container, NULL);
    lv_label_set_style(label_loop, &labelstyle_sm);
    lv_obj_set_pos(label_loop, 125, 205);
    lv_label_set_text(label_loop, "Loop");

    text_loop = lv_label_create(container, NULL);
    lv_label_set_style(text_loop, &textstyle_sm);
    lv_obj_set_pos(text_loop, 125, 220);
    lv_label_set_text(text_loop, "1 / 1");




    lv_style_copy(&style_bar_track, &lv_style_plain);
    style_bar_track.body.main_color = LV_COLOR_MAKE(0,0,32);
    style_bar_track.body.grad_color = LV_COLOR_MAKE(0,0,200);
    lv_style_copy(&style_bar_trackloop, &lv_style_plain);
    style_bar_trackloop.body.main_color = LV_COLOR_MAKE(0,32,0);
    style_bar_trackloop.body.grad_color = LV_COLOR_MAKE(0,200,0);
    lv_style_copy(&style_bar_scrub, &lv_style_plain);
    style_bar_scrub.body.main_color = LV_COLOR_MAKE(255,255,255);
    style_bar_scrub.body.grad_color = LV_COLOR_MAKE(255,255,255);
    bar_track = lv_obj_create(container, NULL);
    lv_obj_set_style(bar_track, &style_bar_track);
    lv_obj_set_pos(bar_track, 10, 128);
    lv_obj_set_size(bar_track, 220, 10);
    bar_trackloop = lv_obj_create(container, NULL);
    lv_obj_set_style(bar_trackloop, &style_bar_trackloop);
    lv_obj_set_pos(bar_trackloop, 230, 128);
    lv_obj_set_size(bar_trackloop, 0, 10);
    bar_scrub = lv_obj_create(container, NULL);
    lv_obj_set_style(bar_scrub, &style_bar_scrub);
    lv_obj_set_pos(bar_scrub, 10, 128);
    lv_obj_set_size(bar_scrub, 2, 10);

    static lv_style_t style_time_warning;
    lv_style_copy(&style_time_warning, &lv_style_plain);
    style_time_warning.text.font = &lv_font_monospace_8;
    style_time_warning.text.color = LV_COLOR_MAKE(255,255,255);
    broken_vgm_time_warning = lv_label_create(container, NULL);
    lv_label_set_style(broken_vgm_time_warning, &style_time_warning);
    lv_obj_set_pos(broken_vgm_time_warning, 13, 129);
    lv_label_set_long_mode(broken_vgm_time_warning, LV_LABEL_LONG_ROLL);
    lv_obj_set_width(broken_vgm_time_warning, 217);
    lv_obj_set_hidden(broken_vgm_time_warning, true);



    lv_style_copy(&divstyle, &lv_style_plain);
    divstyle.body.main_color = divstyle.body.grad_color = LV_COLOR_MAKE(255,255,255);
    bar_div = lv_obj_create(container, NULL);
    lv_obj_set_style(bar_div, &divstyle);
    lv_obj_set_pos(bar_div, 120, 128+10);
    lv_obj_set_size(bar_div, 1, 250-(128+10));

    //options
    lv_style_copy(&coverstyle, &containerstyle);
    cover = lv_obj_create(container, NULL);
    lv_obj_set_style(cover, &coverstyle);
    lv_obj_set_pos(cover, 121, 128+10);
    lv_obj_set_size(cover, 120, 128+10);

    lv_style_copy(&textstyle_sm_sel, &textstyle_sm);
    textstyle_sm_sel.text.color = LV_COLOR_MAKE(255,255,0);

    label_opt_playmode = lv_label_create(container, NULL);
    lv_label_set_style(label_opt_playmode, &labelstyle_sm);
    lv_obj_set_pos(label_opt_playmode, 125, 145);
    lv_label_set_static_text(label_opt_playmode, "Play Mode");

    text_opt_playmode = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_playmode, &textstyle_sm);
    lv_obj_set_pos(text_opt_playmode, 125, 160);
    lv_label_set_text(text_opt_playmode, "Repeat All");

    label_opt_loops = lv_label_create(container, NULL);
    lv_label_set_style(label_opt_loops, &labelstyle_sm);
    lv_obj_set_pos(label_opt_loops, 125, 175);
    lv_label_set_text(label_opt_loops, "Loop Count");

    text_opt_loops = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_loops, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_loops, 125, 190);
    lv_label_set_text(text_opt_loops, "2");

    text_opt_muting = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_muting, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_muting, 125, 220);
    lv_label_set_text(text_opt_muting, "Ch. Muting >>");

    text_opt_more = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_more, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_more, 125, 235);
    lv_label_set_text(text_opt_more, "More Options >>");

    lv_obj_set_hidden(cover, true);
    lv_obj_set_hidden(label_opt_playmode, true);
    lv_obj_set_hidden(text_opt_playmode, true);
    lv_obj_set_hidden(label_opt_loops, true);
    lv_obj_set_hidden(text_opt_loops, true);
    lv_obj_set_hidden(text_opt_more, true);
    lv_obj_set_hidden(text_opt_muting, true);

    optionsopen = false;






    lv_style_copy(&style_dpad, &lv_style_plain);
    style_dpad.body.main_color = LV_COLOR_MAKE(127,127,127);
    style_dpad.body.grad_color = LV_COLOR_MAKE(127,127,127);
    style_dpad.body.radius = LV_RADIUS_CIRCLE;
    style_dpad.text.color = LV_COLOR_MAKE(0,0,0);
    style_dpad.text.font = &lv_font_dejavu_20;
    for (uint8_t i=0;i<4;i++) {
        dpad[i] = lv_obj_create(container, NULL);
        lv_obj_set_style(dpad[i], &style_dpad);
        lv_obj_set_size(dpad[i], 30, 30);
        dpadtext[i] = lv_label_create(dpad[i], NULL);
        lv_obj_set_style(dpadtext[i], &style_dpad);
        lv_obj_set_size(dpadtext[i], 30, 30);
    }
    lv_obj_set_pos(dpad[0], 45, 138+4);
    lv_obj_set_pos(dpad[1], 45, 250-4-30);
    lv_obj_set_pos(dpad[2], 8, 179);
    lv_obj_set_pos(dpad[3], 120-8-30, 179);
    lv_label_set_text(dpadtext[0], SYMBOL_PLAY);
    lv_obj_set_pos(dpadtext[0], 9, 5);
    lv_label_set_text(dpadtext[1], SYMBOL_STOP);
    lv_obj_set_pos(dpadtext[1], 7, 5);
    lv_label_set_text(dpadtext[2], SYMBOL_LEFT);
    lv_obj_set_pos(dpadtext[2], 7, 5);
    lv_label_set_text(dpadtext[3], SYMBOL_RIGHT);
    lv_obj_set_pos(dpadtext[3], 11, 5);
    




    Ui_SoftBar_Update(2, true, "Options", false);
    Ui_SoftBar_Update(1, true, "Browser", false);
    Ui_SoftBar_Update(0, true, SYMBOL_HOME "Home", false);
    LcdDma_Mutex_Give();


    Ui_NowPlaying_FirstDraw = true;
    xEventGroupClearBits(Player_Status, PLAYER_STATUS_RAN_OUT);

    return true;
}

void drawopts() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_set_hidden(cover, !optionsopen);
    lv_obj_set_hidden(label_opt_playmode, !optionsopen);
    lv_obj_set_hidden(text_opt_playmode, !optionsopen);
    lv_obj_set_hidden(label_opt_loops, !optionsopen);
    lv_obj_set_hidden(text_opt_loops, !optionsopen);
    lv_obj_set_hidden(text_opt_more, !optionsopen);
    lv_obj_set_hidden(text_opt_muting, !optionsopen);
    lv_obj_set_style(text_opt_playmode, selectedopt==0?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_loops, selectedopt==1?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_muting, selectedopt==2?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_more, selectedopt==3?&textstyle_sm_sel:&textstyle_sm);
    switch (Player_RepeatMode) {
        case REPEAT_NONE:
            lv_label_set_text(text_opt_playmode, "Repeat None");
            break;
        case REPEAT_ONE:
            lv_label_set_text(text_opt_playmode, "Repeat One");
            break;
        case REPEAT_ALL:
            lv_label_set_text(text_opt_playmode, "Repeat All");
            break;
        default:
            break;
    }
    if (Player_SetLoopCount != 255) {
        sprintf(loopcountbuf, "%d", Player_SetLoopCount);
        lv_label_set_text(text_opt_loops, loopcountbuf);
    } else {
        lv_label_set_text(text_opt_loops, "inf.");
    }
    LcdDma_Mutex_Give();
}

void Ui_NowPlaying_Key(KeyEvent_t event) {
    if (event.State == KEY_EVENT_PRESS || (optionsopen && event.State & KEY_EVENT_PRESS)) {
        switch (event.Key) {
            case KEY_LEFT:
                if (!optionsopen) {
                    if (QueueLength) xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_PREV, eSetValueWithoutOverwrite);
                } else {
                    if (selectedopt == 0) {
                        if (Player_RepeatMode > 0) {
                            Player_RepeatMode--;
                            OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 1) {
                        if (Player_SetLoopCount > 1) {
                            if (Player_SetLoopCount != 255) {
                                Player_LoopCount = --Player_SetLoopCount;
                                OptionsMgr_Touch();
                            } else {
                                Player_LoopCount = Player_SetLoopCount = 10;
                                OptionsMgr_Touch();
                            }
                        }
                        drawopts();
                    }
                }
                break;
            case KEY_RIGHT:
                if (!optionsopen) {
                    if (QueueLength) xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_NEXT, eSetValueWithoutOverwrite);
                } else {
                    if (selectedopt == 0) {
                        if (Player_RepeatMode < REPEAT_COUNT-1) {
                            Player_RepeatMode++;
                            OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 1) {
                        if (Player_SetLoopCount == 10) {
                            Player_LoopCount = Player_SetLoopCount = 255;
                                OptionsMgr_Touch();
                        } else if (Player_SetLoopCount < 10) {
                            Player_LoopCount = ++Player_SetLoopCount;
                                OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 2) {
                        Ui_Screen = UISCREEN_MUTING;
                    } else if (selectedopt == 3) {
                        Ui_Screen = UISCREEN_OPTIONS_CATS;
                    }
                }
                break;
            case KEY_B:
                KeyMgr_Consume(KEY_B);
                Ui_Screen = UISCREEN_FILEBROWSER;
                break;
            case KEY_UP:
                if (!optionsopen) {
                    if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) {
                        if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_PAUSED) { //already paused
                            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_PLAY, eSetValueWithoutOverwrite);
                            //todo check it happened
                        } else {
                            xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_PAUSE, eSetValueWithoutOverwrite);
                            //todo check it happened
                        }
                    }
                } else {
                    if (selectedopt > 0) selectedopt--;
                    drawopts();
                }
                break;
            case KEY_DOWN:
                if (!optionsopen) {
                    ESP_LOGI(TAG, "request stop");
                    xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
                    ESP_LOGI(TAG, "wait stop");
                    xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
                    ESP_LOGI(TAG, "reset vars");
                    QueueLength = 0;
                    QueuePosition = 0;
                    ESP_LOGI(TAG, "clear ui");
                    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
                    /*lv_label_set_static_text(text_title, loading);
                    lv_label_set_static_text(text_author, loading);
                    lv_label_set_static_text(text_game, loading);
                    lv_label_set_text(text_playlist, "0 / 0");
                    lv_label_set_text(text_time, "00:00 / 00:00");
                    lv_label_set_text(text_loop, "1 / 1");*/
                    Ui_NowPlaying_FirstDraw = Ui_NowPlaying_NewTrack = false;
                    LcdDma_Mutex_Give();
                    ESP_LOGI(TAG, "yeet");
                    Ui_Screen = UISCREEN_MAINMENU;
                } else {
                    if (selectedopt < 3) {
                        selectedopt++;
                        drawopts();
                    }
                }
                break;
            case KEY_C:
                optionsopen = !optionsopen;
                if (optionsopen) Ui_SoftBar_Update(2, true, "Done", true);
                else Ui_SoftBar_Update(2, true, "Options", true);
                drawopts();
                break;
            case KEY_A:
                KeyMgr_Consume(KEY_A);
                Ui_Screen = UISCREEN_MAINMENU;
                break;
            default:
                break;
        }
    }
}

char npnone[] = "(none)";
uint32_t nptimer = 0;
char timebuf[14];
char loopbuf[11]; //size?
char plsbuf[14]; //size?
void Ui_NowPlaying_Tick() {
    if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RAN_OUT) {
        ESP_LOGW(TAG, "player run out detected!!");
        ESP_LOGW(TAG, "request stop");
        xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
        ESP_LOGW(TAG, "wait stop");
        xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
        QueueLength = 0;
        QueuePosition = 0;
        Ui_NowPlaying_FirstDraw = Ui_NowPlaying_NewTrack = false;
        ESP_LOGW(TAG, "return to mainmenu");
        Ui_Screen = UISCREEN_MAINMENU;
        return;
    }
    uint32_t total = Player_Info.TotalSamples;
    uint32_t pos = Driver_Sample;
    if (Driver_MitigateVgmTrim && Driver_FirstWait) pos = 0;
    uint32_t loopsamples = Player_Info.LoopSamples;
    uint32_t looppoint = total - loopsamples;
    bool dodraw = false;
    if (Ui_NowPlaying_FirstDraw || Ui_NowPlaying_NewTrack) {
        if (Ui_NowPlaying_DataAvail) {
            dodraw = true;
            Ui_NowPlaying_FirstDraw = Ui_NowPlaying_NewTrack = false;
        }
    }
    //if (!(xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_RUNNING)) dodraw = false;   //todo fixme
    if (dodraw) {
        ESP_LOGD(TAG, "total %d pos %d loopS %d loopP %d", total, pos, loopsamples, looppoint);
        
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        lv_obj_set_pos(bar_scrub, 10, 128);
        if (strlen(Player_Gd3_Title) == 0) {
            uint16_t nameoff = 0;
            for (uint16_t i=0;i<strlen(QueuePlayingFilename);i++) {
                if (QueuePlayingFilename[i] == '/') {
                    nameoff = i+1;
                }
            }
            strcpy(Player_Gd3_Title, &QueuePlayingFilename[nameoff]); //vile. disgusting. awful.
        }
        lv_label_set_static_text(text_title, &Player_Gd3_Title[0]);
        lv_label_set_long_mode(text_title, LV_LABEL_LONG_ROLL);
        lv_obj_set_width(text_title, 230);
        
        if (strlen(Player_Gd3_Author)) {
            lv_label_set_style(text_author, &textstyle);
            lv_label_set_static_text(text_author, &Player_Gd3_Author[0]);
        } else {
            lv_label_set_style(text_author, &labelstyle);
            lv_label_set_static_text(text_author, npnone);
        }
        lv_label_set_long_mode(text_author, LV_LABEL_LONG_ROLL);
        lv_obj_set_width(text_author, 230);

        if (strlen(Player_Gd3_Game)) {
            lv_label_set_style(text_game, &textstyle);
            lv_label_set_static_text(text_game, &Player_Gd3_Game[0]);
        } else {
            lv_label_set_style(text_game, &labelstyle);
            lv_label_set_static_text(text_game, npnone);
        }
        lv_label_set_long_mode(text_game, LV_LABEL_LONG_ROLL);
        lv_obj_set_width(text_game, 230);

        uint32_t p = QueuePosition+1;
        sprintf(plsbuf, "%d / %d", (p<QueueLength)?p:QueueLength, QueueLength);
        lv_label_set_static_text(text_playlist, plsbuf);

        if (total) {
            uint32_t tracklength = map(looppoint, 0, total, 0, 220);
            uint32_t looplength = map(loopsamples, 0, total, 0, 220);
            lv_obj_set_size(bar_track, tracklength, 10);
            lv_obj_set_size(bar_trackloop, looplength, 10);
            lv_obj_set_pos(bar_trackloop, 10+tracklength, 128);
        } else {
            lv_obj_set_size(bar_track, 220, 10);
            lv_obj_set_size(bar_trackloop, 0, 10);
            lv_label_set_static_text(broken_vgm_time_warning, broken_vgm_time_warning_text); //do this now, rather than once at init, to reset scroll
        }
        lv_obj_set_hidden(broken_vgm_time_warning, total>0);

        lv_label_set_text(dpadtext[0], SYMBOL_PAUSE);
        lv_obj_set_pos(dpadtext[0], 7, 5);

        LcdDma_Mutex_Give();
    }

    if (esp_timer_get_time() - nptimer >= 250000) {
        bool inloop = false;
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        //these are getting drawn all the time for no reason, we should watch for states to actually change...
        if (xEventGroupGetBits(Player_Status) & (PLAYER_STATUS_PAUSED|PLAYER_STATUS_NOT_RUNNING)) {
            lv_label_set_text(dpadtext[0], SYMBOL_PLAY);
            lv_obj_set_pos(dpadtext[0], 9, 5);
        } else {
            lv_label_set_text(dpadtext[0], SYMBOL_PAUSE);
            lv_obj_set_pos(dpadtext[0], 7, 5);
        }

        if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RUNNING) {
            if (total) {
                if (loopsamples && pos > looppoint) {
                    inloop = true;
                    pos = looppoint + ((pos-looppoint)%loopsamples);
                }
                lv_obj_set_pos(bar_scrub, 10+map(pos, 0, total, 0, 220), 128);
            }
            uint32_t totalwithloops = (looppoint + (loopsamples * Player_LoopCount))/44100;
            uint8_t totalmins = totalwithloops/60;
            uint8_t totalsecs = totalwithloops%60;
            uint8_t elapsedmins = Driver_Sample/44100/60;
            uint8_t elapsedsecs = (Driver_Sample/44100)%60;
            if (Driver_MitigateVgmTrim && Driver_FirstWait) {
                elapsedmins = 0;
                elapsedsecs = 0;
            }
            if (Player_LoopCount < 255 || (Player_LoopCount < 255 && !(Player_Info.LoopOffset >= 0 && loopsamples && Driver_Sample > looppoint))) {
                sprintf(timebuf, "%02d:%02d / %02d:%02d", elapsedmins, elapsedsecs, totalmins, totalsecs);
            } else {
                sprintf(timebuf, "%02d:%02d / inf.", elapsedmins, elapsedsecs);
            }
            lv_label_set_static_text(text_time, timebuf);

            uint8_t curloop = 1;
            if (Player_Info.LoopOffset >= 0 && loopsamples/* && Driver_Sample > looppoint*/) {
                if (Driver_Sample <= looppoint) {
                    curloop = 1;
                } else {
                    curloop = ((Driver_Sample - looppoint) / loopsamples) + 1;
                }
                if (Player_LoopCount < 255) {
                    sprintf(loopbuf, "%d / %d", curloop, (Player_Info.LoopOffset >= 0 && loopsamples)?Player_LoopCount:1);
                } else {
                    sprintf(loopbuf, "%d / inf.", curloop);
                }
            } else {
                sprintf(loopbuf, "1 / 1");
            }
            lv_label_set_static_text(text_loop, loopbuf);
        }
        nptimer = esp_timer_get_time();
        LcdDma_Mutex_Give();
    }
}