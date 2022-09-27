#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>
#include <inttypes.h>

#include "compat/compat.h"

#include "dsp/helpers/tables.h"

/* Convert UC8 values to unsigned 16-bit magnitudes */

void STARCH_IMPL(magnitude_power_uc8, twopass) (const uc8_t *in, uint16_t *out, unsigned len, double *out_level, double *out_power)
{
#if STARCH_ALIGNMENT > 1
    starch_magnitude_uc8_aligned(in, out, len);
    starch_mean_power_u16_aligned(out, len, out_level, out_power);
#else
    starch_magnitude_uc8(in, out, len);
    starch_mean_power_u16(out, len, out_level, out_power);
#endif
}

void STARCH_IMPL(magnitude_power_uc8, lookup) (const uc8_t *in, uint16_t *out, unsigned len, double *out_level, double *out_power)
{
    const uint16_t * const restrict mag_table = get_uc8_mag_table();

    const uc8_u16_t * restrict in_align = (const uc8_u16_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    uint64_t sum_level = 0;
    uint64_t sum_power = 0;

    unsigned len1 = len;
    while (len1--) {
        uint16_t mag = mag_table[in_align[0].u16];
        out_align[0] = mag;
        sum_level += mag;
        sum_power += (uint32_t)mag * mag;

        out_align += 1;
        in_align += 1;
    }

    *out_level = sum_level / 65536.0 / len;
    *out_power = sum_power / 65536.0 / 65536.0 / len;
}

void STARCH_IMPL(magnitude_power_uc8, lookup_unroll_4) (const uc8_t *in, uint16_t *out, unsigned len, double *out_level, double *out_power)
{
    const uint16_t * const restrict mag_table = get_uc8_mag_table();

    const uc8_u16_t * restrict in_align = (const uc8_u16_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    uint64_t sum_level = 0;
    uint64_t sum_power = 0;

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

        sum_level = sum_level + mag0 + mag1 + mag2 + mag3;
        sum_power = sum_power + (uint32_t)mag0 * mag0 + (uint32_t)mag1 * mag1 + (uint32_t)mag2 * mag2 + (uint32_t)mag3 * mag3;

        out_align += 4;
        in_align += 4;
    }

    while (len1--) {
        uint16_t mag = mag_table[in_align[0].u16];

        out_align[0] = mag;

        sum_level = sum_level + mag;
        sum_power = sum_power + mag * mag;

        out_align += 1;
        in_align += 1;
    }

    *out_level = sum_level / 65536.0 / len;
    *out_power = sum_power / 65536.0 / 65536.0 / len;
}

#ifdef STARCH_FEATURE_NEON

#include <arm_neon.h>

void STARCH_IMPL_REQUIRES(magnitude_power_uc8, neon_vrsqrte, STARCH_FEATURE_NEON) (const uc8_t *in, uint16_t *out, unsigned len, double *out_level, double *out_power)
{
    const uint8_t * restrict in_align = (const uint8_t *) STARCH_ALIGNED(in);
    uint16_t * restrict out_align = STARCH_ALIGNED(out);

    const uint16x8_t offset = vdupq_n_u16((uint16_t) (127.4 * 256));
    const float32x4_t almost_one = vdupq_n_f32(65535.0 / 65536.0);

    float32x4_t sum_level = vdupq_n_f32(0);
    float32x4_t sum_power = vdupq_n_f32(0);

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
        float32x4_t magsq_f32_low = vcvtq_n_f32_u32(magsq_low, 30); /* input values are Q15, magsq is Q30 */
        float32x4_t mag_f32_low = vmulq_f32(magsq_f32_low, vrsqrteq_f32(magsq_f32_low));
        uint16x4_t mag_u16_low = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_low, 16));

        sum_level = vaddq_f32(sum_level, vminq_f32(mag_f32_low, almost_one));
        sum_power = vaddq_f32(sum_power, vminq_f32(magsq_f32_low, almost_one));

        // high half
        int16x4_t i_s16_high = vget_high_s16(i_s16);
        int16x4_t q_s16_high = vget_high_s16(q_s16);
        uint32x4_t isq_high = vreinterpretq_u32_s32(vmull_s16(i_s16_high, i_s16_high));
        uint32x4_t qsq_high = vreinterpretq_u32_s32(vmull_s16(q_s16_high, q_s16_high));
        uint32x4_t magsq_high = vqaddq_u32(isq_high, qsq_high);
        float32x4_t magsq_f32_high = vcvtq_n_f32_u32(magsq_high, 30);
        float32x4_t mag_f32_high = vmulq_f32(magsq_f32_high, vrsqrteq_f32(magsq_f32_high));
        uint16x4_t mag_u16_high = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_high, 16));

        sum_level = vaddq_f32(sum_level, vminq_f32(mag_f32_high, almost_one));
        sum_power = vaddq_f32(sum_power, vminq_f32(magsq_f32_high, almost_one));

        // store
        uint16x8_t result = vcombine_u16(mag_u16_low, mag_u16_high);
        vst1q_u16(out_align, result);

        in_align += 16;
        out_align += 8;
    }

    const int16x8_t lane0_mask = { 0xFF, 0, 0, 0, 0, 0, 0, 0 };

    unsigned len1 = len & 7;
    while (len1--) {
        uint8x8x2_t iq = vld2_dup_u8(in_align);

        // widen to 16 bits, convert to signed
        uint16x8_t i_u16 = vshll_n_u8(iq.val[0], 8);
        uint16x8_t q_u16 = vshll_n_u8(iq.val[1], 8);
        int16x8_t i_s16 = vreinterpretq_s16_u16(vsubq_u16(i_u16, offset));
        int16x8_t q_s16 = vreinterpretq_s16_u16(vsubq_u16(q_u16, offset));

        // mask so only lane 0 has a non-zero value
        // (important for sum_level / sum_power later)
        i_s16 = vandq_s16(i_s16, lane0_mask);
        q_s16 = vandq_s16(q_s16, lane0_mask);

        // low half (don't care about high half)
        int16x4_t i_s16_low = vget_low_s16(i_s16);
        int16x4_t q_s16_low = vget_low_s16(q_s16);
        uint32x4_t isq_low = vreinterpretq_u32_s32(vmull_s16(i_s16_low, i_s16_low));
        uint32x4_t qsq_low = vreinterpretq_u32_s32(vmull_s16(q_s16_low, q_s16_low));
        uint32x4_t magsq_low = vqaddq_u32(isq_low, qsq_low);
        float32x4_t magsq_f32_low = vcvtq_n_f32_u32(magsq_low, 30); /* input values are Q15, magsq is Q30 */
        float32x4_t mag_f32_low = vmulq_f32(magsq_f32_low, vrsqrteq_f32(magsq_f32_low));
        uint16x4_t mag_u16_low = vqmovn_u32(vcvtq_n_u32_f32(mag_f32_low, 16));

        sum_level = vaddq_f32(sum_level, vminq_f32(mag_f32_low, almost_one));
        sum_power = vaddq_f32(sum_power, vminq_f32(magsq_f32_low, almost_one));

        // store 1 lane only
        vst1_lane_u16(out_align, mag_u16_low, 0);

        in_align += 2;
        out_align += 1;
    }

    // add sums across vector
    float32x2_t sum2_level = vadd_f32(vget_low_f32(sum_level), vget_high_f32(sum_level));
    float32x2_t sum4_level = vpadd_f32(sum2_level, sum2_level);
    *out_level = vget_lane_f32(sum4_level, 0) / len;

    float32x2_t sum2_power = vadd_f32(vget_low_f32(sum_power), vget_high_f32(sum_power));
    float32x2_t sum4_power = vpadd_f32(sum2_power, sum2_power);
    *out_power = vget_lane_f32(sum4_power, 0) / len;
}

#endif
