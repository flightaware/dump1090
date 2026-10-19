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

#include "dsp/helpers/demod2400x_dsp.h"

#define DEMOD2400X_BENCH_LEN   131072   // one FIFO buffer of I/Q samples
#define DEMOD2400X_BENCH_SHIFT 8

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


void STARCH_BENCHMARK(upsample12) (void)
{
    const int len = DEMOD2400X_BENCH_LEN;
    int16_t *in = demod2400x_make_iq(len);
    uint16_t *out = calloc(demod2400x_upsample_size(len), sizeof(*out));

    if (in && out) {
        memset(out, DEMOD2400X_BENCH_POISON, demod2400x_upsample_size(len) * sizeof(*out));
        STARCH_BENCHMARK_RUN( upsample12, in, len, DEMOD2400X_BENCH_SHIFT, out );
    }

    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(upsample12) (const int16_t *in, int len, int shift, uint16_t *out)
{
    size_t size = demod2400x_upsample_size(len);
    uint16_t *ref = malloc(size * sizeof(*ref));
    if (!ref)
        return false;

    memset(ref, DEMOD2400X_BENCH_POISON, size * sizeof(*ref));
    starch_upsample12_generic_generic(in, len, shift, ref);

    bool okay = true;
    for (size_t k = 0; k < size; ++k) {
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
    uint16_t *pp = calloc(demod2400x_upsample_size(len), sizeof(*pp));
    uint8_t *mm = NULL;

    if (iq && pp) {
        int nt = starch_upsample12_generic_generic(iq, len, DEMOD2400X_BENCH_SHIFT, pp);
        if ((mm = malloc((size_t) nt))) {
            // thres 0 gives every metric; 50 zeroes most of them (noise) without dividing
            memset(mm, DEMOD2400X_BENCH_POISON, (size_t) nt);
            STARCH_BENCHMARK_RUN( locate_preamble, pp, nt, 0, mm );
            STARCH_BENCHMARK_RUN( locate_preamble, pp, nt, 50, mm );
        }
    }

    free(iq);
    free(pp);
    free(mm);
}

bool STARCH_BENCHMARK_VERIFY(locate_preamble) (const uint16_t *pp, int nt, unsigned thres, uint8_t *mm)
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
