// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x_dsp.h: constants shared by the DEMOD_2400X starch kernels
// (dsp/impl/demod2400x.*.c) and demod_2400x.c
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#ifndef DEMOD2400X_DSP_H
#define DEMOD2400X_DSP_H

#include <stdint.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

#define LONG_FRAME 112
#define SHORT_FRAME 56

#define NU_SAMP(t) (5*((t)-7))
#define NU_STRIDE(t) ((((t)+5)/6+7)&~7)
#define METRIC_SIZE(t) ((t)-6*15-12*LONG_FRAME)

#define PREAMBLE_KK 2

// locate_preamble() sets metrics below its 'thres' argument to 0. That is exact
// without dividing (floor(n / d) >= t <=> n >= t * d), and the peak search can't
// tell the difference as long as thres <= its own threshold.
#define PREAMBLE_MIN_PULSE

#endif
