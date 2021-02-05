#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void STARCH_BENCHMARK(magnitude_u16o12) (void)
{
    uint16_t *in = NULL;
    uint16_t *out_mag = NULL;
    const unsigned len = 4096;
    unsigned i;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, uint16_t)) || !(out_mag = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    for (i = 1; i < len; i++) {
        in[i] = i;
    }

    STARCH_BENCHMARK_RUN( magnitude_u16o12, in, out_mag, len );

 done:
    STARCH_BENCHMARK_FREE(in);
    STARCH_BENCHMARK_FREE(out_mag);
}

bool STARCH_BENCHMARK_VERIFY(magnitude_u16o12) (const uint16_t *in, uint16_t *out, unsigned len)
{
    bool okay = true;

    for (unsigned i = 1; i < len; ++i) {
        uint16_t j;
        if (in[i] < 2048) {
            j = 2048 - (out[i] * 2047.0 / 65535.0f - 0.5f);
        } else {
            j = 2048 + (out[i] * 2047.0 / 65535.0f + 0.5f);
        }
        okay = (j == in[j]);
        if (!okay) {
            fprintf(stderr, "verification failed: in[%u]=%d out[%u]=%u\n", i, in[i], i, out[i]);
        }
    }

    return okay;
}

#undef CONVERT_AND_SCALE
#undef MAGIC_SCALER
