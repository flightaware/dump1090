// demod2400x_coherent.h: shared pieces of the coherent_decode starch kernel
// (dsp/impl/demod2400x.*.c). Every implementation uses these for the scalar steps,
// so all of them give the same bits.
//
// coherent_decode(iq, pos, thres, out, info) decodes the frame whose preamble
// starts at 12 MHz position pos, from 12-bit I/Q (DC removed), coherently:
//
// Each 56-bit half is decoded on its own, with nothing shared: the first always, the
// second when the first passed the gate, whatever the DF says (scoring can correct the
// DF bits, and every short DF is one bit from a long one). So a short frame never
// depends on bits after it, and the second half of a long one gets a fresh frequency
// and phase rather than the first half's, extrapolated.
// For each half (bits i = 0..55 below are counted from its start):
//
// 1. Chips: complex filter outputs at t = pos + 96 + 6k (bit i: chips 2i, 2i+1),
//    same polyphase table as upsample12, 16x16 -> 32-bit sums. They are shifted so
//    the largest of the half is below 2^COH_CHIP_BITS and clamped to that, as int16.
// 2. Carrier sample of each bit: x_a if |x_a|^2 >= |x_b|^2 (int32), else x_b (one pulse
//    per bit, in the larger chip unless noise wins). 28 block sums of 2 bits,
//    normalised below 2^COH_WIDE_BITS (int16).
// 3. Wide search, two stages, over +-250 kHz (transponders are allowed +-1 MHz, and
//    many are 100-240 kHz off):
//    a. For each 14-block piece (bits 0..27, 28..55), for each of 16 wide bins (31.25 kHz
//       apart, none at 0 Hz), sum in int16 of rot(block m, coh_wide[m][bin]) for m < 14
//       (piece-relative rows); power (int32); the first largest bin is the piece's peak.
//    b. For each peak k (one if both pieces agree): the 28 blocks rotated by
//       coh_wide[m][k] are summed in fours into 7 byte blocks, >> 2 (coh_wide_regroup);
//       then the 16 central fine bins (8..23, +-30 kHz) of the 32 below, as before:
//       sum over blocks of rot(block, coh_bin[block][bin]) in int16, power (int32),
//       first largest wins, parabola to 1/8 of a bin (coh_fine_index, the other bins 0).
//       The peak whose best power is larger wins (the first on a tie).
// 4. Phasors P[i] = e^{-j 2 w i} per bit, w the wide bin plus the fine frequency:
//    rb = rot(coh_fine_bit[f], coh_wide_bit[k]), rc = rot(coh_fine_chip[f],
//    coh_wide_chip[k]); P[0] = 32767, P[i] = rot(P[i-1], rb) for i < 8, then
//    P[i] = rot(P[i-8], rb8) with rb8 = rot(P[7], rb). Q[i] = rot(rc, P[i]) for the
//    second chip of each bit.
// 5. Correlation: c_i = rot(x_a, P[i]) or rot(x_b, Q[i]), the chip chosen in step 2
//    (int16), h = sum c_i (int32), e2 = sum (c.re>>4)^2 + (c.im>>4)^2 (uint32);
//    rho2 = coh_rho2(h, e2) in 1/1024. The first half stops here below 'thres' (the
//    whole frame is then not decoded); the second half is always sliced.
// 6. Slice the 56 bits: g = h scaled below 2^15; y = mulq15(re, g.re) + mulq15(im, g.im)
//    for rot(x_a, P[i]) and rot(x_b, Q[i]); bit = 1 if y_a > y_b. Quality 256 sum|y_a - y_b| /
//    (sum (|y_a| + |y_b|) + 1).
//
// rot(x, p) = (mulq15(x.re, p.re) - mulq15(x.im, p.im), mulq15(x.re, p.im) + mulq15(x.im, p.re))
// with mulq15(a, b) = (a*b + 2^14) >> 15 (NEON vqrdmulh, AVX2/SSSE3 mulhrs).
//
// out[14] gets the bits (zero where not decoded); info[COH_INFO_N] the results (zero
// for a half not done).

#ifndef DEMOD2400X_COHERENT_H
#define DEMOD2400X_COHERENT_H

