#include "ringbuf.h"

void Ringbuf_Init(Ringbuf_t *Ringbuf) {
    Ringbuf->head = 0;
    Ringbuf->tail = 0;
}

void Ringbuf_Push_SingleByte(Ringbuf_t *Ringbuf, uint8_t Value) {
    ((uint8_t *)Ringbuf->Buf)[Ringbuf->head] = Value;
    Ringbuf->head = (Ringbuf->head+1)%Ringbuf->Size;
}

uint8_t Ringbuf_Pop_Single(Ringbuf_t *Ringbuf) {
    uint8_t b = ((uint8_t *)Ringbuf->Buf)[Ringbuf->tail];
    Ringbuf->tail = (Ringbuf->tail+1)%Ringbuf->Size;
    return b;
}
uint8_t Ringbuf_Peek_Single(Ringbuf_t *Ringbuf) {
    return ((uint8_t *)Ringbuf->Buf)[Ringbuf->tail];
}

uint32_t Ringbuf_Available(Ringbuf_t *Ringbuf) {
    if ((Ringbuf->head+1)%Ringbuf->Size == Ringbuf->tail) return 0;
    return Ringbuf->Size - (Ringbuf->head - Ringbuf->tail);
}