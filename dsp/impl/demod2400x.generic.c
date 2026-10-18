// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.generic.c: DEMOD_2400X upsampler and preamble search, plain C.
// Also built into every SIMD flavour, where the compiler may auto-vectorise it.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#include "dsp/helpers/demod2400x_dsp.h"
#include "dsp/helpers/demod2400x_coeffs.h"

// Get power
static inline unsigned getpow_generic(
    const int8_t *cv,
    const int16_t *ii,
    const int16_t *qq,
    int shift)
{
    int32_t si =
          cv[0] * ii[0]
        + cv[1] * ii[1]
        + cv[2] * ii[2]
        + cv[3] * ii[3]
        + cv[4] * ii[4]
        + cv[5] * ii[5]
        + cv[6] * ii[6]
        + cv[7] * ii[7];

    int32_t sq =
          cv[0] * qq[0]
        + cv[1] * qq[1]
        + cv[2] * qq[2]
        + cv[3] * qq[3]
        + cv[4] * qq[4]
        + cv[5] * qq[5]
        + cv[6] * qq[6]
        + cv[7] * qq[7];

    si = (si+16)>>5;
    sq = (sq+16)>>5;
    uint32_t pwr = (uint32_t)(si*si) + (uint32_t)(sq*sq);

    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;

    return pwr;
}


// Upsample
int STARCH_IMPL(upsample12, generic) (const int16_t * restrict in, int len, int shift, uint16_t * restrict oo)
{
    if (len < 8)
        return 0;

    int16_t ii[8];
    int16_t qq[8];

    int nt      = NU_SAMP(len);
    int stride  = NU_STRIDE(nt);
    int stride6 = 6 * stride;
    int bpos    = 0;

    /* Initial 8-sample window */
    for (int i = 0; i < 8; i++) {

        ii[i] = in[0];
        qq[i] = in[1];
        in += 2;
    }
    len -= 8;

    while (1) {

        oo[bpos] = getpow_generic(upsample_coeffs[0], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow_generic(upsample_coeffs[1], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow_generic(upsample_coeffs[2], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow_generic(upsample_coeffs[3], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow_generic(upsample_coeffs[4], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        if (len == 0)
            break;

        len--;

        /* Shift window left by one sample */
        for (int i = 0; i < 7; i++) {
            ii[i] = ii[i + 1];
            qq[i] = qq[i + 1];
        }

        ii[7] = in[0];
        qq[7] = in[1];
        in += 2;
    }

    return nt;
}


// Locate preamble
int STARCH_IMPL(locate_preamble, generic) (const uint16_t * restrict pp, int nt, uint8_t * restrict mm)
{
    int stride = NU_STRIDE(nt);

    for (int j = 0; j < 6; j++) {

        const uint16_t *restrict p = pp + j * stride;
        uint8_t *restrict mp = mm + j;

        int plen = (nt + 5 - j) / 6;

        plen -= 2 * LONG_FRAME;

        for (int i = 0; i < plen - 15; i++) {

#ifdef PREAMBLE_MIN_PULSE
            uint32_t minp =
                MIN(
                    MIN(p[0], p[2]),
                    MIN(p[7], p[9])
                );
#endif
#ifdef PREAMBLE_SUM_PULSE
            uint32_t minp = p[0] + p[2] + p[7] + p[9];
#endif
            uint32_t gap =
                p[ 1] +
                p[ 3] +
                p[ 4] +
                p[ 5] +
                p[ 6] +
                p[ 8] +
                p[10] +
                p[11] +
                p[12] +
                p[13] +
                p[14] +
                p[15];

            *mp = (256 * PREAMBLE_KK * minp)
                / (PREAMBLE_KK * minp + gap + 1);

            p++;
            mp += 6;
        }
    }

    return METRIC_SIZE(nt);
}
