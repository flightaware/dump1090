#include <stdio.h>
#include <stdlib.h>

void STARCH_BENCHMARK(count_above_u16) (void)
{
    uint16_t *in = NULL;
    const unsigned len = 96; /* Typical use is with short burst windows (40us) */
    const unsigned threshold = 46395; /* -3dBFS */

    if (!(in = calloc(len, sizeof(*in))))
        goto done;

    srand(1);
    for (unsigned i = 0; i < len; ++i) {
        in[i] = rand() % 65536;
    }

    unsigned count;
    STARCH_BENCHMARK_RUN( count_above_u16, in, len, threshold, &count );
    /* Validation only: also check lengths that leave a partial vector at the
       end, which the main run (a multiple of every vector width) never does */
    if (starch_benchmark_validate_only) {
        static const unsigned extra_len[] = { 1, 3, 7, 9, 15, 17, 33 };
        const unsigned n_extra = sizeof(extra_len) / sizeof(extra_len[0]);
        for (unsigned k = 0; k <= n_extra; ++k) {
            unsigned n = (k < n_extra) ? extra_len[k] : len - 3;
            fprintf(stderr, "  (length %u)\n", n);
            STARCH_BENCHMARK_RUN( count_above_u16, in, n, threshold, &count );
        }
    }


 done:
    free(in);
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
