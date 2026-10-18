// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.avx2.c: DEMOD_2400X upsampler and preamble search, x86 AVX2.
// Structured the same way as demod2400x.neon.c.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#ifndef __AVX2__
#  error AVX2 support not available
#endif

#include <immintrin.h>

#include "dsp/helpers/demod2400x_dsp.h"
#include "dsp/helpers/demod2400x_coeffs.h"

// Two vectors of 8 x int16: val[0] = I, val[1] = Q (same layout as NEON's int16x8x2_t)
typedef struct {
    __m128i val[2];
} iq16x8x2_avx2_t;

// Horizontal add
static inline __attribute__((always_inline)) int32_t hsum_s32x4_avx2(const __m128i v)
{
    __m128i s = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}

// Horizontal add
static inline __attribute__((always_inline)) uint32_t hsum_u32x4_avx2(const __m128i v)
{
    return (uint32_t) hsum_s32x4_avx2(v);
}

// Load 8 interleaved I/Q samples and deinterleave them (like NEON's vld2q_s16)
static inline __attribute__((always_inline)) iq16x8x2_avx2_t load2_s16_avx2(const int16_t *in)
{
    const __m128i deint = _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13,  2, 3, 6, 7, 10, 11, 14, 15);

    __m128i a = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *) (in + 0)), deint); // I0-3 Q0-3
    __m128i b = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *) (in + 8)), deint); // I4-7 Q4-7

    iq16x8x2_avx2_t iq;
    iq.val[0] = _mm_unpacklo_epi64(a, b);  // I0-7
    iq.val[1] = _mm_unpackhi_epi64(a, b);  // Q0-7
    return iq;
}


static inline __attribute__((always_inline)) unsigned getpow_avx2(const __m128i cv8, const iq16x8x2_avx2_t iq, int shift)
{
    const __m128i cv = _mm_cvtepi8_epi16(cv8);  // widen coefficients to 16-bit

    // madd: multiply 8 pairs and add neighbours -> 4 x int32 partial sums
    int32_t resi = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[0], cv)) + 16) >> 5;
    int32_t resq = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[1], cv)) + 16) >> 5;

    uint32_t pwr = (uint32_t)(resi*resi) + (uint32_t)(resq*resq);

    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;

    return pwr;
}


// Upsample
int STARCH_IMPL(upsample12, avx2) (const int16_t * restrict in, int len, int shift, uint16_t * restrict oo)
{
   if (len < 8) return 0;

   __m128i c0 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[0]);  // 8 x int8
   __m128i c1 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[1]);  // 8 x int8
   __m128i c2 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[2]);  // 8 x int8
   __m128i c3 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[3]);  // 8 x int8
   __m128i c4 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[4]);  // 8 x int8

   iq16x8x2_avx2_t iq = load2_s16_avx2(in);

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int stride6 = 6*stride;
   int bpos = 0;

   in += 16;
   len -= 8;
   while (1) {

        // Rearrage output
        oo[bpos] = getpow_avx2(c0, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_avx2(c1, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_avx2(c2, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_avx2(c3, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow_avx2(c4, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        if (len == 0) break;
        len--;

        // Shift
        iq.val[0] = _mm_insert_epi16(_mm_srli_si128(iq.val[0], 2), in[0], 7);
        iq.val[1] = _mm_insert_epi16(_mm_srli_si128(iq.val[1], 2), in[1], 7);
        in += 2;
   }

   return nt;
}


// Locate preamble
int STARCH_IMPL(locate_preamble, avx2) (const uint16_t * restrict pp, int nt, uint8_t * restrict mm)
{
   int stride = NU_STRIDE(nt); // Stride
   const int16_t mask_g[] = { 0,-1, 0,-1,-1,-1,-1, 0,-1, 0,-1,-1,-1,-1,-1,-1 };

   const __m128i gm1 = _mm_loadu_si128((const __m128i *) (mask_g + 0));
   const __m128i gm2 = _mm_loadu_si128((const __m128i *) (mask_g + 8));
   const __m128i zero = _mm_setzero_si128();

   for (int j = 0; j < 6; j++) { // Look at each phase in turn

       const uint16_t * restrict p = &pp[j*stride];
       int plen = (nt + 5 - j) / 6; // Length of each phase

       plen -= 2 * LONG_FRAME; // No point in searching at end
       if (plen < 16) continue;

       __m128i p1 = _mm_loadu_si128((const __m128i *) (p + 0));
       __m128i p2 = _mm_loadu_si128((const __m128i *) (p + 8));

       plen -= 16;
       p += 16;
       uint8_t * restrict mp = &mm[j];

       while (1) {

          // Preamble detection
          // P G P G, G G G P, G P G G, G G G G

          // Extract gaps and sum together (widened to 32-bit, values are unsigned)
          __m128i g1 = _mm_and_si128(p1, gm1);
          __m128i g2 = _mm_and_si128(p2, gm2);
          __m128i gs = _mm_add_epi32(_mm_add_epi32(_mm_unpacklo_epi16(g1, zero), _mm_unpackhi_epi16(g1, zero)),
                                     _mm_add_epi32(_mm_unpacklo_epi16(g2, zero), _mm_unpackhi_epi16(g2, zero)));
          uint32_t gap = hsum_u32x4_avx2(gs);

          // Extract pulses: low 4 lanes of px = p[0], p[9], p[2], p[7]
          __m128i dm1 = _mm_andnot_si128(gm1, p1);
          __m128i dm2 = _mm_andnot_si128(gm2, p2);

          __m128i px = _mm_or_si128(_mm_or_si128(dm1, _mm_srli_si128(dm1, 8)), dm2);

#ifdef PREAMBLE_MIN_PULSE
          __m128i m = _mm_min_epu16(px, _mm_srli_si128(px, 4));
          m = _mm_min_epu16(m, _mm_srli_si128(m, 2));
          unsigned minp = (unsigned) _mm_extract_epi16(m, 0);
#endif
#ifdef PREAMBLE_SUM_PULSE
          uint32_t minp = hsum_u32x4_avx2(_mm_cvtepu16_epi32(px));
#endif
          // Metric
          *mp = (256 * PREAMBLE_KK * minp) / (PREAMBLE_KK * minp + gap + 1);
          mp += 6;

          if (plen == 0) break;
          plen--;

          unsigned nv = *p++;
          p1 = _mm_alignr_epi8(p2, p1, 2);
          p2 = _mm_insert_epi16(_mm_srli_si128(p2, 2), (int) nv, 7);
       }
   }
   return METRIC_SIZE(nt);
}
