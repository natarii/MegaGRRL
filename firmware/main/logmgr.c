#include "logmgr.h"
#include "esp_log.h"

static const char* TAG = "LogMgr";

volatile uint8_t logmgr_loglevel = 0;

void logmgr_update_loglevel() {
    //map from our loglevels to espressif's. could probably use theirs, but we don't want to screw up the settings if they change it...
    esp_log_level_t l = ESP_LOG_VERBOSE;
    switch (logmgr_loglevel) {
        case 0:
            l = ESP_LOG_WARN;
            break;
        case 1:
            l = ESP_LOG_INFO;
            break;
        case 2:
            l = ESP_LOG_DEBUG;
            break;
        case 3:
            l = ESP_LOG_VERBOSE;
            break;
        default:
            ESP_LOGE(TAG, "Trying to set invalid loglevel %d!", logmgr_loglevel);
            break;
    }
    ESP_LOGE(TAG, "/!\\ Updating global loglevel to %d /!\\", logmgr_loglevel);
    esp_log_level_set("*", l);
}