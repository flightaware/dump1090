#include <stdio.h>

void STARCH_BENCHMARK(subtract_n) (void)
{
    uint16_t *in = NULL, *out = NULL;
    const unsigned len = 65536;
    const unsigned n = 42;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, uint16_t)) || !(out = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    STARCH_BENCHMARK_RUN( subtract_n, in, len, n, out );

 done:
    STARCH_BENCHMARK_FREE(in);
    STARCH_BENCHMARK_FREE(out);
}

bool STARCH_BENCHMARK_VERIFY(subtract_n)(const uint16_t *in, unsigned len, uint16_t n, uint16_t *out)
{
    bool okay = true;

    for (unsigned i = 0; i < len; ++i) {
        uint16_t expected = in[i] - n;
        if (out[i] != expected) {
            fprintf(stderr, "verification failed: in[%u]=%u n=%u out[%u]=%u expected=%u\n", i, in[i], n, i, out[i], expected);
            okay = false;
        }
    }

    return okay;
}
