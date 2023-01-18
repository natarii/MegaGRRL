#pragma once
#include <stdint.h>

#define GETBIT(var, bit) (var & (1<<bit))
#define SETBIT(var, bit) (var |= (1<<bit))
#define CLEARBIT(var, bit) (var &= ~(1<<bit))
#define INRANGE(var, lower, upper) (var >= lower && var <= upper)
#define LONIB(var) (var & 0xf)
#define HINIB(var) ((var >> 4) & 0xf)

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);