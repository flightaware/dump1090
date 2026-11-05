// demod2400x_coeffs.h: upsampler coefficients for the DEMOD_2400X starch kernels
// (dsp/impl/demod2400x.*.c only)

#ifndef DEMOD2400X_COEFFS_H
#define DEMOD2400X_COEFFS_H

#include <stdint.h>

// 2.4 -> 12 MHz polyphase FIR, 5 phases x 8 taps, gain 128 per phase
/*static const int8_t upsample_coeffs[5][8] = {
    {   -1,    4,  -10,  125,   14,   -6,    3,   -1 }, // phase=0 sum=128 energy=15984 gd=3.0938
    {   -3,    8,  -21,  109,   47,  -16,    7,   -3 }, // phase=1 sum=128 energy=14918 gd=3.2969
    {   -4,   10,  -23,   81,   81,  -23,   10,   -4 }, // phase=2 sum=128 energy=14412 gd=3.5000
    {   -3,    7,  -16,   47,  109,  -21,    8,   -3 }, // phase=3 sum=128 energy=14918 gd=3.7031
    {   -1,    3,   -6,   14,  125,  -10,    4,   -1 }, // phase=4 sum=128 energy=15984 gd=3.9062
};*/

// sinc fc=1.2 win=7.8 a=0 gain=128
// gain 128 per phase, noise gain 0.992
/*static const int8_t upsample_coeffs[5][8] = {
    {  -3,   5, -11, 127,  14,  -6,   3,  -1 }, // phase=0 sum=128
    {  -6,  12, -24, 113,  47, -18,   9,  -5 }, // phase=1 sum=128
    {  -7,  13, -26,  84,  84, -26,  13,  -7 }, // phase=2 sum=128
    {  -5,   9, -18,  47, 113, -24,  12,  -6 }, // phase=3 sum=128
    {  -1,   3,  -6,  14, 127, -11,   5,  -3 }, // phase=4 sum=128
};*/

// sinc fc=1.2 win=12 gain=126
// gain 126 per phase, noise gain 1.035
/*static const int8_t upsample_coeffs[5][8] = {
    {  -4,   6, -11, 126,  14,  -6,   4,  -3 }, // phase=0 sum=126
    {  -9,  14, -25, 113,  48, -19,  11,  -7 }, // phase=1 sum=126
    { -10,  15, -27,  85,  85, -27,  15, -10 }, // phase=2 sum=126
    {  -7,  11, -19,  48, 113, -25,  14,  -9 }, // phase=3 sum=126
    {  -3,   4,  -6,  14, 126, -11,   6,  -4 }, // phase=4 sum=126
};*/

// Least-squares fractional delay: each phase fitted to delay 3.1 + 0.2p input
// samples with flat gain over 0..1.0 MHz, then rounded with a common offset
// (binary search) so every phase sums to exactly 128 (gen/ls_coeffs.py 1.0 128).
// Group delay error between phases <= 0.028 samples up to 0.8 MHz, noise gain 0.94.
#define UPSAMPLE_ABS_SUM 244    // largest sum of |taps| over the phases
static const int8_t upsample_coeffs[5][8] = {
    {  -2,   4, -10, 126,  14,  -6,   3,  -1 }, // phase=0 sum=128
    {  -4,  10, -22, 109,  47, -17,   9,  -4 }, // phase=1 sum=128
    {  -5,  12, -24,  81,  81, -24,  12,  -5 }, // phase=2 sum=128
    {  -4,   9, -17,  47, 109, -22,  10,  -4 }, // phase=3 sum=128
    {  -1,   3,  -6,  14, 126, -10,   4,  -2 }, // phase=4 sum=128
};

