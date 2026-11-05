// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.avx2.c: DEMOD_2400X input conversion, upsampler and preamble search, x86 AVX2.
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
#include "dsp/helpers/demod2400x_iqconv.h"
#include "dsp/helpers/demod2400x_coherent.h"

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


static inline __attribute__((always_inline)) uint8_t getlog_avx2(const __m128i cv8, const iq16x8x2_avx2_t iq)
{
    const __m128i cv = _mm_cvtepi8_epi16(cv8);  // widen coefficients to 16-bit

    // madd: multiply 8 pairs and add neighbours -> 4 x int32 partial sums
    int32_t resi = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[0], cv)) + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;
    int32_t resq = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[1], cv)) + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;

    return demod_log8((uint32_t)(resi*resi) + (uint32_t)(resq*resq));
}


// Exact floor(sqrt(x)) for x < 2^31, same steps as demod_isqrt()
static inline __attribute__((always_inline))
__m256i isqrt_u32x8_avx2(const __m256i x)
{
    // -ffast-math lets the compiler use a reciprocal sqrt estimate, so truncation
    // is within one either way: correct in both directions
    const __m256 f = _mm256_add_ps(_mm256_sqrt_ps(_mm256_cvtepi32_ps(x)), _mm256_set1_ps(1.0f / 64));
    __m256i q = _mm256_cvttps_epi32(f);
    // q * q > x (unsigned, q * q may reach 2^31): q - 1 unless max(q * q, x) == x
    const __m256i qq = _mm256_mullo_epi32(q, q);
    const __m256i ok = _mm256_cmpeq_epi32(_mm256_max_epu32(qq, x), x);
    q = _mm256_sub_epi32(_mm256_sub_epi32(q, _mm256_set1_epi32(1)), ok);
    // (q + 1)^2 <= x (unsigned, < 2^32): q + 1
    const __m256i q1 = _mm256_add_epi32(q, _mm256_set1_epi32(1));
    const __m256i q1q1 = _mm256_mullo_epi32(q1, q1);
    return _mm256_sub_epi32(q, _mm256_cmpeq_epi32(_mm256_max_epu32(q1q1, x), x));
}

// demod_log8() of 8 filter outputs already rounded by UPSAMPLE_SHIFT, as 8 bytes (low half).
// P + 1 < 2^31 (see demod2400x_coeffs.h), so the signed conversion is fine
static inline __attribute__((always_inline))
__m128i log8_rounded_avx2(__m256i si, __m256i sq)
{
    const __m256i pwr = _mm256_add_epi32(_mm256_mullo_epi32(si, si), _mm256_mullo_epi32(sq, sq));
    const __m256 f = _mm256_cvtepi32_ps(_mm256_add_epi32(pwr, _mm256_set1_epi32(1)));
    const __m256i l = _mm256_sub_epi32(_mm256_srli_epi32(_mm256_castps_si256(f), 20), _mm256_set1_epi32(127 << 3));
    const __m128i l16 = _mm_packus_epi32(_mm256_castsi256_si128(l), _mm256_extracti128_si256(l, 1));
    return _mm_packus_epi16(l16, l16);
}

// Log power of 8 filter outputs, as getlog_avx2()
static inline __attribute__((always_inline))
__m128i getlog8_avx2(__m256i si, __m256i sq)
{
    const __m256i round = _mm256_set1_epi32(UPSAMPLE_ROUND);
    return log8_rounded_avx2(_mm256_srai_epi32(_mm256_add_epi32(si, round), UPSAMPLE_SHIFT),
                             _mm256_srai_epi32(_mm256_add_epi32(sq, round), UPSAMPLE_SHIFT));
}

// The same for doubled filter outputs: rounded (2s) >> (UPSAMPLE_SHIFT + 1) == rounded s >> UPSAMPLE_SHIFT
static inline __attribute__((always_inline))
__m128i getlog8_x2_avx2(__m256i si2, __m256i sq2)
{
    const __m256i round = _mm256_set1_epi32(2 * UPSAMPLE_ROUND);
    return log8_rounded_avx2(_mm256_srai_epi32(_mm256_add_epi32(si2, round), UPSAMPLE_SHIFT + 1),
                             _mm256_srai_epi32(_mm256_add_epi32(sq2, round), UPSAMPLE_SHIFT + 1));
}

// 16-bit pairs (a[w], b[w]) of 8 windows as 8 x int32 lanes, windows 0..7 in order, for madd
static inline __attribute__((always_inline)) __m256i pairs8_avx2(const __m128i a, const __m128i b)
{
    return _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_unpacklo_epi16(a, b)), _mm_unpackhi_epi16(a, b), 1);
}

// Two coefficients as a madd pair
static inline __attribute__((always_inline)) __m256i coeff_pair_avx2(int16_t c0, int16_t c1)
{
    return _mm256_set1_epi32((int32_t) ((uint16_t) c0 | ((uint32_t) (uint16_t) c1 << 16)));
}

// 16-bit pairs (v[k], v[k+1]) for 8 consecutive windows, from v[0..15] = lo, hi
#define PAIRS_AVX2(lo, hi, k)                                                                   \
    _mm256_inserti128_si256(                                                                    \
        _mm256_castsi128_si256(_mm_unpacklo_epi16(_mm_alignr_epi8(hi, lo, 2 * (k)),             \
                                                  _mm_alignr_epi8(hi, lo, 2 * (k) + 2))),       \
        _mm_unpackhi_epi16(_mm_alignr_epi8(hi, lo, 2 * (k)), _mm_alignr_epi8(hi, lo, 2 * (k) + 2)), 1)

