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
// (binary search) so every phase sums to exactly 112.
// Group delay error between phases <= 0.046 samples up to 0.8 MHz, noise gain 0.94.
static const int8_t upsample_coeffs[5][8] = {
    {  -2,   4,  -9, 110,  12,  -5,   3,  -1 }, // phase=0 sum=112
    {  -4,   9, -19,  95,  41, -15,   8,  -3 }, // phase=1 sum=112
    {  -4,  10, -21,  71,  71, -21,  10,  -4 }, // phase=2 sum=112
    {  -3,   8, -15,  41,  95, -19,   9,  -4 }, // phase=3 sum=112
    {  -1,   3,  -5,  12, 110,  -9,   4,  -2 }, // phase=4 sum=112
};

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

#endif
