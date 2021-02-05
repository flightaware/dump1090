#include <stdlib.h>
#include <math.h>
#include <endian.h>
#include "dsp/helpers/tables.h"

/*
 * Convert (little-endian) unsigned 16 offset 12 ( Excess 2048 format)
 * values to unsigned 16-bit magnitudes.
 *
 * Offset 12:
 *
 *          Signed      Scaled
 * Input    Value       Magnitude
 * ------------------------------
 * 0        <invalid>   <invalid>
 * 1        -2047       65535
 * 2        -2046       65503
 * ...
 * 2046     -2          64
 * 2047     -1          32
 * 2048      0          0
 * 2049      1          32
 * 2050      2          64
 *
 * ...
 * 4093      2045       65471
 * 4094      2046       65503
 * 4095      2047       65535
 *
 */

#define CONVERT_AND_SCALE(__in) ceil(abs(le16toh(__in) - 2048) * (32767.0 / 2047.0))

void STARCH_IMPL(magnitude_u16o12, exact) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    while (len--) {
        out_align[0] = CONVERT_AND_SCALE(in_align[0]);

        out_align += 1;
        in_align += 1;
    }
}

void STARCH_IMPL(magnitude_u16o12, exact_unroll_4) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    unsigned len4 = len >> 2;
    unsigned len1 = len & 3;

    while (len4--) {

        out_align[0] = CONVERT_AND_SCALE(in_align[0]);
        out_align[1] = CONVERT_AND_SCALE(in_align[1]);
        out_align[2] = CONVERT_AND_SCALE(in_align[2]);
        out_align[3] = CONVERT_AND_SCALE(in_align[3]);

        out_align += 4;
        in_align += 4;
    }

    while (len1--) {
        out_align[0] = CONVERT_AND_SCALE(in_align[0]);

        out_align += 1;
        in_align += 1;
    }

}

void STARCH_IMPL(magnitude_u16o12, exact_unroll_8) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    unsigned len8 = len >> 3;
    unsigned len1 = len & 4;

    while (len8--) {

        out_align[0] = CONVERT_AND_SCALE(in_align[0]);
        out_align[1] = CONVERT_AND_SCALE(in_align[1]);
        out_align[2] = CONVERT_AND_SCALE(in_align[2]);
        out_align[3] = CONVERT_AND_SCALE(in_align[3]);
        out_align[4] = CONVERT_AND_SCALE(in_align[4]);
        out_align[5] = CONVERT_AND_SCALE(in_align[5]);
        out_align[6] = CONVERT_AND_SCALE(in_align[6]);
        out_align[7] = CONVERT_AND_SCALE(in_align[7]);


        out_align += 8;
        in_align += 8;
    }

    while (len1--) {
        out_align[0] = CONVERT_AND_SCALE(in_align[0]);

        out_align += 1;
        in_align += 1;
    }

}

void STARCH_IMPL(magnitude_u16o12, lookup) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);
    const uint16_t * const mag_table = get_u16o12_mag_table();

    while (len--) {
        out_align[0] = mag_table[in_align[0]];

        out_align += 1;
        in_align += 1;
    }
}

void STARCH_IMPL(magnitude_u16o12, lookup_unroll_4) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);
    const uint16_t * const mag_table = get_u16o12_mag_table();

    unsigned len4 = len >> 2;
    unsigned len1 = len & 3;

    while (len4--) {
        out_align[0] = mag_table[in_align[0]];
        out_align[1] = mag_table[in_align[1]];
        out_align[2] = mag_table[in_align[2]];
        out_align[3] = mag_table[in_align[3]];

        out_align += 4;
        in_align += 4;
    }

    while (len1--) {
        out_align[0] = mag_table[in_align[0]];

        out_align += 1;
        in_align += 1;
    }
}

void STARCH_IMPL(magnitude_u16o12, lookup_unroll_8) (const uint16_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);
    const uint16_t * const mag_table = get_u16o12_mag_table();

    unsigned len8 = len >> 3;
    unsigned len1 = len & 4;

    while (len8--) {
        out_align[0] = mag_table[in_align[0]];
        out_align[1] = mag_table[in_align[1]];
        out_align[2] = mag_table[in_align[2]];
        out_align[3] = mag_table[in_align[3]];
        out_align[4] = mag_table[in_align[4]];
        out_align[5] = mag_table[in_align[5]];
        out_align[6] = mag_table[in_align[6]];
        out_align[7] = mag_table[in_align[7]];

        out_align += 8;
        in_align += 8;
    }

    while (len1--) {
        out_align[0] = mag_table[in_align[0]];

        out_align += 1;
        in_align += 1;
    }

}

#undef CONVERT_AND_SCALE
