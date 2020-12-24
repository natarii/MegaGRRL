#include "queue.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "taskmgr.h"
#include "driver.h" //get access to the pcm buffer

static const char* TAG = "Queue";

IRAM_ATTR uint32_t QueuePosition = 0;
IRAM_ATTR uint32_t QueueLength = 0;
static uint8_t QueueSource = QUEUE_SOURCE_UNDEFINED;
char QueueM3uFilename[256]; //always absolute
static char QueueM3uPath[256];
char QueuePlayingFilename[512]; //always absolute
static FILE *QueueM3uFile = NULL;
static FILE *cachefile = NULL;
static char QueueLine[256];
volatile bool Queue_Shuffle = false;
#define QUEUE_CACHE_VER 2
static const char cachefilename[] = "/sd/.mega/m3ucach2.bin";

void QueueLoadM3u(const char *M3uPath, const char *M3uFilename, uint32_t pos, bool CountComments, bool ShufflePreserveCurrentEntry) { //make sure to never call this while player is running! it fucks with the fileptr, temp vars, and queue position!
    CountComments = false; //needs to either be handled properly, or deleted. just doing this for now...
    ESP_LOGI(TAG, "QueueLoadM3u() starting");

    strcpy(QueueM3uFilename, M3uFilename);
    strcpy(QueueM3uPath, M3uPath);
    QueuePosition = pos;
    QueueSource = QUEUE_SOURCE_M3U;

    //figure out how many entries are available, also generate offset cache
    Driver_PcmBuf[0] = QUEUE_CACHE_VER;
    QueueLength = 0;
    if (QueueM3uFile) fclose(QueueM3uFile);
    QueueM3uFile = fopen(&QueueM3uFilename[0], "r");
    while (!feof(QueueM3uFile)) {
        pos = ftell(QueueM3uFile); //might be faster to add up strlens rather than ftell?
        if (fgets(&QueueLine[0], 255, QueueM3uFile) == NULL) break;
        if (QueueLine[0] != 0 && QueueLine[0] != 0x0d && QueueLine[0] != 0x0a) {
            if ((QueueLine[0] == '#' && CountComments) || QueueLine[0] != '#') {
                memcpy(&Driver_PcmBuf[5+(QueueLength*4)], &pos, 4);
                QueueLength++;
            }
        }
    }
    //fclose(QueueM3uFile);

    ESP_LOGI(TAG, "QueueLoadM3u() found %d entries", QueueLength);

    uint32_t tmp = QueueLength; //QueueLength is in IRAM, can't fwrite it directly...
    memcpy(&Driver_PcmBuf[1], &tmp, 4);
    
    //write out the non-shuffled playlist cache
    ESP_LOGI(TAG, "Writing playlist cache");
    if (cachefile) fclose(cachefile);
    cachefile = fopen(cachefilename, "w");
    fwrite(Driver_PcmBuf, 1, 5 + (QueueLength*4), cachefile);

    //shuffle and write out the shuffled one
    ESP_LOGI(TAG, "Shuffling");
    if (QueueLength > 1) {
        for (size_t i=0;i<QueueLength-1;i++) {
            size_t j = i+esp_random() / (UINT32_MAX/(QueueLength-i)+1);
            if (ShufflePreserveCurrentEntry && (i == QueuePosition || j == QueuePosition)) { //don't screw with the currently selected one - little more user friendly behavior if they turn shuffle on/off
                ESP_LOGI(TAG, "Shuffle is on, not swapping %d and %d", j, i);
                continue;
            }
            uint32_t t = 0;
            memcpy(&t, &Driver_PcmBuf[5+(j*4)], 4);
            memcpy(&Driver_PcmBuf[5+(j*4)], &Driver_PcmBuf[5+(i*4)], 4);
            memcpy(&Driver_PcmBuf[5+(i*4)], &t, 4);
        }
    }
    ESP_LOGI(TAG, "Writing shuffled playlist cache");
    fwrite(Driver_PcmBuf, 1, 5 + (QueueLength*4), cachefile);
    fclose(cachefile); //could leave it open, but then it's never properly written to the card
    cachefile = fopen(cachefilename, "r");
    ESP_LOGI(TAG, "Done");

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


void QueueSetupEntry(bool ReturnComments, bool ProcessShuffle) {
    if (QueueSource == QUEUE_SOURCE_M3U) {
        uint32_t off = 5 + (QueuePosition*4);
        //note here: queue length is stored in the file, but we don't actually need to use it, we can use the one in mem
        if (ProcessShuffle && Queue_Shuffle) off += (5+(QueueLength*4)); //skip to the second half if we're shufflin'
        fseek(cachefile, off, SEEK_SET); //ver and entry count + file offset
        fread(&off, 4, 1, cachefile);
        ESP_LOGD(TAG, "Entry at m3u file offset %d", off);
        if (QueueM3uFile == NULL) QueueM3uFile = fopen(&QueueM3uFilename[0], "r");
        fseek(QueueM3uFile, off, SEEK_SET);
        fgets(&QueueLine[0], 255, QueueM3uFile);
        for (uint8_t i=0;i<255;i++) {
            if (QueueLine[i] == 0x0d || QueueLine[i] == 0x0a) QueueLine[i] = 0;
        }
        if (QueueLine[0] != '#' && QueueLine[0] != 0) {
            if (QueueLine[0] != '/') { //build absolute path
                strcpy(QueuePlayingFilename, QueueM3uPath);
                strcat(QueuePlayingFilename, "/");
                strcat(QueuePlayingFilename, QueueLine);
            } else { //already absolute
                strcpy(QueuePlayingFilename, QueueLine);
            }
            //fclose(QueueM3uFile);
            return;
        } else if (QueueLine[0] == '#') {
            if (ReturnComments) {
                strcpy(QueuePlayingFilename, QueueLine);
                //fclose(QueueM3uFile);
                return;
            }
        }
        //fclose(QueueM3uFile);
    }
}
