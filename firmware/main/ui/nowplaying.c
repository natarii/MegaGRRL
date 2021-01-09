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
#include "../loader.h"
#include "../pitch.h"

#include "softbar.h"

static const char* TAG = "Ui_NowPlaying";



volatile bool Ui_NowPlaying_DataAvail = false;
volatile bool Ui_NowPlaying_DriverRunning = false;

static uint8_t lastbarx = 0xff;
static EventBits_t laststatus = 0;
static UiScreen_t lastscreen = UISCREEN_FILEBROWSER;

static IRAM_ATTR lv_obj_t *container;
static lv_style_t labelstyle;
static lv_style_t textstyle;
static lv_style_t labelstyle_sm;
static lv_style_t textstyle_sm;
static IRAM_ATTR lv_obj_t *label_title;
static IRAM_ATTR lv_obj_t *label_author;
static IRAM_ATTR lv_obj_t *label_game;
static IRAM_ATTR lv_obj_t *label_time;
static IRAM_ATTR lv_obj_t *label_loop;
static IRAM_ATTR lv_obj_t *label_playlist;
static IRAM_ATTR lv_obj_t *text_title;
static IRAM_ATTR lv_obj_t *text_author;
static IRAM_ATTR lv_obj_t *text_game;
static IRAM_ATTR lv_obj_t *text_time;
static IRAM_ATTR lv_obj_t *text_loop;
static IRAM_ATTR lv_obj_t *text_playlist;
static IRAM_ATTR lv_obj_t *bar_track;
static IRAM_ATTR lv_obj_t *bar_trackloop;
static IRAM_ATTR lv_obj_t *bar_scrub;
static lv_style_t style_bar_track;
static lv_style_t style_bar_trackloop;
static lv_style_t style_bar_scrub;
static IRAM_ATTR lv_obj_t *bar_div;
static lv_style_t divstyle;
static IRAM_ATTR lv_obj_t *dpad[4];
static lv_style_t style_dpad;
static IRAM_ATTR lv_obj_t *dpadtext[4];
//this is really a mess and should be redone, but for now...
static IRAM_ATTR lv_obj_t *text_opt_shuffle;
static IRAM_ATTR lv_obj_t *text_opt_loops;
static IRAM_ATTR lv_obj_t *text_opt_pitch;
static IRAM_ATTR lv_obj_t *label_options;
static IRAM_ATTR lv_obj_t *text_opt_playmode;
static IRAM_ATTR lv_obj_t *text_opt_more;
static IRAM_ATTR lv_obj_t *text_opt_muting;
static IRAM_ATTR lv_obj_t *broken_vgm_time_warning;
static uint8_t selectedopt = 0;
static bool optionsopen = false;
static lv_style_t textstyle_sm_sel;
static char loopcountbuf[4] = {0};
static char pitchbuf[10] = "";
static const char *loading = "Nothing playing...";
static const char *broken_vgm_time_warning_text = " Playback time unavailable  due to broken VGM file";
static IRAM_ATTR uint32_t lastelapsedsecs = 0xffffffff;
static IRAM_ATTR uint32_t lastelapsedmins = 0xffffffff;

static char loopbuf[11]; //size?

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void newtrack();
static void do_tick();

void Ui_NowPlaying_Destroy() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    lv_obj_del(container);
    LcdDma_Mutex_Give();
}

