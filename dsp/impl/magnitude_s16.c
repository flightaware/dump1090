#include <math.h>
#include <endian.h>

/* Convert (little-endian) signed 16-bit values to unsigned 16-bit magnitudes */

void STARCH_IMPL(magnitude_s16, exact_u32) (const int16_t *in, uint16_t *out, unsigned len)
{
    const int16_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    while (len--) {
        out_align[0] = abs((int16_t) le16toh(in_align[0]));

        out_align += 1;
        in_align += 1;
    }
}
