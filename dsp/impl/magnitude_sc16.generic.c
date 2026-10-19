#include <math.h>

#include "compat/compat.h"

/* Convert (little-endian) SC16 values to unsigned 16-bit magnitudes */

void STARCH_IMPL(magnitude_sc16, exact_u32) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    while (len--) {
        uint32_t I = abs((int16_t) le16toh(in[0].I));
        uint32_t Q = abs((int16_t) le16toh(in[0].Q));

        uint32_t magsq = I * I + Q * Q;
        float mag = sqrtf(magsq) * 2;
        if (mag > 65535.0)
            mag = 65535.0;
        out[0] = (uint16_t)mag;

        out += 1;
        in += 1;
    }
}

void STARCH_IMPL(magnitude_sc16, exact_float) (const sc16_t * restrict in, uint16_t * restrict out, unsigned len)
{
    while (len--) {
        float I = abs((int16_t) le16toh(in[0].I)) * 2;
        float Q = abs((int16_t) le16toh(in[0].Q)) * 2;

        float magsq = I * I + Q * Q;
        float mag = sqrtf(magsq);
        if (mag > 65535.0)
            mag = 65535.0;
        out[0] = (uint16_t)mag;

        out += 1;
        in += 1;
    }
}
