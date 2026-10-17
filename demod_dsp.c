#include <stdio.h>
#include <stdint.h>
#include "demod_2400x.h"

#if defined(__aarch64__)
    #define TARGET_NEON
    #define TARGET_SIMD
#elif defined(__arm__) && defined(__ARM_NEON)
    #define TARGET_NEON
#elif defined(__x86_64__) && defined(__AVX2__)
    #define TARGET_AVX2
#else
    #define TARGET_GENERIC
#endif

static const int8_t coeffs[5][8] = {
    {   -1,    4,  -10,  125,   14,   -6,    3,   -1 }, // phase=0 sum=128 energy=15984 gd=3.0938
    {   -3,    8,  -21,  109,   47,  -16,    7,   -3 }, // phase=1 sum=128 energy=14918 gd=3.2969
    {   -4,   10,  -23,   81,   81,  -23,   10,   -4 }, // phase=2 sum=128 energy=14412 gd=3.5000
    {   -3,    7,  -16,   47,  109,  -21,    8,   -3 }, // phase=3 sum=128 energy=14918 gd=3.7031
    {   -1,    3,   -6,   14,  125,  -10,    4,   -1 }, // phase=4 sum=128 energy=15984 gd=3.9062
};


#ifdef TARGET_NEON
#include <arm_neon.h>

// Horizontal add
static inline __attribute__((always_inline)) int hsum_s16x8(const int16x8_t v)
{
#ifdef TARGET_SIMD
    return vaddvq_s16(v);
#else
    int16x4_t s = vadd_s16(vget_low_s16(v),
                           vget_high_s16(v));
    s = vpadd_s16(s, s);
    s = vpadd_s16(s, s);
    return vget_lane_s16(s, 0);
#endif
}