#include <stdint.h>
#include <string.h>

#include "dsp/helpers/demod2400x_coeffs.h"
#include "dsp/helpers/demod2400x_coh_tables.h"

#define COH_HALF        56      // bits per half, each estimated on its own
#define COH_BITS        112
#define COH_CHIP_BITS   13      // chips below 2^13: rotated < 2^13 * sqrt(2), pair sums < 2^15
#define COH_BLOCK_BITS  11      // byte block sums below 2^11: 7 rotated < 2^15
#define COH_WIDE_BITS   10      // 2-bit block sums below 2^10: 14 rotated < 2^15
#define COH_REF_LO      8       // fine bins searched around a wide peak: 8 .. 23 (+-30 kHz)
#define COH_REF_HI      24
#define COH_DATA_OFFSET 96      // 16 preamble chips * 6 (12 MHz positions)

// info[] layout
#define COH_INFO_DECODED 0      // 1 if the first half passed the gate and was sliced
#define COH_INFO_RHO2    1      // carrier coherence of bits 0..55 in 1/1024 (1024 = perfect)
#define COH_INFO_Q1      2      // quality of bits 0..55 (0..256)
#define COH_INFO_Q2      3      // quality of bits 56..111
#define COH_INFO_FINE    4      // fine frequency index of bits 0..55 (0..COH_FINE_BINS-1, centre = 0 Hz)
#define COH_INFO_RHO2B   5      // carrier coherence of bits 56..111
#define COH_INFO_FINE2   6      // fine frequency index of bits 56..111
#define COH_INFO_WIDE    7      // wide bin of bits 0..55 (0..COH_WBINS-1); frequency = wide + fine
#define COH_INFO_WIDE2   8      // wide bin of bits 56..111
#define COH_INFO_N       9

// Samples the kernel reads: iq[0 .. 2 * coh_iq_needed(pos)) (I/Q pairs up to that index)
static inline unsigned coh_iq_needed(int pos)
{
    return (unsigned) ((pos + COH_DATA_OFFSET + 6 * (2 * COH_BITS - 1)) / 5 + 8);
}

static inline int16_t coh_mulq15(int16_t a, int16_t b)
{
    return (int16_t) (((int32_t) a * b + (1 << 14)) >> 15);
}

static inline int coh_bits(uint32_t v)
{
    return v ? 32 - __builtin_clz(v) : 0;
}

// Shift that brings vmax below 2^bits
static inline int coh_shift(uint32_t vmax, int bits)
{
    const int n = coh_bits(vmax);
    return n > bits ? n - bits : 0;
}

static inline uint32_t coh_abs(int32_t v)
{
    return v < 0 ? (uint32_t) -v : (uint32_t) v;
}

// Shift and clamp a chip value to int16 below 2^COH_CHIP_BITS
static inline int16_t coh_chip16(int32_t v, int shift)
{
    const int32_t lim = (1 << COH_CHIP_BITS) - 1;
    v >>= shift;
    return (int16_t) (v > lim ? lim : v < -lim ? -lim : v);
}

// One chip: filter output at 12 MHz position t (|s| < 2^19)
static inline void coh_chip(const int16_t *iq, int t, int32_t *re, int32_t *im)
{
    const int n = t / 5, c = t % 5;
    int32_t si = 0, sq = 0;
    for (int j = 0; j < 8; j++) {
        si += upsample_coeffs[c][j] * iq[2 * (n + j)];
        sq += upsample_coeffs[c][j] * iq[2 * (n + j) + 1];
    }
    *re = si;
    *im = sq;
}

// Fine frequency index from the bin powers: parabola through the best bin and its
// neighbours, rounded to 1/COH_FINE of a bin (powers >> 4 keep it in int32)
static inline int coh_fine_index(const int32_t *p, int best)
{
    int f = best * COH_FINE;
    if (best > 0 && best < COH_BINS - 1) {
        const int32_t pm = p[best - 1] >> 4, p0 = p[best] >> 4, pp = p[best + 1] >> 4;
        const int32_t den = 2 * (2 * p0 - pm - pp);
        if (den > 0) {
            const int32_t num = COH_FINE * (pp - pm);
            int32_t d = (num >= 0 ? num + den / 2 : num - den / 2) / den;
            if (d > COH_FINE / 2) d = COH_FINE / 2;
            if (d < -COH_FINE / 2) d = -COH_FINE / 2;
            f += d;
        }
    }
    return f;
}

