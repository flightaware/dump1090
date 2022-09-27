#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void STARCH_BENCHMARK(magnitude_power_uc8) (void)
{
    uc8_t *in = NULL;
    uint16_t *out_mag = NULL;
    const unsigned len = 65536;
    double out_level, out_power;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, uc8_t)) || !(out_mag = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    unsigned i = 0;

    // 0.9 magnitude, varying phase
    double degrees = 0;
    for (; i < len && degrees < 360; i += 1, degrees += 1) {
        in[i].I = (uint8_t) (0.9 * cos(degrees * M_PI / 180.0) * 128 + 127.4);
        in[i].Q = (uint8_t) (0.9 * sin(degrees * M_PI / 180.0) * 128 + 127.4);
    }

    // 0, 45, 90 degree phase, full input range
    unsigned sequence = 0;
    for (; (i+3) <= len && sequence < 256; i += 3, sequence += 1) {
        in[i + 0].I = sequence;
        in[i + 0].Q = 0;

        in[i + 1].I = sequence;
        in[i + 1].Q = sequence;

        in[i + 2].I = 0;
        in[i + 2].Q = sequence;
    }

    // Fill the rest with random values
    srand(1);
    for (; i < len; ++i) {
        in[i].I = rand() % 256;
        in[i].Q = rand() % 256;
    }

    STARCH_BENCHMARK_RUN( magnitude_power_uc8, in, out_mag, len, &out_level, &out_power );

 done:
    STARCH_BENCHMARK_FREE(in);
    STARCH_BENCHMARK_FREE(out_mag);
}

bool STARCH_BENCHMARK_VERIFY(magnitude_power_uc8) (const uc8_t *in, uint16_t *out, unsigned len, double *out_level, double *out_power)
{
    const double max_error = 0.015; // tolerate 1.5% error
    const double epsilon = 1.0;
    bool okay = true;

    double sum_level = 0, sum_power = 0;

    for (unsigned i = 0; i < len; ++i) {
        double I = (in[i].I - 127.4) / 128;
        double Q = (in[i].Q - 127.4) / 128;
        double magsq = I * I + Q * Q;
        double expected = round(sqrt(magsq) * 65536.0);
        if (expected > 65535.0)
            expected = 65535.0;
        double actual = out[i];

        double error = fabs(expected - actual);
        double error_fraction = error / (expected > epsilon ? expected : epsilon);
        if (error > epsilon && error_fraction > max_error) {
            fprintf(stderr, "verification failed: in[%u].I=%u in[%u].Q=%u out[%u]=%u, expected=%.0f, error=%.2f%%\n",
                    i, in[i].I,
                    i, in[i].Q,
                    i, out[i],
                    expected,
                    error_fraction * 100.0);
            okay = false;
        }

        sum_level += expected;
        sum_power += expected * expected;
    }

    sum_level = sum_level / len / 65536.0;
    sum_power = sum_power / len / (65536.0 * 65536.0);

    double level_error = sum_level - *out_level;
    if (fabs(level_error / sum_level) > max_error) {
        fprintf(stderr, "verification failed: expected mean level %.5f, got mean level %.5f, error=%.2f%%\n",
                sum_level, *out_level, 100.0 * level_error / sum_level);
        okay = false;
    }

    double power_error = sum_power - *out_power;
    if (fabs(power_error / sum_power) > max_error) {
        fprintf(stderr, "verification failed: expected mean power %.5f, got mean power %.5f, error=%.2f%%\n",
                sum_power, *out_power, 100.0 * power_error / sum_power);
        okay = false;
    }

    return okay;
}
