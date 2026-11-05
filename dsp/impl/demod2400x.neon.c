// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.neon.c: DEMOD_2400X input conversion, upsampler and preamble search, ARM NEON.
// Built for both 32-bit (armv7a_neon_vfpv4) and 64-bit (armv8_neon_simd) ARM.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#ifndef __ARM_NEON
#  error Neon support not available
#endif

#include <arm_neon.h>

#include "dsp/helpers/demod2400x_dsp.h"
#include "dsp/helpers/demod2400x_coeffs.h"
#include "dsp/helpers/demod2400x_iqconv.h"
#include "dsp/helpers/demod2400x_coherent.h"

// Horizontal add of two vectors at once: returns [sum of a, sum of b]
static inline __attribute__((always_inline))
int32x2_t hsum2_s32x4_neon(const int32x4_t a, const int32x4_t b)
{
#ifdef __aarch64__
    int32x4_t ab = vpaddq_s32(a, b);                        // [a0+a1, a2+a3, b0+b1, b2+b3]
    return vpadd_s32(vget_low_s32(ab), vget_high_s32(ab));
#else
    return vpadd_s32(vadd_s32(vget_low_s32(a), vget_high_s32(a)),
                     vadd_s32(vget_low_s32(b), vget_high_s32(b)));
#endif
}


static inline __attribute__((always_inline)) uint8_t getlog_neon(const int8x8_t cv8, const int16x8x2_t iq)
{
    const int16x8_t cv = vmovl_s8(cv8);  // widen coefficients to 16-bit
    int32x4_t si, sq;

    si = vmull_s16(vget_low_s16(iq.val[0]), vget_low_s16(cv));
    si = vmlal_s16(si, vget_high_s16(iq.val[0]), vget_high_s16(cv));

    sq = vmull_s16(vget_low_s16(iq.val[1]), vget_low_s16(cv));
    sq = vmlal_s16(sq, vget_high_s16(iq.val[1]), vget_high_s16(cv));

    int32x2_t res = hsum2_s32x4_neon(si, sq);                  // [resi, resq]
    res = vrshr_n_s32(res, UPSAMPLE_SHIFT);                    // rounded shift on both

    uint32x2_t sqr = vreinterpret_u32_s32(vmul_s32(res, res)); // [resi^2, resq^2]
    return demod_log8(vget_lane_u32(sqr, 0) + vget_lane_u32(sqr, 1));
}

// Exact floor(sqrt(x)) for x < 2^31, same result as demod_isqrt()
static inline __attribute__((always_inline))
uint32x4_t isqrt_u32x4_neon(const uint32x4_t x)
{
    const float32x4_t f = vcvtq_f32_u32(x);
#ifdef __aarch64__
    // sqrt is correctly rounded, so after adding 2^-6 truncation is exact or one too large
    const uint32x4_t q = vcvtq_u32_f32(vaddq_f32(vsqrtq_f32(f), vdupq_n_f32(1.0f / 64)));
    return vaddq_u32(q, vcgtq_u32(vmulq_u32(q, q), x));
#else
    // Reciprocal square root estimate + one Newton step: within one either way
    // (worst error 0.57 for x < 2^31)
    float32x4_t r = vrsqrteq_f32(f);
    r = vmulq_f32(vrsqrtsq_f32(vmulq_f32(f, r), r), r);
    uint32x4_t q = vcvtq_u32_f32(vmulq_f32(f, r));       // x = 0 gives NaN, converted to 0
    q = vaddq_u32(q, vcgtq_u32(vmulq_u32(q, q), x));     // too large: -1
    const uint32x4_t q1 = vaddq_u32(q, vdupq_n_u32(1));
    return vsubq_u32(q, vcleq_u32(vmulq_u32(q1, q1), x)); // too small: +1
#endif
}

// demod_log8() of 4 filter outputs already rounded by UPSAMPLE_SHIFT, as bits >> 16 of the
// float of P + 1; log8_u8x8_neon() finishes 8 of them
static inline __attribute__((always_inline))
uint16x4_t log4_rounded_neon(int32x4_t si, int32x4_t sq)
{
    const uint32x4_t pwr = vreinterpretq_u32_s32(vmlaq_s32(vmulq_s32(si, si), sq, sq));
    return vshrn_n_u32(vreinterpretq_u32_f32(vcvtq_f32_u32(vaddq_u32(pwr, vdupq_n_u32(1)))), 16);
}

// (bits >> 20) - 127 * 8 = the low byte of bits >> 20, + 8 (L <= 247, and 127 * 8 = 3 * 256 + 248)
static inline __attribute__((always_inline))
uint8x8_t log8_u8x8_neon(uint16x4_t lo, uint16x4_t hi)
{
    return vadd_u8(vshrn_n_u16(vcombine_u16(lo, hi), 4), vdup_n_u8(8));
}

// Log power of 4 filter outputs, as getlog_neon()
static inline __attribute__((always_inline))
uint16x4_t getlog4_neon(int32x4_t si, int32x4_t sq)
{
    return log4_rounded_neon(vrshrq_n_s32(si, UPSAMPLE_SHIFT), vrshrq_n_s32(sq, UPSAMPLE_SHIFT));
}

// One phase of the upsampler for 8 consecutive windows; s[k] = I/Q at window + k
static inline __attribute__((always_inline))
uint8x8_t upsample_phase8_neon(const int16x8x2_t * restrict s, const int8_t * restrict coeffs)
{
    const int16x8_t cv = vmovl_s8(vld1_s8(coeffs));
    const int16x4_t c0 = vget_low_s16(cv);
    const int16x4_t c1 = vget_high_s16(cv);

    int32x4_t si_lo = vmull_lane_s16(vget_low_s16(s[0].val[0]), c0, 0);
    int32x4_t si_hi = vmull_lane_s16(vget_high_s16(s[0].val[0]), c0, 0);
    int32x4_t sq_lo = vmull_lane_s16(vget_low_s16(s[0].val[1]), c0, 0);
    int32x4_t sq_hi = vmull_lane_s16(vget_high_s16(s[0].val[1]), c0, 0);

#define TAP(k, c, l)                                                        \
    si_lo = vmlal_lane_s16(si_lo, vget_low_s16(s[k].val[0]), c, l);         \
    si_hi = vmlal_lane_s16(si_hi, vget_high_s16(s[k].val[0]), c, l);        \
    sq_lo = vmlal_lane_s16(sq_lo, vget_low_s16(s[k].val[1]), c, l);         \
    sq_hi = vmlal_lane_s16(sq_hi, vget_high_s16(s[k].val[1]), c, l)

    TAP(1, c0, 1); TAP(2, c0, 2); TAP(3, c0, 3);
    TAP(4, c1, 0); TAP(5, c1, 1); TAP(6, c1, 2); TAP(7, c1, 3);
#undef TAP

    return log8_u8x8_neon(getlog4_neon(si_lo, sq_lo), getlog4_neon(si_hi, sq_hi));
}

// Doubled filter outputs -> log power of 4 outputs, as getlog4_neon()
static inline __attribute__((always_inline))
uint16x4_t getlog4_x2_neon(int32x4_t si2, int32x4_t sq2)
{
    // rounded (2s) >> (UPSAMPLE_SHIFT + 1) == rounded s >> UPSAMPLE_SHIFT
    return log4_rounded_neon(vrshrq_n_s32(si2, UPSAMPLE_SHIFT + 1), vrshrq_n_s32(sq2, UPSAMPLE_SHIFT + 1));
}

