/*
 * Given a buffer of uint16_t Q16 magnitude values
 * return the mean magnitude and mean squared magnitude
 * (normalized to 0..1)
 */

void STARCH_IMPL(mean_power_u16, float) (const uint16_t *in, unsigned len, double *out_mean_mag, double *out_mean_magsq)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);

    float sum = 0, sumsq = 0;
    unsigned n = len;
    while (n--) {
        uint16_t mag = in_align[0];
        sum += mag;
        sumsq += (uint32_t)mag * mag;
        in_align += 1;
    }

    *out_mean_mag = sum / len / 65536.0;
    *out_mean_magsq = sumsq / len / 65536.0 / 65536.0;
}

void STARCH_IMPL(mean_power_u16, u32) (const uint16_t *in, unsigned len, double *out_mean_mag, double *out_mean_magsq)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);

    double sum = 0, sumsq = 0;

    unsigned remaining = len;
    while (remaining > 0) {
        uint32_t sum32 = 0, sumsq32 = 0;
        unsigned blocklen = (remaining > 65536 ? 65536 : remaining);
        remaining -= blocklen;

        while (blocklen--) {
            uint16_t mag = in_align[0];
            sum32 += mag;
            sumsq32 += ((uint32_t)mag * mag) >> 16;
            in_align += 1;
        }

        sum += sum32;
        sumsq += sumsq32;
    }

    *out_mean_mag = (double)sum / len / 65536.0;
    *out_mean_magsq = (double)sumsq / len / 65536.0;
}

void STARCH_IMPL(mean_power_u16, u64) (const uint16_t *in, unsigned len, double *out_mean_mag, double *out_mean_magsq)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);

    uint64_t sum = 0, sumsq = 0;
    unsigned n = len;
    while (n--) {
        uint16_t mag = in_align[0];
        sum += mag;
        sumsq += (uint32_t)mag * mag;
        in_align += 1;
    }

    *out_mean_mag = (double)sum / len / 65536.0;
    *out_mean_magsq = (double)sumsq / len / 65536.0 / 65536.0;
}

#ifdef STARCH_FEATURE_NEON

#include <arm_neon.h>

void STARCH_IMPL_REQUIRES(mean_power_u16, neon_float, STARCH_FEATURE_NEON) (const uint16_t *in, unsigned len, double *out_mean_mag, double *out_mean_magsq)
{
    const uint16_t * restrict in_align = STARCH_ALIGNED(in);

    float32x4_t mag_sum_0 = vdupq_n_f32(0);
    float32x4_t magsq_sum_0 = vdupq_n_f32(0);
    float32x4_t mag_sum_1 = vdupq_n_f32(0);
    float32x4_t magsq_sum_1 = vdupq_n_f32(0);

    unsigned len8 = len >> 3;
    while (len8--) {
        uint16x8_t mag_u16 = vld1q_u16(in_align);
        uint16x4_t mag_u16_0 = vget_low_u16(mag_u16);
        uint16x4_t mag_u16_1 = vget_high_u16(mag_u16);

        float32x4_t mag_float32_0 = vcvtq_n_f32_u32(vmovl_u16(mag_u16_0), 16);
        float32x4_t mag_float32_1 = vcvtq_n_f32_u32(vmovl_u16(mag_u16_1), 16);

        mag_sum_0 = vaddq_f32(mag_sum_0, mag_float32_0);
        mag_sum_1 = vaddq_f32(mag_sum_1, mag_float32_1);

        magsq_sum_0 = vfmaq_f32(magsq_sum_0, mag_float32_0, mag_float32_0);
        magsq_sum_1 = vfmaq_f32(magsq_sum_1, mag_float32_1, mag_float32_1);

        in_align += 8;
    }

    // reduce sums to lane 0
    float32x4_t mag_sum_q = vaddq_f32(mag_sum_0, mag_sum_1);
    float32x2_t mag_sum = vadd_f32(vget_low_f32(mag_sum_q), vget_high_f32(mag_sum_q));
    mag_sum = vpadd_f32(mag_sum, mag_sum);

    float32x4_t magsq_sum_q = vaddq_f32(magsq_sum_0, magsq_sum_1);
    float32x2_t magsq_sum = vadd_f32(vget_low_f32(magsq_sum_q), vget_high_f32(magsq_sum_q));
    magsq_sum = vpadd_f32(magsq_sum, magsq_sum);

    unsigned len1 = len & 7;
    while (len1--) {
        uint16x4_t mag_u16 = vld1_dup_u16(in_align);
        // we process both lanes here, but lane 1's sums are ignored
        float32x2_t mag_float32 = vcvt_n_f32_u32(vget_low_u32(vmovl_u16(mag_u16)), 16);
        mag_sum = vadd_f32(mag_sum, mag_float32);
        magsq_sum = vfma_f32(magsq_sum, mag_float32, mag_float32);
        in_align += 1;
    }

    *out_mean_mag = vget_lane_f32(mag_sum, 0) / len;
    *out_mean_magsq = vget_lane_f32(magsq_sum, 0) / len;
}

#endif /* STARCH_FEATURE_NEON */