// Horizontal add
static inline __attribute__((always_inline))
uint32_t hsum_u32x4(const uint32x4_t v)
{
#ifdef TARGET_SIMD
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
int32x2_t hsum2_s32x4(const int32x4_t a, const int32x4_t b)
{
#ifdef TARGET_SIMD
    int32x4_t ab = vpaddq_s32(a, b);                        // [a0+a1, a2+a3, b0+b1, b2+b3]
    return vpadd_s32(vget_low_s32(ab), vget_high_s32(ab));
#else
    return vpadd_s32(vadd_s32(vget_low_s32(a), vget_high_s32(a)),
                     vadd_s32(vget_low_s32(b), vget_high_s32(b)));
#endif
}


static inline __attribute__((always_inline)) unsigned getpow(const int8x8_t cv8, const int16x8x2_t iq, int shift)
{
    const int16x8_t cv = vmovl_s8(cv8);  // widen coefficients to 16-bit
    int32x4_t si, sq;

    si = vmull_s16(vget_low_s16(iq.val[0]), vget_low_s16(cv));
    si = vmlal_s16(si, vget_high_s16(iq.val[0]), vget_high_s16(cv));

    sq = vmull_s16(vget_low_s16(iq.val[1]), vget_low_s16(cv));
    sq = vmlal_s16(sq, vget_high_s16(iq.val[1]), vget_high_s16(cv));

    int32x2_t res = hsum2_s32x4(si, sq);                       // [resi, resq]
    res = vrshr_n_s32(res, 5);                                 // (x+16)>>5 on both

    uint32x2_t sqr = vreinterpret_u32_s32(vmul_s32(res, res)); // [resi^2, resq^2]
    uint32_t pwr = vget_lane_u32(sqr, 0) + vget_lane_u32(sqr, 1);

    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;

    return pwr;
}

// Upsample
int upsample12(const int16_t * restrict in, int len, int shift, uint16_t * restrict oo) {

   if (len < 8) return 0;

   int8x8_t c0 = vld1_s8(coeffs[0]); 
   int8x8_t c1 = vld1_s8(coeffs[1]); 
   int8x8_t c2 = vld1_s8(coeffs[2]); 
   int8x8_t c3 = vld1_s8(coeffs[3]); 
   int8x8_t c4 = vld1_s8(coeffs[4]); 
   
   int16x8x2_t iq = vld2q_s16(in);
      
   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int stride6 = 6*stride;
   int bpos = 0;
   
   in += 16;
   len -= 8;
   while (1) {
         
        // Rearrage output
        oo[bpos] = getpow(c0, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;
        
        oo[bpos] = getpow(c1, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;
        
        oo[bpos] = getpow(c2, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;
        
        oo[bpos] = getpow(c3, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;
        
        oo[bpos] = getpow(c4, iq, shift);
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
int locate_preamble(const uint16_t * restrict pp, int nt, uint8_t * restrict mm) {

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
          uint32_t gap = hsum_u32x4(gs);

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

#endif


#ifdef TARGET_GENERIC

// Get power
static inline unsigned getpow(
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
int upsample12(
    const int16_t * restrict in,
    int len, int shift,
    uint16_t * restrict oo)
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

        oo[bpos] = getpow(coeffs[0], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow(coeffs[1], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow(coeffs[2], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow(coeffs[3], ii, qq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6 - 1;

        oo[bpos] = getpow(coeffs[4], ii, qq, shift);
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
int locate_preamble(const uint16_t * restrict pp, int nt, uint8_t * restrict mm) {

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
#endif

#ifdef TARGET_AVX2
#include <immintrin.h>

// Two vectors of 8 x int16: val[0] = I, val[1] = Q (same layout as NEON's int16x8x2_t)
typedef struct {
    __m128i val[2];
} iq16x8x2_t;

// Horizontal add
static inline __attribute__((always_inline)) int32_t hsum_s32x4(const __m128i v)
{
    __m128i s = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}

// Horizontal add
static inline __attribute__((always_inline)) uint32_t hsum_u32x4(const __m128i v)
{
    return (uint32_t) hsum_s32x4(v);
}

// Load 8 interleaved I/Q samples and deinterleave them (like NEON's vld2q_s16)
static inline __attribute__((always_inline)) iq16x8x2_t load2_s16(const int16_t *in)
{
    const __m128i deint = _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13,  2, 3, 6, 7, 10, 11, 14, 15);

    __m128i a = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *) (in + 0)), deint); // I0-3 Q0-3
    __m128i b = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *) (in + 8)), deint); // I4-7 Q4-7

    iq16x8x2_t iq;
    iq.val[0] = _mm_unpacklo_epi64(a, b);  // I0-7
    iq.val[1] = _mm_unpackhi_epi64(a, b);  // Q0-7
    return iq;
}


static inline __attribute__((always_inline)) unsigned getpow(const __m128i cv8, const iq16x8x2_t iq, int shift)
{
    const __m128i cv = _mm_cvtepi8_epi16(cv8);  // widen coefficients to 16-bit

    // madd: multiply 8 pairs and add neighbours -> 4 x int32 partial sums
    int32_t resi = (hsum_s32x4(_mm_madd_epi16(iq.val[0], cv)) + 16) >> 5;
    int32_t resq = (hsum_s32x4(_mm_madd_epi16(iq.val[1], cv)) + 16) >> 5;

    uint32_t pwr = (uint32_t)(resi*resi) + (uint32_t)(resq*resq);

    pwr >>= shift; // Set gain
    if (pwr > 65535) pwr = 65535;

    return pwr;
}


// Upsample
int upsample12(const int16_t * restrict in, int len, int shift, uint16_t * restrict oo) {

   if (len < 8) return 0;

   __m128i c0 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[0]);  // 8 x int8
   __m128i c1 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[1]);  // 8 x int8
   __m128i c2 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[2]);  // 8 x int8
   __m128i c3 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[3]);  // 8 x int8
   __m128i c4 = _mm_loadl_epi64((const __m128i *) upsample_coeffs[4]);  // 8 x int8

   iq16x8x2_t iq = load2_s16(in);

   int nt = NU_SAMP(len); // Total samples
   int stride = NU_STRIDE(nt); // Stride
   int stride6 = 6*stride;
   int bpos = 0;

   in += 16;
   len -= 8;
   while (1) {

        // Rearrage output
        oo[bpos] = getpow(c0, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow(c1, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow(c2, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow(c3, iq, shift);
        if ((bpos += stride) >= stride6) bpos -= stride6-1;

        oo[bpos] = getpow(c4, iq, shift);
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
int locate_preamble(const uint16_t * restrict pp, int nt, uint8_t * restrict mm) {

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
          uint32_t gap = hsum_u32x4(gs);

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
          uint32_t minp = hsum_u32x4(_mm_cvtepu16_epi32(px));
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

#endif

