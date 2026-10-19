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


static inline __attribute__((always_inline)) unsigned getpow_neon(const int8x8_t cv8, const int16x8x2_t iq, int shift)
{
    const int16x8_t cv = vmovl_s8(cv8);  // widen coefficients to 16-bit
    int32x4_t si, sq;

    si = vmull_s16(vget_low_s16(iq.val[0]), vget_low_s16(cv));
    si = vmlal_s16(si, vget_high_s16(iq.val[0]), vget_high_s16(cv));

    sq = vmull_s16(vget_low_s16(iq.val[1]), vget_low_s16(cv));
    sq = vmlal_s16(sq, vget_high_s16(iq.val[1]), vget_high_s16(cv));

    int32x2_t res = hsum2_s32x4_neon(si, sq);                  // [resi, resq]
    res = vrshr_n_s32(res, 5);                                 // (x+16)>>5 on both

    uint32x2_t sqr = vreinterpret_u32_s32(vmul_s32(res, res)); // [resi^2, resq^2]
    uint32_t pwr = vget_lane_u32(sqr, 0) + vget_lane_u32(sqr, 1);

#ifdef USE_MAGNITUDE
    (void)shift;
    pwr = demod_isqrt(pwr);
#else
    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;
#endif

    return pwr;
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

// Power (or magnitude) of 4 filter outputs, as getpow_neon()
static inline __attribute__((always_inline))
uint16x4_t getpow4_neon(int32x4_t si, int32x4_t sq, int shift)
{
    si = vrshrq_n_s32(si, 5);                                  // (x+16)>>5
    sq = vrshrq_n_s32(sq, 5);
    const uint32x4_t pwr = vreinterpretq_u32_s32(vmlaq_s32(vmulq_s32(si, si), sq, sq));

#ifdef USE_MAGNITUDE
    (void)shift;
    return vmovn_u32(isqrt_u32x4_neon(pwr));
#else
    return vqmovn_u32(vshlq_u32(pwr, vdupq_n_s32(-shift)));  // Set gain, saturate to 65535
#endif
}

// One phase of the upsampler for 8 consecutive windows; s[k] = I/Q at window + k
static inline __attribute__((always_inline))
uint16x8_t upsample_phase8_neon(const int16x8x2_t * restrict s, const int8_t * restrict coeffs, int shift)
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

    return vcombine_u16(getpow4_neon(si_lo, sq_lo, shift), getpow4_neon(si_hi, sq_hi, shift));
}

#ifdef __aarch64__
// 5-way interleave of 16-bit lanes: bytes from ph[0..3] (tbl4), then ph[4] (tbx1)
static const uint8_t interleave5_tbl4[5][16] = {
    {   0,   1,  16,  17,  32,  33,  48,  49, 255, 255,   2,   3,  18,  19,  34,  35 },
    {  50,  51, 255, 255,   4,   5,  20,  21,  36,  37,  52,  53, 255, 255,   6,   7 },
    {  22,  23,  38,  39,  54,  55, 255, 255,   8,   9,  24,  25,  40,  41,  56,  57 },
    { 255, 255,  10,  11,  26,  27,  42,  43,  58,  59, 255, 255,  12,  13,  28,  29 },
    {  44,  45,  60,  61, 255, 255,  14,  15,  30,  31,  46,  47,  62,  63, 255, 255 },
};
static const uint8_t interleave5_tbx1[5][16] = {
    { 255, 255, 255, 255, 255, 255, 255, 255,   0,   1, 255, 255, 255, 255, 255, 255 },
    { 255, 255,   2,   3, 255, 255, 255, 255, 255, 255, 255, 255,   4,   5, 255, 255 },
    { 255, 255, 255, 255, 255, 255,   6,   7, 255, 255, 255, 255, 255, 255, 255, 255 },
    {   8,   9, 255, 255, 255, 255, 255, 255, 255, 255,  10,  11, 255, 255, 255, 255 },
    { 255, 255, 255, 255,  12,  13, 255, 255, 255, 255, 255, 255, 255, 255,  14,  15 },
};
#endif