// Filter outputs are rounded by (x + 2^(UPSAMPLE_SHIFT-1)) >> UPSAMPLE_SHIFT. With 12-bit input
// (|x| <= 2048) each component is at most 2048 * UPSAMPLE_ABS_SUM >> UPSAMPLE_SHIFT, and the
// power I^2 + Q^2 must stay below 2^31: signed 32-bit arithmetic, and P + 1 stays a positive int32 for AVX2's
// signed conversion in demod_log8().
#define UPSAMPLE_SHIFT 4
#define UPSAMPLE_ROUND (1 << (UPSAMPLE_SHIFT - 1))
#define UPSAMPLE_MAX_COMPONENT ((2048LL * UPSAMPLE_ABS_SUM + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT)
_Static_assert(2 * UPSAMPLE_MAX_COMPONENT * UPSAMPLE_MAX_COMPONENT < (1LL << 31),
               "upsampler power can overflow: raise UPSAMPLE_SHIFT");

// The phases are mirror images: phase 4-c is phase c reversed and phase 2 is symmetric.
// With u[k] = x[k] + x[7-k] and v[k] = x[k] - x[7-k], k = 0..3:
//   2 * out[c]   = sum pc[c][k] u[k] + sum mc[c][k] v[k]
//   2 * out[4-c] = sum pc[c][k] u[k] - sum mc[c][k] v[k]
// with pc[c][k] = a[c][k] + a[c][7-k] and mc[c][k] = a[c][k] - a[c][7-k], which halves the
// multiplies. Exact integers (u, v fit in 16 bits), so the result is identical to the direct
// form; the doubled sums are rounded by UPSAMPLE_SHIFT + 1. Used by the SIMD versions, which
// fall back to the direct form if the table isn't mirrored.
static inline int upsample_coeffs_mirrored(void)
{
    for (int c = 0; c < 5; c++)
        for (int k = 0; k < 8; k++)
            if (upsample_coeffs[c][k] != upsample_coeffs[4 - c][7 - k])
                return 0;
    return 1;
}

static inline void upsample_mirror_coeffs(int16_t pc[3][4], int16_t mc[3][4])
{
    for (int c = 0; c < 3; c++)
        for (int k = 0; k < 4; k++) {
            pc[c][k] = (int16_t) (upsample_coeffs[c][k] + upsample_coeffs[c][7 - k]);
            mc[c][k] = (int16_t) (upsample_coeffs[c][k] - upsample_coeffs[c][7 - k]);
        }
}

// Exact floor(sqrt(x)) for x < 2^31, for USE_MAGNITUDE.
// After adding 2^-6 the truncated sqrtf is the exact result or within one of it
// (sqrtf may be an estimate under -ffast-math), fixed with integer checks. Exact,
// so generic, NEON and AVX2
// (which use the same steps in vector form) give bit-identical results.
static inline __attribute__((always_inline))  uint32_t demod_isqrt(uint32_t x)
{
    uint32_t q = (uint32_t) (__builtin_sqrtf((float) x) + 1.0f / 64);
    // Correct in both directions: under -ffast-math the sqrt may be an estimate
    q -= (q * q > x);
    return q + ((q + 1) * (q + 1) <= x);
}

// Square root used by getpow() in USE_MAGNITUDE mode. Override on the command line
// to compare, e.g. -DDEMOD_SQRT(x)='((uint32_t)sqrtf((float)(x)))'
#ifndef DEMOD_SQRT
#define DEMOD_SQRT(x) demod_isqrt(x)
#endif

// 8-bit log of the upsampler power, L ~ 8 log2(P + 1): the float of P + 1 is
// 2^e (1 + f), so its bits >> 20 are 8 (e + 127) + the top 3 bits of f, and
// L = 8e + floor(8f). f is the linear mantissa, at most 0.69 of a step below
// 8 log2(1 + f). The conversion rounds to nearest in C, NEON and AVX2 alike, so
// all versions give the same codes. P < 2^31, so L <= 247.
static inline __attribute__((always_inline)) uint8_t demod_log8(uint32_t pwr)
{
    union { float f; uint32_t u; } v = { .f = (float) (pwr + 1) };
    return (uint8_t) ((v.u >> 20) - (127u << 3));
}

#endif