// sum over k = 0..3 of x[k] * c[k], 8 windows -> lo and hi halves
#define MAC4(x, c, lo, hi)                                                  \
    do {                                                                    \
        lo = vmull_lane_s16(vget_low_s16((x)[0]), c, 0);                    \
        hi = vmull_lane_s16(vget_high_s16((x)[0]), c, 0);                   \
        lo = vmlal_lane_s16(lo, vget_low_s16((x)[1]), c, 1);                \
        hi = vmlal_lane_s16(hi, vget_high_s16((x)[1]), c, 1);               \
        lo = vmlal_lane_s16(lo, vget_low_s16((x)[2]), c, 2);                \
        hi = vmlal_lane_s16(hi, vget_high_s16((x)[2]), c, 2);               \
        lo = vmlal_lane_s16(lo, vget_low_s16((x)[3]), c, 3);                \
        hi = vmlal_lane_s16(hi, vget_high_s16((x)[3]), c, 3);               \
    } while (0)

// All 5 phases for 8 consecutive windows using the mirror symmetry (see demod2400x_coeffs.h);
// s[k] = I/Q at window + k, pc/mc from upsample_mirror_coeffs()
static inline __attribute__((always_inline))
void upsample_mirror8_neon(const int16x8x2_t * restrict s, const int16x4_t * restrict pc,
                           const int16x4_t * restrict mc, uint8x8_t * restrict ph)
{
    int16x8_t ui[4], vi[4], uq[4], vq[4];
    for (int k = 0; k < 4; k++) {
        ui[k] = vaddq_s16(s[k].val[0], s[7 - k].val[0]);
        vi[k] = vsubq_s16(s[k].val[0], s[7 - k].val[0]);
        uq[k] = vaddq_s16(s[k].val[1], s[7 - k].val[1]);
        vq[k] = vsubq_s16(s[k].val[1], s[7 - k].val[1]);
    }

    int32x4_t pi_lo, pi_hi, pq_lo, pq_hi, mi_lo, mi_hi, mq_lo, mq_hi;

    // Phases 0 and 4, then 1 and 3
    for (int c = 0; c < 2; c++) {
        MAC4(ui, pc[c], pi_lo, pi_hi);
        MAC4(uq, pc[c], pq_lo, pq_hi);
        MAC4(vi, mc[c], mi_lo, mi_hi);
        MAC4(vq, mc[c], mq_lo, mq_hi);
        ph[c] = log8_u8x8_neon(getlog4_x2_neon(vaddq_s32(pi_lo, mi_lo), vaddq_s32(pq_lo, mq_lo)),
                               getlog4_x2_neon(vaddq_s32(pi_hi, mi_hi), vaddq_s32(pq_hi, mq_hi)));
        ph[4 - c] = log8_u8x8_neon(getlog4_x2_neon(vsubq_s32(pi_lo, mi_lo), vsubq_s32(pq_lo, mq_lo)),
                                   getlog4_x2_neon(vsubq_s32(pi_hi, mi_hi), vsubq_s32(pq_hi, mq_hi)));
    }

    // Phase 2 (symmetric)
    MAC4(ui, pc[2], pi_lo, pi_hi);
    MAC4(uq, pc[2], pq_lo, pq_hi);
    ph[2] = log8_u8x8_neon(getlog4_x2_neon(pi_lo, pq_lo), getlog4_x2_neon(pi_hi, pq_hi));
}
#undef MAC4

#ifdef __aarch64__
// out[5*w + c] = ph[c][w], w = 0..7: bytes 8c + w of { ph[0] ph[1], ph[2] ph[3], ph[4] }
static const uint8_t interleave5_u8_tbl[3][16] = {
    {  0,  8, 16, 24, 32,  1,  9, 17, 25, 33,  2, 10, 18, 26, 34,  3 },
    { 11, 19, 27, 35,  4, 12, 20, 28, 36,  5, 13, 21, 29, 37,  6, 14 },
    { 22, 30, 38,  7, 15, 23, 31, 39,  0,  0,  0,  0,  0,  0,  0,  0 },
};

// out[5*w + c] = ph[c][w], w = 0..7 (writes 48 bytes, 40 used)
static inline __attribute__((always_inline))
void interleave5_u8x8_neon(const uint8x8_t * restrict ph, uint8_t * restrict out)
{
    uint8x16x3_t t;
    t.val[0] = vcombine_u8(ph[0], ph[1]);
    t.val[1] = vcombine_u8(ph[2], ph[3]);
    t.val[2] = vcombine_u8(ph[4], ph[4]);
    for (int v = 0; v < 3; v++)
        vst1q_u8(out + 16 * v, vqtbl3q_u8(t, vld1q_u8(interleave5_u8_tbl[v])));
}
#else
// out[5*w + c] = ph[c][w], w = 0..7 (writes 43 bytes, 40 used). ARMv7 has no 16-byte table
// lookup, so a partial transpose: vtrn.8 then vtrn.16 give [c0 c1 c2 c3] of windows k and
// k + 4 in one register; ph[4] shifted by 8k (as 32-bit lanes) puts c4 of the same windows at
// bytes 0 and 4, and vtrn.32 joins them into [c0 c1 c2 c3 c4 . . .] for each window. Each
// window's 8 bytes are stored at out + 5w in order, overwriting the previous one's 3 spare bytes
static inline __attribute__((always_inline))
void interleave5_u8x8_neon(const uint8x8_t * restrict ph, uint8_t * restrict out)
{
    const uint8x8x2_t x = vtrn_u8(ph[0], ph[1]);           // (c0, c1) of windows 0 2 4 6 / 1 3 5 7
    const uint8x8x2_t y = vtrn_u8(ph[2], ph[3]);           // (c2, c3)
    const uint16x4x2_t e = vtrn_u16(vreinterpret_u16_u8(x.val[0]), vreinterpret_u16_u8(y.val[0]));  // w 0|4, 2|6
    const uint16x4x2_t o = vtrn_u16(vreinterpret_u16_u8(x.val[1]), vreinterpret_u16_u8(y.val[1]));  // w 1|5, 3|7
    const uint32x2_t c4 = vreinterpret_u32_u8(ph[4]);
    const uint32x2x2_t w0 = vtrn_u32(vreinterpret_u32_u16(e.val[0]), c4);                   // windows 0, 4
    const uint32x2x2_t w1 = vtrn_u32(vreinterpret_u32_u16(o.val[0]), vshr_n_u32(c4, 8));    // 1, 5
    const uint32x2x2_t w2 = vtrn_u32(vreinterpret_u32_u16(e.val[1]), vshr_n_u32(c4, 16));   // 2, 6
    const uint32x2x2_t w3 = vtrn_u32(vreinterpret_u32_u16(o.val[1]), vshr_n_u32(c4, 24));   // 3, 7
    vst1_u8(out +  0, vreinterpret_u8_u32(w0.val[0]));
    vst1_u8(out +  5, vreinterpret_u8_u32(w1.val[0]));
    vst1_u8(out + 10, vreinterpret_u8_u32(w2.val[0]));
    vst1_u8(out + 15, vreinterpret_u8_u32(w3.val[0]));
    vst1_u8(out + 20, vreinterpret_u8_u32(w0.val[1]));
    vst1_u8(out + 25, vreinterpret_u8_u32(w1.val[1]));
    vst1_u8(out + 30, vreinterpret_u8_u32(w2.val[1]));
    vst1_u8(out + 35, vreinterpret_u8_u32(w3.val[1]));
}
#endif

// All 5 phases for the 8 windows at in[2 * (0..14)]
static inline __attribute__((always_inline))
void upsample8_neon(const int16_t * restrict p, int mirrored, const int16x4_t * restrict pc,
                    const int16x4_t * restrict mc, uint8x8_t * restrict ph)
{
    int16x8x2_t s[8];
    for (int k = 0; k < 8; k++)
        s[k] = vld2q_s16(p + 2 * k);

    if (mirrored) {
        upsample_mirror8_neon(s, pc, mc, ph);
    } else {
        for (int c = 0; c < 5; c++)
            ph[c] = upsample_phase8_neon(s, upsample_coeffs[c]);
    }
}