bool Ui_NowPlaying_Setup(lv_obj_t *uiscreen) {
    if (Ui_Screen_Last != UISCREEN_NOWPLAYING) lastscreen = Ui_Screen_Last;

    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));

    container = lv_cont_create(uiscreen, NULL);

    lv_style_copy(&labelstyle, &lv_style_plain);
    labelstyle.text.color = LV_COLOR_MAKE(100, 100, 230); //127,127,127);
    labelstyle.text.font = &lv_font_dejavu_18;

    lv_style_copy(&textstyle, &labelstyle);
    textstyle.text.color = LV_COLOR_MAKE(220, 220, 220); //255,255,255);

    lv_style_copy(&labelstyle_sm, &labelstyle);
    labelstyle_sm.text.font = &lv_font_dejavu_14_2bpp;
    labelstyle_sm.text.line_space = -2;

    lv_style_copy(&textstyle_sm, &textstyle);
    textstyle_sm.text.font = &lv_font_dejavu_14_2bpp;

    lv_cont_set_style(container, LV_CONT_STYLE_MAIN, &lv_style_transp);
    lv_obj_set_height(container, 250);
    lv_obj_set_width(container, 240);
    lv_obj_set_pos(container, 0, 34+1);
    //lv_cont_set_fit(container, false, false);

    label_title = lv_label_create(container, NULL);
    lv_label_set_style(label_title, LV_LABEL_STYLE_MAIN, &labelstyle);
    lv_obj_set_pos(label_title, 5, 5);
    lv_label_set_static_text(label_title, "Title");

    label_author = lv_label_create(container, NULL);
    lv_label_set_style(label_author, LV_LABEL_STYLE_MAIN, &labelstyle);
    lv_obj_set_pos(label_author, 5, 45);
    lv_label_set_static_text(label_author, "Authors");

    label_game = lv_label_create(container, NULL);
    lv_label_set_style(label_game, LV_LABEL_STYLE_MAIN, &labelstyle);
    lv_obj_set_pos(label_game, 5, 85);
    lv_label_set_static_text(label_game, "Game");

    text_title = lv_label_create(container, NULL);
    lv_label_set_style(text_title, LV_LABEL_STYLE_MAIN, &textstyle);
    lv_obj_set_pos(text_title, 5, 25);
    lv_label_set_anim_speed(text_title, 50);
    lv_label_set_static_text(text_title, loading);

    text_author = lv_label_create(container, NULL);
    lv_label_set_style(text_author, LV_LABEL_STYLE_MAIN, &textstyle);
    lv_obj_set_pos(text_author, 5, 65);
    lv_label_set_anim_speed(text_author, 50);
    lv_label_set_static_text(text_author, loading);

    text_game = lv_label_create(container, NULL);
    lv_label_set_style(text_game, LV_LABEL_STYLE_MAIN, &textstyle);
    lv_obj_set_pos(text_game, 5, 105);
    lv_label_set_anim_speed(text_game, 50);
    lv_label_set_static_text(text_game, loading);



    label_playlist = lv_label_create(container, NULL);
    lv_label_set_style(label_playlist, LV_LABEL_STYLE_MAIN, &labelstyle_sm);
    lv_obj_set_pos(label_playlist, 125, 145);
    lv_label_set_static_text(label_playlist, "Track No.");

    text_playlist = lv_label_create(container, NULL);
    lv_label_set_style(text_playlist, LV_LABEL_STYLE_MAIN, &textstyle_sm);
    lv_obj_set_pos(text_playlist, 125, 160);
    lv_label_set_static_text(text_playlist, "0 / 0");

    label_time = lv_label_create(container, NULL);
    lv_label_set_style(label_time, LV_LABEL_STYLE_MAIN, &labelstyle_sm);
    lv_obj_set_pos(label_time, 125, 175);
    lv_label_set_static_text(label_time, "Time");

    text_time = lv_label_create(container, NULL);
    lv_label_set_style(text_time, LV_LABEL_STYLE_MAIN, &textstyle_sm);
    lv_obj_set_pos(text_time, 125, 190);
    lv_label_set_static_text(text_time, "00:00 / 00:00");

    label_loop = lv_label_create(container, NULL);
    lv_label_set_style(label_loop, LV_LABEL_STYLE_MAIN, &labelstyle_sm);
    lv_obj_set_pos(label_loop, 125, 205);
    lv_label_set_static_text(label_loop, "Loop");

    text_loop = lv_label_create(container, NULL);
    lv_label_set_style(text_loop, LV_LABEL_STYLE_MAIN, &textstyle_sm);
    lv_obj_set_pos(text_loop, 125, 220); 
    if (Player_LoopCount < 255) {
        sprintf(loopbuf, "1 / %d", Player_LoopCount);
    } else {
        strcpy(loopbuf, "1 / inf.");
    }
    lv_label_set_static_text(text_loop, loopbuf);





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
    //style_time_warning.text.font = &lv_font_monospace_8;
    style_time_warning.text.color = LV_COLOR_MAKE(255,255,255);
    broken_vgm_time_warning = lv_label_create(container, NULL);
    lv_label_set_style(broken_vgm_time_warning, LV_LABEL_STYLE_MAIN, &style_time_warning);
    lv_obj_set_pos(broken_vgm_time_warning, 13, 129);
    lv_label_set_long_mode(broken_vgm_time_warning, Ui_GetScrollType());
    lv_obj_set_width(broken_vgm_time_warning, 217);
    lv_obj_set_hidden(broken_vgm_time_warning, true);



    lv_style_copy(&divstyle, &lv_style_plain);
    divstyle.body.main_color = divstyle.body.grad_color = LV_COLOR_MAKE(255,255,255);
    bar_div = lv_obj_create(container, NULL);
    lv_obj_set_style(bar_div, &divstyle);
    lv_obj_set_pos(bar_div, 120, 128+10);
    lv_obj_set_size(bar_div, 1, 250-(128+10));

    //options
    lv_style_copy(&textstyle_sm_sel, &textstyle_sm);
    textstyle_sm_sel.text.color = LV_COLOR_MAKE(255,255,0);

    label_options = lv_label_create(container, NULL);
    lv_label_set_style(label_options, LV_LABEL_STYLE_MAIN, &labelstyle_sm);
    lv_obj_set_pos(label_options, 3, 139);
    lv_obj_set_size(label_options, 120, 100);
    lv_label_set_align(label_options, LV_LABEL_ALIGN_RIGHT);
    lv_label_set_static_text(label_options, "Play Mode\nLoop Count\nShuffle\nPitch\n\nChannel Muting\nMore Settings");

    int16_t yd = 15;
    int16_t y = 139;

    text_opt_playmode = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_playmode, LV_LABEL_STYLE_MAIN, &textstyle_sm);
    lv_obj_set_pos(text_opt_playmode, 125, y);
    y += yd;
    lv_label_set_static_text(text_opt_playmode, "Repeat All");

    text_opt_loops = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_loops, LV_LABEL_STYLE_MAIN, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_loops, 125, y);
    y += yd;
    lv_label_set_static_text(text_opt_loops, "2");

    text_opt_shuffle = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_shuffle, LV_LABEL_STYLE_MAIN, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_shuffle, 125, y);
    y += yd;
    lv_label_set_static_text(text_opt_shuffle, "Off");

    text_opt_pitch = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_pitch, LV_LABEL_STYLE_MAIN, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_pitch, 125, y);
    y += yd;
    lv_label_set_static_text(text_opt_pitch, "0%");

    y += (yd*1); //no of blank lines

    text_opt_muting = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_muting, LV_LABEL_STYLE_MAIN, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_muting, 125, y);
    y += yd;
    lv_label_set_static_text(text_opt_muting, "Press C "LV_SYMBOL_RIGHT);

    text_opt_more = lv_label_create(container, NULL);
    lv_label_set_style(text_opt_more, LV_LABEL_STYLE_MAIN, &textstyle_sm_sel);
    lv_obj_set_pos(text_opt_more, 125, y);
    lv_label_set_static_text(text_opt_more, "Press C "LV_SYMBOL_RIGHT);

    lv_obj_set_hidden(label_options, true);
    lv_obj_set_hidden(text_opt_playmode, true);
    lv_obj_set_hidden(text_opt_loops, true);
    lv_obj_set_hidden(text_opt_shuffle, true);
    lv_obj_set_hidden(text_opt_pitch, true);
    lv_obj_set_hidden(text_opt_more, true);
    lv_obj_set_hidden(text_opt_muting, true);

    optionsopen = false;






    lv_style_copy(&style_dpad, &lv_style_plain);
    style_dpad.body.main_color = LV_COLOR_MAKE(127,127,127);
    style_dpad.body.grad_color = LV_COLOR_MAKE(127,127,127);
    style_dpad.body.radius = LV_RADIUS_CIRCLE;
    style_dpad.text.color = LV_COLOR_MAKE(0,0,0);
    style_dpad.text.font = &lv_font_dejavu_18;
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
    lv_label_set_static_text(dpadtext[0], LV_SYMBOL_PLAY);
    lv_obj_set_pos(dpadtext[0], 9, 5);
    lv_label_set_static_text(dpadtext[1], LV_SYMBOL_STOP);
    lv_obj_set_pos(dpadtext[1], 7, 5);
    lv_label_set_static_text(dpadtext[2], LV_SYMBOL_LEFT);
    lv_obj_set_pos(dpadtext[2], 7, 5);
    lv_label_set_static_text(dpadtext[3], LV_SYMBOL_RIGHT);
    lv_obj_set_pos(dpadtext[3], 11, 5);
    




    Ui_SoftBar_Update(2, true, "Settings", false);
    Ui_SoftBar_Update(1, true, "Browser", false);
    Ui_SoftBar_Update(0, true, LV_SYMBOL_HOME "Home", false);

    xEventGroupClearBits(Player_Status, PLAYER_STATUS_RAN_OUT);

    lastbarx = 0xff;
    laststatus = 0;
    lastelapsedsecs = 0xffffffff;
    lastelapsedsecs = 0xffffffff;
    if (Ui_NowPlaying_DriverRunning) {
        newtrack();
        do_tick();
        Ui_NowPlaying_DataAvail = false;
    }

    LcdDma_Mutex_Give();

    return true;
}

