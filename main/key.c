#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "key.h"
#include "ioexp.h"

static const char* TAG = "KeyMgr";

void(*KeyMgr_Listener)(uint8_t, uint8_t) = NULL;
KeyState_t KeyStates[KEY_COUNT];

bool KeyMgr_Setup() {
    ESP_LOGI(TAG, "Setting up");

    for (uint8_t i=0;i<KEY_COUNT;i++) {
        KeyStates[i].RawState = 0;
        KeyStates[i].DebouncedState = 0;
    }

    ESP_LOGI(TAG, "OK");
    return true;
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
                if (KeyMgr_Listener != NULL) {
                    KeyMgr_Listener(bit, KEY_EVENT_DOWN);
                    KeyMgr_Listener(bit, KEY_EVENT_PRESS);
                }
            } else if (!KeyStates[bit].RawState && KeyStates[bit].DebouncedState) { //up
                if (now - KeyStates[bit].RawStateTs >= KEY_DEBOUNCE_TIME) {
                    KeyStates[bit].DebouncedState = 0;
                    KeyStates[bit].TsUp = now;

                    //edge
                    if (KeyMgr_Listener != NULL) KeyMgr_Listener(bit, KEY_EVENT_UP);
                }
            }

            //hold notif
            if (KeyStates[bit].DebouncedState && !KeyStates[bit].AlreadyHeld && now - KeyStates[bit].TsDown >= KEY_HOLD_DELAY) {
                if (KeyMgr_Listener != NULL) KeyMgr_Listener(bit, KEY_EVENT_HOLD);
                KeyStates[bit].AlreadyHeld = true;
            }

            //repeat
            if (KeyStates[bit].DebouncedState && now - KeyStates[bit].TsDown >= KEY_REPEAT_DELAY) {
                if (now - KeyStates[bit].TsLastRepeat >= KEY_REPEAT_INTERVAL) {
                    if (KeyMgr_Listener != NULL) KeyMgr_Listener(bit, KEY_EVENT_PRESS | KEY_EVENT_REPEAT);
                    KeyStates[bit].TsLastRepeat = now;
                }
            }
        }
    }
}