#define UPSAMPLE_BLOCK 48 // Windows per block, multiple of 48: 5*48 outputs = 40 per phase

// Upsample
int STARCH_IMPL(upsample12, neon) (const int16_t * restrict in, int len, uint8_t * restrict oo)
{
   if (len < 8) return 0;

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int nw = len - 7; // Windows: 5 outputs each, output o goes to oo[(o % 6) * stride + o / 6]
   int n = 0;

   // Mirror-symmetric phases (see demod2400x_coeffs.h): half the multiplies
   const int mirrored = upsample_coeffs_mirrored();
   int16_t pc4[3][4], mc4[3][4];
   upsample_mirror_coeffs(pc4, mc4);
   int16x4_t pc[3], mc[3];
   for (int c = 0; c < 3; c++) {
       pc[c] = vld1_s16(pc4[c]);
       mc[c] = vld1_s16(mc4[c]);
   }

   // 8 windows at a time for each phase, a block at a time so the outputs
   // can be put in order then split into the 6 phases with vector loads/stores
   for (; n + UPSAMPLE_BLOCK <= nw; n += UPSAMPLE_BLOCK) {
       uint8_t tmp[5 * UPSAMPLE_BLOCK + 8] __attribute__((aligned(16))); // tmp[o - 5*n]

       for (int w = 0; w < UPSAMPLE_BLOCK; w += 8) {
           uint8x8_t ph[5];
           upsample8_neon(in + 2 * (n + w), mirrored, pc, mc, ph);
           interleave5_u8x8_neon(ph, tmp + 5 * w);
       }

       // 5*n is a multiple of 6, so tmp[6*m + r] -> oo[r * stride + 5*n/6 + m]
       uint8_t * restrict op = oo + 5 * n / 6;
       for (int m = 0; m < 5 * UPSAMPLE_BLOCK / 6; m += 8) {
           const uint16x8x3_t a = vld3q_u16((const uint16_t *) (tmp + 6 * m));      // byte pairs (r, r+1)
           for (int r = 0; r < 3; r++) {
               vst1_u8(op + (2 * r) * stride + m, vmovn_u16(a.val[r]));
               vst1_u8(op + (2 * r + 1) * stride + m, vshrn_n_u16(a.val[r], 8));
           }
       }
   }

   // Remainder
   for (; n < nw; n++) {
       const int16x8x2_t iq = vld2q_s16(in + 2 * n);
       for (int c = 0; c < 5; c++) {
           const int o = 5 * n + c;
           oo[(o % 6) * stride + o / 6] = getlog_neon(vld1_s8(upsample_coeffs[c]), iq);
       }
   }

   return nt;
}


// Scalar preamble metric at p[0], 0 if below thres
static inline __attribute__((always_inline))
uint8_t preamble_metric1(const uint8_t * restrict p, unsigned thres)
{
    unsigned minp = MIN(MIN(p[0], p[2]), MIN(p[7], p[9]));
    unsigned gap = p[1] + p[3] + p[4] + p[5] + p[6] + p[8] +
                   p[10] + p[11] + p[12] + p[13] + p[14] + p[15];

    unsigned v = 12 * minp > gap ? MIN((12 * minp - gap) >> 1, 255u) : 0;
    return v < thres ? 0 : (uint8_t) v;
}

#define PREAMBLE_BLOCK 64 // Positions per phase per block, multiple of 16

// Widened sum of 4 consecutive samples, p[0..3] .. p[15..18]: lanes 0-7 and 8-15
static inline __attribute__((always_inline))
uint16x8x2_t sum4_u8x16_neon(const uint8_t * restrict p)
{
    const uint8x16_t a = vld1q_u8(p + 0), b = vld1q_u8(p + 1);
    const uint8x16_t c = vld1q_u8(p + 2), d = vld1q_u8(p + 3);
    uint16x8x2_t s;
    s.val[0] = vaddq_u16(vaddl_u8(vget_low_u8(a), vget_low_u8(b)), vaddl_u8(vget_low_u8(c), vget_low_u8(d)));
    s.val[1] = vaddq_u16(vaddl_u8(vget_high_u8(a), vget_high_u8(b)), vaddl_u8(vget_high_u8(c), vget_high_u8(d)));
    return s;
}

// (12 * minp - gap) >> 1 saturated to 0..255, 0 below thres, for 8 positions
static inline __attribute__((always_inline))
uint8x8_t preamble_metric8_neon(const uint8x8_t minp, const uint16x8_t gap, const uint8x8_t vthres)
{
    const uint16x8_t v = vqsubq_u16(vmull_u8(minp, vdup_n_u8(12)), gap);   // negative: 0
    const uint8x8_t m = vqshrn_n_u16(v, 1);
    return vand_u8(m, vcge_u8(m, vthres));
}

// Preamble metrics for 16*n16 consecutive positions of one phase
static inline __attribute__((always_inline))
void preamble_metric_block_neon(const uint8_t * restrict p, int n16, unsigned thres, uint8_t * restrict out)
{
    // Preamble detection
    // P G P G, G G G P, G P G G, G G G G
    // gap = p[1] + p[8] + p[10] + p[15] + s4[3] + s4[11], s4[x] = p[x] + .. + p[x+3]
    // s4[11] for these 16 positions is s4[3] lanes 8-15, then lanes 0-7 of the next 16

    const uint8x8_t vthres = vdup_n_u8((uint8_t) thres);
    uint16x8x2_t s4a = sum4_u8x16_neon(p + 3);

    for (int k = 0; k < 16 * n16; k += 16, p += 16) {
        const uint16x8x2_t s4n = sum4_u8x16_neon(p + 19);       // s4[3] of the next 16

        const uint8x16_t v1 = vld1q_u8(p + 1), v8 = vld1q_u8(p + 8);
        const uint8x16_t v10 = vld1q_u8(p + 10), v15 = vld1q_u8(p + 15);
        const uint16x8_t gap_lo = vaddq_u16(vaddq_u16(vaddl_u8(vget_low_u8(v1), vget_low_u8(v8)),
                                                      vaddl_u8(vget_low_u8(v10), vget_low_u8(v15))),
                                            vaddq_u16(s4a.val[0], s4a.val[1]));
        const uint16x8_t gap_hi = vaddq_u16(vaddq_u16(vaddl_u8(vget_high_u8(v1), vget_high_u8(v8)),
                                                      vaddl_u8(vget_high_u8(v10), vget_high_u8(v15))),
                                            vaddq_u16(s4a.val[1], s4n.val[0]));
        s4a = s4n;

        const uint8x16_t m = vminq_u8(vminq_u8(vld1q_u8(p + 0), vld1q_u8(p + 2)),
                                      vminq_u8(vld1q_u8(p + 7), vld1q_u8(p + 9)));
        vst1q_u8(out + k, vcombine_u8(preamble_metric8_neon(vget_low_u8(m), gap_lo, vthres),
                                      preamble_metric8_neon(vget_high_u8(m), gap_hi, vthres)));
    }
}

