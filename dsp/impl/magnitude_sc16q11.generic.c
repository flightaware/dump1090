#include <math.h>

#include "compat/compat.h"

#include "dsp/helpers/tables.h"

/* Convert (little-endian) SC16 values with a range of -2048..+2047 to unsigned 16-bit magnitudes */

void STARCH_IMPL(magnitude_sc16q11, exact_u32) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    while (len--) {
        uint32_t I = abs((int16_t) le16toh(in[0].I));
        uint32_t Q = abs((int16_t) le16toh(in[0].Q));

        uint32_t magsq = I * I + Q * Q;
        float mag = sqrtf(magsq) * 32;
        if (mag > 65535.0)
            mag = 65535.0;
        out[0] = (uint16_t)mag;

        out += 1;
        in += 1;
    }
}

void STARCH_IMPL(magnitude_sc16q11, exact_float) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    while (len--) {
        float I = abs((int16_t) le16toh(in[0].I)) * 32;
        float Q = abs((int16_t) le16toh(in[0].Q)) * 32;

        float magsq = I * I + Q * Q;
        float mag = sqrtf(magsq);
        if (mag > 65535.0)
            mag = 65535.0;
        out[0] = (uint16_t)mag;

        out += 1;
        in += 1;
    }
}

void STARCH_IMPL(magnitude_sc16q11, 11bit_table) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    const uint16_t * restrict table = get_sc16q11_mag_11bit_table();

    while (len--) {
        uint16_t I = abs((int16_t)le16toh(in[0].I));
        if (I >= 2048)
            I = 2047;
        uint16_t Q = abs((int16_t)le16toh(in[0].Q));
        if (Q >= 2048)
            Q = 2047;
        out[0] = table[(Q << 11) | I];

        in += 1;
        out += 1;
    }
}

void STARCH_IMPL(magnitude_sc16q11, 12bit_table) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    const uint16_t * restrict table = get_sc16q11_mag_12bit_table();

    while (len--) {
        unsigned index = ((in[0].I & 4095) << 12) | (in[0].Q & 4095);
        out[0] = table[index];

        in += 1;
        out += 1;
    }
}


/* SC16Q11 (already 12-bit, little-endian) -> 12-bit signed interleaved I,Q,
 * plus mean level and power normalised to 12-bit full scale */

void STARCH_IMPL(iqdata_conv_sc16q11, arith) (const sc16_t * restrict in, uint16_t * restrict out_u, unsigned len, double *out_level, double *out_power)
{
    int16_t * restrict out = (int16_t *) out_u;   // signed I/Q stored in the uint16_t FIFO buffer

    double sum_level = 0;
    uint64_t sum_power = 0;

    for (unsigned n = 0; n < len; ++n) {
        int32_t I = (int16_t) le16toh(in[n].I);
        int32_t Q = (int16_t) le16toh(in[n].Q);

        // keep to 12-bit like the stock 11-bit table path
        if (I > 2047) I = 2047; else if (I < -2048) I = -2048;
        if (Q > 2047) Q = 2047; else if (Q < -2048) Q = -2048;

        out[2 * n]     = (int16_t) I;
        out[2 * n + 1] = (int16_t) Q;

        uint32_t magsq = (uint32_t) (I * I + Q * Q);
        sum_power += magsq;
        sum_level += sqrtf((float) magsq);
    }

    if (out_level != NULL)
        *out_level = sum_level / 2048.0 / len;

    if (out_power != NULL)
       *out_power = (double) sum_power / 2048.0 / 2048.0 / len;
}

