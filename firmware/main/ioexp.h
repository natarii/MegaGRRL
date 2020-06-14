#ifndef AGR_IOEXP_H
#define AGR_IOEXP_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"

#define I2C_ADDR_IOEXP 0b0100000

typedef struct PORTAQueueItem {
    TickType_t Timestamp;
    uint8_t PORTA;
} PORTAQueueItem_t;

extern QueueHandle_t IoExp_PORTAQueue;

extern volatile uint8_t IoExp_PowerOnKeys;

bool IoExp_Setup();
void IoExp_Main();
bool IoExp_WriteLed(uint8_t LedNo, bool LedStatus);
bool IoExp_WriteLeds(bool FlashlightStatus, bool Led1Status, bool Led2Status);
bool IoExp_WriteLedsBin(uint8_t ThreeBits);
bool IoExp_PowerControl(bool HoldPower);
bool IoExp_BacklightControl(bool On);
bool IoExp_ChargeStatus();
bool IoExp_BatSenseControl(bool SenseEn);

#endif