// Locate preamble
int STARCH_IMPL(locate_preamble, neon) (const uint8_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
{
   int stride = NU_STRIDE(nt); // Stride
   int count[6];               // Number of positions to evaluate in each phase
   int cmin = 0x7fffffff;

   for (int j = 0; j < 6; j++) {
       int plen = (nt + 5 - j) / 6; // Length of each phase
       plen -= 2 * LONG_FRAME;      // No point in searching at end
       count[j] = plen - 15;
       cmin = MIN(cmin, count[j]);
   }

   // All phases together, a block at a time, so the outputs can be interleaved on store
   int i = 0;
   while (i + 16 <= cmin) {
       const int n16 = MIN((cmin - i) / 16, PREAMBLE_BLOCK / 16);
       uint8_t t[6][PREAMBLE_BLOCK];

       for (int j = 0; j < 6; j++)
           preamble_metric_block_neon(&pp[j*stride + i], n16, thres, t[j]);

       // mm[6*(i+k) + j] = t[j][k]: interleave phases in pairs, then 3-way as 16-bit
       uint8_t * restrict mp = &mm[6*i];
       for (int k = 0; k < 16 * n16; k += 8, mp += 48) {
           const uint8x8x2_t z01 = vzip_u8(vld1_u8(t[0] + k), vld1_u8(t[1] + k));
           const uint8x8x2_t z23 = vzip_u8(vld1_u8(t[2] + k), vld1_u8(t[3] + k));
           const uint8x8x2_t z45 = vzip_u8(vld1_u8(t[4] + k), vld1_u8(t[5] + k));
           uint16x8x3_t o;
           o.val[0] = vreinterpretq_u16_u8(vcombine_u8(z01.val[0], z01.val[1]));
           o.val[1] = vreinterpretq_u16_u8(vcombine_u8(z23.val[0], z23.val[1]));
           o.val[2] = vreinterpretq_u16_u8(vcombine_u8(z45.val[0], z45.val[1]));
           vst3q_u16((uint16_t *) mp, o);
       }
       i += 16 * n16;
   }

   // Remainder
   for (int j = 0; j < 6; j++)
       for (int k = i; k < count[j]; k++)
           mm[6*k + j] = preamble_metric1(&pp[j*stride + k], thres);

   return METRIC_SIZE(nt);
}


// Input conversion, as the generic iqdata_conv_* (dsp/helpers/demod2400x_iqconv.h)

// 16 raw samples as 12-bit I and Q (samples 0-7 in [0], 8-15 in [1])
typedef struct {
    int16x8_t i[2];
    int16x8_t q[2];
} iq16x16_neon_t;

static inline __attribute__((always_inline)) iq16x16_neon_t iqconv_load_uc8_neon(const uc8_t *in)
{
    const uint8x16x2_t v = vld2q_u8((const uint8_t *) in);    // I0-15, Q0-15
    const int16x8_t c = vdupq_n_s16(2040);
    iq16x16_neon_t r;
    // (2 * v - 255) * 8 = 16 * v - 2040
    r.i[0] = vsubq_s16(vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(v.val[0]), 4)), c);
    r.i[1] = vsubq_s16(vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(v.val[0]), 4)), c);
    r.q[0] = vsubq_s16(vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(v.val[1]), 4)), c);
    r.q[1] = vsubq_s16(vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(v.val[1]), 4)), c);
    return r;
}

static inline __attribute__((always_inline)) iq16x16_neon_t iqconv_load_sc16_neon(const sc16_t *in)
{
    const int16x8x2_t a = vld2q_s16((const int16_t *) in);
    const int16x8x2_t b = vld2q_s16((const int16_t *) (in + 8));
    iq16x16_neon_t r;
    r.i[0] = vshrq_n_s16(a.val[0], 4);
    r.i[1] = vshrq_n_s16(b.val[0], 4);
    r.q[0] = vshrq_n_s16(a.val[1], 4);
    r.q[1] = vshrq_n_s16(b.val[1], 4);
    return r;
}

static inline __attribute__((always_inline)) int16x8_t clamp12_s16x8_neon(const int16x8_t v)
{
    return vmaxq_s16(vminq_s16(v, vdupq_n_s16(2047)), vdupq_n_s16(-2048));
}

static inline __attribute__((always_inline)) iq16x16_neon_t iqconv_load_sc16q11_neon(const sc16_t *in)
{
    const int16x8x2_t a = vld2q_s16((const int16_t *) in);
    const int16x8x2_t b = vld2q_s16((const int16_t *) (in + 8));
    iq16x16_neon_t r;
    r.i[0] = clamp12_s16x8_neon(a.val[0]);
    r.i[1] = clamp12_s16x8_neon(b.val[0]);
    r.q[0] = clamp12_s16x8_neon(a.val[1]);
    r.q[1] = clamp12_s16x8_neon(b.val[1]);
    return r;
}

// Same as iqconv_dc_block() for a full block. off = [I, Q] offsets; the block sum
// of (raw - corr) is sum(raw) - 16 * corr, so the serial part is just a few ops.
static inline __attribute__((always_inline))
void iqconv_block_neon(const iq16x16_neon_t r, int16_t * restrict out, int32x2_t *off,
                       uint64x2_t *acc_level, uint64x2_t *acc_power)
{
    const int32x2_t corr = vrshr_n_s32(*off, IQ_DC_SHIFT);              // (off + 2048) >> 12
    const int32x2_t sums = hsum2_s32x4_neon(vpaddlq_s16(vaddq_s16(r.i[0], r.i[1])),
                                            vpaddlq_s16(vaddq_s16(r.q[0], r.q[1])));
    *off = vadd_s32(*off, vsub_s32(sums, vshl_n_s32(corr, 4)));

    const int16x4_t c16 = vmovn_s32(vcombine_s32(corr, corr));          // [ci, cq, ci, cq]
    const int16x8_t ci = vdupq_lane_s16(c16, 0);
    const int16x8_t cq = vdupq_lane_s16(c16, 1);

    uint32x4_t level = vdupq_n_u32(0), power = vdupq_n_u32(0);
    for (int h = 0; h < 2; ++h) {
        int16x8x2_t x;
        x.val[0] = clamp12_s16x8_neon(vsubq_s16(r.i[h], ci));
        x.val[1] = clamp12_s16x8_neon(vsubq_s16(r.q[h], cq));
        vst2q_s16(out + 16 * h, x);

        const uint32x4_t m0 = vreinterpretq_u32_s32(
            vmlal_s16(vmull_s16(vget_low_s16(x.val[0]), vget_low_s16(x.val[0])), vget_low_s16(x.val[1]), vget_low_s16(x.val[1])));
        const uint32x4_t m1 = vreinterpretq_u32_s32(
            vmlal_s16(vmull_s16(vget_high_s16(x.val[0]), vget_high_s16(x.val[0])), vget_high_s16(x.val[1]), vget_high_s16(x.val[1])));
        power = vaddq_u32(power, vaddq_u32(m0, m1));
        level = vaddq_u32(level, vaddq_u32(isqrt_u32x4_neon(vshlq_n_u32(m0, IQ_LEVEL_SHIFT)),
                                           isqrt_u32x4_neon(vshlq_n_u32(m1, IQ_LEVEL_SHIFT))));
    }

    *acc_level = vpadalq_u32(*acc_level, level);
    *acc_power = vpadalq_u32(*acc_power, power);
}

#define IQDATA_CONV_NEON_BODY(fmt)                                                            \
    int16_t * restrict out = (int16_t *) out_u;                                               \
    int32x2_t off = vld1_s32(dc_in);                                                      \
    uint64x2_t acc_level = vdupq_n_u64(0), acc_power = vdupq_n_u64(0);                        \
    unsigned n = 0;                                                                           \
                                                                                              \
    for (; n + IQ_DC_BLOCK <= len; n += IQ_DC_BLOCK)                                          \
        iqconv_block_neon(iqconv_load_##fmt##_neon(in + n), out + 2 * n, &off, &acc_level, &acc_power); \
                                                                                              \
    vst1_s32(dc_out, off);                                                                 \
    uint64_t sum_level = vgetq_lane_u64(acc_level, 0) + vgetq_lane_u64(acc_level, 1);         \
    uint64_t sum_power = vgetq_lane_u64(acc_power, 0) + vgetq_lane_u64(acc_power, 1);         \
                                                                                              \
    if (n < len) {                                                                            \
        int16_t raw[2 * IQ_DC_BLOCK];                                                         \
        iqconv_raw_##fmt(in + n, raw, len - n);                                               \
        iqconv_dc_block(raw, out + 2 * n, len - n, dc_out, &sum_level, &sum_power);        \
    }                                                                                         \
                                                                                              \
    iqconv_stats(sum_level, sum_power, len, out_level, out_power);

void STARCH_IMPL(iqdata_conv_uc8, neon) (const uc8_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                         const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_NEON_BODY(uc8)
}

void STARCH_IMPL(iqdata_conv_sc16, neon) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                          const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_NEON_BODY(sc16)
}

