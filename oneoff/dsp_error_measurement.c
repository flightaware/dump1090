/* measures actual vs expected magnitude values for various magnitude_*
 * implementations
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "dsp-types.h"
#include "dsp/generated/starch.h"

static void write_results_uc8(const uc8_t *in, uint16_t *out, unsigned len, char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        return;
    }

    while (--len) {
        float I = (in[0].I - 127.4) / 128;
        float Q = (in[0].Q - 127.4) / 128;

        float phase = atan2(Q, I) * 180.0 / M_PI;
        float expected = round(sqrtf(I * I + Q * Q) * 65536);
        if (expected > 65535)
            expected = 65535;
        fprintf(fp, "%u %u %.3f %.0f %u\n", in[0].I, in[0].Q, phase, expected, out[0]);

        ++in;
        ++out;
    }

    fclose(fp);
    fprintf(stderr, "wrote %s\n", path);
}

static void write_results_sc16(const sc16_t *in, uint16_t *out, unsigned len, char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        return;
    }

    while (--len) {
        float I = in[0].I / 32768.0;
        float Q = in[0].Q / 32768.0;

        float phase = atan2(Q, I) * 180.0 / M_PI;
        float expected = round(sqrtf(I * I + Q * Q) * 65536);
        if (expected > 65535)
            expected = 65535;
        fprintf(fp, "%d %d %.3f %.0f %u\n", in[0].I, in[0].Q, phase, expected, out[0]);

        ++in;
        ++out;
    }

    fclose(fp);
    fprintf(stderr, "wrote %s\n", path);
}

static void write_results_sc16q11(const sc16_t *in, uint16_t *out, unsigned len, char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        return;
    }

    while (--len) {
        float I = in[0].I / 2048.0;
        float Q = in[0].Q / 2048.0;

        float phase = atan2(Q, I) * 180.0 / M_PI;
        float expected = round(sqrtf(I * I + Q * Q) * 65536);
        if (expected > 65535)
            expected = 65535;
        fprintf(fp, "%d %d %.3f %.0f %u\n", in[0].I, in[0].Q, phase, expected, out[0]);

        ++in;
        ++out;
    }

    fclose(fp);
    fprintf(stderr, "wrote %s\n", path);
}

static void process_uc8()
{
    const float mag_min = 0.05;
    const float mag_max = 0.95;

    const unsigned mag_steps = 5;
    const unsigned phase_steps = 256;

    const unsigned len = mag_steps * phase_steps;

    uc8_t *in = malloc(len * sizeof(*in));
    uint16_t *out = malloc(len * sizeof(*out));
    uc8_t *fill = in;

    for (unsigned mag_step = 0; mag_step < mag_steps; ++mag_step) {
        float mag = mag_min + mag_step * (mag_max - mag_min) / (mag_steps - 1);
        for (unsigned phase_step = 0; phase_step < phase_steps; ++phase_step) {
            float phase = 360.0 * phase_step / phase_steps;
            fill->I = 128 * mag * cos(phase * M_PI / 180.0) + 127.4;
            fill->Q = 128 * mag * sin(phase * M_PI / 180.0) + 127.4;

            if (fill == in || fill[-1].I != fill[0].I || fill[-1].Q != fill[0].Q)
                ++fill;
        }
    }

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_uc8_exact_generic(in, out, len);
    write_results_uc8(in, out, fill - in, "uc8-exact.tsv");
#endif

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_uc8_lookup_generic(in, out, len);
    write_results_uc8(in, out, fill - in, "uc8-lookup.tsv");
#endif

#ifdef STARCH_FLAVOR_ARMV7A_NEON_VFPV4
    starch_magnitude_uc8_neon_vrsqrte_armv7a_neon_vfpv4(in, out, len);
    write_results_uc8(in, out, fill - in, "uc8-neon-vrsqrte.tsv");
#endif
}

static void process_sc16()
{
    const float mag_min = 0.05;
    const float mag_max = 0.95;

    const unsigned mag_steps = 5;
    const unsigned phase_steps = 65536;

    const unsigned len = mag_steps * phase_steps;

    sc16_t *in = malloc(len * sizeof(*in));
    uint16_t *out = malloc(len * sizeof(*out));
    sc16_t *fill = in;

    for (unsigned mag_step = 0; mag_step < mag_steps; ++mag_step) {
        float mag = mag_min + mag_step * (mag_max - mag_min) / (mag_steps - 1);
        for (unsigned phase_step = 0; phase_step < phase_steps; ++phase_step) {
            float phase = 360.0 * phase_step / phase_steps;
            fill->I = 32768.0f * mag * cos(phase * M_PI / 180.0);
            fill->Q = 32768.0f * mag * sin(phase * M_PI / 180.0);

            if (fill == in || fill[-1].I != fill[0].I || fill[-1].Q != fill[0].Q)
                ++fill;
        }
    }

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_sc16_exact_generic(in, out, len);
    write_results_sc16(in, out, fill - in, "sc16-exact.tsv");
#endif

#ifdef STARCH_FLAVOR_ARMV7A_NEON_VFPV4
    starch_magnitude_sc16_neon_vrsqrte_armv7a_neon_vfpv4(in, out, len);
    write_results_sc16(in, out, fill - in, "sc16-neon-vrsqrte.tsv");
#endif
}

static void process_sc16q11()
{
    const float mag_min = 0.05;
    const float mag_max = 0.95;

    const unsigned mag_steps = 5;
    const unsigned phase_steps = 2048;

    const unsigned len = mag_steps * phase_steps;

    sc16_t *in = malloc(len * sizeof(*in));
    uint16_t *out = malloc(len * sizeof(*out));
    sc16_t *fill = in;

    for (unsigned mag_step = 0; mag_step < mag_steps; ++mag_step) {
        float mag = mag_min + mag_step * (mag_max - mag_min) / (mag_steps - 1);
        for (unsigned phase_step = 0; phase_step < phase_steps; ++phase_step) {
            float phase = 360.0 * phase_step / phase_steps;
            fill->I = 2048.0f * mag * cos(phase * M_PI / 180.0);
            fill->Q = 2048.0f * mag * sin(phase * M_PI / 180.0);
            if (fill == in || fill[-1].I != fill[0].I || fill[-1].Q != fill[0].Q)
                ++fill;
        }
    }

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_sc16q11_exact_generic(in, out, len);
    write_results_sc16q11(in, out, fill - in, "sc16q11-exact.tsv");
#endif

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_sc16q11_11bit_table_generic(in, out, len);
    write_results_sc16q11(in, out, fill - in, "sc16q11-lookup.tsv");
#endif

#ifdef STARCH_FLAVOR_GENERIC
    starch_magnitude_sc16q11_12bit_table_generic(in, out, len);
    write_results_sc16q11(in, out, fill - in, "sc16q11-lookup.tsv");
#endif

#ifdef STARCH_FLAVOR_ARMV7A_NEON_VFPV4
    starch_magnitude_sc16q11_neon_vrsqrte_armv7a_neon_vfpv4(in, out, len);
    write_results_sc16q11(in, out, fill - in, "sc16q11-neon-vrsqrte.tsv");
#endif
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    process_uc8();
    process_sc16();
    process_sc16q11();

    return 0;
}


