#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "key.h"
#include "ioexp.h"
#include "sdcard.h"

static const char* TAG = "KeyMgr";

volatile QueueHandle_t KeyMgr_TargetQueue = NULL;
volatile TaskHandle_t KeyMgr_TargetTask = NULL;
KeyState_t KeyStates[KEY_COUNT];
volatile bool KeyLockout[KEY_COUNT];

void KeyMgr_Consume(uint8_t key) {
    KeyLockout[key] = true;
}

bool KeyMgr_Setup() {
    ESP_LOGI(TAG, "Setting up");

    for (uint8_t i=0;i<KEY_COUNT;i++) {
        KeyStates[i].RawState = 0;
        KeyStates[i].DebouncedState = 0;
        KeyLockout[i] = false;
    }

    ESP_LOGI(TAG, "OK");
    return true;
}

void KeyMgr_SendEvent(uint8_t key, uint8_t state) {
    if (KeyMgr_TargetQueue == NULL) {
        ESP_LOGW(TAG, "Warning: Sending key event when no target queue is set !!");
        return;
    }
    KeyEvent_t KeyEvent;
    KeyEvent.Key = key;
    KeyEvent.State = state;
    BaseType_t ret = xQueueSend(KeyMgr_TargetQueue, &KeyEvent, 0);
    if (ret == errQUEUE_FULL) {
        ESP_LOGW(TAG, "Warning: Target queue for key events is full, event will be dropped !!");
        return;
    }
    if (KeyMgr_TargetTask != NULL && (state & KEY_EVENT_PRESS)) xTaskNotify(KeyMgr_TargetTask, 0, eNoAction);
}

void KeyMgr_Main() {
    ESP_LOGI(TAG, "Task start");
    while (1) {
        if (IoExp_PORTAQueue == NULL) {
            ESP_LOGE(TAG, "PORTAQueue not set up yet !!");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        PORTAQueueItem_t event;
        bool avail = xQueueReceive(IoExp_PORTAQueue, &event, pdMS_TO_TICKS(25));
        uint32_t now = xTaskGetTickCount();
        for (uint8_t bit=0;bit<KEY_COUNT;bit++) {
            //handle new inputs
            if (avail) {
                KeyStates[bit].RawState = (event.PORTA >> bit) & 1;
                KeyStates[bit].RawStateTs = event.Timestamp;
            }

            //debounce, badly
            if (KeyStates[bit].RawState && !KeyStates[bit].DebouncedState) { //down
                KeyStates[bit].DebouncedState = 1;
                KeyStates[bit].TsDown = KeyStates[bit].RawStateTs;
                KeyStates[bit].AlreadyHeld = false;

                //edge
                KeyMgr_SendEvent(bit, KEY_EVENT_DOWN);
                KeyMgr_SendEvent(bit, KEY_EVENT_PRESS);
            } else if (!KeyStates[bit].RawState && KeyStates[bit].DebouncedState) { //up
                if (now - KeyStates[bit].RawStateTs >= KEY_DEBOUNCE_TIME) {
                    KeyStates[bit].DebouncedState = 0;
                    KeyStates[bit].TsUp = now;

                    //edge
                    if (!KeyLockout[bit]) KeyMgr_SendEvent(bit, KEY_EVENT_UP | (KeyStates[bit].AlreadyHeld?KEY_EVENT_HELD:0));
                    else KeyLockout[bit] = false;
                }
            }

            //hold notif
            if (KeyStates[bit].DebouncedState && !KeyStates[bit].AlreadyHeld && now - KeyStates[bit].TsDown >= KEY_HOLD_DELAY) {
                if (!KeyLockout[bit]) KeyMgr_SendEvent(bit, KEY_EVENT_HOLD);
                KeyStates[bit].AlreadyHeld = true;
            }

            //repeat
            if (KeyStates[bit].DebouncedState && now - KeyStates[bit].TsDown >= KEY_REPEAT_DELAY) {
                if (now - KeyStates[bit].TsLastRepeat >= KEY_REPEAT_INTERVAL) {
                    if (!KeyLockout[bit]) KeyMgr_SendEvent(bit, KEY_EVENT_PRESS | KEY_EVENT_REPEAT);
                    KeyStates[bit].TsLastRepeat = now;
                    if (bit == KEY_A && now - KeyStates[bit].TsDown >= 2500) {
                        Sdcard_Destroy();
                        vTaskDelay(pdMS_TO_TICKS(500));
                        /*do shutdown stuff*/
                        IoExp_BacklightControl(false);
                        IoExp_PowerControl(false);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        //if we're still alive, we must be on usb power...
                        ESP_LOGE(TAG, "Reset by holding back !!");
                        esp_restart();
                    }
                }
            }
        }
    }
}