// Upsample
int STARCH_IMPL(upsample12, avx2) (const int16_t * restrict in, int len, uint8_t * restrict oo)
{
   if (len < 8) return 0;

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int stride6 = 6*stride;
   int bpos = 0;
   int nw = len - 7; // Windows, 5 outputs each
   int n = 0;

   // Coefficient pairs (c[2j], c[2j+1]) for madd, two taps at a time
   __m256i cp[5][4];
   for (int c = 0; c < 5; c++)
       for (int j = 0; j < 4; j++)
           cp[c][j] = _mm256_set1_epi32((int32_t) ((uint16_t) upsample_coeffs[c][2 * j] |
                                                   ((uint32_t) (uint16_t) upsample_coeffs[c][2 * j + 1] << 16)));

   // Mirror-symmetric phases (see demod2400x_coeffs.h): half the multiplies
   const int mirrored = upsample_coeffs_mirrored();
   int16_t pc[3][4], mc[3][4];
   upsample_mirror_coeffs(pc, mc);
   __m256i pcp[3][2], mcp[3][2];
   for (int c = 0; c < 3; c++)
       for (int j = 0; j < 2; j++) {
           pcp[c][j] = coeff_pair_avx2(pc[c][2 * j], pc[c][2 * j + 1]);
           mcp[c][j] = coeff_pair_avx2(mc[c][2 * j], mc[c][2 * j + 1]);
       }

   // 8 windows at a time; reads samples n .. n+15
   for (; n + 9 <= nw; n += 8) {
       const iq16x8x2_avx2_t a = load2_s16_avx2(in + 2 * n);      // samples n .. n+7
       const iq16x8x2_avx2_t b = load2_s16_avx2(in + 2 * n + 16); // samples n+8 .. n+15
       uint8_t t[5][8];

       if (mirrored) {
           // x[k] = tap k of the 8 windows; u[k] = x[k] + x[7-k], v[k] = x[k] - x[7-k] as madd pairs
           __m128i xi[8], xq[8];
           xi[0] = a.val[0];
           xq[0] = a.val[1];
#define TAPS_AVX2(k) xi[k] = _mm_alignr_epi8(b.val[0], a.val[0], 2 * (k)); xq[k] = _mm_alignr_epi8(b.val[1], a.val[1], 2 * (k))
           TAPS_AVX2(1); TAPS_AVX2(2); TAPS_AVX2(3); TAPS_AVX2(4); TAPS_AVX2(5); TAPS_AVX2(6); TAPS_AVX2(7);
#undef TAPS_AVX2
           __m256i ui[2], vi[2], uq[2], vq[2];
           for (int j = 0; j < 2; j++) {
               const int k = 2 * j;
               ui[j] = pairs8_avx2(_mm_add_epi16(xi[k], xi[7 - k]), _mm_add_epi16(xi[k + 1], xi[6 - k]));
               vi[j] = pairs8_avx2(_mm_sub_epi16(xi[k], xi[7 - k]), _mm_sub_epi16(xi[k + 1], xi[6 - k]));
               uq[j] = pairs8_avx2(_mm_add_epi16(xq[k], xq[7 - k]), _mm_add_epi16(xq[k + 1], xq[6 - k]));
               vq[j] = pairs8_avx2(_mm_sub_epi16(xq[k], xq[7 - k]), _mm_sub_epi16(xq[k + 1], xq[6 - k]));
           }

           for (int c = 0; c < 3; c++) {
               const __m256i pi = _mm256_add_epi32(_mm256_madd_epi16(ui[0], pcp[c][0]), _mm256_madd_epi16(ui[1], pcp[c][1]));
               const __m256i pq = _mm256_add_epi32(_mm256_madd_epi16(uq[0], pcp[c][0]), _mm256_madd_epi16(uq[1], pcp[c][1]));
               if (c == 2) {                                       // symmetric: no v terms
                   _mm_storel_epi64((__m128i *) t[2], getlog8_x2_avx2(pi, pq));
                   break;
               }
               const __m256i mi = _mm256_add_epi32(_mm256_madd_epi16(vi[0], mcp[c][0]), _mm256_madd_epi16(vi[1], mcp[c][1]));
               const __m256i mq = _mm256_add_epi32(_mm256_madd_epi16(vq[0], mcp[c][0]), _mm256_madd_epi16(vq[1], mcp[c][1]));
               _mm_storel_epi64((__m128i *) t[c], getlog8_x2_avx2(_mm256_add_epi32(pi, mi), _mm256_add_epi32(pq, mq)));
               _mm_storel_epi64((__m128i *) t[4 - c], getlog8_x2_avx2(_mm256_sub_epi32(pi, mi), _mm256_sub_epi32(pq, mq)));
           }
       } else {
           const __m256i pi0 = PAIRS_AVX2(a.val[0], b.val[0], 0), pq0 = PAIRS_AVX2(a.val[1], b.val[1], 0);
           const __m256i pi2 = PAIRS_AVX2(a.val[0], b.val[0], 2), pq2 = PAIRS_AVX2(a.val[1], b.val[1], 2);
           const __m256i pi4 = PAIRS_AVX2(a.val[0], b.val[0], 4), pq4 = PAIRS_AVX2(a.val[1], b.val[1], 4);
           const __m256i pi6 = PAIRS_AVX2(a.val[0], b.val[0], 6), pq6 = PAIRS_AVX2(a.val[1], b.val[1], 6);

           for (int c = 0; c < 5; c++) {
               const __m256i si = _mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(pi0, cp[c][0]), _mm256_madd_epi16(pi2, cp[c][1])),
                                                   _mm256_add_epi32(_mm256_madd_epi16(pi4, cp[c][2]), _mm256_madd_epi16(pi6, cp[c][3])));
               const __m256i sq = _mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(pq0, cp[c][0]), _mm256_madd_epi16(pq2, cp[c][1])),
                                                   _mm256_add_epi32(_mm256_madd_epi16(pq4, cp[c][2]), _mm256_madd_epi16(pq6, cp[c][3])));
               _mm_storel_epi64((__m128i *) t[c], getlog8_avx2(si, sq));
           }
       }

       // Rearrange output
       for (int w = 0; w < 8; w++) {
           for (int c = 0; c < 5; c++) {
               oo[bpos] = t[c][w];
               if ((bpos += stride) >= stride6) bpos -= stride6-1;
           }
       }
   }

   // Remainder
   __m128i cv[5];
   for (int c = 0; c < 5; c++)
       cv[c] = _mm_loadl_epi64((const __m128i *) upsample_coeffs[c]);  // 8 x int8

   for (; n < nw; n++) {
       const iq16x8x2_avx2_t iq = load2_s16_avx2(in + 2 * n);
       for (int c = 0; c < 5; c++) {
           oo[bpos] = getlog_avx2(cv[c], iq);
           if ((bpos += stride) >= stride6) bpos -= stride6-1;
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

// Load 16 x uint8 and widen to 16 x uint16
static inline __attribute__((always_inline))
__m256i load_u8x16_u16_avx2(const uint8_t * restrict p)
{
    return _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *) p));
}