static char npnone[] = "(none)";
static IRAM_ATTR uint32_t nptimer = 0;
static char timebuf[16];
static char plsbuf[14]; //size?
static uint8_t loopcount;
static IRAM_ATTR uint32_t loopsamples;
static IRAM_ATTR uint32_t totalsamples;
static IRAM_ATTR uint32_t looppoint;
static IRAM_ATTR uint32_t totalwithloops;
static IRAM_ATTR uint32_t totalmins;
static IRAM_ATTR uint32_t totalsecs;
static IRAM_ATTR uint32_t elapsedmins;
static IRAM_ATTR uint32_t elapsedsecs;

static void calc_length() {
    totalwithloops = (looppoint + (loopsamples * Player_LoopCount)) / 44100;
    if (Driver_FadeEnabled && loopsamples) totalwithloops += Driver_FadeLength;
    totalmins = totalwithloops/60;
    totalsecs = totalwithloops%60;
    loopcount = (Player_Info.LoopOffset && loopsamples)?Player_LoopCount:1;
}

static void newtrack() { //gd3, pls position, loop count/samples
    //title
    if (strlen(Player_Gd3_Title) == 0) { //no title in gd3
        uint16_t nameoff = 0;
        for (uint16_t i=0;i<strlen(QueuePlayingFilename);i++) {
            if (QueuePlayingFilename[i] == '/') {
                nameoff = i+1;
            }
        }
        lv_label_set_text(text_title, &QueuePlayingFilename[nameoff]); //not using this as static, because we get a flash of the next track name when seeking from non-gd3 to with-gd3
    } else {
        lv_label_set_static_text(text_title, Player_Gd3_Title);
    }
    lv_label_set_long_mode(text_title, Ui_GetScrollType());
    lv_obj_set_width(text_title, 230);

    //author
    if (strlen(Player_Gd3_Author)) {
        lv_label_set_style(text_author, LV_LABEL_STYLE_MAIN, &textstyle);
        lv_label_set_static_text(text_author, &Player_Gd3_Author[0]);
    } else {
        lv_label_set_style(text_author, LV_LABEL_STYLE_MAIN, &labelstyle);
        lv_label_set_static_text(text_author, npnone);
    }
    lv_label_set_long_mode(text_author, Ui_GetScrollType());
    lv_obj_set_width(text_author, 230);

    //game
    if (strlen(Player_Gd3_Game)) {
        lv_label_set_style(text_game, LV_LABEL_STYLE_MAIN, &textstyle);
        lv_label_set_static_text(text_game, &Player_Gd3_Game[0]);
    } else {
        lv_label_set_style(text_game, LV_LABEL_STYLE_MAIN, &labelstyle);
        lv_label_set_static_text(text_game, npnone);
    }
    lv_label_set_long_mode(text_game, Ui_GetScrollType());
    lv_obj_set_width(text_game, 230);

    //playlist position
    sprintf(plsbuf, "%d / %d", QueuePosition+1, QueueLength);
    lv_label_set_static_text(text_playlist, plsbuf);

    //loop stuff
    totalsamples = Player_Info.TotalSamples;
    ESP_LOGI(TAG, "total samples %d", totalsamples);
    if (!Loader_IgnoreZeroSampleLoops && Player_Info.LoopSamples == 0 && Player_Info.LoopOffset) {
        ESP_LOGW(TAG, "broken looping vgm without IgnoreZeroSampleLoops");
        looppoint = 0;
        loopsamples = totalsamples;
    } else {
        ESP_LOGI(TAG, "vgm doesn't have broken loop");
        loopsamples = Player_Info.LoopSamples;
        looppoint = totalsamples - loopsamples;
    }
    ESP_LOGI(TAG, "loop point at %d, loop length %d samples", looppoint, loopsamples);

    //reset stuff
    lv_obj_set_pos(bar_scrub, 10, 128);

    if (Ui_NowPlaying_DriverRunning) {
        elapsedmins = Driver_Sample/44100/60;
        elapsedsecs = (Driver_Sample/44100)%60;
    } else {
        elapsedmins = elapsedsecs = 0;
    }
    if (Player_Info.LoopOffset && loopsamples && Player_LoopCount == 255) {
        sprintf(timebuf, "%02d:%02d / inf.", elapsedmins, elapsedsecs);
    } else {
        calc_length();
        sprintf(timebuf, "%02d:%02d / %02d:%02d", elapsedmins, elapsedsecs, totalmins, totalsecs);
    }
    lv_label_set_static_text(text_time, timebuf);

    //scrub
    if (totalsamples) {
        uint32_t tracklength = map(looppoint/100, 0, totalsamples/100, 0, 220);
        uint32_t looplength = map(loopsamples/100, 0, totalsamples/100, 0, 220);
        if (loopsamples && looplength == 0) looplength = 1; //make sure very short loops are still visible
        lv_obj_set_size(bar_track, tracklength, 10);
        lv_obj_set_size(bar_trackloop, looplength, 10);
        lv_obj_set_pos(bar_trackloop, 10+tracklength, 128);
    } else {
        lv_obj_set_size(bar_track, 220, 10);
        lv_obj_set_size(bar_trackloop, 0, 10);
        lv_label_set_static_text(broken_vgm_time_warning, broken_vgm_time_warning_text); //do this now, rather than once at init, to reset scroll
    }
    lv_obj_set_hidden(broken_vgm_time_warning, totalsamples>0);

    //initial dpad status - especially important if going mainmenu->nowplaying
    if (xEventGroupGetBits(Player_Status) & (PLAYER_STATUS_PAUSED|PLAYER_STATUS_NOT_RUNNING)) {
        lv_label_set_static_text(dpadtext[0], LV_SYMBOL_PLAY);
        lv_obj_set_pos(dpadtext[0], 9, 5);
    } else {
        lv_label_set_static_text(dpadtext[0], LV_SYMBOL_PAUSE);
        lv_obj_set_pos(dpadtext[0], 7, 5);
    }

    //loop
    if (Player_Info.LoopOffset && loopsamples) {
        if (Player_LoopCount < 255) {
            sprintf(loopbuf, "1 / %d", loopcount);
        } else {
            strcpy(loopbuf, "1 / inf.");
        }
    } else {
        strcpy(loopbuf, "1 / 1");
    }
    lv_label_set_static_text(text_loop, loopbuf);
}

