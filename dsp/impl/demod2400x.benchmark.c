// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod2400x.benchmark.c: starch benchmarks for the DEMOD_2400X kernels.
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

    if (in && out)
        STARCH_BENCHMARK_RUN( upsample12, in, len, DEMOD2400X_BENCH_SHIFT, out );

    free(in);
    free(out);
}

bool STARCH_BENCHMARK_VERIFY(upsample12) (const int16_t *in, int len, int shift, uint16_t *out)
{
    size_t size = demod2400x_upsample_size(len);
    uint16_t *ref = calloc(size, sizeof(*ref));
    if (!ref)
        return false;

    starch_upsample12_generic_generic(in, len, shift, ref);

    bool okay = true;
    for (size_t k = 0; k < size; ++k) {
        if (out[k] != ref[k]) {
            fprintf(stderr, "verification failed: out[%zu]=%u, expected %u\n", k, out[k], ref[k]);
            okay = false;
            break;
        }
    }

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
        if ((mm = calloc((size_t) nt, 1)))
            STARCH_BENCHMARK_RUN( locate_preamble, pp, nt, mm );
    }

    free(iq);
    free(pp);
    free(mm);
}

bool STARCH_BENCHMARK_VERIFY(locate_preamble) (const uint16_t *pp, int nt, uint8_t *mm)
{
    uint8_t *ref = calloc((size_t) nt, 1);
    if (!ref)
        return false;

    starch_locate_preamble_generic_generic(pp, nt, ref);

    bool okay = true;
    for (int k = 0; k < nt; ++k) {
        if (mm[k] != ref[k]) {
            fprintf(stderr, "verification failed: mm[%d]=%u, expected %u\n", k, mm[k], ref[k]);
            okay = false;
            break;
        }
    }

    free(ref);
    return okay;
}