// out[5*w + c] = ph[c][w], w = 0..7
static inline __attribute__((always_inline))
void interleave5_u16x8_neon(const uint16x8_t * restrict ph, uint16_t * restrict out)
{
#ifdef __aarch64__
    uint8x16x4_t t;
    t.val[0] = vreinterpretq_u8_u16(ph[0]);
    t.val[1] = vreinterpretq_u8_u16(ph[1]);
    t.val[2] = vreinterpretq_u8_u16(ph[2]);
    t.val[3] = vreinterpretq_u8_u16(ph[3]);
    const uint8x16_t e = vreinterpretq_u8_u16(ph[4]);

    for (int v = 0; v < 5; v++) {
        const uint8x16_t o = vqtbx1q_u8(vqtbl4q_u8(t, vld1q_u8(interleave5_tbl4[v])), e, vld1q_u8(interleave5_tbx1[v]));
        vst1q_u16(out + 8 * v, vreinterpretq_u16_u8(o));
    }
#else
    uint16_t t[5][8];
    for (int c = 0; c < 5; c++)
        vst1q_u16(t[c], ph[c]);
    for (int w = 0; w < 8; w++)
        for (int c = 0; c < 5; c++)
            out[5 * w + c] = t[c][w];
#endif
}

#define UPSAMPLE_BLOCK 48 // Windows per block, multiple of 48: 5*48 outputs = 40 per phase

// Upsample
int STARCH_IMPL(upsample12, neon) (const int16_t * restrict in, int len, int shift, uint16_t * restrict oo)
{
   if (len < 8) return 0;

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int nw = len - 7; // Windows: 5 outputs each, output o goes to oo[(o % 6) * stride + o / 6]
   int n = 0;

   // 8 windows at a time for each phase, a block at a time so the outputs
   // can be put in order then split into the 6 phases with vector loads/stores
   for (; n + UPSAMPLE_BLOCK <= nw; n += UPSAMPLE_BLOCK) {
       uint16_t tmp[5 * UPSAMPLE_BLOCK] __attribute__((aligned(16))); // tmp[o - 5*n]

       for (int w = 0; w < UPSAMPLE_BLOCK; w += 8) {
           const int16_t * restrict p = in + 2 * (n + w);
           int16x8x2_t s[8];
           for (int k = 0; k < 8; k++)
               s[k] = vld2q_s16(p + 2 * k);

           uint16x8_t ph[5];
           for (int c = 0; c < 5; c++)
               ph[c] = upsample_phase8_neon(s, upsample_coeffs[c], shift);

           interleave5_u16x8_neon(ph, tmp + 5 * w);
       }

       // 5*n is a multiple of 6, so tmp[6*m + r] -> oo[r * stride + 5*n/6 + m]
       uint16_t * restrict op = oo + 5 * n / 6;
       for (int m = 0; m < 5 * UPSAMPLE_BLOCK / 6; m += 8) {
           const uint32x4x3_t a = vld3q_u32((const uint32_t *) (tmp + 6 * m));      // pairs (r, r+1)
           const uint32x4x3_t b = vld3q_u32((const uint32_t *) (tmp + 6 * m + 24));
           for (int r = 0; r < 3; r++) {
               const uint16x8x2_t z = vuzpq_u16(vreinterpretq_u16_u32(a.val[r]), vreinterpretq_u16_u32(b.val[r]));
               vst1q_u16(op + (2 * r) * stride + m, z.val[0]);
               vst1q_u16(op + (2 * r + 1) * stride + m, z.val[1]);
           }
       }
   }

   // Remainder
   for (; n < nw; n++) {
       const int16x8x2_t iq = vld2q_s16(in + 2 * n);
       for (int c = 0; c < 5; c++) {
           const int o = 5 * n + c;
           oo[(o % 6) * stride + o / 6] = getpow_neon(vld1_s8(upsample_coeffs[c]), iq, shift);
       }
   }

   return nt;
}


// Exact floor(n / d) for d > 0, n < 2^28 and n / d < 256 (the preamble metric range)
static inline __attribute__((always_inline))
uint32x4_t div_u32x4_neon(const uint32x4_t n, const uint32x4_t d)
{
    const float32x4_t nf = vcvtq_f32_u32(n);
    const float32x4_t df = vcvtq_f32_u32(d);
#ifdef __aarch64__
    // Correctly rounded quotient: truncation is at most one too large
    uint32x4_t q = vcvtq_u32_f32(vdivq_f32(nf, df));
    return vaddq_u32(q, vcgtq_u32(vmulq_u32(q, d), n));
#else
    // Reciprocal estimate + two Newton steps: truncation is within one either way
    float32x4_t r = vrecpeq_f32(df);
    r = vmulq_f32(vrecpsq_f32(df, r), r);
    r = vmulq_f32(vrecpsq_f32(df, r), r);
    uint32x4_t q = vcvtq_u32_f32(vmulq_f32(nf, r));
    const uint32x4_t qd = vmulq_u32(q, d);
    q = vaddq_u32(q, vcgtq_u32(qd, n));               // too large: -1
    q = vsubq_u32(q, vcleq_u32(vaddq_u32(qd, d), n)); // too small: +1
    return q;
#endif
}

