#include <stdlib.h>

void STARCH_BENCHMARK(mean_power_u16) (void)
{
    uint16_t *in = NULL;
    double mean_mag, mean_magsq;
    const unsigned len = 65536;

    if (!(in = STARCH_BENCHMARK_ALLOC(len, uint16_t))) {
        goto done;
    }

    for (unsigned i = 0; i < len; ++i) {
        in[i] = i;
    }

    STARCH_BENCHMARK_RUN( mean_power_u16, in, len, &mean_mag, &mean_magsq );

 done:
    STARCH_BENCHMARK_FREE(in);
}

bool STARCH_BENCHMARK_VERIFY(mean_power_u16) (const uint16_t *in, unsigned len, double *out_mag, double *out_magsq)
{
    const double max_error = 0.01; // tolerate 1% error

    double sum_mag = 0;
    double sum_magsq = 0;

    for (unsigned i = 0; i < len; ++i) {
        double mag = in[i] / 65536.0;
        sum_mag += mag;
        sum_magsq += mag * mag;
    }

    sum_mag /= len;
    sum_magsq /= len;

    bool okay = true;

    double mag_error = sum_mag - *out_mag;
    if (fabs(mag_error / sum_mag) > max_error) {
        fprintf(stderr, "verification failed: expected mean magnitude %.5f, got %.5f, error=%.2f%%\n",
                sum_mag, *out_mag, 100.0 * mag_error / sum_mag);
        okay = false;
    }


    double magsq_error = sum_magsq - *out_magsq;
    if (fabs(magsq_error / sum_magsq) > max_error) {
        fprintf(stderr, "verification failed: expected mean magnitude-squared %.5f, got %.5f, error=%.2f%%\n",
                sum_magsq, *out_magsq, 100.0 * magsq_error / sum_magsq);
        okay = false;
    }

    return okay;
}
