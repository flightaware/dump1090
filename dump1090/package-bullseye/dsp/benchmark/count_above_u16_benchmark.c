#include <stdlib.h>

void STARCH_BENCHMARK(count_above_u16) (void)
{
    uint16_t *in = NULL;
    const unsigned len = 96; /* Typical use is with short burst windows (40us) */
    const unsigned threshold = 46395; /* -3dBFS */

    if (!(in = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    srand(1);
    for (unsigned i = 0; i < len; ++i) {
        in[i] = rand() % 65536;
    }

    unsigned count;
    STARCH_BENCHMARK_RUN( count_above_u16, in, len, threshold, &count );

 done:
    STARCH_BENCHMARK_FREE(in);
}

bool STARCH_BENCHMARK_VERIFY(count_above_u16) (const uint16_t *in, unsigned len, uint16_t threshold, unsigned *out_count)    
{
    unsigned expected = 0;
    for (unsigned i = 0; i < len; ++i) {
        if (in[i] >= threshold)
            ++expected;
    }

    if (expected != *out_count) {
        fprintf(stderr, "verification failed: expected count %u, got count %u\n", expected, *out_count);
        return false;
    }

    return true;
}