static void do_tick() {
    //recalc these every tick now, since loopcount can change under our feet
    calc_length();

    if (Driver_FirstWait) {
        elapsedmins = 0;
        elapsedsecs = 0;
    } else {
        elapsedmins = Driver_Sample/44100/60;
        elapsedsecs = (Driver_Sample/44100)%60;
    }
    if (lastelapsedmins != elapsedmins || lastelapsedsecs != elapsedsecs) {
        if (Player_Info.LoopOffset && Player_Info.LoopSamples && Player_LoopCount == 255) {
            sprintf(timebuf, "%02d:%02d / inf.", elapsedmins, elapsedsecs);
        } else {
            sprintf(timebuf, "%02d:%02d / %02d:%02d", elapsedmins, elapsedsecs, totalmins, totalsecs);
        }
    }
    uint32_t pos = Driver_Sample;
    if (loopsamples && pos > looppoint) {
        pos = looppoint + ((pos-looppoint)%loopsamples);
    }
    uint8_t barx;
    if (totalsamples && !Driver_FirstWait) {
        barx = 10+map(pos/100, 0, totalsamples/100, 0, 220-2);
    } else {
        barx = 10;
    }
    if (xEventGroupGetBits(Driver_CommandEvents) & DRIVER_EVENT_FINISHED) {
        barx = 10+(220-2);
    }
    EventBits_t bits = xEventGroupGetBits(Player_Status);
    if (bits != laststatus) {
        if (bits & (PLAYER_STATUS_PAUSED|PLAYER_STATUS_NOT_RUNNING)) {
            lv_label_set_static_text(dpadtext[0], LV_SYMBOL_PLAY);
            lv_obj_set_pos(dpadtext[0], 9, 5);
        } else {
            lv_label_set_static_text(dpadtext[0], LV_SYMBOL_PAUSE);
            lv_obj_set_pos(dpadtext[0], 7, 5);
        }
    }
    if (barx != lastbarx) lv_obj_set_pos(bar_scrub, barx, 128);
    if (lastelapsedmins != elapsedmins || lastelapsedsecs != elapsedsecs) { //time keeps on slippin' into the future
        lv_label_set_static_text(text_time, timebuf);
        if (Player_Info.LoopOffset && loopsamples) {
            uint8_t curloop;
            if (Driver_Sample <= looppoint) {
                curloop = 1;
            } else {
                curloop = ((Driver_Sample - looppoint) / loopsamples) + 1;
            }
            if (Player_LoopCount < 255) {
                sprintf(loopbuf, "%d / %d", (curloop<loopcount)?curloop:loopcount, loopcount);
            } else {
                sprintf(loopbuf, "%d / inf.", curloop);
            }
        } else {
            strcpy(loopbuf, "1 / 1");
        }
        lv_label_set_static_text(text_loop, loopbuf);
    }
    lastelapsedmins = elapsedmins;
    lastelapsedsecs = elapsedsecs;
    lastbarx = barx;
    laststatus = bits;
}

