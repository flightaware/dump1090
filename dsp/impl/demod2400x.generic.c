// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.generic.c: DEMOD_2400X input conversion, upsampler and preamble search, plain C.
// Also built into every SIMD flavour, where the compiler may auto-vectorise it.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#include "dsp/helpers/demod2400x_dsp.h"
#include "dsp/helpers/demod2400x_coeffs.h"
#include "dsp/helpers/demod2400x_iqconv.h"

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

#ifdef USE_MAGNITUDE
    (void)shift;
    pwr = demod_isqrt(pwr);
#else
    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;
#endif

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
int STARCH_IMPL(locate_preamble, generic) (const uint16_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
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

            uint32_t n = 256 * PREAMBLE_KK * minp;
            uint32_t d = PREAMBLE_KK * minp + gap + 1;
            *mp = n < thres * d ? 0 : n / d;           // below threshold: 0, no divide

            p++;
            mp += 6;
        }
    }

    return METRIC_SIZE(nt);
}


// Input conversion: raw samples -> DC-removed 12-bit signed interleaved I,Q in the
// FIFO buffer, plus mean level and power. The DC filter starts from dc_in[2] (I, Q)
// and leaves its final state in dc_out[2]; they may be the same array.
void STARCH_IMPL(iqdata_conv_uc8, generic) (const uc8_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                         const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    int16_t * restrict out = (int16_t *) out_u;   // signed I/Q stored in the uint16_t FIFO buffer
    int16_t raw[2 * IQ_DC_BLOCK];
    int32_t offset[2] = { dc_in[0], dc_in[1] };
    uint64_t sum_level = 0, sum_power = 0;

    for (unsigned n = 0; n < len; n += IQ_DC_BLOCK) {
        unsigned nb = MIN(IQ_DC_BLOCK, len - n);
        iqconv_raw_uc8(in + n, raw, nb);
        iqconv_dc_block(raw, out + 2 * n, nb, offset, &sum_level, &sum_power);
    }

    dc_out[0] = offset[0];
    dc_out[1] = offset[1];
    iqconv_stats(sum_level, sum_power, len, out_level, out_power);
}

void STARCH_IMPL(iqdata_conv_sc16, generic) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                         const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    int16_t * restrict out = (int16_t *) out_u;   // signed I/Q stored in the uint16_t FIFO buffer
    int16_t raw[2 * IQ_DC_BLOCK];
    int32_t offset[2] = { dc_in[0], dc_in[1] };
    uint64_t sum_level = 0, sum_power = 0;

    for (unsigned n = 0; n < len; n += IQ_DC_BLOCK) {
        unsigned nb = MIN(IQ_DC_BLOCK, len - n);
        iqconv_raw_sc16(in + n, raw, nb);
        iqconv_dc_block(raw, out + 2 * n, nb, offset, &sum_level, &sum_power);
    }

    dc_out[0] = offset[0];
    dc_out[1] = offset[1];
    iqconv_stats(sum_level, sum_power, len, out_level, out_power);
}

void STARCH_IMPL(iqdata_conv_sc16q11, generic) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                         const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    int16_t * restrict out = (int16_t *) out_u;   // signed I/Q stored in the uint16_t FIFO buffer
    int16_t raw[2 * IQ_DC_BLOCK];
    int32_t offset[2] = { dc_in[0], dc_in[1] };
    uint64_t sum_level = 0, sum_power = 0;

    for (unsigned n = 0; n < len; n += IQ_DC_BLOCK) {
        unsigned nb = MIN(IQ_DC_BLOCK, len - n);
        iqconv_raw_sc16q11(in + n, raw, nb);
        iqconv_dc_block(raw, out + 2 * n, nb, offset, &sum_level, &sum_power);
    }

    dc_out[0] = offset[0];
    dc_out[1] = offset[1];
    iqconv_stats(sum_level, sum_power, len, out_level, out_power);
}
