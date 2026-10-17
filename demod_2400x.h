#ifndef DEMOD_2400X_H
#define DEMOD_2400X_H

#include "fifo.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

#define NU_SAMP(t) (5*((t)-7))
#define NU_STRIDE(t) ((((t)+5)/6+7)&~7)
#define METRIC_SIZE(t) ((t)-15-12*LONG_FRAME)

#define LONG_FRAME 112
#define SHORT_FRAME 56

#define PREAMBLE_KK 2
#define PREAMBLE_MIN_PULSE

#define OVERLAP_WINDOW \
    (7 + ((15 + 1 + 2*LONG_FRAME + 4) / 5))

extern int upsample12(const int16_t * restrict in, int len, int shift, uint16_t * restrict oo);
extern int locate_preamble(const uint16_t * restrict pp, int nt, uint8_t * restrict mm);
extern void demodulate2400x(struct mag_buf *mbuf);
    
#endif
