#include <stdio.h>
#include <stdlib.h>

void STARCH_BENCHMARK(mean_power_u16) (void)
{
    uint16_t *in = NULL;
    double mean_mag, mean_magsq;
    const unsigned len = 65536;

    if (!(in = calloc(len, sizeof(*in))))
        goto done;

    for (unsigned i = 0; i < len; ++i) {
        in[i] = i;
    }

    STARCH_BENCHMARK_RUN( mean_power_u16, in, len, &mean_mag, &mean_magsq );
    /* Validation only: also check lengths that leave a partial vector at the
       end, which the main run (a multiple of every vector width) never does */
    if (starch_benchmark_validate_only) {
        static const unsigned extra_len[] = { 1, 3, 7, 9, 15, 17, 33 };
        const unsigned n_extra = sizeof(extra_len) / sizeof(extra_len[0]);
        for (unsigned k = 0; k <= n_extra; ++k) {
            unsigned n = (k < n_extra) ? extra_len[k] : len - 3;
            fprintf(stderr, "  (length %u)\n", n);
            STARCH_BENCHMARK_RUN( mean_power_u16, in, n, &mean_mag, &mean_magsq );
        }
    }


 done:
    free(in);
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
    /* the u32 flavors work in units of 1/65536 of full scale, so allow that
       much absolute error as well (it only matters for very small inputs) */
    const double quantum = 1.0 / 65536.0;

    if (!(fabs(mag_error) <= max_error * sum_mag + quantum)) {  /* also fails on NaN */
        fprintf(stderr, "verification failed: expected mean magnitude %.5f, got %.5f, error=%.2f%%\n",
                sum_mag, *out_mag, 100.0 * mag_error / sum_mag);
        okay = false;
    }


    double magsq_error = sum_magsq - *out_magsq;
    if (!(fabs(magsq_error) <= max_error * sum_magsq + quantum)) {  /* also fails on NaN */
        fprintf(stderr, "verification failed: expected mean magnitude-squared %.5f, got %.5f, error=%.2f%%\n",
                sum_magsq, *out_magsq, 100.0 * magsq_error / sum_magsq);
        okay = false;
    }

    return okay;
}
