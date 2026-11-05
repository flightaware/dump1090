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
#include "dsp/helpers/demod2400x_coherent.h"

// 8-bit log power of one filter output (demod_log8)
static inline uint8_t getlog_generic(
    const int8_t *cv,
    const int16_t *ii,
    const int16_t *qq)
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

    si = (si + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;
    sq = (sq + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;
    return demod_log8((uint32_t)(si*si) + (uint32_t)(sq*sq));
}


// Upsample
int STARCH_IMPL(upsample12, generic) (const int16_t * restrict in, int len, uint8_t * restrict oo)
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

        oo[bpos] = getlog_generic(upsample_coeffs[0], ii, qq);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getlog_generic(upsample_coeffs[1], ii, qq);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getlog_generic(upsample_coeffs[2], ii, qq);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getlog_generic(upsample_coeffs[3], ii, qq);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getlog_generic(upsample_coeffs[4], ii, qq);
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
int STARCH_IMPL(locate_preamble, generic) (const uint8_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
{
    int stride = NU_STRIDE(nt);

    for (int j = 0; j < 6; j++) {

        const uint8_t *restrict p = pp + j * stride;
        uint8_t *restrict mp = mm + j;

        int plen = (nt + 5 - j) / 6;

        plen -= 2 * LONG_FRAME;

        for (int i = 0; i < plen - 15; i++) {

            unsigned minp =
                MIN(
                    MIN(p[0], p[2]),
                    MIN(p[7], p[9])
                );
            unsigned gap =
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

            unsigned v = 12 * minp > gap ? MIN((12 * minp - gap) >> 1, 255u) : 0;   // negative: 0
            *mp = v < thres ? 0 : (uint8_t) v;

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


// Decode bits from chip pairs (see demod2400x_dsp.h). Branch-free: on noise the bit
// values are random, and a branch on them mispredicts half the time.
void STARCH_IMPL(decode_bits, generic) (const uint8_t * restrict p, unsigned nbytes, uint8_t * restrict msg,
                                     unsigned * restrict quality)
{
    uint32_t qual = 0;

    for (unsigned i = 0; i < nbytes; i++) {           // 16 chips per byte

        unsigned bits = 0;
        for (int k = 0; k < 8; k++) {
            int d = p[2 * k + 1] - p[2 * k];
            bits = (bits << 1) | ((unsigned) d >> 31);   // 1 if the first is larger
            qual += (uint32_t) MIN(d < 0 ? -d : d, DECODE_BITS_CAP);
        }

        msg[i] = (uint8_t) bits;
        p += 16;
    }
    *quality = DECODE_BITS_QUALITY(qual, nbytes);
}


// Coherent decode (see demod2400x_coherent.h for the exact definition).
// This is the plain reference: the NEON and AVX2 versions must give exactly the
// same out[] and info[], so the order of every rounded step here is the definition.
typedef struct { int16_t re, im; } coh_c16;

// x * p for a Q15 phasor p; each product rounded separately, as the SIMD versions do
static inline coh_c16 coh_rot(coh_c16 x, coh_c16 p)
{
    coh_c16 r = { (int16_t) (coh_mulq15(x.re, p.re) - coh_mulq15(x.im, p.im)),
                  (int16_t) (coh_mulq15(x.re, p.im) + coh_mulq15(x.im, p.re)) };
    return r;
}

// One half of the frame, bits first .. first+55, on its own: chips and their shift,
// carrier frequency, channel h and rho2; if rho2 >= thres, slice its 56 bits into
// out[first/8 ..] and return 1 with the quality in *q
static int coh_half_generic(const int16_t *iq, int pos, int first, unsigned thres,
                            uint8_t *out, uint32_t *rho2_out, uint32_t *f_out, uint32_t *k_out, uint32_t *q)
{
    coh_c16 a[COH_HALF], b[COH_HALF], P[COH_HALF], Q[COH_HALF];

    // Chips: the largest sets the shift. One shift for the half keeps the relative
    // amplitudes, which the slicer compares
    int32_t r32[2 * COH_HALF], i32[2 * COH_HALF];
    uint32_t cmax = 0;
    for (int k = 0; k < 2 * COH_HALF; k++) {
        coh_chip(iq, pos + COH_DATA_OFFSET + 6 * (2 * first + k), &r32[k], &i32[k]);
        if (coh_abs(r32[k]) > cmax) cmax = coh_abs(r32[k]);
        if (coh_abs(i32[k]) > cmax) cmax = coh_abs(i32[k]);
    }
    const int cs = coh_shift(cmax, COH_CHIP_BITS);
    for (int i = 0; i < COH_HALF; i++) {
        a[i] = (coh_c16) { coh_chip16(r32[2 * i], cs), coh_chip16(i32[2 * i], cs) };
        b[i] = (coh_c16) { coh_chip16(r32[2 * i + 1], cs), coh_chip16(i32[2 * i + 1], cs) };
    }

    // Carrier sample of each bit: the chip with the larger magnitude, which holds the
    // pulse unless noise wins (a decision, but 3 dB less noise than a + b when right)
    uint8_t pick_a[COH_HALF];
    for (int i = 0; i < COH_HALF; i++)
        pick_a[i] = a[i].re * a[i].re + a[i].im * a[i].im >= b[i].re * b[i].re + b[i].im * b[i].im;

    // 2-bit block sums of the carrier samples, normalised below 2^COH_WIDE_BITS
    int32_t sre[COH_WBLK], sim[COH_WBLK];
    uint32_t bmax = 0;
    for (int m = 0; m < COH_WBLK; m++) {
        int32_t re = 0, im = 0;
        for (int i = m * COH_WBITS; i < (m + 1) * COH_WBITS; i++) {
            re += pick_a[i] ? a[i].re : b[i].re;
            im += pick_a[i] ? a[i].im : b[i].im;
        }
        sre[m] = re; sim[m] = im;
        if (coh_abs(re) > bmax) bmax = coh_abs(re);
        if (coh_abs(im) > bmax) bmax = coh_abs(im);
    }
    const int bs = coh_shift(bmax, COH_WIDE_BITS);
    int16_t wr[COH_WBLK], wi[COH_WBLK];
    for (int m = 0; m < COH_WBLK; m++) {
        wr[m] = (int16_t) (sre[m] >> bs);
        wi[m] = (int16_t) (sim[m] >> bs);
    }

    // Wide search, first stage: the peak of each 14-block piece over 16 wide bins
    int kc[2];
    for (int h = 0; h < 2; h++) {
        int32_t p1[COH_WBINS];
        for (int k = 0; k < COH_WBINS; k++) {
            int16_t re = 0, im = 0;
            for (int m = 0; m < COH_WBLK / 2; m++) {
                const coh_c16 r = coh_rot((coh_c16) { wr[h * COH_WBLK / 2 + m], wi[h * COH_WBLK / 2 + m] },
                                          (coh_c16) { coh_wide_re[m][k], coh_wide_im[m][k] });
                re = (int16_t) (re + r.re);
                im = (int16_t) (im + r.im);
            }
            p1[k] = re * re + im * im;
        }
        kc[h] = coh_argmax(p1, COH_WBINS);
    }

    // Second stage: around each peak, the central fine bins; the stronger peak wins
    int k = kc[0], f = 0;
    int32_t pbest = -1;
    for (int c = 0; c < (kc[1] != kc[0] ? 2 : 1); c++) {
        int16_t br8[COH_NBLOCKS], bi8[COH_NBLOCKS];
        coh_wide_regroup(wr, wi, kc[c], br8, bi8);
        int32_t p[COH_BINS] = { 0 };
        for (int j = COH_REF_LO; j < COH_REF_HI; j++) {
            int16_t re = 0, im = 0;
            for (int m = 0; m < COH_NBLOCKS; m++) {
                const coh_c16 r = coh_rot((coh_c16) { br8[m], bi8[m] }, (coh_c16) { coh_bin_re[m][j], coh_bin_im[m][j] });
                re = (int16_t) (re + r.re);
                im = (int16_t) (im + r.im);
            }
            p[j] = re * re + im * im;
        }
        const int best = coh_argmax(p, COH_BINS);
        if (p[best] > pbest) {
            pbest = p[best];
            k = kc[c];
            f = coh_fine_index(p, best);
        }
    }
    coh_c16 rc, rb;
    coh_steps(k, f, &rb.re, &rb.im, &rc.re, &rc.im);

    // Phasors per bit: P[i] = e^{-j 2 w i} de-rotates bit i, Q[i] its second chip
    // (half a bit later). Built in blocks of 8 so SIMD versions can do 8 at a time
    P[0] = (coh_c16) { 32767, 0 };
    for (int i = 1; i < 8; i++)
        P[i] = coh_rot(P[i - 1], rb);
    const coh_c16 rb8 = coh_rot(P[7], rb);
    for (int i = 8; i < COH_HALF; i++)
        P[i] = coh_rot(P[i - 8], rb8);
    for (int i = 0; i < COH_HALF; i++)
        Q[i] = coh_rot(rc, P[i]);

    // Correlation: with the rotation removed, the carrier samples of a coherent frame
    // all point the same way, so h (their sum) is the channel: its phase is the carrier
    // phase. rho2 compares |h|^2 with what a perfectly coherent half of the same energy
    // would give
    int32_t hre = 0, him = 0;
    uint32_t e2 = 0;
    for (int i = 0; i < COH_HALF; i++) {
        const coh_c16 c = pick_a[i] ? coh_rot(a[i], P[i]) : coh_rot(b[i], Q[i]);
        const int16_t cre = c.re, cim = c.im;
        hre += cre; him += cim;
        e2 += (uint32_t) ((cre >> 4) * (cre >> 4)) + (uint32_t) ((cim >> 4) * (cim >> 4));
    }
    const uint32_t rho2 = coh_rho2(hre, him, e2);
    *rho2_out = rho2;
    *f_out = (uint32_t) f;
    *k_out = (uint32_t) k;
    if (rho2 < thres)
        return 0;

    // Slice: projecting each de-rotated chip on h keeps the signal and only the noise
    // in phase with it; the pulse is in the chip with the larger projection
    int16_t gre, gim;
    coh_channel16(hre, him, &gre, &gim);
    uint32_t qual = 0, pw = 0;
    for (int i = 0; i < COH_HALF; i++) {
        const coh_c16 ra = coh_rot(a[i], P[i]), rbb = coh_rot(b[i], Q[i]);
        const int16_t ya = (int16_t) (coh_mulq15(ra.re, gre) + coh_mulq15(ra.im, gim));
        const int16_t yb = (int16_t) (coh_mulq15(rbb.re, gre) + coh_mulq15(rbb.im, gim));
        if (ya > yb)
            out[(first + i) >> 3] |= (uint8_t) (0x80 >> (i & 7));
        qual += coh_abs(ya - yb);
        pw += coh_abs(ya) + coh_abs(yb);
    }
    *q = coh_quality(qual, pw);
    return 1;
}

void STARCH_IMPL(coherent_decode, generic) (const int16_t * restrict iq, int pos, unsigned thres,
                                            uint8_t * restrict out, uint32_t * restrict info)
{
    memset(out, 0, 14);
    memset(info, 0, COH_INFO_N * sizeof(*info));

    if (!coh_half_generic(iq, pos, 0, thres, out, &info[COH_INFO_RHO2], &info[COH_INFO_FINE], &info[COH_INFO_WIDE], &info[COH_INFO_Q1]))
        return;
    info[COH_INFO_DECODED] = 1;
    // The second half whatever the DF says (as the magnitude decoder): scoring can
    // correct the DF bits, and every short DF is one bit from a long one
    coh_half_generic(iq, pos, COH_HALF, 0, out, &info[COH_INFO_RHO2B], &info[COH_INFO_FINE2], &info[COH_INFO_WIDE2], &info[COH_INFO_Q2]);
}
