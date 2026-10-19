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


static inline __attribute__((always_inline)) unsigned getpow_avx2(const __m128i cv8, const iq16x8x2_avx2_t iq, int shift)
{
    const __m128i cv = _mm_cvtepi8_epi16(cv8);  // widen coefficients to 16-bit

    // madd: multiply 8 pairs and add neighbours -> 4 x int32 partial sums
    int32_t resi = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[0], cv)) + 16) >> 5;
    int32_t resq = (hsum_s32x4_avx2(_mm_madd_epi16(iq.val[1], cv)) + 16) >> 5;

    uint32_t pwr = (uint32_t)(resi*resi) + (uint32_t)(resq*resq);

#ifdef USE_MAGNITUDE
    (void)shift;
    pwr = demod_isqrt(pwr);
#else
    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;
#endif

    return pwr;
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

// Power (or magnitude) of 8 filter outputs, as getpow_avx2()
static inline __attribute__((always_inline))
__m128i getpow8_avx2(__m256i si, __m256i sq, int shift)
{
    const __m256i round = _mm256_set1_epi32(16);
    si = _mm256_srai_epi32(_mm256_add_epi32(si, round), 5);   // (x+16)>>5
    sq = _mm256_srai_epi32(_mm256_add_epi32(sq, round), 5);
    __m256i pwr = _mm256_add_epi32(_mm256_mullo_epi32(si, si), _mm256_mullo_epi32(sq, sq));

#ifdef USE_MAGNITUDE
    (void)shift;
    pwr = isqrt_u32x8_avx2(pwr);
#else
    pwr = _mm256_srl_epi32(pwr, _mm_cvtsi32_si128(shift));    // Set gain
    pwr = _mm256_min_epu32(pwr, _mm256_set1_epi32(65535));
#endif

    return _mm_packus_epi32(_mm256_castsi256_si128(pwr), _mm256_extracti128_si256(pwr, 1));
}

// 16-bit pairs (v[k], v[k+1]) for 8 consecutive windows, from v[0..15] = lo, hi
#define PAIRS_AVX2(lo, hi, k)                                                                   \
    _mm256_inserti128_si256(                                                                    \
        _mm256_castsi128_si256(_mm_unpacklo_epi16(_mm_alignr_epi8(hi, lo, 2 * (k)),             \
                                                  _mm_alignr_epi8(hi, lo, 2 * (k) + 2))),       \
        _mm_unpackhi_epi16(_mm_alignr_epi8(hi, lo, 2 * (k)), _mm_alignr_epi8(hi, lo, 2 * (k) + 2)), 1)

// Upsample
int STARCH_IMPL(upsample12, avx2) (const int16_t * restrict in, int len, int shift, uint16_t * restrict oo)
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

   // 8 windows at a time; reads samples n .. n+15
   for (; n + 9 <= nw; n += 8) {
       const iq16x8x2_avx2_t a = load2_s16_avx2(in + 2 * n);      // samples n .. n+7
       const iq16x8x2_avx2_t b = load2_s16_avx2(in + 2 * n + 16); // samples n+8 .. n+15

       const __m256i pi0 = PAIRS_AVX2(a.val[0], b.val[0], 0), pq0 = PAIRS_AVX2(a.val[1], b.val[1], 0);
       const __m256i pi2 = PAIRS_AVX2(a.val[0], b.val[0], 2), pq2 = PAIRS_AVX2(a.val[1], b.val[1], 2);
       const __m256i pi4 = PAIRS_AVX2(a.val[0], b.val[0], 4), pq4 = PAIRS_AVX2(a.val[1], b.val[1], 4);
       const __m256i pi6 = PAIRS_AVX2(a.val[0], b.val[0], 6), pq6 = PAIRS_AVX2(a.val[1], b.val[1], 6);

       uint16_t t[5][8];
       for (int c = 0; c < 5; c++) {
           const __m256i si = _mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(pi0, cp[c][0]), _mm256_madd_epi16(pi2, cp[c][1])),
                                               _mm256_add_epi32(_mm256_madd_epi16(pi4, cp[c][2]), _mm256_madd_epi16(pi6, cp[c][3])));
           const __m256i sq = _mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(pq0, cp[c][0]), _mm256_madd_epi16(pq2, cp[c][1])),
                                               _mm256_add_epi32(_mm256_madd_epi16(pq4, cp[c][2]), _mm256_madd_epi16(pq6, cp[c][3])));
           _mm_storeu_si128((__m128i *) t[c], getpow8_avx2(si, sq, shift));
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
           oo[bpos] = getpow_avx2(cv[c], iq, shift);
           if ((bpos += stride) >= stride6) bpos -= stride6-1;
       }
   }

   return nt;
}