// Scalar preamble metric at p[0], 0 if below thres
static inline __attribute__((always_inline))
uint8_t preamble_metric1(const uint16_t * restrict p, unsigned thres)
{
#ifdef PREAMBLE_MIN_PULSE
    uint32_t minp = MIN(MIN(p[0], p[2]), MIN(p[7], p[9]));
#endif
#ifdef PREAMBLE_SUM_PULSE
    uint32_t minp = p[0] + p[2] + p[7] + p[9];
#endif
    uint32_t gap = p[1] + p[3] + p[4] + p[5] + p[6] + p[8] +
                   p[10] + p[11] + p[12] + p[13] + p[14] + p[15];

    uint32_t n = 256 * PREAMBLE_KK * minp;
    uint32_t d = PREAMBLE_KK * minp + gap + 1;
    return n < thres * d ? 0 : n / d;
}

#define PREAMBLE_BLOCK 64 // Positions per phase per block, multiple of 8

// Widened sum of 4 consecutive samples, p[0..3] .. p[7..10]
static inline __attribute__((always_inline))
uint32x4x2_t sum4_u16x8_neon(const uint16_t * restrict p)
{
    const uint16x8_t a = vld1q_u16(p + 0), b = vld1q_u16(p + 1);
    const uint16x8_t c = vld1q_u16(p + 2), d = vld1q_u16(p + 3);
    uint32x4x2_t s;
    s.val[0] = vaddq_u32(vaddl_u16(vget_low_u16(a), vget_low_u16(b)),
                         vaddl_u16(vget_low_u16(c), vget_low_u16(d)));
    s.val[1] = vaddq_u32(vaddl_u16(vget_high_u16(a), vget_high_u16(b)),
                         vaddl_u16(vget_high_u16(c), vget_high_u16(d)));
    return s;
}