void STARCH_IMPL(iqdata_conv_sc16q11, neon) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                             const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_NEON_BODY(sc16q11)
}


// Decode bits from chip pairs (see demod2400x_dsp.h): one byte = 8 pairs per vld2
void STARCH_IMPL(decode_bits, neon) (const uint8_t * restrict p, unsigned nbytes, uint8_t * restrict msg,
                                     unsigned * restrict quality)
{
    static const uint8_t weights[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
    const uint8x8_t w = vld1_u8(weights);
    uint32x2_t qual = vdup_n_u32(0);

    for (unsigned i = 0; i < nbytes; i++) {
        const uint8x8x2_t ab = vld2_u8(p + 16 * i);       // val[0] = first chips, val[1] = second
        const uint8x8_t bits = vand_u8(vcgt_u8(ab.val[0], ab.val[1]), w);
#ifdef __aarch64__
        msg[i] = vaddv_u8(bits);
#else
        uint8x8_t b8 = vpadd_u8(bits, bits);
        b8 = vpadd_u8(b8, b8);
        msg[i] = vget_lane_u8(vpadd_u8(b8, b8), 0);
#endif
        qual = vpadal_u16(qual, vpaddl_u8(vmin_u8(vabd_u8(ab.val[0], ab.val[1]), vdup_n_u8(DECODE_BITS_CAP))));
    }

#ifdef __aarch64__
    const uint32_t q = vaddv_u32(qual);
#else
    const uint32_t q = vget_lane_u32(vpadd_u32(qual, qual), 0);
#endif
    *quality = DECODE_BITS_QUALITY(q, nbytes);
}


// Coherent decode (see demod2400x_coherent.h for the exact definition), 8 lanes at a time.
// vqrdmulh(a, b) = sat((2ab + 2^15) >> 16) = (ab + 2^14) >> 15 = coh_mulq15(a, b); it only
// saturates for -32768 * -32768, which can't occur (chips < 2^13, phasors within +-32767).
// Integer sums are exact, so adding lanes in a different order gives the same result.

static inline __attribute__((always_inline)) int32_t coh_hsum_s32_neon(int32x4_t v)
{
#ifdef __aarch64__
    return vaddvq_s32(v);
#else
    int32x2_t s = vadd_s32(vget_low_s32(v), vget_high_s32(v));
    return vget_lane_s32(vpadd_s32(s, s), 0);
#endif
}

static inline __attribute__((always_inline)) uint32_t coh_hsum_u32_neon(uint32x4_t v)
{
#ifdef __aarch64__
    return vaddvq_u32(v);
#else
    uint32x2_t s = vadd_u32(vget_low_u32(v), vget_high_u32(v));
    return vget_lane_u32(vpadd_u32(s, s), 0);
#endif
}

static inline __attribute__((always_inline)) int32_t coh_hsum_s16_neon(int16x8_t v)
{
    return coh_hsum_s32_neon(vpaddlq_s16(v));
}

// 8 compare results (all ones / zero) -> one byte, lane 0 in the top bit
static inline __attribute__((always_inline)) uint8_t coh_bits8_neon(uint16x8_t mask)
{
    static const uint16_t weights[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
    const uint16x8_t b = vandq_u16(mask, vld1q_u16(weights));
#ifdef __aarch64__
    return (uint8_t) vaddvq_u16(b);
#else
    uint16x4_t s = vadd_u16(vget_low_u16(b), vget_high_u16(b));
    s = vpadd_u16(s, s);
    return (uint8_t) vget_lane_u16(vpadd_u16(s, s), 0);
#endif
}

// x * p (complex, Q15 phasor p), 8 lanes
static inline __attribute__((always_inline))
void coh_rot_neon(int16x8_t xr, int16x8_t xi, int16x8_t pr, int16x8_t pi, int16x8_t *rr, int16x8_t *ri)
{
    *rr = vsubq_s16(vqrdmulhq_s16(xr, pr), vqrdmulhq_s16(xi, pi));
    *ri = vaddq_s16(vqrdmulhq_s16(xr, pi), vqrdmulhq_s16(xi, pr));
}

// One chip's partial sums, I/Q kept interleaved: 8 complex taps from p loaded as they
// are and multiplied by the interleaved coefficients c = [c0, c0, c1, c1, ..] (c[0] taps
// 0..3, c[1] taps 4..7), giving lanes [I, Q, I, Q] (each I and Q split over two lanes)
static inline __attribute__((always_inline))
int32x4_t coh_chip_partial_neon(const int16_t *p, const int16x8_t *c)
{
    const int16x8_t v0 = vld1q_s16(p), v1 = vld1q_s16(p + 8);
    int32x4_t s = vmull_s16(vget_low_s16(v0), vget_low_s16(c[0]));
    s = vmlal_s16(s, vget_high_s16(v0), vget_high_s16(c[0]));
    s = vmlal_s16(s, vget_low_s16(v1), vget_low_s16(c[1]));
    return vmlal_s16(s, vget_high_s16(v1), vget_high_s16(c[1]));
}

// Chips of the 56 bits of a half, int32 I/Q interleaved, in chip order (a, b of each
// bit): c32[2k] = I, c32[2k+1] = Q of chip k; returns the largest |I| or |Q|.
// Chip k is at 12 MHz position t0 + 6k, and 6k mod 5 = k mod 5, so chips j, j+5, j+10, ..
// share one tap set, kept in registers, and are 6 samples apart. Each chip's partial sums
// fold to [I, Q] with a 64-bit add, with no moves out of the NEON registers (which stall
// an in-order core such as the Cortex-A53)
static uint32_t coh_chips32_neon(const int16_t *iq, int pos, const int16x8_t (*ci)[2], int32_t *c32)
{
    uint32x2_t mx = vdup_n_u32(0);
    for (int j = 0; j < 5; j++) {
        const int t = pos + COH_DATA_OFFSET + 6 * j;
        const int16x8_t *c = ci[t % 5];
        const int16_t *p = iq + 2 * (t / 5);
        int k = j;
        // 4 chips at a time, step by step, so an in-order core overlaps their latencies
        for (; k + 15 < 2 * COH_HALF; k += 20, p += 48) {
            // the 4 windows (12 int16 apart, p .. p + 51) from 7 loads, 3 parts by vext
            // (none reads past the last window)
            const int16x8_t a0 = vld1q_s16(p), a1 = vld1q_s16(p + 8), q2 = vld1q_s16(p + 16);
            const int16x8_t d0 = vld1q_s16(p + 24), d1 = vld1q_s16(p + 32), q5 = vld1q_s16(p + 40), e1 = vld1q_s16(p + 44);
            const int16x8_t b0 = vextq_s16(a1, q2, 4), b1 = vextq_s16(q2, d0, 4), e0 = vextq_s16(d1, q5, 4);
            int32x4_t sa = vmull_s16(vget_low_s16(a0), vget_low_s16(c[0]));
            int32x4_t sb = vmull_s16(vget_low_s16(b0), vget_low_s16(c[0]));
            int32x4_t sd = vmull_s16(vget_low_s16(d0), vget_low_s16(c[0]));
            int32x4_t se = vmull_s16(vget_low_s16(e0), vget_low_s16(c[0]));
            sa = vmlal_s16(sa, vget_high_s16(a0), vget_high_s16(c[0]));
            sb = vmlal_s16(sb, vget_high_s16(b0), vget_high_s16(c[0]));
            sd = vmlal_s16(sd, vget_high_s16(d0), vget_high_s16(c[0]));
            se = vmlal_s16(se, vget_high_s16(e0), vget_high_s16(c[0]));
            sa = vmlal_s16(sa, vget_low_s16(a1), vget_low_s16(c[1]));
            sb = vmlal_s16(sb, vget_low_s16(b1), vget_low_s16(c[1]));
            sd = vmlal_s16(sd, vget_low_s16(d1), vget_low_s16(c[1]));
            se = vmlal_s16(se, vget_low_s16(e1), vget_low_s16(c[1]));
            sa = vmlal_s16(sa, vget_high_s16(a1), vget_high_s16(c[1]));
            sb = vmlal_s16(sb, vget_high_s16(b1), vget_high_s16(c[1]));
            sd = vmlal_s16(sd, vget_high_s16(d1), vget_high_s16(c[1]));
            se = vmlal_s16(se, vget_high_s16(e1), vget_high_s16(c[1]));
            const int32x2_t va = vadd_s32(vget_low_s32(sa), vget_high_s32(sa)), vb = vadd_s32(vget_low_s32(sb), vget_high_s32(sb));
            const int32x2_t vd = vadd_s32(vget_low_s32(sd), vget_high_s32(sd)), ve = vadd_s32(vget_low_s32(se), vget_high_s32(se));
            vst1_s32(c32 + 2 * k, va);
            vst1_s32(c32 + 2 * k + 10, vb);
            vst1_s32(c32 + 2 * k + 20, vd);
            vst1_s32(c32 + 2 * k + 30, ve);
            mx = vmax_u32(mx, vmax_u32(vreinterpret_u32_s32(vabs_s32(va)), vreinterpret_u32_s32(vabs_s32(vb))));
            mx = vmax_u32(mx, vmax_u32(vreinterpret_u32_s32(vabs_s32(vd)), vreinterpret_u32_s32(vabs_s32(ve))));
        }
        for (; k < 2 * COH_HALF; k += 5, p += 12) {
            const int32x4_t s = coh_chip_partial_neon(p, c);
            const int32x2_t v = vadd_s32(vget_low_s32(s), vget_high_s32(s));
            vst1_s32(c32 + 2 * k, v);
            mx = vmax_u32(mx, vreinterpret_u32_s32(vabs_s32(v)));
        }
    }
    return vget_lane_u32(vpmax_u32(mx, mx), 0);
}

// int32 chips of bits 0 .. n-1 -> int16 a/b (shift, clamp), 4 bits at a time. The
// shift (coh_shift of the largest |I| or |Q|) leaves every value in -2^13 .. 2^13 - 1,
// so it narrows to int16 first, and only -2^13 needs the clamp to -(2^13 - 1);
// [aI aQ bI bQ] of 4 bits are then transposed with vtrn (no vld4)
static void coh_chips16_neon(const int32_t *c32, int n, int shift,
                             int16_t *ar, int16_t *ai, int16_t *br, int16_t *bi)
{
    const int32x4_t sh = vdupq_n_s32(-shift);
    const int16x8_t nlim = vdupq_n_s16(-((1 << COH_CHIP_BITS) - 1));
    for (int i = 0; i < n; i += 4) {
#define COH_N2(k) vmaxq_s16(vcombine_s16(vmovn_s32(vshlq_s32(vld1q_s32(c32 + 4 * i + 8 * (k)), sh)), \
                                                   vmovn_s32(vshlq_s32(vld1q_s32(c32 + 4 * i + 8 * (k) + 4), sh))), nlim)
        const int16x8_t x01 = COH_N2(0), x23 = COH_N2(1);      // bits i, i+1 and i+2, i+3
#undef COH_N2
        const int16x4x2_t t01 = vtrn_s16(vget_low_s16(x01), vget_high_s16(x01));  // [aI0 aI1 bI0 bI1], [aQ0 aQ1 bQ0 bQ1]
        const int16x4x2_t t23 = vtrn_s16(vget_low_s16(x23), vget_high_s16(x23));
        const int32x2x2_t re = vtrn_s32(vreinterpret_s32_s16(t01.val[0]), vreinterpret_s32_s16(t23.val[0]));
        const int32x2x2_t im = vtrn_s32(vreinterpret_s32_s16(t01.val[1]), vreinterpret_s32_s16(t23.val[1]));
        vst1_s16(ar + i, vreinterpret_s16_s32(re.val[0]));
        vst1_s16(br + i, vreinterpret_s16_s32(re.val[1]));
        vst1_s16(ai + i, vreinterpret_s16_s32(im.val[0]));
        vst1_s16(bi + i, vreinterpret_s16_s32(im.val[1]));
    }
}