// First largest of n powers
static inline int coh_argmax(const int32_t *p, int n)
{
    int best = 0;
    for (int j = 1; j < n; j++)
        if (p[j] > p[best])
            best = j;
    return best;
}

// Stage 3b: the 28 2-bit blocks rotated by wide bin k, summed in fours, >> 2 (7 blocks
// below 2^11, as the byte blocks of the fine search expect)
static inline void coh_wide_regroup(const int16_t *wr, const int16_t *wi, int k, int16_t *br, int16_t *bi)
{
    for (int m = 0; m < COH_NBLOCKS; m++) {
        int16_t re = 0, im = 0;
        for (int q = 4 * m; q < 4 * m + 4; q++) {
            const int16_t pr = coh_widet_re[k][q], pi = coh_widet_im[k][q];
            re = (int16_t) (re + (int16_t) (coh_mulq15(wr[q], pr) - coh_mulq15(wi[q], pi)));
            im = (int16_t) (im + (int16_t) (coh_mulq15(wr[q], pi) + coh_mulq15(wi[q], pr)));
        }
        br[m] = (int16_t) (re >> 2);
        bi[m] = (int16_t) (im >> 2);
    }
}

// Phasor step per bit (rb) and per chip (rc) for wide bin k and fine frequency f
static inline void coh_steps(int k, int f, int16_t *rbr, int16_t *rbi, int16_t *rcr, int16_t *rci)
{
    const int16_t fbr = coh_fine_bit[f][0], fbi = coh_fine_bit[f][1], wbr = coh_wide_bit[k][0], wbi = coh_wide_bit[k][1];
    const int16_t fcr = coh_fine_chip[f][0], fci = coh_fine_chip[f][1], wcr = coh_wide_chip[k][0], wci = coh_wide_chip[k][1];
    *rbr = (int16_t) (coh_mulq15(fbr, wbr) - coh_mulq15(fbi, wbi));
    *rbi = (int16_t) (coh_mulq15(fbr, wbi) + coh_mulq15(fbi, wbr));
    *rcr = (int16_t) (coh_mulq15(fcr, wcr) - coh_mulq15(fci, wci));
    *rci = (int16_t) (coh_mulq15(fcr, wci) + coh_mulq15(fci, wcr));
}

// rho2 = |h|^2 / (56 sum |c|^2) over a half in 1/1024; e2 holds sum |c >> 4|^2, so
// rho2 = 2^8 |h>>8|^2 / (56 e2) (|h| < 2^21, |h>>8|^2 < 2^27)
static inline uint32_t coh_rho2(int32_t hre, int32_t him, uint32_t e2)
{
    if (!e2)
        return 0;
    const int32_t hr = hre >> 8, hi = him >> 8;
    const uint64_t h2 = (uint64_t) ((uint32_t) (hr * hr) + (uint32_t) (hi * hi));
    const uint64_t r = (h2 << 18) / ((uint64_t) COH_HALF * e2);
    return r > 0xffffffffU ? 0xffffffffU : (uint32_t) r;
}

// Channel for slicing: h scaled below 2^15 (clamped to +-32767)
static inline void coh_channel16(int32_t hre, int32_t him, int16_t *gre, int16_t *gim)
{
    const int s = coh_shift(coh_abs(hre) > coh_abs(him) ? coh_abs(hre) : coh_abs(him), 15);
    int32_t re = hre >> s, im = him >> s;
    *gre = (int16_t) (re > 32767 ? 32767 : re < -32767 ? -32767 : re);
    *gim = (int16_t) (im > 32767 ? 32767 : im < -32767 ? -32767 : im);
}

static inline unsigned coh_quality(uint32_t qual, uint32_t pw)
{
    return 256 * qual / (pw + 1);                 // qual, pw < 56 * 2^15: no overflow
}

#endif
