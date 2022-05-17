#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>
#include <inttypes.h>

#include "compat/compat.h"

#include "dsp/helpers/tables.h"

/* Convert UC8 values to unsigned 16-bit magnitudes */

void STARCH_IMPL(magnitude_uc8, lookup) (const uc8_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * const mag_table = get_uc8_mag_table();

    const uc8_u16_t * restrict in_align = (const uc8_u16_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    unsigned len1 = len;
    while (len1--) {
        uint16_t mag = mag_table[in_align[0].u16];
        out_align[0] = mag;

        out_align += 1;
        in_align += 1;
    }
}

void STARCH_IMPL(magnitude_uc8, lookup_unroll_4) (const uc8_t *in, uint16_t *out, unsigned len)
{
    const uint16_t * const mag_table = get_uc8_mag_table();

    const uc8_u16_t * restrict in_align = (const uc8_u16_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    unsigned len4 = len >> 2;
    unsigned len1 = len & 3;

    while (len4--) {
        uint16_t mag0 = mag_table[in_align[0].u16];
        uint16_t mag1 = mag_table[in_align[1].u16];
        uint16_t mag2 = mag_table[in_align[2].u16];
        uint16_t mag3 = mag_table[in_align[3].u16];

        out_align[0] = mag0;
        out_align[1] = mag1;
        out_align[2] = mag2;
        out_align[3] = mag3;

        out_align += 4;
        in_align += 4;
    }

    while (len1--) {
        uint16_t mag = mag_table[in_align[0].u16];

        out_align[0] = mag;

        out_align += 1;
        in_align += 1;
    }
}

void STARCH_IMPL(magnitude_uc8, exact) (const uc8_t *in, uint16_t *out, unsigned len)
{
    const uc8_t * restrict in_align = STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    unsigned len1 = len;

    while (len1--) {
        float I = (in_align[0].I - 127.4);
        float Q = (in_align[0].Q - 127.4);

        float magsq = I * I + Q * Q;
        float mag = sqrtf(magsq) * 65536.0 / 128.0;
        if (mag > 65535.0)
            mag = 65535.0;

        out_align[0] = (uint16_t)mag;

        in_align += 1;
        out_align += 1;
    }
}

#ifdef STARCH_FEATURE_NEON

#include <arm_neon.h>

void STARCH_IMPL_REQUIRES(magnitude_uc8, neon_vrsqrte, STARCH_FEATURE_NEON) (const uc8_t *in, uint16_t *out, unsigned len)
{
    const uint8_t * restrict in_align = (const uint8_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    const uint16x8_t offset = vdupq_n_u16((uint16_t) (127.4 * 256));

    unsigned len8 = len >> 3;
    while (len8--) {
        uint8x8x2_t iq = vld2_u8(in_align);

        // widen to 16 bits, convert to signed
        uint16x8_t i_u16 = vshll_n_u8(iq.val[0], 8);
        uint16x8_t q_u16 = vshll_n_u8(iq.val[1], 8);
        int16x8_t i_s16 = vreinterpretq_s16_u16(vsubq_u16(i_u16, offset));
        int16x8_t q_s16 = vreinterpretq_s16_u16(vsubq_u16(q_u16, offset));

        // low half
        int16x4_t i_s16_low = vget_low_s16(i_s16);
        int16x4_t q_s16_low = vget_low_s16(q_s16);
        uint32x4_t isq_low = vreinterpretq_u32_s32(vmull_s16(i_s16_low, i_s16_low));
        uint32x4_t qsq_low = vreinterpretq_u32_s32(vmull_s16(q_s16_low, q_s16_low));
        uint32x4_t magsq_low = vqaddq_u32(isq_low, qsq_low);
        float32x4_t magsq_f32_low = vcvtq_n_f32_u32(magsq_low, 30);                      /* input values are Q15, magsq is Q30 */
        float32x4_t mag_f32_low = vmulq_f32(vrsqrteq_f32(magsq_f32_low), magsq_f32_low); /* sqrt(x) = x * (1/sqrt(x)) */
        uint16x4_t mag_u16_low = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_low, 16));

        // high half
        int16x4_t i_s16_high = vget_high_s16(i_s16);
        int16x4_t q_s16_high = vget_high_s16(q_s16);
        uint32x4_t isq_high = vreinterpretq_u32_s32(vmull_s16(i_s16_high, i_s16_high));
        uint32x4_t qsq_high = vreinterpretq_u32_s32(vmull_s16(q_s16_high, q_s16_high));
        uint32x4_t magsq_high = vqaddq_u32(isq_high, qsq_high);
        float32x4_t magsq_f32_high = vcvtq_n_f32_u32(magsq_high, 30);
        float32x4_t mag_f32_high = vmulq_f32(vrsqrteq_f32(magsq_f32_high), magsq_f32_high);
        uint16x4_t mag_u16_high = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_high, 16));

        // store
        uint16x8_t result = vcombine_u16(mag_u16_low, mag_u16_high);
        vst1q_u16(out_align, result);

        in_align += 16;
        out_align += 8;
    }

    unsigned len1 = len & 7;
    while (len1--) {
        uint8x8x2_t iq = vld2_dup_u8(in_align);

        // widen to 16 bits, convert to signed
        uint16x8_t i_u16 = vshll_n_u8(iq.val[0], 8);
        uint16x8_t q_u16 = vshll_n_u8(iq.val[1], 8);
        int16x8_t i_s16 = vreinterpretq_s16_u16(vsubq_u16(i_u16, offset));
        int16x8_t q_s16 = vreinterpretq_s16_u16(vsubq_u16(q_u16, offset));

        // low half (don't care about high half)
        int16x4_t i_s16_low = vget_low_s16(i_s16);
        int16x4_t q_s16_low = vget_low_s16(q_s16);
        uint32x4_t isq_low = vreinterpretq_u32_s32(vmull_s16(i_s16_low, i_s16_low));
        uint32x4_t qsq_low = vreinterpretq_u32_s32(vmull_s16(q_s16_low, q_s16_low));
        uint32x4_t magsq_low = vqaddq_u32(isq_low, qsq_low);
        float32x4_t magsq_f32_low = vcvtq_n_f32_u32(magsq_low, 30); /* input values are Q15, magsq is Q30 */
        float32x4_t mag_f32_low = vmulq_f32(vrsqrteq_f32(magsq_f32_low), magsq_f32_low); /* sqrt(x) = x * (1/sqrt(x)) */
        uint16x4_t mag_u16_low = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_low, 16));

        // store 1 lane only
        vst1_lane_u16(out_align, mag_u16_low, 0);

        in_align += 2;
        out_align += 1;
    }
}

#endif /* STARCH_FEATURE_NEON */