// Exact floor(n / d) for d > 0, n < 2^28 and n / d < 256 (the preamble metric range)
static inline __attribute__((always_inline))
__m256i div_u32x8_avx2(const __m256i n, const __m256i d)
{
    // Values are < 2^31, so the signed conversions and compares are fine
    // -ffast-math lets the compiler use a reciprocal estimate for the divide, so
    // truncation is within one either way: correct in both directions
    const __m256i q = _mm256_cvttps_epi32(_mm256_div_ps(_mm256_cvtepi32_ps(n), _mm256_cvtepi32_ps(d)));
    const __m256i qd = _mm256_mullo_epi32(q, d);
    // q + 1, -1 if q * d > n (too large), -1 if (q + 1) * d > n (not too small)
    return _mm256_add_epi32(_mm256_add_epi32(q, _mm256_set1_epi32(1)),
                            _mm256_add_epi32(_mm256_cmpgt_epi32(qd, n),
                                             _mm256_cmpgt_epi32(_mm256_add_epi32(qd, d), n)));
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

// Load 8 x uint16 and widen to 8 x uint32
static inline __attribute__((always_inline))
__m256i load_u16x8_u32_avx2(const uint16_t * restrict p)
{
    return _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i *) p));
}

// Widened sum of 4 consecutive samples, p[0..3] .. p[7..10]
static inline __attribute__((always_inline))
__m256i sum4_u16x8_avx2(const uint16_t * restrict p)
{
    return _mm256_add_epi32(_mm256_add_epi32(load_u16x8_u32_avx2(p + 0), load_u16x8_u32_avx2(p + 1)),
                            _mm256_add_epi32(load_u16x8_u32_avx2(p + 2), load_u16x8_u32_avx2(p + 3)));
}

// Preamble metrics for 8*n8 consecutive positions of one phase
static inline __attribute__((always_inline))
void preamble_metric_block_avx2(const uint16_t * restrict p, int n8, unsigned thres, uint8_t * restrict out)
{
    // Preamble detection
    // P G P G, G G G P, G P G G, G G G G
    // gap = p[1] + p[8] + p[10] + p[15] + s4[3] + s4[11], s4[x] = p[x] + .. + p[x+3]
    // s4[11] for these 8 positions is s4[3] for the next 8

    const __m256i one = _mm256_set1_epi32(1);
    const __m256i vthres = _mm256_set1_epi32((int) thres);
    __m256i s4a = sum4_u16x8_avx2(p + 3);

    for (int k = 0; k < 8 * n8; k += 8, p += 8) {
        const __m256i s4b = sum4_u16x8_avx2(p + 11);

        __m256i gap = _mm256_add_epi32(_mm256_add_epi32(load_u16x8_u32_avx2(p + 1), load_u16x8_u32_avx2(p + 8)),
                                       _mm256_add_epi32(load_u16x8_u32_avx2(p + 10), load_u16x8_u32_avx2(p + 15)));
        gap = _mm256_add_epi32(_mm256_add_epi32(gap, one), _mm256_add_epi32(s4a, s4b));
        s4a = s4b;

        // Metric = (256 * KK * minp) / (KK * minp + gap + 1)
#ifdef PREAMBLE_MIN_PULSE
        const __m128i m16 = _mm_min_epu16(_mm_min_epu16(_mm_loadu_si128((const __m128i *) (p + 0)),
                                                        _mm_loadu_si128((const __m128i *) (p + 2))),
                                          _mm_min_epu16(_mm_loadu_si128((const __m128i *) (p + 7)),
                                                        _mm_loadu_si128((const __m128i *) (p + 9))));
        const __m256i minp = _mm256_cvtepu16_epi32(m16);
#endif
#ifdef PREAMBLE_SUM_PULSE
        const __m256i minp = _mm256_add_epi32(_mm256_add_epi32(load_u16x8_u32_avx2(p + 0), load_u16x8_u32_avx2(p + 2)),
                                              _mm256_add_epi32(load_u16x8_u32_avx2(p + 7), load_u16x8_u32_avx2(p + 9)));
#endif
        const __m256i n = _mm256_mullo_epi32(minp, _mm256_set1_epi32(256 * PREAMBLE_KK));
        const __m256i d = _mm256_add_epi32(gap, _mm256_mullo_epi32(minp, _mm256_set1_epi32(PREAMBLE_KK)));

        // Most positions are noise below the threshold: skip the divide when all 8 are
        // (values < 2^31, so the signed compare is fine)
        const __m256i below = _mm256_cmpgt_epi32(_mm256_mullo_epi32(d, vthres), n);
        if (_mm256_movemask_epi8(below) == -1) {
            _mm_storel_epi64((__m128i *) (out + k), _mm_setzero_si128());
            continue;
        }
        const __m256i q = _mm256_andnot_si256(below, div_u32x8_avx2(n, d));

        // Metric < 256
        const __m128i q16 = _mm_packus_epi32(_mm256_castsi256_si128(q), _mm256_extracti128_si256(q, 1));
        _mm_storel_epi64((__m128i *) (out + k), _mm_packus_epi16(q16, q16));
    }
}

// Locate preamble
int STARCH_IMPL(locate_preamble, avx2) (const uint16_t * restrict pp, int nt, unsigned thres, uint8_t * restrict mm)
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
   while (i + 8 <= cmin) {
       const int n8 = MIN((cmin - i) / 8, PREAMBLE_BLOCK / 8);
       uint8_t t[6][PREAMBLE_BLOCK];

       for (int j = 0; j < 6; j++)
           preamble_metric_block_avx2(&pp[j*stride + i], n8, thres, t[j]);

       // mm[6*(i+k) + j] = t[j][k]: interleave phases in pairs, then 3-way as 16-bit
       uint8_t * restrict mp = &mm[6*i];
       for (int k = 0; k < 8 * n8; k += 8, mp += 48) {
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
       i += 8 * n8;
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
