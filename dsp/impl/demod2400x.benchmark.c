// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.benchmark.c: starch benchmarks for the DEMOD_2400X kernels
// (input conversion, upsampler and preamble search).
// Every implementation is checked to be bit-exact against the plain C
// version (generic_generic) before it is timed.
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "dsp/helpers/demod2400x_dsp.h"
#include "dsp/helpers/demod2400x_coeffs.h"
#include "dsp/helpers/demod2400x_coherent.h"

#define DEMOD2400X_BENCH_LEN   131072   // one FIFO buffer of I/Q samples

// All implementations write to the same output buffer in turn. The verify
// functions refill it with this pattern after checking, so values left by the
// previous implementation can't hide ones that were never written; the
// reference starts from the same pattern, so untouched padding still matches.
#define DEMOD2400X_BENCH_POISON 0xA5

// Size of the upsample12 output (and locate_preamble input) for 'len' I/Q samples
static size_t demod2400x_upsample_size(int len)
{
    return 6 * (size_t) NU_STRIDE(NU_SAMP(len));
}

// 12-bit I/Q: background noise, occasional strong bursts and a few full-scale samples
static int16_t *demod2400x_make_iq(int len)
{
    int16_t *iq = malloc(2 * (size_t) len * sizeof(*iq));
    if (!iq)
        return NULL;

    srand(1);
    for (int n = 0; n < len; ++n) {
        int amp = (rand() % 50 == 0) ? 2047 : 200;
        int i = (rand() % (2 * amp + 1)) - amp;
        int q = (rand() % (2 * amp + 1)) - amp;
        if (rand() % 1000 == 0) {
            i = -2048;
            q = -2048;
        }
        iq[2 * n] = (int16_t) i;
        iq[2 * n + 1] = (int16_t) q;
    }
    return iq;
}

// Full-range 12-bit I/Q, to check the upsampler can't overflow: the first half uniform over
// -2048..2047, the second half worst-case windows where every sample has the sign of its
// tap in one phase, at full scale (that phase's largest output, 2048 * sum |tap|), cycling
// through the phases, both signs, and Q equal to or opposite I
static int16_t *demod2400x_make_iq_full(int len)
{
    int16_t *iq = malloc(2 * (size_t) len * sizeof(*iq));
    if (!iq)
        return NULL;

    srand(2);
    int n = 0;
    for (; n < len / 2; ++n) {
        iq[2 * n] = (int16_t) ((rand() % 4096) - 2048);
        iq[2 * n + 1] = (int16_t) ((rand() % 4096) - 2048);
    }
    for (int w = 0; n < len; ++n) {
        int k = n % 8, c = (w / 4) % 5;
        int sign_i = (w & 1) ? -1 : 1, sign_q = (w & 2) ? -sign_i : sign_i;
        int t = upsample_coeffs[c][k] < 0 ? -1 : 1;
        iq[2 * n] = (int16_t) (sign_i * t > 0 ? 2047 : -2048);
        iq[2 * n + 1] = (int16_t) (sign_q * t > 0 ? 2047 : -2048);
        if (k == 7)
            ++w;
    }
    return iq;
}