// Widened sum of 4 consecutive samples, p[0..3] .. p[15..18]
static inline __attribute__((always_inline))
__m256i sum4_u8x16_avx2(const uint8_t * restrict p)
{
    return _mm256_add_epi16(_mm256_add_epi16(load_u8x16_u16_avx2(p + 0), load_u8x16_u16_avx2(p + 1)),
                            _mm256_add_epi16(load_u8x16_u16_avx2(p + 2), load_u8x16_u16_avx2(p + 3)));
}

// Preamble metrics for 16*n16 consecutive positions of one phase
static inline __attribute__((always_inline))
void preamble_metric_block_avx2(const uint8_t * restrict p, int n16, unsigned thres, uint8_t * restrict out)
{
    // Preamble detection
    // P G P G, G G G P, G P G G, G G G G
    // gap = p[1] + p[8] + p[10] + p[15] + s4[3] + s4[11], s4[x] = p[x] + .. + p[x+3]
    // s4[11] for these 16 positions is s4[3] lanes 8-15, then lanes 0-7 of the next 16

    const __m128i vthres = _mm_set1_epi8((char) thres);
    const __m256i twelve = _mm256_set1_epi16(12);
    __m256i s4a = sum4_u8x16_avx2(p + 3);

    for (int k = 0; k < 16 * n16; k += 16, p += 16) {
        const __m256i s4n = sum4_u8x16_avx2(p + 19);           // s4[3] of the next 16
        const __m256i s4b = _mm256_permute2x128_si256(s4a, s4n, 0x21);

        const __m256i gap = _mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(load_u8x16_u16_avx2(p + 1), load_u8x16_u16_avx2(p + 8)),
                                                              _mm256_add_epi16(load_u8x16_u16_avx2(p + 10), load_u8x16_u16_avx2(p + 15))),
                                             _mm256_add_epi16(s4a, s4b));
        s4a = s4n;

        const __m128i m8 = _mm_min_epu8(_mm_min_epu8(_mm_loadu_si128((const __m128i *) (p + 0)),
                                                     _mm_loadu_si128((const __m128i *) (p + 2))),
                                        _mm_min_epu8(_mm_loadu_si128((const __m128i *) (p + 7)),
                                                     _mm_loadu_si128((const __m128i *) (p + 9))));

        // (12 * minp - gap) >> 1, negative 0, saturated to 255; 0 below thres
        const __m256i v = _mm256_srli_epi16(_mm256_subs_epu16(_mm256_mullo_epi16(_mm256_cvtepu8_epi16(m8), twelve), gap), 1);
        const __m128i m = _mm_packus_epi16(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        const __m128i ok = _mm_cmpeq_epi8(_mm_max_epu8(m, vthres), m);
        _mm_storeu_si128((__m128i *) (out + k), _mm_and_si128(m, ok));
    }
}

// Locate preamble
int STARCH_IMPL(locate_preamble, avx2) (const uint8_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
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

   // 3-way interleave of 16-bit elements a, b, c: o0 = a0 b0 c0 a1 b1 c1 a2 b2,
   // o1 = c2 a3 b3 c3 a4 b4 c4 a5, o2 = b5 c5 a6 b6 c6 a7 b7 c7
   const __m128i s0a = _mm_setr_epi8( 0,  1, -1, -1, -1, -1,  2,  3, -1, -1, -1, -1,  4,  5, -1, -1);
   const __m128i s0b = _mm_setr_epi8(-1, -1,  0,  1, -1, -1, -1, -1,  2,  3, -1, -1, -1, -1,  4,  5);
   const __m128i s0c = _mm_setr_epi8(-1, -1, -1, -1,  0,  1, -1, -1, -1, -1,  2,  3, -1, -1, -1, -1);
   const __m128i s1a = _mm_setr_epi8(-1, -1,  6,  7, -1, -1, -1, -1,  8,  9, -1, -1, -1, -1, 10, 11);
   const __m128i s1b = _mm_setr_epi8(-1, -1, -1, -1,  6,  7, -1, -1, -1, -1,  8,  9, -1, -1, -1, -1);
   const __m128i s1c = _mm_setr_epi8( 4,  5, -1, -1, -1, -1,  6,  7, -1, -1, -1, -1,  8,  9, -1, -1);
   const __m128i s2a = _mm_setr_epi8(-1, -1, -1, -1, 12, 13, -1, -1, -1, -1, 14, 15, -1, -1, -1, -1);
   const __m128i s2b = _mm_setr_epi8(10, 11, -1, -1, -1, -1, 12, 13, -1, -1, -1, -1, 14, 15, -1, -1);
   const __m128i s2c = _mm_setr_epi8(-1, -1, 10, 11, -1, -1, -1, -1, 12, 13, -1, -1, -1, -1, 14, 15);

   // All phases together, a block at a time, so the outputs can be interleaved on store
   int i = 0;
   while (i + 16 <= cmin) {
       const int n16 = MIN((cmin - i) / 16, PREAMBLE_BLOCK / 16);
       uint8_t t[6][PREAMBLE_BLOCK];

       for (int j = 0; j < 6; j++)
           preamble_metric_block_avx2(&pp[j*stride + i], n16, thres, t[j]);

       // mm[6*(i+k) + j] = t[j][k]: interleave phases in pairs, then 3-way as 16-bit
       uint8_t * restrict mp = &mm[6*i];
       for (int k = 0; k < 16 * n16; k += 8, mp += 48) {
           const __m128i a = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *) (t[0] + k)),
                                               _mm_loadl_epi64((const __m128i *) (t[1] + k)));
           const __m128i b = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *) (t[2] + k)),
                                               _mm_loadl_epi64((const __m128i *) (t[3] + k)));
           const __m128i c = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *) (t[4] + k)),
                                               _mm_loadl_epi64((const __m128i *) (t[5] + k)));

           const __m128i o0 = _mm_or_si128(_mm_or_si128(_mm_shuffle_epi8(a, s0a), _mm_shuffle_epi8(b, s0b)),
                                           _mm_shuffle_epi8(c, s0c));
           const __m128i o1 = _mm_or_si128(_mm_or_si128(_mm_shuffle_epi8(a, s1a), _mm_shuffle_epi8(b, s1b)),
                                           _mm_shuffle_epi8(c, s1c));
           const __m128i o2 = _mm_or_si128(_mm_or_si128(_mm_shuffle_epi8(a, s2a), _mm_shuffle_epi8(b, s2b)),
                                           _mm_shuffle_epi8(c, s2c));

           _mm_storeu_si128((__m128i *) (mp +  0), o0);
           _mm_storeu_si128((__m128i *) (mp + 16), o1);
           _mm_storeu_si128((__m128i *) (mp + 32), o2);
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