void Ui_NowPlaying_Tick() {
    if (xEventGroupGetBits(Player_Status) & PLAYER_STATUS_RAN_OUT) {
        ESP_LOGW(TAG, "player run out detected!!");
        ESP_LOGW(TAG, "request stop");
        xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_STOP_RUNNING, eSetValueWithoutOverwrite);
        ESP_LOGW(TAG, "wait stop");
        xEventGroupWaitBits(Player_Status, PLAYER_STATUS_NOT_RUNNING, false, true, pdMS_TO_TICKS(3000));
        QueueLength = 0;
        QueuePosition = 0;
        ESP_LOGW(TAG, "return to last screen");
        Ui_Screen = lastscreen;
        return;
    }
    
    bool tookmutex = false;
    //there are races with these vars, but they should be extremely unlikely
    if (Ui_NowPlaying_DataAvail) {
        Ui_NowPlaying_DataAvail = false;
        tookmutex = true;
        LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
        newtrack();
    }
    if (esp_timer_get_time() - nptimer >= 100000) {
        if (Ui_NowPlaying_DriverRunning) {
            if (!tookmutex) LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
            tookmutex = true;
            do_tick();
        }
        nptimer = esp_timer_get_time();
    }
    if (tookmutex) LcdDma_Mutex_Give();
}