// Preamble metrics for 8*n8 consecutive positions of one phase
static inline __attribute__((always_inline))
void preamble_metric_block_neon(const uint16_t * restrict p, int n8, unsigned thres, uint8_t * restrict out)
{
    // Preamble detection
    // P G P G, G G G P, G P G G, G G G G
    // gap = p[1] + p[8] + p[10] + p[15] + s4[3] + s4[11], s4[x] = p[x] + .. + p[x+3]
    // s4[11] for these 8 positions is s4[3] for the next 8

    const uint32x4_t one = vdupq_n_u32(1);
    const uint32x4_t vthres = vdupq_n_u32(thres);
    uint32x4x2_t s4a = sum4_u16x8_neon(p + 3);

    for (int k = 0; k < 8 * n8; k += 8, p += 8) {
        const uint32x4x2_t s4b = sum4_u16x8_neon(p + 11);

        const uint16x8_t v1 = vld1q_u16(p + 1), v8 = vld1q_u16(p + 8);
        const uint16x8_t v10 = vld1q_u16(p + 10), v15 = vld1q_u16(p + 15);

        uint32x4_t gap_lo = vaddq_u32(vaddl_u16(vget_low_u16(v1), vget_low_u16(v8)),
                                      vaddl_u16(vget_low_u16(v10), vget_low_u16(v15)));
        uint32x4_t gap_hi = vaddq_u32(vaddl_u16(vget_high_u16(v1), vget_high_u16(v8)),
                                      vaddl_u16(vget_high_u16(v10), vget_high_u16(v15)));
        gap_lo = vaddq_u32(vaddq_u32(gap_lo, one), vaddq_u32(s4a.val[0], s4b.val[0]));
        gap_hi = vaddq_u32(vaddq_u32(gap_hi, one), vaddq_u32(s4a.val[1], s4b.val[1]));
        s4a = s4b;

        // Metric = n / d = (256 * KK * minp) / (KK * minp + gap + 1), or 0 if below thres
#ifdef PREAMBLE_MIN_PULSE
        const uint16x8_t m = vminq_u16(vminq_u16(vld1q_u16(p + 0), vld1q_u16(p + 2)),
                                       vminq_u16(vld1q_u16(p + 7), vld1q_u16(p + 9)));
        const uint32x4_t n_lo = vmull_n_u16(vget_low_u16(m), 256 * PREAMBLE_KK);
        const uint32x4_t n_hi = vmull_n_u16(vget_high_u16(m), 256 * PREAMBLE_KK);
        const uint32x4_t d_lo = vmlal_n_u16(gap_lo, vget_low_u16(m), PREAMBLE_KK);
        const uint32x4_t d_hi = vmlal_n_u16(gap_hi, vget_high_u16(m), PREAMBLE_KK);
#endif
#ifdef PREAMBLE_SUM_PULSE
        const uint16x8_t v0 = vld1q_u16(p + 0), v2 = vld1q_u16(p + 2);
        const uint16x8_t v7 = vld1q_u16(p + 7), v9 = vld1q_u16(p + 9);
        const uint32x4_t m_lo = vaddq_u32(vaddl_u16(vget_low_u16(v0), vget_low_u16(v2)),
                                          vaddl_u16(vget_low_u16(v7), vget_low_u16(v9)));
        const uint32x4_t m_hi = vaddq_u32(vaddl_u16(vget_high_u16(v0), vget_high_u16(v2)),
                                          vaddl_u16(vget_high_u16(v7), vget_high_u16(v9)));
        const uint32x4_t n_lo = vmulq_n_u32(m_lo, 256 * PREAMBLE_KK);
        const uint32x4_t n_hi = vmulq_n_u32(m_hi, 256 * PREAMBLE_KK);
        const uint32x4_t d_lo = vmlaq_n_u32(gap_lo, m_lo, PREAMBLE_KK);
        const uint32x4_t d_hi = vmlaq_n_u32(gap_hi, m_hi, PREAMBLE_KK);
#endif

        // Most positions are noise below the threshold: skip the divide when all 8 are
        const uint32x4_t ok_lo = vcgeq_u32(n_lo, vmulq_u32(d_lo, vthres));
        const uint32x4_t ok_hi = vcgeq_u32(n_hi, vmulq_u32(d_hi, vthres));
        const uint32x4_t any = vorrq_u32(ok_lo, ok_hi);
#ifdef __aarch64__
        if (vmaxvq_u32(any) == 0) {
#else
        if (vget_lane_u64(vreinterpret_u64_u32(vorr_u32(vget_low_u32(any), vget_high_u32(any))), 0) == 0) {
#endif
            vst1_u8(out + k, vdup_n_u8(0));
            continue;
        }
        const uint32x4_t q_lo = vandq_u32(div_u32x4_neon(n_lo, d_lo), ok_lo);
        const uint32x4_t q_hi = vandq_u32(div_u32x4_neon(n_hi, d_hi), ok_hi);

        // Metric < 256
        vst1_u8(out + k, vmovn_u16(vcombine_u16(vmovn_u32(q_lo), vmovn_u32(q_hi))));
    }
}

// Locate preamble
int STARCH_IMPL(locate_preamble, neon) (const uint16_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
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
   while (i + 8 <= cmin) {
       const int n8 = MIN((cmin - i) / 8, PREAMBLE_BLOCK / 8);
       uint8_t t[6][PREAMBLE_BLOCK];

       for (int j = 0; j < 6; j++)
           preamble_metric_block_neon(&pp[j*stride + i], n8, thres, t[j]);

       // mm[6*(i+k) + j] = t[j][k]: interleave phases in pairs, then 3-way as 16-bit
       uint8_t * restrict mp = &mm[6*i];
       for (int k = 0; k < 8 * n8; k += 8, mp += 48) {
           const uint8x8x2_t z01 = vzip_u8(vld1_u8(t[0] + k), vld1_u8(t[1] + k));
           const uint8x8x2_t z23 = vzip_u8(vld1_u8(t[2] + k), vld1_u8(t[3] + k));
           const uint8x8x2_t z45 = vzip_u8(vld1_u8(t[4] + k), vld1_u8(t[5] + k));
           uint16x8x3_t o;
           o.val[0] = vreinterpretq_u16_u8(vcombine_u8(z01.val[0], z01.val[1]));
           o.val[1] = vreinterpretq_u16_u8(vcombine_u8(z23.val[0], z23.val[1]));
           o.val[2] = vreinterpretq_u16_u8(vcombine_u8(z45.val[0], z45.val[1]));
           vst3q_u16((uint16_t *) mp, o);
       }
       i += 8 * n8;
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