// 16 raw samples as 12-bit interleaved I,Q (samples 0-7 in [0], 8-15 in [1])
typedef struct {
    __m256i v[2];
} iq16x16_avx2_t;

static inline __attribute__((always_inline)) iq16x16_avx2_t iqconv_load_uc8_avx2(const uc8_t *in)
{
    const __m256i c = _mm256_set1_epi16(2040);
    iq16x16_avx2_t r;
    // (2 * v - 255) * 8 = 16 * v - 2040
    r.v[0] = _mm256_sub_epi16(_mm256_slli_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *) in)), 4), c);
    r.v[1] = _mm256_sub_epi16(_mm256_slli_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *) (in + 8))), 4), c);
    return r;
}

static inline __attribute__((always_inline)) iq16x16_avx2_t iqconv_load_sc16_avx2(const sc16_t *in)
{
    iq16x16_avx2_t r;
    r.v[0] = _mm256_srai_epi16(_mm256_loadu_si256((const __m256i *) in), 4);
    r.v[1] = _mm256_srai_epi16(_mm256_loadu_si256((const __m256i *) (in + 8)), 4);
    return r;
}

static inline __attribute__((always_inline)) __m256i clamp12_s16x16_avx2(const __m256i v)
{
    return _mm256_max_epi16(_mm256_min_epi16(v, _mm256_set1_epi16(2047)), _mm256_set1_epi16(-2048));
}

static inline __attribute__((always_inline)) iq16x16_avx2_t iqconv_load_sc16q11_avx2(const sc16_t *in)
{
    iq16x16_avx2_t r;
    r.v[0] = clamp12_s16x16_avx2(_mm256_loadu_si256((const __m256i *) in));
    r.v[1] = clamp12_s16x16_avx2(_mm256_loadu_si256((const __m256i *) (in + 8)));
    return r;
}

// Add the 8 x u32 lanes of v into the 4 x u64 lanes of acc
static inline __attribute__((always_inline)) __m256i acc_u32x8_u64_avx2(const __m256i acc, const __m256i v)
{
    const __m256i zero = _mm256_setzero_si256();
    return _mm256_add_epi64(acc, _mm256_add_epi64(_mm256_unpacklo_epi32(v, zero), _mm256_unpackhi_epi32(v, zero)));
}

// Same as iqconv_dc_block() for a full block. The block sum of (raw - corr) is
// sum(raw) - 16 * corr, so the serial part is just a few scalar ops.
static inline __attribute__((always_inline))
void iqconv_block_avx2(const iq16x16_avx2_t r, int16_t * restrict out, int32_t *off_i, int32_t *off_q,
                       __m256i *acc_level, __m256i *acc_power)
{
    const int32_t corr_i = (*off_i + (1 << (IQ_DC_SHIFT - 1))) >> IQ_DC_SHIFT;
    const int32_t corr_q = (*off_q + (1 << (IQ_DC_SHIFT - 1))) >> IQ_DC_SHIFT;

    // Raw sums: madd with (1, 0) picks I, with (0, 1) picks Q
    const __m256i rs = _mm256_add_epi16(r.v[0], r.v[1]);
    __m256i s = _mm256_hadd_epi32(_mm256_madd_epi16(rs, _mm256_set1_epi32(0x00000001)),
                                  _mm256_madd_epi16(rs, _mm256_set1_epi32(0x00010000)));
    __m128i s4 = _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1)); // [I, I, Q, Q]
    s4 = _mm_hadd_epi32(s4, s4);                                                           // [I, Q, I, Q]
    *off_i += _mm_cvtsi128_si32(s4) - 16 * corr_i;
    *off_q += _mm_extract_epi32(s4, 1) - 16 * corr_q;

    const __m256i corr = _mm256_set1_epi32((int32_t) (((uint32_t) corr_q << 16) | (uint16_t) corr_i));

    __m256i level = _mm256_setzero_si256(), power = _mm256_setzero_si256();
    for (int h = 0; h < 2; ++h) {
        const __m256i x = clamp12_s16x16_avx2(_mm256_sub_epi16(r.v[h], corr));
        _mm256_storeu_si256((__m256i *) (out + 16 * h), x);

        const __m256i m = _mm256_madd_epi16(x, x);              // I * I + Q * Q per sample
        power = _mm256_add_epi32(power, m);
        level = _mm256_add_epi32(level, isqrt_u32x8_avx2(_mm256_slli_epi32(m, IQ_LEVEL_SHIFT)));
    }

    *acc_level = acc_u32x8_u64_avx2(*acc_level, level);
    *acc_power = acc_u32x8_u64_avx2(*acc_power, power);
}

