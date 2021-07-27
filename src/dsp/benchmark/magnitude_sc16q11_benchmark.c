#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void STARCH_BENCHMARK(magnitude_sc16q11) (void)
{
    sc16_t *in = NULL;
    uint16_t *out_mag = NULL;
    const unsigned len = 65536;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, sc16_t)) || !(out_mag = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    unsigned i = 0;

    // 0.9 magnitude, varying phase
    double degrees = 0;
    for (; i < len && degrees < 360; i += 1, degrees += 1) {
        in[i].I = (int16_t) (0.9 * cos(degrees * M_PI / 180.0) * 2048.0);
        in[i].Q = (int16_t) (0.9 * sin(degrees * M_PI / 180.0) * 2048.0);
    }

    // 0, 45, 90 degree phase, full input range
    unsigned sequence = 0;
    for (; (i+3) <= len && sequence < 4096; i += 3, sequence += 1) {
        in[i + 0].I = (int16_t) (sequence - 2048);
        in[i + 0].Q = 0;

        in[i + 1].I = (int16_t) (sequence - 2048);
        in[i + 1].Q = (int16_t) (sequence - 2048);

        in[i + 2].I = 0;
        in[i + 2].Q = (int16_t) (sequence - 2048);
    }

    // Fill the rest with random values
    srand(1);
    for (; i < len; ++i) {
        in[i].I = rand() % 4096 - 2048;
        in[i].Q = rand() % 4096 - 2048;
    }

    STARCH_BENCHMARK_RUN( magnitude_sc16q11, in, out_mag, len );

 done:
    STARCH_BENCHMARK_FREE(in);
    STARCH_BENCHMARK_FREE(out_mag);
}

bool STARCH_BENCHMARK_VERIFY(magnitude_sc16q11) (const sc16_t *in, uint16_t *out, unsigned len)
{
    const double max_error = 0.015; // tolerate 1.5% error
    const double epsilon = 3.0;
    bool okay = true;

    for (unsigned i = 0; i < len; ++i) {
        double I = in[i].I / 2048.0;
        double Q = in[i].Q / 2048.0;
        double expected = round(sqrt(I * I + Q * Q) * 65536.0);
        if (expected > 65535.0)
            expected = 65535.0;
        double actual = out[i];

        double error = fabs(expected - actual);
        double error_fraction = error / (expected > epsilon ? expected : epsilon);
        if (error > epsilon && error_fraction > max_error) {
            fprintf(stderr, "verification failed: in[%u].I=%d in[%u].Q=%d out[%u]=%u, expected=%.0f, error=%.2f%%\n",
                    i, in[i].I,
                    i, in[i].Q,
                    i, out[i],
                    expected,
                    error_fraction * 100.0);
            okay = false;
        }
    }

    return okay;
}