// One half of the frame, bits first .. first+55, on its own (as coh_half_generic)
// coh_wide_regroup, 8 blocks at a time: rotate by wide bin k (transposed table), sum
// each 4 with pairwise adds (int16, wrapping, so any order gives the same), >> 2
static inline __attribute__((always_inline))
void coh_wide_regroup_neon(const int16_t *wr, const int16_t *wi, int k, int16_t *br, int16_t *bi)
{
    for (int q = 0; q < 32; q += 8) {
        int16x8_t rr, ri;
        coh_rot_neon(vld1q_s16(wr + q), vld1q_s16(wi + q), vld1q_s16(&coh_widet_re[k][q]), vld1q_s16(&coh_widet_im[k][q]), &rr, &ri);
        const int16x4_t pr = vpadd_s16(vget_low_s16(rr), vget_high_s16(rr));
        const int16x4_t pi = vpadd_s16(vget_low_s16(ri), vget_high_s16(ri));
        const int16x4_t sr = vshr_n_s16(vpadd_s16(pr, pr), 2), si = vshr_n_s16(vpadd_s16(pi, pi), 2);
        vst1_lane_s32((int32_t *) (br + q / 4), vreinterpret_s32_s16(sr), 0);
        vst1_lane_s32((int32_t *) (bi + q / 4), vreinterpret_s32_s16(si), 0);
    }
}

// One block of a bin search: block m (lane l of the int16x4 pair wr, wi) rotated by
// the 8 bins j.. of table t (t##_re/im[m][j]) and added to the accumulators. A lane
// multiply instead of a broadcast; vqrdmulh is symmetric, so the products are those of
// coh_rot_neon, and the int16 adds wrap, so splitting them over two accumulators
// gives the same sums
#define COH_LROT(t, j, accr, acci, wr, wi, l, m) do { \
        const int16x8_t _tr = vld1q_s16(&t##_re[m][j]), _ti = vld1q_s16(&t##_im[m][j]); \
        accr = vaddq_s16(accr, vsubq_s16(vqrdmulhq_lane_s16(_tr, wr, l), vqrdmulhq_lane_s16(_ti, wi, l))); \
        acci = vaddq_s16(acci, vaddq_s16(vqrdmulhq_lane_s16(_ti, wr, l), vqrdmulhq_lane_s16(_tr, wi, l))); \
    } while (0)