static inline __attribute__((always_inline)) uint64_t hsum_u64x4_avx2(const __m256i v)
{
    const __m128i s = _mm_add_epi64(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
    return (uint64_t) _mm_cvtsi128_si64(s) + (uint64_t) _mm_extract_epi64(s, 1);
}

#define IQDATA_CONV_AVX2_BODY(fmt)                                                            \
    int16_t * restrict out = (int16_t *) out_u;                                               \
    int32_t off_i = dc_in[0], off_q = dc_in[1];                                       \
    __m256i acc_level = _mm256_setzero_si256(), acc_power = _mm256_setzero_si256();           \
    unsigned n = 0;                                                                           \
                                                                                              \
    for (; n + IQ_DC_BLOCK <= len; n += IQ_DC_BLOCK)                                          \
        iqconv_block_avx2(iqconv_load_##fmt##_avx2(in + n), out + 2 * n, &off_i, &off_q, &acc_level, &acc_power); \
                                                                                              \
    dc_out[0] = off_i;                                                                     \
    dc_out[1] = off_q;                                                                     \
    uint64_t sum_level = hsum_u64x4_avx2(acc_level);                                          \
    uint64_t sum_power = hsum_u64x4_avx2(acc_power);                                          \
                                                                                              \
    if (n < len) {                                                                            \
        int16_t raw[2 * IQ_DC_BLOCK];                                                         \
        iqconv_raw_##fmt(in + n, raw, len - n);                                               \
        iqconv_dc_block(raw, out + 2 * n, len - n, dc_out, &sum_level, &sum_power);        \
    }                                                                                         \
                                                                                              \
    iqconv_stats(sum_level, sum_power, len, out_level, out_power);

void STARCH_IMPL(iqdata_conv_uc8, avx2) (const uc8_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                         const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_AVX2_BODY(uc8)
}

void STARCH_IMPL(iqdata_conv_sc16, avx2) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                          const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_AVX2_BODY(sc16)
}

void STARCH_IMPL(iqdata_conv_sc16q11, avx2) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len,
                                             const int32_t *dc_in, int32_t *dc_out, double *out_level, double *out_power)
{
    IQDATA_CONV_AVX2_BODY(sc16q11)
}


// Decode bits from chip pairs (see demod2400x_dsp.h): one byte = 8 pairs per 256-bit load,
// as 32-bit lanes with the first chip in the low half
void STARCH_IMPL(decode_bits, avx2) (const uint8_t * restrict p, unsigned nbytes, uint8_t * restrict msg,
                                     unsigned * restrict quality)
{
    const __m128i lo8 = _mm_set1_epi16(0x00FF);
    const __m128i reverse = _mm_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1);  // pair 0 -> bit 7
    const __m128i ones = _mm_set1_epi16(1);
    const __m128i cap = _mm_set1_epi16(DECODE_BITS_CAP);
    __m128i qual = _mm_setzero_si128();

    for (unsigned i = 0; i < nbytes; i++) {
        const __m128i x = _mm_loadu_si128((const __m128i *) (p + 16 * i));
        const __m128i a = _mm_and_si128(x, lo8);          // first chips, < 256 in 16-bit lanes
        const __m128i b = _mm_srli_epi16(x, 8);           // second chips
        const __m128i gt = _mm_shuffle_epi8(_mm_packs_epi16(_mm_cmpgt_epi16(a, b), _mm_setzero_si128()), reverse);
        msg[i] = (uint8_t) _mm_movemask_epi8(gt);
        qual = _mm_add_epi32(qual, _mm_madd_epi16(_mm_min_epi16(_mm_abs_epi16(_mm_sub_epi16(a, b)), cap), ones));
    }

    // Horizontal sum
    __m128i q = _mm_hadd_epi32(qual, qual);
    q = _mm_hadd_epi32(q, q);
    *quality = DECODE_BITS_QUALITY((uint32_t) _mm_cvtsi128_si32(q), nbytes);
}


// Coherent decode (see demod2400x_coherent.h for the exact definition), 8 lanes at a time
// with 128-bit mulhrs: ((a*b >> 14) + 1) >> 1 = (a*b + 2^14) >> 15, as coh_mulq15().
// 8 lanes match the 8-bit blocks and bytes of the frame; integer sums are exact, so
// the lane order doesn't change any result.

static inline __attribute__((always_inline)) int32_t coh_hsum_epi32_128(__m128i v)
{
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(v);
}

// Sum of 8 int16 lanes as int32
static inline __attribute__((always_inline)) int32_t coh_hsum_epi16_128(__m128i v)
{
    return coh_hsum_epi32_128(_mm_madd_epi16(v, _mm_set1_epi16(1)));
}

static inline __attribute__((always_inline))
void coh_rot_sse(__m128i xr, __m128i xi, __m128i pr, __m128i pi, __m128i *rr, __m128i *ri)
{
    *rr = _mm_sub_epi16(_mm_mulhrs_epi16(xr, pr), _mm_mulhrs_epi16(xi, pi));
    *ri = _mm_add_epi16(_mm_mulhrs_epi16(xr, pi), _mm_mulhrs_epi16(xi, pr));
}

