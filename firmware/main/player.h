#ifndef AGR_PLAYER_H
#define AGR_PLAYER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "unistd.h"
#include "mallocs.h"
#include "vgm.h"

typedef enum {
    REPEAT_NONE,
    REPEAT_ONE,
    REPEAT_ALL,
    REPEAT_COUNT
} RepeatMode_t;

enum {
    PLAYER_NOTIFY_START_RUNNING = 1,
    PLAYER_NOTIFY_STOP_RUNNING,
    PLAYER_NOTIFY_PLAY,
    PLAYER_NOTIFY_PAUSE,
    PLAYER_NOTIFY_STOP,
    PLAYER_NOTIFY_PREV,
    PLAYER_NOTIFY_NEXT,
    PLAYER_NOTIFY_TRACK_DONE,
};

#define PLAYER_STATUS_NOT_RUNNING   0x01
#define PLAYER_STATUS_RUNNING       0x02
#define PLAYER_STATUS_PAUSED        0x08
#define PLAYER_STATUS_RAN_OUT       0x10

extern EventGroupHandle_t Player_Status;
extern char Player_Gd3_Title[PLAYER_GD3_FIELD_SIZES+1];
extern char Player_Gd3_Game[PLAYER_GD3_FIELD_SIZES+1];
extern char Player_Gd3_Author[PLAYER_GD3_FIELD_SIZES+1];
extern VgmInfoStruct_t Player_Info;
extern volatile uint8_t Player_LoopCount;
extern volatile RepeatMode_t Player_RepeatMode;
extern volatile bool Player_UnvgzReplaceOriginal;

bool Player_StartTrack(char *FilePath);
bool Player_StopTrack();
void Player_Main();
bool Player_Setup();

#endif