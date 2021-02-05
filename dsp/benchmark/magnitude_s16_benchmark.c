#include <stdlib.h>
#include <stdio.h>
#include <math.h>


void STARCH_BENCHMARK(magnitude_s16) (void)
{
    int16_t *in = NULL;
    uint16_t *out_mag = NULL;
    const unsigned len = 65535;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, int16_t)) || !(out_mag = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    unsigned i = 0;

    for (; i < len; ++i) {
        in[i] = i - 32767;
        out_mag[i] = abs(in[i]);
    }

    STARCH_BENCHMARK_RUN( magnitude_s16, in, out_mag, len );

 done:
    STARCH_BENCHMARK_FREE(in);
    STARCH_BENCHMARK_FREE(out_mag);
}

bool STARCH_BENCHMARK_VERIFY(magnitude_s16) (const int16_t *in, uint16_t *out, unsigned len)
{
    bool okay = true;

    for (unsigned i = 0; i < len; ++i) {
        okay = (out[i] == abs(in[i]));
        if (!okay) {
            fprintf(stderr, "verification failed: in[%u]=%d out[%u]=%u\n", i, in[i], i, out[i]);
        }
    }

    return okay;
}