// One chip: 8 interleaved I/Q pairs at 12 MHz position t, madd with [c,0,..] and [0,c,..]
// gives the I and Q tap products in pairs, then two horizontal adds finish both sums
static inline __attribute__((always_inline))
void coh_chip_avx2(const int16_t *iq, int t, const __m256i *ci, const __m256i *cq, int32_t *re, int32_t *im)
{
    const int n = t / 5, c = t % 5;
    const __m256i x = _mm256_loadu_si256((const __m256i *) (iq + 2 * n));
    const __m256i s = _mm256_hadd_epi32(_mm256_madd_epi16(x, ci[c]), _mm256_madd_epi16(x, cq[c]));
    // s = [I01 I23 Q01 Q23 | I45 I67 Q45 Q67]
    const __m128i h = _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1));
    const __m128i h2 = _mm_hadd_epi32(h, h);               // [I, Q, I, Q]
    *re = _mm_cvtsi128_si32(h2);
    *im = _mm_extract_epi32(h2, 1);
}

// Chips of bits 0 .. n-1 as int32 a/b, I/Q
static void coh_chips32_avx2(const int16_t *iq, int pos, int n, const __m256i *ci, const __m256i *cq,
                             int32_t *ar, int32_t *ai, int32_t *br, int32_t *bi)
{
    for (int i = 0; i < n; i++) {
        coh_chip_avx2(iq, pos + COH_DATA_OFFSET + 12 * i, ci, cq, &ar[i], &ai[i]);
        coh_chip_avx2(iq, pos + COH_DATA_OFFSET + 12 * i + 6, ci, cq, &br[i], &bi[i]);
    }
}

// 8 int32 -> 8 int16: shift, clamp to the chip range, pack (in order)
static inline __attribute__((always_inline)) __m128i coh_to16_avx2(const int32_t *v, int shift)
{
    const __m256i lim = _mm256_set1_epi32((1 << COH_CHIP_BITS) - 1), nlim = _mm256_set1_epi32(-((1 << COH_CHIP_BITS) - 1));
    __m256i x = _mm256_sra_epi32(_mm256_loadu_si256((const __m256i *) v), _mm_cvtsi32_si128(shift));
    x = _mm256_max_epi32(_mm256_min_epi32(x, lim), nlim);
    return _mm_packs_epi32(_mm256_castsi256_si128(x), _mm256_extracti128_si256(x, 1));
}

// One half of the frame, bits first .. first+55, on its own (as coh_half_generic)
// coh_wide_regroup, 8 blocks at a time: rotate by wide bin k (transposed table), sum
// each 4 with horizontal adds (int16, wrapping, so any order gives the same), >> 2
static inline __attribute__((always_inline))
void coh_wide_regroup_sse(const int16_t *wr, const int16_t *wi, int k, int16_t *br, int16_t *bi)
{
    for (int q = 0; q < 32; q += 8) {
        __m128i rr, ri;
        coh_rot_sse(_mm_loadu_si128((const __m128i *) (wr + q)), _mm_loadu_si128((const __m128i *) (wi + q)),
                    _mm_loadu_si128((const __m128i *) &coh_widet_re[k][q]), _mm_loadu_si128((const __m128i *) &coh_widet_im[k][q]), &rr, &ri);
        const __m128i pr = _mm_hadd_epi16(rr, rr), pi = _mm_hadd_epi16(ri, ri);
        const __m128i sr = _mm_srai_epi16(_mm_hadd_epi16(pr, pr), 2), si = _mm_srai_epi16(_mm_hadd_epi16(pi, pi), 2);
        const int32_t vr = _mm_cvtsi128_si32(sr), vi = _mm_cvtsi128_si32(si);
        memcpy(br + q / 4, &vr, 4);
        memcpy(bi + q / 4, &vi, 4);
    }
}

