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
#define METRIC_SIZE(t) ((t)-15-12*LONG_FRAME)

#define PREAMBLE_KK 2
#define PREAMBLE_MIN_PULSE

#define OVERLAP_WINDOW \
    (7 + ((15 + 1 + 2*LONG_FRAME + 4) / 5))

// 2.4 -> 12 MHz polyphase FIR, 5 phases x 8 taps, gain 128 per phase
static const int8_t upsample_coeffs[5][8] = {
    {   -1,    4,  -10,  125,   14,   -6,    3,   -1 }, // phase=0 sum=128 energy=15984 gd=3.0938
    {   -3,    8,  -21,  109,   47,  -16,    7,   -3 }, // phase=1 sum=128 energy=14918 gd=3.2969
    {   -4,   10,  -23,   81,   81,  -23,   10,   -4 }, // phase=2 sum=128 energy=14412 gd=3.5000
    {   -3,    7,  -16,   47,  109,  -21,    8,   -3 }, // phase=3 sum=128 energy=14918 gd=3.7031
    {   -1,    3,   -6,   14,  125,  -10,    4,   -1 }, // phase=4 sum=128 energy=15984 gd=3.9062
};

#endif