static int coh_half_neon(const int16_t *iq, int pos, int first, unsigned thres,
                         const int16x8_t (*ci)[2], uint8_t *out, uint32_t *rho2_out, uint32_t *f_out, uint32_t *k_out, uint32_t *q)
{
    int32_t c32[4 * COH_HALF] __attribute__((aligned(16)));
    int16_t ar[COH_HALF], ai[COH_HALF], br[COH_HALF], bi[COH_HALF];
    int16_t Pr[COH_HALF], Pi[COH_HALF], Qr[COH_HALF], Qi[COH_HALF];

    // Chips and their shift
    const uint32_t cmax = coh_chips32_neon(iq, pos + 12 * first, ci, c32);
    const int cs = coh_shift(cmax, COH_CHIP_BITS);
    coh_chips16_neon(c32, COH_HALF, cs, ar, ai, br, bi);

    // Carrier sample of each bit: the chip with the larger magnitude (|a|^2 >= |b|^2,
    // int32), as a 16-bit lane mask per bit; then 2-bit block sums of the samples, normalised
    uint16x8_t pick_a[COH_NBLOCKS];
    int32_t sre[COH_WBLK] __attribute__((aligned(16))), sim[COH_WBLK] __attribute__((aligned(16)));
    uint32x4_t mxb = vdupq_n_u32(0);
    for (int m = 0; m < COH_NBLOCKS; m++) {
        const int i = m * COH_BLOCK;
        const int16x8_t xar = vld1q_s16(ar + i), xai = vld1q_s16(ai + i), xbr = vld1q_s16(br + i), xbi = vld1q_s16(bi + i);
        const int32x4_t pal = vmlal_s16(vmull_s16(vget_low_s16(xar), vget_low_s16(xar)), vget_low_s16(xai), vget_low_s16(xai));
        const int32x4_t pah = vmlal_s16(vmull_s16(vget_high_s16(xar), vget_high_s16(xar)), vget_high_s16(xai), vget_high_s16(xai));
        const int32x4_t pbl = vmlal_s16(vmull_s16(vget_low_s16(xbr), vget_low_s16(xbr)), vget_low_s16(xbi), vget_low_s16(xbi));
        const int32x4_t pbh = vmlal_s16(vmull_s16(vget_high_s16(xbr), vget_high_s16(xbr)), vget_high_s16(xbi), vget_high_s16(xbi));
        pick_a[m] = vcombine_u16(vmovn_u32(vcgeq_s32(pal, pbl)), vmovn_u32(vcgeq_s32(pah, pbh)));
        const int32x4_t s2r = vpaddlq_s16(vbslq_s16(pick_a[m], xar, xbr));      // 4 sums of 2 bits
        const int32x4_t s2i = vpaddlq_s16(vbslq_s16(pick_a[m], xai, xbi));
        vst1q_s32(sre + 4 * m, s2r);
        vst1q_s32(sim + 4 * m, s2i);
        mxb = vmaxq_u32(mxb, vmaxq_u32(vreinterpretq_u32_s32(vabsq_s32(s2r)), vreinterpretq_u32_s32(vabsq_s32(s2i))));
    }
#ifdef __aarch64__
    const uint32_t bmax = vmaxvq_u32(mxb);
#else
    uint32x2_t mb2 = vmax_u32(vget_low_u32(mxb), vget_high_u32(mxb));
    const uint32_t bmax = vget_lane_u32(vpmax_u32(mb2, mb2), 0);
#endif
    const int bs = coh_shift(bmax, COH_WIDE_BITS);
    int16_t wr[32] __attribute__((aligned(16))) = { 0 }, wi[32] __attribute__((aligned(16))) = { 0 };
    for (int m = 0; m < COH_WBLK; m++) {
        wr[m] = (int16_t) (sre[m] >> bs);
        wi[m] = (int16_t) (sim[m] >> bs);
    }

    // Wide search, first stage: the peak of each 14-block piece over 16 wide bins (two vectors)
    int kc[2];
    for (int h = 0; h < 2; h++) {
        int32_t p1[COH_WBINS] __attribute__((aligned(16)));
        const int16_t *pr = wr + h * COH_WBLK / 2, *pi = wi + h * COH_WBLK / 2;
        const int16x4_t r0 = vld1_s16(pr), r1 = vld1_s16(pr + 4), r2 = vld1_s16(pr + 8), r3 = vld1_s16(pr + 12);
        const int16x4_t i0 = vld1_s16(pi), i1 = vld1_s16(pi + 4), i2 = vld1_s16(pi + 8), i3 = vld1_s16(pi + 12);
        for (int j = 0; j < COH_WBINS; j += 8) {
            int16x8_t ar0 = vdupq_n_s16(0), ai0 = vdupq_n_s16(0), ar1 = vdupq_n_s16(0), ai1 = vdupq_n_s16(0);
            COH_LROT(coh_wide, j, ar0, ai0, r0, i0, 0, 0);  COH_LROT(coh_wide, j, ar1, ai1, r0, i0, 1, 1);
            COH_LROT(coh_wide, j, ar0, ai0, r0, i0, 2, 2);  COH_LROT(coh_wide, j, ar1, ai1, r0, i0, 3, 3);
            COH_LROT(coh_wide, j, ar0, ai0, r1, i1, 0, 4);  COH_LROT(coh_wide, j, ar1, ai1, r1, i1, 1, 5);
            COH_LROT(coh_wide, j, ar0, ai0, r1, i1, 2, 6);  COH_LROT(coh_wide, j, ar1, ai1, r1, i1, 3, 7);
            COH_LROT(coh_wide, j, ar0, ai0, r2, i2, 0, 8);  COH_LROT(coh_wide, j, ar1, ai1, r2, i2, 1, 9);
            COH_LROT(coh_wide, j, ar0, ai0, r2, i2, 2, 10); COH_LROT(coh_wide, j, ar1, ai1, r2, i2, 3, 11);
            COH_LROT(coh_wide, j, ar0, ai0, r3, i3, 0, 12); COH_LROT(coh_wide, j, ar1, ai1, r3, i3, 1, 13);
            const int16x8_t sre16 = vaddq_s16(ar0, ar1), sim16 = vaddq_s16(ai0, ai1);
            vst1q_s32(p1 + j, vmlal_s16(vmull_s16(vget_low_s16(sre16), vget_low_s16(sre16)), vget_low_s16(sim16), vget_low_s16(sim16)));
            vst1q_s32(p1 + j + 4, vmlal_s16(vmull_s16(vget_high_s16(sre16), vget_high_s16(sre16)), vget_high_s16(sim16), vget_high_s16(sim16)));
        }
        kc[h] = coh_argmax(p1, COH_WBINS);
    }

    // Second stage: around each peak, the central fine bins (two vectors); the stronger peak wins
    int k = kc[0], f = 0;
    int32_t pbest = -1;
    for (int c = 0; c < (kc[1] != kc[0] ? 2 : 1); c++) {
        int16_t br8[8] __attribute__((aligned(8))) = { 0 }, bi8[8] __attribute__((aligned(8))) = { 0 };
        coh_wide_regroup_neon(wr, wi, kc[c], br8, bi8);
        int32_t p[COH_BINS_PAD] __attribute__((aligned(16))) = { 0 };
        const int16x4_t r0 = vld1_s16(br8), r1 = vld1_s16(br8 + 4), i0 = vld1_s16(bi8), i1 = vld1_s16(bi8 + 4);
        for (int j = COH_REF_LO; j < COH_REF_HI; j += 8) {
            int16x8_t ar0 = vdupq_n_s16(0), ai0 = vdupq_n_s16(0), ar1 = vdupq_n_s16(0), ai1 = vdupq_n_s16(0);
            COH_LROT(coh_bin, j, ar0, ai0, r0, i0, 0, 0); COH_LROT(coh_bin, j, ar1, ai1, r0, i0, 1, 1);
            COH_LROT(coh_bin, j, ar0, ai0, r0, i0, 2, 2); COH_LROT(coh_bin, j, ar1, ai1, r0, i0, 3, 3);
            COH_LROT(coh_bin, j, ar0, ai0, r1, i1, 0, 4); COH_LROT(coh_bin, j, ar1, ai1, r1, i1, 1, 5);
            COH_LROT(coh_bin, j, ar0, ai0, r1, i1, 2, 6);
            const int16x8_t sre16 = vaddq_s16(ar0, ar1), sim16 = vaddq_s16(ai0, ai1);
            vst1q_s32(p + j, vmlal_s16(vmull_s16(vget_low_s16(sre16), vget_low_s16(sre16)), vget_low_s16(sim16), vget_low_s16(sim16)));
            vst1q_s32(p + j + 4, vmlal_s16(vmull_s16(vget_high_s16(sre16), vget_high_s16(sre16)), vget_high_s16(sim16), vget_high_s16(sim16)));
        }
        const int best = coh_argmax(p, COH_BINS);
        if (p[best] > pbest) {
            pbest = p[best];
            k = kc[c];
            f = coh_fine_index(p, best);
        }
    }
    int16_t rbr, rbi, rcr, rci;
    coh_steps(k, f, &rbr, &rbi, &rcr, &rci);

    // Phasors: first 8 one by one (as generic), then 8 at a time
    Pr[0] = 32767; Pi[0] = 0;
    for (int i = 1; i < 8; i++) {
        Pr[i] = (int16_t) (coh_mulq15(Pr[i - 1], rbr) - coh_mulq15(Pi[i - 1], rbi));
        Pi[i] = (int16_t) (coh_mulq15(Pr[i - 1], rbi) + coh_mulq15(Pi[i - 1], rbr));
    }
    const int16_t r8r = (int16_t) (coh_mulq15(Pr[7], rbr) - coh_mulq15(Pi[7], rbi));
    const int16_t r8i = (int16_t) (coh_mulq15(Pr[7], rbi) + coh_mulq15(Pi[7], rbr));
    // P and Q, 8 at a time
    for (int i = 0; i < COH_HALF; i += 8) {
        int16x8_t rr, ri;
        if (i >= 8) {
            coh_rot_neon(vld1q_s16(Pr + i - 8), vld1q_s16(Pi + i - 8), vdupq_n_s16(r8r), vdupq_n_s16(r8i), &rr, &ri);
            vst1q_s16(Pr + i, rr);
            vst1q_s16(Pi + i, ri);
        }
        coh_rot_neon(vld1q_s16(Pr + i), vld1q_s16(Pi + i), vdupq_n_s16(rcr), vdupq_n_s16(rci), &rr, &ri);
        vst1q_s16(Qr + i, rr);
        vst1q_s16(Qi + i, ri);
    }

    // Correlation
    int32x4_t hre = vdupq_n_s32(0), him = vdupq_n_s32(0);
    uint32x4_t e2v = vdupq_n_u32(0);
    for (int i = 0; i < COH_HALF; i += 8) {
        // rot(a, P) or rot(b, Q): select the chip and its phasor, then rotate once
        const uint16x8_t k = pick_a[i / 8];
        int16x8_t cre, cim;
        coh_rot_neon(vbslq_s16(k, vld1q_s16(ar + i), vld1q_s16(br + i)), vbslq_s16(k, vld1q_s16(ai + i), vld1q_s16(bi + i)),
                     vbslq_s16(k, vld1q_s16(Pr + i), vld1q_s16(Qr + i)), vbslq_s16(k, vld1q_s16(Pi + i), vld1q_s16(Qi + i)),
                     &cre, &cim);
        hre = vpadalq_s16(hre, cre);
        him = vpadalq_s16(him, cim);
        const int16x8_t sr = vshrq_n_s16(cre, 4), si = vshrq_n_s16(cim, 4);
        int32x4_t e = vmull_s16(vget_low_s16(sr), vget_low_s16(sr));
        e = vmlal_s16(e, vget_high_s16(sr), vget_high_s16(sr));
        e = vmlal_s16(e, vget_low_s16(si), vget_low_s16(si));
        e = vmlal_s16(e, vget_high_s16(si), vget_high_s16(si));
        e2v = vaddq_u32(e2v, vreinterpretq_u32_s32(e));
    }
    const int32_t h_re = coh_hsum_s32_neon(hre), h_im = coh_hsum_s32_neon(him);
    const uint32_t rho2 = coh_rho2(h_re, h_im, coh_hsum_u32_neon(e2v));
    *rho2_out = rho2;
    *f_out = (uint32_t) f;
    *k_out = (uint32_t) k;
    if (rho2 < thres)
        return 0;

    // Slice, one byte per vector
    int16_t gre, gim;
    coh_channel16(h_re, h_im, &gre, &gim);
    const int16x8_t gr = vdupq_n_s16(gre), gi = vdupq_n_s16(gim);
    uint32x4_t qual = vdupq_n_u32(0), pw = vdupq_n_u32(0);
    for (int i = 0; i < COH_HALF; i += 8) {
        int16x8_t xar, xai, xbr, xbi;
        coh_rot_neon(vld1q_s16(ar + i), vld1q_s16(ai + i), vld1q_s16(Pr + i), vld1q_s16(Pi + i), &xar, &xai);
        coh_rot_neon(vld1q_s16(br + i), vld1q_s16(bi + i), vld1q_s16(Qr + i), vld1q_s16(Qi + i), &xbr, &xbi);
        const int16x8_t ya = vaddq_s16(vqrdmulhq_s16(xar, gr), vqrdmulhq_s16(xai, gi));
        const int16x8_t yb = vaddq_s16(vqrdmulhq_s16(xbr, gr), vqrdmulhq_s16(xbi, gi));
        out[(first + i) >> 3] = coh_bits8_neon(vcgtq_s16(ya, yb));
        // |ya - yb| and |ya| + |yb| are below 2^15, so as uint16 they widen-accumulate safely
        qual = vpadalq_u16(qual, vreinterpretq_u16_s16(vabdq_s16(ya, yb)));
        pw = vpadalq_u16(pw, vreinterpretq_u16_s16(vaddq_s16(vabsq_s16(ya), vabsq_s16(yb))));
    }
    *q = coh_quality(coh_hsum_u32_neon(qual), coh_hsum_u32_neon(pw));
    return 1;
}

