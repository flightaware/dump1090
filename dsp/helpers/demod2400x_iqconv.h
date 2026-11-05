// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x_iqconv.h: DC removal and level/power sums shared by the
// iqdata_conv_* starch kernels (dsp/impl/demod2400x.*.c). The SIMD versions
// use these for their tails, so every flavour gives the same bits.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#ifndef DEMOD2400X_IQCONV_H
#define DEMOD2400X_IQCONV_H

#include <stdint.h>

#include "dsp-types.h"
#include "compat/compat.h"
#include "dsp/helpers/demod2400x_coeffs.h"

// DC filter: offset[] holds the running mean << IQ_DC_SHIFT (time constant 4096 samples).
// The correction is held for a block of IQ_DC_BLOCK samples, then the offset is
// updated with the block's sum, so the SIMD versions need no serial loop.
#define IQ_DC_SHIFT 12
#define IQ_DC_BLOCK 16

// Level is summed as floor(sqrt(magsq << IQ_LEVEL_SHIFT)), i.e. magnitude * 8
#define IQ_LEVEL_SHIFT 6

// Raw samples -> 12-bit signed interleaved I,Q (before DC removal)
static inline void iqconv_raw_uc8(const uc8_t *in, int16_t *raw, unsigned n)
{
    for (unsigned k = 0; k < n; ++k) {
        raw[2 * k]     = (int16_t) ((2 * in[k].I - 255) * 8);    // -2040..+2040
        raw[2 * k + 1] = (int16_t) ((2 * in[k].Q - 255) * 8);
    }
}

static inline void iqconv_raw_sc16(const sc16_t *in, int16_t *raw, unsigned n)
{
    for (unsigned k = 0; k < n; ++k) {
        raw[2 * k]     = (int16_t) ((int16_t) le16toh(in[k].I) >> 4);
        raw[2 * k + 1] = (int16_t) ((int16_t) le16toh(in[k].Q) >> 4);
    }
}

static inline int16_t iqconv_clamp12(int32_t v)
{
    return (int16_t) (v > 2047 ? 2047 : v < -2048 ? -2048 : v);
}

static inline void iqconv_raw_sc16q11(const sc16_t *in, int16_t *raw, unsigned n)
{
    for (unsigned k = 0; k < n; ++k) {
        raw[2 * k]     = iqconv_clamp12((int16_t) le16toh(in[k].I));
        raw[2 * k + 1] = iqconv_clamp12((int16_t) le16toh(in[k].Q));
    }
}

// Remove DC from one block of n <= IQ_DC_BLOCK samples, clamp to 12 bits,
// and add the block to the level/power sums
static inline void iqconv_dc_block(const int16_t *raw, int16_t *out, unsigned n,
                                   int32_t *offset, uint64_t *sum_level, uint64_t *sum_power)
{
    const int32_t corr_i = (offset[0] + (1 << (IQ_DC_SHIFT - 1))) >> IQ_DC_SHIFT;
    const int32_t corr_q = (offset[1] + (1 << (IQ_DC_SHIFT - 1))) >> IQ_DC_SHIFT;
    int32_t sum_i = 0, sum_q = 0;
    uint32_t level = 0, power = 0;

    for (unsigned k = 0; k < n; ++k) {
        int32_t ii = raw[2 * k] - corr_i;
        int32_t qq = raw[2 * k + 1] - corr_q;
        sum_i += ii;
        sum_q += qq;

        ii = iqconv_clamp12(ii);
        qq = iqconv_clamp12(qq);
        out[2 * k]     = (int16_t) ii;
        out[2 * k + 1] = (int16_t) qq;

        uint32_t magsq = (uint32_t) (ii * ii + qq * qq);   // <= 2^23
        power += magsq;
        level += demod_isqrt(magsq << IQ_LEVEL_SHIFT);
    }

    offset[0] += sum_i;
    offset[1] += sum_q;
    *sum_level += level;
    *sum_power += power;
}

// Mean level and power, normalised so 12-bit full scale (2048) = 1.0
static inline void iqconv_stats(uint64_t sum_level, uint64_t sum_power, unsigned len,
                                double *out_level, double *out_power)
{
    if (out_level != NULL)
        *out_level = len ? (double) sum_level / (1 << (IQ_LEVEL_SHIFT / 2)) / 2048.0 / len : 0;

    if (out_power != NULL)
        *out_power = len ? (double) sum_power / 2048.0 / 2048.0 / len : 0;
}

#endif
