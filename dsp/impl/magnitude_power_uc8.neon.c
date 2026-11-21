#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>
#include <inttypes.h>

#include "compat/compat.h"

#include "dsp/helpers/tables.h"

/* Convert UC8 values to unsigned 16-bit magnitudes */

#ifndef __ARM_NEON
#  error Neon support not available
#endif

#include <arm_neon.h>

void STARCH_IMPL(magnitude_power_uc8, neon_vrsqrte) (const uc8_t * restrict in, uint16_t * restrict out, unsigned len, double *out_level, double *out_power)
{
    const uint8_t * restrict in_u8 = (const uint8_t *) in;
    
    const uint16x8_t offset = vdupq_n_u16((uint16_t) (127.4 * 256));
    const float32x4_t almost_one = vdupq_n_f32(65535.0 / 65536.0);

    float32x4_t sum_level = vdupq_n_f32(0);
    float32x4_t sum_power = vdupq_n_f32(0);

    unsigned len8 = len >> 3;
    while (len8--) {
        uint8x8x2_t iq = vld2_u8(in_u8);

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
        vst1q_u16(out, result);

        in_u8 += 16;
        out += 8;
    }

    // Handle the 0..7 trailing samples with scalar code.
    // (A single-lane NEON version would need the unused lanes to stay
    // finite: vrsqrteq_f32(0) is +Inf and 0 * Inf is NaN.)
    float tail_level = 0, tail_power = 0;
    unsigned len1 = len & 7;
    while (len1--) {
        float fI = (in_u8[0] * 256.0f - (uint16_t) (127.4 * 256)) / 32768.0f;
        float fQ = (in_u8[1] * 256.0f - (uint16_t) (127.4 * 256)) / 32768.0f;
        float magsq = fI * fI + fQ * fQ;
        float mag = sqrtf(magsq);
        if (magsq > 65535.0f / 65536.0f) magsq = 65535.0f / 65536.0f;
        if (mag > 65535.0f / 65536.0f) mag = 65535.0f / 65536.0f;
        *out = (uint16_t) (mag * 65536.0f);
        tail_level += mag;
        tail_power += magsq;
        in_u8 += 2;
        out += 1;
    }

    // add sums across vector
    float32x2_t sum2_level = vadd_f32(vget_low_f32(sum_level), vget_high_f32(sum_level));
    float32x2_t sum4_level = vpadd_f32(sum2_level, sum2_level);
    *out_level = len ? (vget_lane_f32(sum4_level, 0) + tail_level) / len : 0;

    float32x2_t sum2_power = vadd_f32(vget_low_f32(sum_power), vget_high_f32(sum_power));
    float32x2_t sum4_power = vpadd_f32(sum2_power, sum2_power);
    *out_power = len ? (vget_lane_f32(sum4_power, 0) + tail_power) / len : 0;
}
