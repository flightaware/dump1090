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

// The 12 MHz samples are 8-bit log powers, L ~ 8 log2(P + 1) (demod_log8()).

// decode_bits(p, nbytes, msg, quality): bits from chip pairs p[2k], p[2k+1] (bit = 1 if the
// first is larger), 8 bits per byte, most significant first. *quality gets the mean of
// min(|p[2k] - p[2k+1]|, DECODE_BITS_CAP) (log power ratio of the two chips), scaled to
// roughly the old magnitude contrast, 256 * |a - b| / (a + b), for contrasts up to about
// 0.4, capped at 256. The cap (4 octaves of power) keeps noise chips, whose log has a long
// tail, from inflating the quality of weak frames. nbytes <= 2048.
#define DECODE_BITS_MAX_BYTES 2048
#define DECODE_BITS_CAP 32
#define DECODE_BITS_QUALITY(sum, nbytes) \
    ((nbytes) ? MIN(256u, (unsigned) ((11 * (uint64_t) (sum)) / (16 * (uint64_t) (nbytes)))) : 0u)

// Preamble metric: (12 L(minp) - sum of L over the 12 gaps) / 2, clamped to 0..255, minp
// the smallest of the 4 pulses: 6 (L(minp) - mean L(gap)), the smallest pulse against the
// geometric mean of the gaps, 6 units per step of L. locate_preamble() sets metrics below its
// 'thres' argument to 0; the peak search can't tell the difference as long as thres <= its
// own threshold.

#endif
