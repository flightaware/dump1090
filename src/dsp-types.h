#ifndef DUMP1090_DSP_TYPES_H
#define DUMP1090_DSP_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t I;
    uint8_t Q;
} __attribute__((__packed__, __aligned__(2))) uc8_t;

typedef union {
    uc8_t uc8;
    uint16_t u16;
} uc8_u16_t;

typedef struct {
    int16_t I;
    int16_t Q;
} __attribute__((__packed__, __aligned__(2))) sc16_t;

#endif
