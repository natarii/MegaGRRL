#ifndef AGR_PLAYER_H
#define AGR_PLAYER_H

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "unistd.h"

enum {
    PLAYER_NOTIFY_PLAY = 1,
    PLAYER_NOTIFY_PAUSE,
    PLAYER_NOTIFY_STOP,
    PLAYER_NOTIFY_PREV,
    PLAYER_NOTIFY_NEXT,
};

bool Player_StartTrack(char *FilePath);
bool Player_StopTrack();
void Player_Main();

#endif