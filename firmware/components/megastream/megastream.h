#ifndef MEGASTREAM_H
#define MEGASTREAM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    volatile size_t tail;
    volatile size_t head;
    size_t size;
} MegaStreamContext_t;

void MegaStream_Create(MegaStreamContext_t *ctx, uint8_t *buf, size_t size);
void MegaStream_Reset(MegaStreamContext_t *ctx);
void MegaStream_Send(MegaStreamContext_t *ctx, uint8_t *inbuf, size_t insize);
void MegaStream_Recv(MegaStreamContext_t *ctx, uint8_t *outbuf, size_t outsize);
uint8_t MegaStream_Peek(MegaStreamContext_t *ctx);
size_t MegaStream_Used(MegaStreamContext_t *ctx);
size_t MegaStream_Free(MegaStreamContext_t *ctx);

#endif