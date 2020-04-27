#include "queue.h"
#include <string.h>
#include "esp_log.h"
#include "player.h"
#include "freertos/event_groups.h"
#include "taskmgr.h"

static const char* TAG = "Queue";

uint32_t QueuePosition = 0;
uint32_t QueueLength = 0;
uint8_t QueueSource = QUEUE_SOURCE_UNDEFINED;
char QueueM3uFilename[256]; //always absolute
char QueueM3uPath[256];
char QueuePlayingFilename[512]; //always absolute
FILE *QueueM3uFile;
char QueueLine[256];

void QueueLoadM3u(char *M3uPath, char *M3uFilename, uint32_t pos, bool CountComments) { //make sure to never call this while player is running! it fucks with the fileptr, temp vars, and queue position!
    strcpy(QueueM3uFilename, M3uFilename);
    strcpy(QueueM3uPath, M3uPath);
    QueuePosition = pos;
    QueueSource = QUEUE_SOURCE_M3U;

    //figure out how many entries are available
    QueueLength = 0;
    QueueM3uFile = fopen(&QueueM3uFilename[0], "r");
    while (!feof(QueueM3uFile)) {
        if (fgets(&QueueLine[0], 255, QueueM3uFile) == NULL) break;
        if (QueueLine[0] != 0 && QueueLine[0] != 0x0d && QueueLine[0] != 0x0a) {
            if ((QueueLine[0] == '#' && CountComments) || QueueLine[0] != '#') QueueLength++;
        }
    }
    fclose(QueueM3uFile);

    ESP_LOGI(TAG, "QueueLoadM3u() found %d entries", QueueLength);

    if (QueuePosition > QueueLength-1) {
        ESP_LOGE(TAG, "QueueLoadM3u() got bad queue pos %d, len %d", QueuePosition, QueueLength);
        QueuePosition = QueueLength-1;
    }
}

bool QueueNext() {
    if (QueuePosition == QueueLength-1) return false;
    QueuePosition++;
    return true;
}

bool QueuePrev() {
    if (QueuePosition == 0) return false;
    QueuePosition--;
    return true;
}

void QueueSetupEntry(bool ReturnComments) {
    if (QueueSource == QUEUE_SOURCE_M3U) {
        QueueM3uFile = fopen(&QueueM3uFilename[0], "r");
        uint32_t c = 0;
        while (!feof(QueueM3uFile)) {
            if (fgets(&QueueLine[0], 255, QueueM3uFile) == NULL) break;
            for (uint8_t i=0;i<255;i++) {
                if (QueueLine[i] == 0x0d || QueueLine[i] == 0x0a) QueueLine[i] = 0;
            }
            if (QueueLine[0] != '#' && QueueLine[0] != 0) {
                if (c++ == QueuePosition) {
                    if (QueueLine[0] != '/') { //build absolute path
                        strcpy(QueuePlayingFilename, QueueM3uPath);
                        strcat(QueuePlayingFilename, "/");
                        strcat(QueuePlayingFilename, QueueLine);
                    } else { //already absolute
                        strcpy(QueuePlayingFilename, QueueLine);
                    }
                    fclose(QueueM3uFile);
                    return;
                }
                Player_LoopCount = Player_SetLoopCount;
            } else if (QueueLine[0] == '#') {
                if (ReturnComments) {
                    if (c++ == QueuePosition) {
                        strcpy(QueuePlayingFilename, QueueLine);
                        fclose(QueueM3uFile);
                        return;
                    }
                }
            }
        }
        fclose(QueueM3uFile);
    }
}