// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.neon.c: DEMOD_2400X upsampler and preamble search, ARM NEON.
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

// Horizontal add
static inline __attribute__((always_inline))
uint32_t hsum_u32x4_neon(const uint32x4_t v)
{
#ifdef __aarch64__
    return vaddvq_u32(v);
#else
    uint32x2_t s = vadd_u32(vget_low_u32(v),
                            vget_high_u32(v));
    s = vpadd_u32(s, s);
    return vget_lane_u32(s, 0);
#endif
}

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

// Upsample
int STARCH_IMPL(upsample12, neon) (const int16_t * restrict in, int len, int shift, uint16_t * restrict oo)
{
   if (len < 8) return 0;

   int8x8_t c0 = vld1_s8(upsample_coeffs[0]);
   int8x8_t c1 = vld1_s8(upsample_coeffs[1]);
   int8x8_t c2 = vld1_s8(upsample_coeffs[2]);
   int8x8_t c3 = vld1_s8(upsample_coeffs[3]);
   int8x8_t c4 = vld1_s8(upsample_coeffs[4]);

   int16x8x2_t iq = vld2q_s16(in);

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int stride6 = 6*stride;
   int bpos = 0;

   in += 16;
   len -= 8;
   while (1) {

        // Rearrage output
        oo[bpos] = getpow_neon(c0, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_neon(c1, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_neon(c2, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_neon(c3, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_neon(c4, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        if (len == 0) break;
        len--;

        // Shift
        iq.val[0] = vextq_s16(iq.val[0], iq.val[0], 1);
        iq.val[1] = vextq_s16(iq.val[1], iq.val[1], 1);

        iq.val[0] = vsetq_lane_s16(in[0], iq.val[0], 7);
        iq.val[1] = vsetq_lane_s16(in[1], iq.val[1], 7);
        in += 2;
   }

   return nt;
}


// Locate preamble
int STARCH_IMPL(locate_preamble, neon) (const uint16_t * restrict pp, int nt, uint8_t * restrict mm)
{
   int stride = NU_STRIDE(nt); // Stride
   const int16_t mask_g[] = { 0,-1, 0,-1,-1,-1,-1, 0,-1, 0,-1,-1,-1,-1,-1,-1 };

   uint16x8_t gm1 = vld1q_u16((const uint16_t *)mask_g+0);
   uint16x8_t gm2 = vld1q_u16((const uint16_t *)mask_g+8);

   for (int j = 0; j < 6; j++) { // Look at each phase in turn

       const uint16_t * restrict p = &pp[j*stride];
       int plen = (nt + 5 - j) / 6; // Length of each phase

       plen -= 2 * LONG_FRAME; // No point in searching at end
       if (plen < 16) continue;

       uint16x8_t p1 = vld1q_u16(p + 0);
       uint16x8_t p2 = vld1q_u16(p + 8);

       plen -= 16;
       p += 16;
       uint8_t * restrict mp = &mm[j];

       while (1) {

          // Preamble detection
          // P G P G, G G G P, G P G G, G G G G

          // Extract gaps and sum together
          uint32x4_t g1 = vpaddlq_u16(vandq_u16(p1, gm1));
          uint32x4_t g2 = vpaddlq_u16(vandq_u16(p2, gm2));
          uint32x4_t gs = vaddq_u32(g1, g2);
          uint32_t gap = hsum_u32x4_neon(gs);

          // Extract pulses
          uint16x8_t dm1 = vbicq_u16(p1, gm1);
          uint16x8_t dm2 = vbicq_u16(p2, gm2);

          uint16x4_t d0 = vget_low_u16(dm1);
          uint16x4_t d1 = vget_high_u16(dm1);
          uint16x4_t d2 = vget_low_u16(dm2);

          uint16x4_t px = vorr_u16(vorr_u16(d0, d1), d2);

#ifdef PREAMBLE_MIN_PULSE
          uint16x4_t m = vpmin_u16(px, px);
          m = vpmin_u16(m, m);
          unsigned minp = vget_lane_u16(m, 0);
#endif
#ifdef PREAMBLE_SUM_PULSE
          uint32x2_t s = vpaddl_u16(px);
          s = vpadd_u32(s, s);
          uint32_t minp = vget_lane_u32(s, 0);
#endif
          // Metric
          *mp = (256 * PREAMBLE_KK * minp) / (PREAMBLE_KK * minp + gap + 1);
          mp += 6;

          if (plen == 0) break;
          plen--;

          unsigned nv = *p++;
          p1 = vextq_u16(p1, p2, 1);
          p2 = vextq_u16(p2, p2, 1);
          p2 = vsetq_lane_u16(nv, p2, 7);
       }
   }
   return METRIC_SIZE(nt);
}