void STARCH_IMPL(coherent_decode, neon) (const int16_t * restrict iq, int pos, unsigned thres,
                                         uint8_t * restrict out, uint32_t * restrict info)
{
    int16x8_t ci[5][2];
    for (int c = 0; c < 5; c++) {
        const int16x8_t cv = vmovl_s8(vld1_s8(upsample_coeffs[c]));
        const int16x8x2_t z = vzipq_s16(cv, cv);        // [c0, c0, c1, c1, ..]
        ci[c][0] = z.val[0];
        ci[c][1] = z.val[1];
    }

    memset(out, 0, 14);
    memset(info, 0, COH_INFO_N * sizeof(*info));

    if (!coh_half_neon(iq, pos, 0, thres, (const int16x8_t (*)[2]) ci, out,
                       &info[COH_INFO_RHO2], &info[COH_INFO_FINE], &info[COH_INFO_WIDE], &info[COH_INFO_Q1]))
        return;
    info[COH_INFO_DECODED] = 1;
    // The second half whatever the DF says (as the magnitude decoder): scoring can
    // correct the DF bits, and every short DF is one bit from a long one
    coh_half_neon(iq, pos, COH_HALF, 0, (const int16x8_t (*)[2]) ci, out,
                  &info[COH_INFO_RHO2B], &info[COH_INFO_FINE2], &info[COH_INFO_WIDE2], &info[COH_INFO_Q2]);
}