static int coh_half_avx2(const int16_t *iq, int pos, int first, unsigned thres, const __m256i *ci, const __m256i *cq,
                         uint8_t *out, uint32_t *rho2_out, uint32_t *f_out, uint32_t *k_out, uint32_t *q)
{
    int32_t a32r[COH_HALF], a32i[COH_HALF], b32r[COH_HALF], b32i[COH_HALF];
    int16_t ar[COH_HALF], ai[COH_HALF], br[COH_HALF], bi[COH_HALF];
    int16_t Pr[COH_HALF], Pi[COH_HALF], Qr[COH_HALF], Qi[COH_HALF];

    // Chips and their shift
    coh_chips32_avx2(iq, pos + 12 * first, COH_HALF, ci, cq, a32r, a32i, b32r, b32i);
    __m256i mx = _mm256_setzero_si256();
    for (int i = 0; i < COH_HALF; i += 8) {
        mx = _mm256_max_epu32(mx, _mm256_abs_epi32(_mm256_loadu_si256((const __m256i *) (a32r + i))));
        mx = _mm256_max_epu32(mx, _mm256_abs_epi32(_mm256_loadu_si256((const __m256i *) (a32i + i))));
        mx = _mm256_max_epu32(mx, _mm256_abs_epi32(_mm256_loadu_si256((const __m256i *) (b32r + i))));
        mx = _mm256_max_epu32(mx, _mm256_abs_epi32(_mm256_loadu_si256((const __m256i *) (b32i + i))));
    }
    __m128i m4 = _mm_max_epu32(_mm256_castsi256_si128(mx), _mm256_extracti128_si256(mx, 1));
    m4 = _mm_max_epu32(m4, _mm_shuffle_epi32(m4, _MM_SHUFFLE(1, 0, 3, 2)));
    m4 = _mm_max_epu32(m4, _mm_shuffle_epi32(m4, _MM_SHUFFLE(2, 3, 0, 1)));
    const int cs = coh_shift((uint32_t) _mm_cvtsi128_si32(m4), COH_CHIP_BITS);
    for (int i = 0; i < COH_HALF; i += 8) {
        _mm_storeu_si128((__m128i *) (ar + i), coh_to16_avx2(a32r + i, cs));
        _mm_storeu_si128((__m128i *) (ai + i), coh_to16_avx2(a32i + i, cs));
        _mm_storeu_si128((__m128i *) (br + i), coh_to16_avx2(b32r + i, cs));
        _mm_storeu_si128((__m128i *) (bi + i), coh_to16_avx2(b32i + i, cs));
    }

    // Carrier sample of each bit: the chip with the larger magnitude (|a|^2 >= |b|^2,
    // by madd of interleaved (re, im)), as a 16-bit lane mask per bit that is set when
    // b is taken; then 2-bit block sums of the samples (madd with ones), normalised
    __m128i pick_b[COH_NBLOCKS];
    int32_t sre[COH_WBLK], sim[COH_WBLK];
    const __m128i one16 = _mm_set1_epi16(1);
    for (int m = 0; m < COH_NBLOCKS; m++) {
        const int i = m * COH_BLOCK;
        const __m128i xar = _mm_loadu_si128((const __m128i *) (ar + i)), xai = _mm_loadu_si128((const __m128i *) (ai + i));
        const __m128i xbr = _mm_loadu_si128((const __m128i *) (br + i)), xbi = _mm_loadu_si128((const __m128i *) (bi + i));
        const __m128i al = _mm_unpacklo_epi16(xar, xai), ah = _mm_unpackhi_epi16(xar, xai);
        const __m128i bl = _mm_unpacklo_epi16(xbr, xbi), bh = _mm_unpackhi_epi16(xbr, xbi);
        pick_b[m] = _mm_packs_epi32(_mm_cmpgt_epi32(_mm_madd_epi16(bl, bl), _mm_madd_epi16(al, al)),
                                    _mm_cmpgt_epi32(_mm_madd_epi16(bh, bh), _mm_madd_epi16(ah, ah)));
        _mm_storeu_si128((__m128i *) (sre + 4 * m), _mm_madd_epi16(_mm_blendv_epi8(xar, xbr, pick_b[m]), one16));
        _mm_storeu_si128((__m128i *) (sim + 4 * m), _mm_madd_epi16(_mm_blendv_epi8(xai, xbi, pick_b[m]), one16));
    }
    uint32_t bmax = 0;
    for (int m = 0; m < COH_WBLK; m++) {
        if (coh_abs(sre[m]) > bmax) bmax = coh_abs(sre[m]);
        if (coh_abs(sim[m]) > bmax) bmax = coh_abs(sim[m]);
    }
    const int bs = coh_shift(bmax, COH_WIDE_BITS);
    int16_t wr[32] = { 0 }, wi[32] = { 0 };
    for (int m = 0; m < COH_WBLK; m++) {
        wr[m] = (int16_t) (sre[m] >> bs);
        wi[m] = (int16_t) (sim[m] >> bs);
    }

    // Wide search, first stage: the peak of each 14-block piece over 16 wide bins, 8 per
    // vector; re^2 + im^2 by madd of interleaved (re, im)
    int kc[2];
    for (int h = 0; h < 2; h++) {
        int32_t p1[COH_WBINS];
        for (int j = 0; j < COH_WBINS; j += 8) {
            __m128i s16r = _mm_setzero_si128(), s16i = _mm_setzero_si128();
            for (int m = 0; m < COH_WBLK / 2; m++) {
                __m128i rr, ri;
                coh_rot_sse(_mm_set1_epi16(wr[h * COH_WBLK / 2 + m]), _mm_set1_epi16(wi[h * COH_WBLK / 2 + m]),
                            _mm_loadu_si128((const __m128i *) &coh_wide_re[m][j]), _mm_loadu_si128((const __m128i *) &coh_wide_im[m][j]), &rr, &ri);
                s16r = _mm_add_epi16(s16r, rr);
                s16i = _mm_add_epi16(s16i, ri);
            }
            const __m128i lo = _mm_unpacklo_epi16(s16r, s16i), hi = _mm_unpackhi_epi16(s16r, s16i);
            _mm_storeu_si128((__m128i *) (p1 + j), _mm_madd_epi16(lo, lo));
            _mm_storeu_si128((__m128i *) (p1 + j + 4), _mm_madd_epi16(hi, hi));
        }
        kc[h] = coh_argmax(p1, COH_WBINS);
    }

    // Second stage: around each peak, the central fine bins; the stronger peak wins
    int k = kc[0], f = 0;
    int32_t pbest = -1;
    for (int c = 0; c < (kc[1] != kc[0] ? 2 : 1); c++) {
        int16_t br8[8], bi8[8];
        coh_wide_regroup_sse(wr, wi, kc[c], br8, bi8);
        int32_t p[COH_BINS_PAD] = { 0 };
        for (int j = COH_REF_LO; j < COH_REF_HI; j += 8) {
            __m128i s16r = _mm_setzero_si128(), s16i = _mm_setzero_si128();
            for (int m = 0; m < COH_NBLOCKS; m++) {
                __m128i rr, ri;
                coh_rot_sse(_mm_set1_epi16(br8[m]), _mm_set1_epi16(bi8[m]),
                            _mm_loadu_si128((const __m128i *) &coh_bin_re[m][j]), _mm_loadu_si128((const __m128i *) &coh_bin_im[m][j]), &rr, &ri);
                s16r = _mm_add_epi16(s16r, rr);
                s16i = _mm_add_epi16(s16i, ri);
            }
            const __m128i lo = _mm_unpacklo_epi16(s16r, s16i), hi = _mm_unpackhi_epi16(s16r, s16i);
            _mm_storeu_si128((__m128i *) (p + j), _mm_madd_epi16(lo, lo));
            _mm_storeu_si128((__m128i *) (p + j + 4), _mm_madd_epi16(hi, hi));
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
        __m128i rr, ri;
        if (i >= 8) {
            coh_rot_sse(_mm_loadu_si128((const __m128i *) (Pr + i - 8)), _mm_loadu_si128((const __m128i *) (Pi + i - 8)),
                        _mm_set1_epi16(r8r), _mm_set1_epi16(r8i), &rr, &ri);
            _mm_storeu_si128((__m128i *) (Pr + i), rr);
            _mm_storeu_si128((__m128i *) (Pi + i), ri);
        }
        coh_rot_sse(_mm_loadu_si128((const __m128i *) (Pr + i)), _mm_loadu_si128((const __m128i *) (Pi + i)),
                    _mm_set1_epi16(rcr), _mm_set1_epi16(rci), &rr, &ri);
        _mm_storeu_si128((__m128i *) (Qr + i), rr);
        _mm_storeu_si128((__m128i *) (Qi + i), ri);
    }

#define COH_LD(p) _mm_loadu_si128((const __m128i *) (p))
    // Correlation
    const __m128i ones = _mm_set1_epi16(1);
    __m128i hre = _mm_setzero_si128(), him = _mm_setzero_si128(), e2v = _mm_setzero_si128();
    for (int i = 0; i < COH_HALF; i += 8) {
        // rot(a, P) or rot(b, Q): select the chip and its phasor, then rotate once
        const __m128i k = pick_b[i / 8];
        __m128i cre, cim;
        coh_rot_sse(_mm_blendv_epi8(COH_LD(ar + i), COH_LD(br + i), k), _mm_blendv_epi8(COH_LD(ai + i), COH_LD(bi + i), k),
                    _mm_blendv_epi8(COH_LD(Pr + i), COH_LD(Qr + i), k), _mm_blendv_epi8(COH_LD(Pi + i), COH_LD(Qi + i), k),
                    &cre, &cim);
        hre = _mm_add_epi32(hre, _mm_madd_epi16(cre, ones));
        him = _mm_add_epi32(him, _mm_madd_epi16(cim, ones));
        const __m128i sr = _mm_srai_epi16(cre, 4), si = _mm_srai_epi16(cim, 4);
        e2v = _mm_add_epi32(e2v, _mm_add_epi32(_mm_madd_epi16(sr, sr), _mm_madd_epi16(si, si)));
    }
    const int32_t h_re = coh_hsum_epi32_128(hre), h_im = coh_hsum_epi32_128(him);
    const uint32_t rho2 = coh_rho2(h_re, h_im, (uint32_t) coh_hsum_epi32_128(e2v));
    *rho2_out = rho2;
    *f_out = (uint32_t) f;
    *k_out = (uint32_t) k;
    if (rho2 < thres)
        return 0;

    // Slice, one byte per vector
    int16_t gre, gim;
    coh_channel16(h_re, h_im, &gre, &gim);
    const __m128i gr = _mm_set1_epi16(gre), gi = _mm_set1_epi16(gim);
    const __m128i reverse = _mm_setr_epi8(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
    __m128i qual = _mm_setzero_si128(), pw = _mm_setzero_si128();
    for (int i = 0; i < COH_HALF; i += 8) {
        __m128i xar, xai, xbr, xbi;
        coh_rot_sse(COH_LD(ar + i), COH_LD(ai + i), COH_LD(Pr + i), COH_LD(Pi + i), &xar, &xai);
        coh_rot_sse(COH_LD(br + i), COH_LD(bi + i), COH_LD(Qr + i), COH_LD(Qi + i), &xbr, &xbi);
        const __m128i ya = _mm_add_epi16(_mm_mulhrs_epi16(xar, gr), _mm_mulhrs_epi16(xai, gi));
        const __m128i yb = _mm_add_epi16(_mm_mulhrs_epi16(xbr, gr), _mm_mulhrs_epi16(xbi, gi));
        const __m128i gt = _mm_shuffle_epi8(_mm_cmpgt_epi16(ya, yb), reverse);   // lane 0 -> bit 7
        out[(first + i) >> 3] = (uint8_t) _mm_movemask_epi8(_mm_packs_epi16(gt, _mm_setzero_si128()));
        qual = _mm_add_epi32(qual, _mm_madd_epi16(_mm_abs_epi16(_mm_sub_epi16(ya, yb)), ones));
        pw = _mm_add_epi32(pw, _mm_madd_epi16(_mm_add_epi16(_mm_abs_epi16(ya), _mm_abs_epi16(yb)), ones));
    }
#undef COH_LD
    *q = coh_quality((uint32_t) coh_hsum_epi32_128(qual), (uint32_t) coh_hsum_epi32_128(pw));
    return 1;
}

void STARCH_IMPL(coherent_decode, avx2) (const int16_t * restrict iq, int pos, unsigned thres,
                                         uint8_t * restrict out, uint32_t * restrict info)
{
    __m256i ci[5], cq[5];
    for (int c = 0; c < 5; c++) {
        int16_t vi[16], vq[16];
        for (int j = 0; j < 8; j++) {
            vi[2 * j] = upsample_coeffs[c][j]; vi[2 * j + 1] = 0;
            vq[2 * j] = 0; vq[2 * j + 1] = upsample_coeffs[c][j];
        }
        ci[c] = _mm256_loadu_si256((const __m256i *) vi);
        cq[c] = _mm256_loadu_si256((const __m256i *) vq);
    }

    memset(out, 0, 14);
    memset(info, 0, COH_INFO_N * sizeof(*info));

    if (!coh_half_avx2(iq, pos, 0, thres, ci, cq, out, &info[COH_INFO_RHO2], &info[COH_INFO_FINE], &info[COH_INFO_WIDE], &info[COH_INFO_Q1]))
        return;
    info[COH_INFO_DECODED] = 1;
    // The second half whatever the DF says (as the magnitude decoder): scoring can
    // correct the DF bits, and every short DF is one bit from a long one
    coh_half_avx2(iq, pos, COH_HALF, 0, ci, cq, out, &info[COH_INFO_RHO2B], &info[COH_INFO_FINE2], &info[COH_INFO_WIDE2], &info[COH_INFO_Q2]);
}