void STARCH_BENCHMARK(upsample12) (void)
{
    const int len = DEMOD2400X_BENCH_LEN;
    int16_t *in = demod2400x_make_iq(len);
    int16_t *in_full = demod2400x_make_iq_full(len);
    uint8_t *out = calloc(demod2400x_upsample_size(len), sizeof(*out));

    if (in && in_full && out) {
        memset(out, DEMOD2400X_BENCH_POISON, demod2400x_upsample_size(len) * sizeof(*out));
        STARCH_BENCHMARK_RUN( upsample12, in, len, out );
        STARCH_BENCHMARK_RUN( upsample12, in_full, len, out );
    }
    free(in_full);

    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(upsample12) (const int16_t *in, int len, uint8_t *out)
{
    size_t size = demod2400x_upsample_size(len);
    uint8_t *ref = malloc(size * sizeof(*ref));
    if (!ref)
        return false;

    memset(ref, DEMOD2400X_BENCH_POISON, size * sizeof(*ref));
    starch_upsample12_generic_generic(in, len, ref);

    bool okay = true;

    // Check the reference itself, the power in 64-bit arithmetic, so an overflow can't
    // hide by being the same in every implementation
    int stride = NU_STRIDE(NU_SAMP(len));
    for (int n = 0; n + 8 <= len && okay; ++n) {
        for (int c = 0; c < 5; ++c) {
            int64_t si = 0, sq = 0;
            for (int k = 0; k < 8; ++k) {
                si += (int64_t) upsample_coeffs[c][k] * in[2 * (n + k)];
                sq += (int64_t) upsample_coeffs[c][k] * in[2 * (n + k) + 1];
            }
            si = (si + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;
            sq = (sq + UPSAMPLE_ROUND) >> UPSAMPLE_SHIFT;
            int64_t pwr = si * si + sq * sq;
            int o = 5 * n + c;
            if (pwr >= (1LL << 31) || ref[(o % 6) * stride + o / 6] != demod_log8((uint32_t) pwr)) {
                fprintf(stderr, "reference check failed: window %d phase %d: %u, power %lld\n",
                        n, c, ref[(o % 6) * stride + o / 6], (long long) pwr);
                okay = false;
                break;
            }
        }
    }

    for (size_t k = 0; k < size && okay; ++k) {
        if (out[k] != ref[k]) {
            fprintf(stderr, "verification failed: out[%zu]=%u, expected %u\n", k, out[k], ref[k]);
            okay = false;
            break;
        }
    }

    memset(out, DEMOD2400X_BENCH_POISON, size * sizeof(*out));
    free(ref);
    return okay;
}


void STARCH_BENCHMARK(locate_preamble) (void)
{
    const int len = DEMOD2400X_BENCH_LEN;
    int16_t *iq = demod2400x_make_iq(len);
    uint8_t *pp = calloc(demod2400x_upsample_size(len), sizeof(*pp));
    uint8_t *mm = NULL;

    if (iq && pp) {
        int nt = starch_upsample12_generic_generic(iq, len, pp);
        if ((mm = malloc((size_t) nt))) {
            // thres 0 gives every metric; 88 (the decoder's) zeroes most of them (noise)
            memset(mm, DEMOD2400X_BENCH_POISON, (size_t) nt);
            STARCH_BENCHMARK_RUN( locate_preamble, pp, nt, 0, mm );
            STARCH_BENCHMARK_RUN( locate_preamble, pp, nt, 88, mm );
        }
    }

    free(iq);
    free(pp);
    free(mm);
}

bool STARCH_BENCHMARK_VERIFY(locate_preamble) (const uint8_t *pp, int nt, unsigned thres, uint8_t *mm)
{
    uint8_t *ref = malloc((size_t) nt);
    if (!ref)
        return false;

    memset(ref, DEMOD2400X_BENCH_POISON, (size_t) nt);
    starch_locate_preamble_generic_generic(pp, nt, thres, ref);

    bool okay = true;
    for (int k = 0; k < nt; ++k) {
        if (mm[k] != ref[k]) {
            fprintf(stderr, "verification failed: mm[%d]=%u, expected %u\n", k, mm[k], ref[k]);
            okay = false;
            break;
        }
    }

    memset(mm, DEMOD2400X_BENCH_POISON, (size_t) nt);
    free(ref);
    return okay;
}


// Input conversion. Every call starts the DC filter from the same dc_in, so the
// output and the final state (dc_out) don't depend on earlier calls.
// The length isn't a multiple of IQ_DC_BLOCK, so the tail code is checked too.
#define IQCONV_BENCH_LEN (DEMOD2400X_BENCH_LEN + 5)

static const int32_t iqconv_bench_dc[2] = { 40 * 4096, -25 * 4096 };
static int32_t iqconv_bench_dc_out[2];
static double iqconv_bench_level, iqconv_bench_power;

// Raw I/Q with a DC offset, background noise, bursts and some full-scale samples,
// scaled by 'scale' around 'centre' and limited to lo..hi
static void iqconv_bench_fill(int32_t *v, unsigned n, int scale, int centre, int lo, int hi)
{
    srand(2);
    for (unsigned k = 0; k < n; k += 2) {
        int amp = (rand() % 50 == 0) ? 2047 : 200;
        int i = 40 + (rand() % (2 * amp + 1)) - amp;
        int q = -25 + (rand() % (2 * amp + 1)) - amp;
        if (rand() % 1000 == 0) {
            i = 3000;
            q = -3000;
        }
        i = i * scale + centre;
        q = q * scale + centre;
        v[k]     = i < lo ? lo : i > hi ? hi : i;
        v[k + 1] = q < lo ? lo : q > hi ? hi : q;
    }
}

static int32_t *iqconv_bench_raw(int scale, int centre, int lo, int hi)
{
    int32_t *v = malloc(2 * (size_t) IQCONV_BENCH_LEN * sizeof(*v));
    if (v)
        iqconv_bench_fill(v, 2 * IQCONV_BENCH_LEN, scale, centre, lo, hi);
    return v;
}

static uint16_t *iqconv_bench_out(void)
{
    uint16_t *out = malloc(2 * (size_t) IQCONV_BENCH_LEN * sizeof(*out));
    if (out)
        memset(out, DEMOD2400X_BENCH_POISON, 2 * (size_t) IQCONV_BENCH_LEN * sizeof(*out));
    memset(iqconv_bench_dc_out, DEMOD2400X_BENCH_POISON, sizeof(iqconv_bench_dc_out));
    return out;
}

// Compare against the reference, then poison the outputs for the next implementation
static bool iqconv_bench_check(uint16_t *out, const uint16_t *ref, unsigned len, int32_t *dc, const int32_t *ref_dc,
                               double level, double ref_level, double power, double ref_power)
{
    bool okay = true;
    for (unsigned k = 0; k < 2 * len; ++k) {
        if (out[k] != ref[k]) {
            fprintf(stderr, "verification failed: out[%u]=%d, expected %d\n", k, (int16_t) out[k], (int16_t) ref[k]);
            okay = false;
            break;
        }
    }
    if (okay && (dc[0] != ref_dc[0] || dc[1] != ref_dc[1])) {
        fprintf(stderr, "verification failed: dc_offset=(%d,%d), expected (%d,%d)\n", dc[0], dc[1], ref_dc[0], ref_dc[1]);
        okay = false;
    }
    if (okay && (level != ref_level || power != ref_power)) {
        fprintf(stderr, "verification failed: level=%.9g power=%.9g, expected %.9g %.9g\n", level, power, ref_level, ref_power);
        okay = false;
    }

    memset(out, DEMOD2400X_BENCH_POISON, 2 * (size_t) len * sizeof(*out));
    memset(dc, DEMOD2400X_BENCH_POISON, 2 * sizeof(*dc));
    return okay;
}


void STARCH_BENCHMARK(iqdata_conv_uc8) (void)
{
    int32_t *raw = iqconv_bench_raw(1, 2048, 0, 4095);     // 12-bit offset binary, reduced to 8 bits below
    uc8_t *in = malloc((size_t) IQCONV_BENCH_LEN * sizeof(*in));
    uint16_t *out = iqconv_bench_out();

    if (raw && in && out) {
        for (unsigned n = 0; n < IQCONV_BENCH_LEN; ++n) {
            in[n].I = (uint8_t) (raw[2 * n] >> 4);
            in[n].Q = (uint8_t) (raw[2 * n + 1] >> 4);
        }
        STARCH_BENCHMARK_RUN( iqdata_conv_uc8, in, out, IQCONV_BENCH_LEN, iqconv_bench_dc, iqconv_bench_dc_out, &iqconv_bench_level, &iqconv_bench_power );
    }

    free(raw);
    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(iqdata_conv_uc8) (const uc8_t *in, uint16_t *out, unsigned len, const int32_t *dc_in, int32_t *dc,
                                                double *level, double *power)
{
    uint16_t *ref = malloc(2 * (size_t) len * sizeof(*ref));
    if (!ref)
        return false;

    int32_t ref_dc[2];
    double ref_level, ref_power;
    memset(ref, DEMOD2400X_BENCH_POISON, 2 * (size_t) len * sizeof(*ref));
    starch_iqdata_conv_uc8_generic_generic(in, ref, len, dc_in, ref_dc, &ref_level, &ref_power);

    bool okay = iqconv_bench_check(out, ref, len, dc, ref_dc, *level, ref_level, *power, ref_power);
    free(ref);
    return okay;
}


void STARCH_BENCHMARK(iqdata_conv_sc16) (void)
{
    int32_t *raw = iqconv_bench_raw(16, 0, -32768, 32767);
    sc16_t *in = malloc((size_t) IQCONV_BENCH_LEN * sizeof(*in));
    uint16_t *out = iqconv_bench_out();

    if (raw && in && out) {
        for (unsigned n = 0; n < IQCONV_BENCH_LEN; ++n) {
            in[n].I = (uint16_t) htole16((uint16_t) (raw[2 * n] + (n & 15)));     // low bits dropped by >> 4
            in[n].Q = (uint16_t) htole16((uint16_t) (raw[2 * n + 1] + (n & 15)));
        }
        STARCH_BENCHMARK_RUN( iqdata_conv_sc16, in, out, IQCONV_BENCH_LEN, iqconv_bench_dc, iqconv_bench_dc_out, &iqconv_bench_level, &iqconv_bench_power );
    }

    free(raw);
    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(iqdata_conv_sc16) (const sc16_t *in, uint16_t *out, unsigned len, const int32_t *dc_in, int32_t *dc,
                                                double *level, double *power)
{
    uint16_t *ref = malloc(2 * (size_t) len * sizeof(*ref));
    if (!ref)
        return false;

    int32_t ref_dc[2];
    double ref_level, ref_power;
    memset(ref, DEMOD2400X_BENCH_POISON, 2 * (size_t) len * sizeof(*ref));
    starch_iqdata_conv_sc16_generic_generic(in, ref, len, dc_in, ref_dc, &ref_level, &ref_power);

    bool okay = iqconv_bench_check(out, ref, len, dc, ref_dc, *level, ref_level, *power, ref_power);
    free(ref);
    return okay;
}


void STARCH_BENCHMARK(iqdata_conv_sc16q11) (void)
{
    int32_t *raw = iqconv_bench_raw(1, 0, -4096, 4095);    // includes values beyond 12 bits
    sc16_t *in = malloc((size_t) IQCONV_BENCH_LEN * sizeof(*in));
    uint16_t *out = iqconv_bench_out();

    if (raw && in && out) {
        for (unsigned n = 0; n < IQCONV_BENCH_LEN; ++n) {
            in[n].I = (uint16_t) htole16((uint16_t) raw[2 * n]);
            in[n].Q = (uint16_t) htole16((uint16_t) raw[2 * n + 1]);
        }
        STARCH_BENCHMARK_RUN( iqdata_conv_sc16q11, in, out, IQCONV_BENCH_LEN, iqconv_bench_dc, iqconv_bench_dc_out, &iqconv_bench_level, &iqconv_bench_power );
    }

    free(raw);
    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(iqdata_conv_sc16q11) (const sc16_t *in, uint16_t *out, unsigned len, const int32_t *dc_in, int32_t *dc,
                                                double *level, double *power)
{
    uint16_t *ref = malloc(2 * (size_t) len * sizeof(*ref));
    if (!ref)
        return false;

    int32_t ref_dc[2];
    double ref_level, ref_power;
    memset(ref, DEMOD2400X_BENCH_POISON, 2 * (size_t) len * sizeof(*ref));
    starch_iqdata_conv_sc16q11_generic_generic(in, ref, len, dc_in, ref_dc, &ref_level, &ref_power);

    bool okay = iqconv_bench_check(out, ref, len, dc, ref_dc, *level, ref_level, *power, ref_power);
    free(ref);
    return okay;
}


// Bit decoding: 7 bytes (one 56-bit half frame, as used) on ordinary data, and the
// maximum length on upsampled full-range data
void STARCH_BENCHMARK(decode_bits) (void)
{
    const int len = DEMOD2400X_BENCH_LEN;
    int16_t *in = demod2400x_make_iq(len);
    int16_t *in_full = demod2400x_make_iq_full(len);
    uint8_t *pp = calloc(demod2400x_upsample_size(len), sizeof(*pp));
    uint8_t *pp_full = calloc(demod2400x_upsample_size(len), sizeof(*pp_full));
    uint8_t msg[DECODE_BITS_MAX_BYTES];
    unsigned quality;

    if (in && in_full && pp && pp_full) {
        starch_upsample12_generic_generic(in, len, pp);
        starch_upsample12_generic_generic(in_full, len, pp_full);
        memset(msg, DEMOD2400X_BENCH_POISON, sizeof(msg));
        STARCH_BENCHMARK_RUN( decode_bits, pp + 1001, 7, msg, &quality );
        STARCH_BENCHMARK_RUN( decode_bits, pp_full + 3, DECODE_BITS_MAX_BYTES, msg, &quality );
    }

    free(in);
    free(in_full);
    free(pp);
    free(pp_full);
}

bool STARCH_BENCHMARK_VERIFY(decode_bits) (const uint8_t *p, unsigned nbytes, uint8_t *msg, unsigned *quality)
{
    uint8_t ref[DECODE_BITS_MAX_BYTES];
    unsigned ref_quality = ~0U;

    memset(ref, DEMOD2400X_BENCH_POISON, sizeof(ref));
    starch_decode_bits_generic_generic(p, nbytes, ref, &ref_quality);

    bool okay = true;
    for (unsigned k = 0; k < DECODE_BITS_MAX_BYTES; ++k) {
        if (msg[k] != ref[k]) {
            fprintf(stderr, "verification failed: msg[%u]=%02x, expected %02x\n", k, msg[k], ref[k]);
            okay = false;
            break;
        }
    }
    if (okay && *quality != ref_quality) {
        fprintf(stderr, "verification failed: quality=%u, expected %u\n", *quality, ref_quality);
        okay = false;
    }

    memset(msg, DEMOD2400X_BENCH_POISON, DECODE_BITS_MAX_BYTES);
    *quality = ~0U;
    return okay;
}


// Coherent decode: ordinary and full-range data with the gate open (thres 0, so the
// bits are always sliced), and ordinary data with the usual gate (0.6 = 614/1024)
// Synthetic frames for coherent_decode: preamble and 112 random bits as 0.5 us pulses,
// sampled at 2.4 MHz, each on its own carrier offset (spread over the +-250 kHz search),
// with a little noise; frame n starts at 12 MHz position coh_frame_pos(n)
#define COH_BENCH_FRAMES 7
static const double coh_bench_hz[COH_BENCH_FRAMES] = { -230e3, -120e3, -40e3, 5e3, 75e3, 180e3, 234e3 };

static int coh_frame_pos(int n)
{
    return 2000 + 9000 * n;
}

static int16_t *demod2400x_make_coh_frames(int len)
{
    int16_t *iq = calloc(2 * (size_t) len, sizeof(*iq));
    if (!iq)
        return NULL;
    static const int pre[16] = { 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0 };
    srand(3);
    for (int n = 0; n < len; ++n) {
        iq[2 * n] = (int16_t) ((rand() % 61) - 30);
        iq[2 * n + 1] = (int16_t) ((rand() % 61) - 30);
    }
    for (int fr = 0; fr < COH_BENCH_FRAMES; ++fr) {
        int chip[16 + 2 * 112];
        for (int k = 0; k < 16; ++k)
            chip[k] = pre[k];
        for (int i = 0; i < 112; ++i) {
            const int bit = rand() & 1;
            chip[16 + 2 * i] = bit;
            chip[17 + 2 * i] = !bit;
        }
        const double t0 = (coh_frame_pos(fr) + 15.5) / 12.0 - 0.25; // us: chip centres where the decoder samples
        const double ph0 = 0.7 * fr;
        for (int n = (int) (t0 * 2.4); n < (int) ((t0 + 120.0) * 2.4) + 2 && n < len; ++n) {
            const int k = (int) floor((n / 2.4 - t0) / 0.5);
            if (k < 0 || k >= 16 + 2 * 112 || !chip[k])
                continue;
            const double ph = ph0 + 2 * M_PI * coh_bench_hz[fr] * n / 2.4e6;
            iq[2 * n] = (int16_t) (iq[2 * n] + lrint(1500 * cos(ph)));
            iq[2 * n + 1] = (int16_t) (iq[2 * n + 1] + lrint(1500 * sin(ph)));
        }
    }
    return iq;
}

void STARCH_BENCHMARK(coherent_decode) (void)
{
    const int len = DEMOD2400X_BENCH_LEN;
    int16_t *in = demod2400x_make_iq(len);
    int16_t *in_full = demod2400x_make_iq_full(len);
    uint8_t out[14];
    uint32_t info[COH_INFO_N];

    if (in && in_full) {
        memset(out, DEMOD2400X_BENCH_POISON, sizeof(out));
        memset(info, DEMOD2400X_BENCH_POISON, sizeof(info));
        STARCH_BENCHMARK_RUN( coherent_decode, in, 1001, 0, out, info );
        STARCH_BENCHMARK_RUN( coherent_decode, in_full, 777, 0, out, info );
        STARCH_BENCHMARK_RUN( coherent_decode, in, 50003, 614, out, info );
    }

    // Frames across the +-250 kHz search, at their own positions
    int16_t *fr = demod2400x_make_coh_frames(len);
    if (fr) {
        for (int n = 0; n < COH_BENCH_FRAMES; ++n)
            STARCH_BENCHMARK_RUN( coherent_decode, fr, coh_frame_pos(n), 614, out, info );
    }
    free(fr);

    free(in);
    free(in_full);
}

bool STARCH_BENCHMARK_VERIFY(coherent_decode) (const int16_t *iq, int pos, unsigned thres, uint8_t *out, uint32_t *info)
{
    uint8_t ref[14];
    uint32_t ref_info[COH_INFO_N];

    memset(ref, DEMOD2400X_BENCH_POISON, sizeof(ref));
    memset(ref_info, DEMOD2400X_BENCH_POISON, sizeof(ref_info));
    starch_coherent_decode_generic_generic(iq, pos, thres, ref, ref_info);

    bool okay = true;
    for (int k = 0; k < 14; ++k) {
        if (out[k] != ref[k]) {
            fprintf(stderr, "verification failed: out[%d]=%02x, expected %02x\n", k, out[k], ref[k]);
            okay = false;
            break;
        }
    }
    for (int k = 0; okay && k < COH_INFO_N; ++k) {
        if (info[k] != ref_info[k]) {
            fprintf(stderr, "verification failed: info[%d]=%u, expected %u\n", k, info[k], ref_info[k]);
            okay = false;
        }
    }

    memset(out, DEMOD2400X_BENCH_POISON, 14);
    memset(info, DEMOD2400X_BENCH_POISON, COH_INFO_N * sizeof(*info));
    return okay;
}