void drawopts() {
    LcdDma_Mutex_Take(pdMS_TO_TICKS(1000));
    if (optionsopen) {
        Ui_SoftBar_Update(1, true, LV_SYMBOL_LEFT" Back", false);
        if (selectedopt == 4 || selectedopt == 5) { //links
            Ui_SoftBar_Update(2, true, LV_SYMBOL_RIGHT" Go", false);
        } else if (selectedopt == 3) {
            Ui_SoftBar_Update(2, Pitch_Get()!=0?true:false, LV_SYMBOL_REFRESH" Reset", false);
        } else {
            Ui_SoftBar_Update(2, false, LV_SYMBOL_CLOSE" N/A", false);
        }
    } else {
        Ui_SoftBar_Update(1, true, "Browser", false);
        Ui_SoftBar_Update(2, true, "Settings", false);
    }
    lv_obj_set_hidden(label_options, !optionsopen);
    lv_obj_set_hidden(text_opt_playmode, !optionsopen);
    lv_obj_set_hidden(text_opt_loops, !optionsopen);
    lv_obj_set_hidden(text_opt_shuffle, !optionsopen);
    lv_obj_set_hidden(text_opt_pitch, !optionsopen);
    lv_obj_set_hidden(text_opt_more, !optionsopen);
    lv_obj_set_hidden(text_opt_muting, !optionsopen);
    lv_obj_set_hidden(label_playlist, optionsopen);
    lv_obj_set_hidden(label_time, optionsopen);
    lv_obj_set_hidden(label_loop, optionsopen);
    lv_obj_set_hidden(text_playlist, optionsopen);
    lv_obj_set_hidden(text_time, optionsopen);
    lv_obj_set_hidden(text_loop, optionsopen);
    lv_obj_set_style(text_opt_playmode, selectedopt==0?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_loops, selectedopt==1?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_shuffle, selectedopt==2?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_pitch, selectedopt==3?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_muting, selectedopt==4?&textstyle_sm_sel:&textstyle_sm);
    lv_obj_set_style(text_opt_more, selectedopt==5?&textstyle_sm_sel:&textstyle_sm);
    for (uint8_t i=0;i<4;i++) {
        lv_obj_set_hidden(dpad[i], optionsopen);
    }
    switch (Player_RepeatMode) {
        case REPEAT_NONE:
            lv_label_set_static_text(text_opt_playmode, "Repeat None");
            break;
        case REPEAT_ONE:
            lv_label_set_static_text(text_opt_playmode, "Repeat One");
            break;
        case REPEAT_ALL:
            lv_label_set_static_text(text_opt_playmode, "Repeat All");
            break;
        default:
            break;
    }
    if (Player_LoopCount != 255) {
        sprintf(loopcountbuf, "%d", Player_LoopCount);
        lv_label_set_text(text_opt_loops, loopcountbuf);
    } else {
        lv_label_set_static_text(text_opt_loops, "inf.");
    }
    if (Pitch_Get() < 0) *pitchbuf = '-';
    else *pitchbuf = '+';
    sprintf(pitchbuf+1, "%.1f%%", (float)abs(Pitch_Get())/10.0f);
    lv_label_set_text(text_opt_pitch, pitchbuf);
    if (Queue_Shuffle) {
        lv_label_set_static_text(text_opt_shuffle, "On");
    } else {
        lv_label_set_static_text(text_opt_shuffle, "Off");
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
                    if (selectedopt == 0) { //play mode
                        if (Player_RepeatMode > 0) {
                            Player_RepeatMode--;
                            OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 1) { //loop count
                        if (Player_LoopCount > 1) {
                            if (Player_LoopCount != 255) {
                                --Player_LoopCount;
                                OptionsMgr_Touch();
                            } else {
                                Player_LoopCount = 10;
                                OptionsMgr_Touch();
                            }
                        }
                        drawopts();
                    } else if (selectedopt == 2) { //shuffle
                        Queue_Shuffle = false;
                        OptionsMgr_Touch();
                        drawopts();
                    } else if (selectedopt == 3) { //pitch
                        int16_t p = Pitch_Get();
                        if ((event.State & KEY_EVENT_REPEAT) && p % 10 != 0) {
                            if (p <= 0) p -= 10;
                            p /= 10;
                            p *= 10;
                        } else {
                            p -= (event.State & KEY_EVENT_REPEAT)?10:1;
                        }
                        Pitch_Adjust(p);
                        drawopts();
                    }
                }
                break;
            case KEY_RIGHT:
                if (!optionsopen) {
                    if (QueueLength) xTaskNotify(Taskmgr_Handles[TASK_PLAYER], PLAYER_NOTIFY_NEXT, eSetValueWithoutOverwrite);
                } else {
                    if (selectedopt == 0) { //play mode
                        if (Player_RepeatMode < REPEAT_COUNT-1) {
                            Player_RepeatMode++;
                            OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 1) { //loop count
                        if (Player_LoopCount == 10) {
                            Player_LoopCount = 255;
                            OptionsMgr_Touch();
                        } else if (Player_LoopCount < 10) {
                            ++Player_LoopCount;
                            OptionsMgr_Touch();
                        }
                        drawopts();
                    } else if (selectedopt == 2) { //shuffle
                        Queue_Shuffle = true;
                        OptionsMgr_Touch();
                        drawopts();
                    } else if (selectedopt == 3) { //pitch
                        int16_t p = Pitch_Get();
                        if ((event.State & KEY_EVENT_REPEAT) && p % 10 != 0) {
                            if (p > 0) p += 10;
                            p /= 10;
                            p *= 10;
                        } else {
                            p += (event.State & KEY_EVENT_REPEAT)?10:1;
                        }
                        Pitch_Adjust(p);
                        drawopts();
                    }
                }
                break;
            case KEY_B:
                KeyMgr_Consume(KEY_B);
                if (optionsopen) { //closing options
                    optionsopen = false;
                    drawopts();
                    //force redraw of time and stuff, if loop count changed
                    lastelapsedmins = 0;
                    lastelapsedsecs = 0;
                    do_tick();
                } else {
                    Ui_Screen = UISCREEN_FILEBROWSER;
                }
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
                    Ui_Screen = UISCREEN_MAINMENU;
                } else {
                    if (selectedopt < 5) {
                        selectedopt++;
                        drawopts();
                    }
                }
                break;
            case KEY_C:
                if (optionsopen) { //options open, are we pressing go on muting or options links?
                    //if one of the "links" is selected, go there
                    if (selectedopt == 4) {
                        Ui_Screen = UISCREEN_MUTING;
                    } else if (selectedopt == 5) {
                        Ui_Screen = UISCREEN_OPTIONS_CATS;
                    } else if (selectedopt == 3) {
                        Pitch_Adjust(0);
                        Ui_SoftBar_Update(2, false, LV_SYMBOL_REFRESH" Reset", false);
                    } else {
                        //do nothing
                    }
                    drawopts();
                } else { //options not open, we're opening it
                    optionsopen = true;
                    drawopts();
                }
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
