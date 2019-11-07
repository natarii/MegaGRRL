#ifndef AGR_RINGBUF_H
#define AGR_RINGBUF_H

#include "esp_system.h"
#include "freertos/FreeRTOS.h"

//#define SAFE_RINGBUFS //not implemented yet

typedef struct {
    uint32_t head;
    uint32_t tail;
    void *Buf;
    uint32_t Size;
} Ringbuf_t;

void Ringbuf_Init(Ringbuf_t *Ringbuf);
void Ringbuf_Push_SingleByte(Ringbuf_t *Ringbuf, uint8_t Value);
//void Ringbuf_Push_SinglePtr(Ringbuf_t *Ringbuf, uint8_t *Value);
uint8_t Ringbuf_Pop_Single(Ringbuf_t *Ringbuf);
//void Ringbuf_Pop_Multiple(Ringbuf_t *Ringbuf, uint8_t Length, void *Buf);
uint8_t Ringbuf_Peek_Single(Ringbuf_t *Ringbuf);
//bool Ringbuf_IsEmpty(Ringbuf_t *Ringbuf);
//bool Ringbuf_IsFull(Ringbuf_t *Ringbuf);
uint32_t Ringbuf_Available(Ringbuf_t *Ringbuf);

#endif