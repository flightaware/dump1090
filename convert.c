// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// convert.c: support for various IQ -> magnitude conversions
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "dump1090.h"

struct converter_state {
    float dc_a;
    float dc_b;
    float z1_I;
    float z1_Q;
};

static uint16_t *uc8_lookup;
static bool init_uc8_lookup()
{
    if (uc8_lookup)
        return true;

    uc8_lookup = malloc(sizeof(uint16_t) * 256 * 256);
    if (!uc8_lookup) {
        fprintf(stderr, "can't allocate UC8 conversion lookup table\n");
        return false;
    }

    for (int i = 0; i <= 255; i++) {
        for (int q = 0; q <= 255; q++) {
            float fI, fQ, magsq;

            fI = (i - 127.5) / 127.5;
            fQ = (q - 127.5) / 127.5;
            magsq = fI * fI + fQ * fQ;
            if (magsq > 1)
                magsq = 1;
            float mag = sqrtf(magsq);

            uc8_lookup[le16toh((i*256)+q)] = (uint16_t) (mag * 65535.0f + 0.5f);
        }
    }

    return true;
}

static void convert_uc8_nodc(void *iq_data,
                             uint16_t *mag_data,
                             unsigned nsamples,
                             struct converter_state *state,
                             double *out_mean_level,
                             double *out_mean_power)
{
    uint16_t *in = iq_data;
    unsigned i;
    uint64_t sum_level = 0;
    uint64_t sum_power = 0;
    uint16_t mag;

    MODES_NOTUSED(state);

    // unroll this a bit

#define DO_ONE_SAMPLE \
    do {                                            \
        mag = uc8_lookup[*in++];                    \
        *mag_data++ = mag;                          \
        sum_level += mag;                           \
        sum_power += (uint32_t)mag * (uint32_t)mag; \
    } while(0)

    // unroll this a bit
    for (i = 0; i < (nsamples>>3); ++i) {
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
        DO_ONE_SAMPLE;
    }

    for (i = 0; i < (nsamples&7); ++i) {
        DO_ONE_SAMPLE;
    }

#undef DO_ONE_SAMPLE

    if (out_mean_level) {
        *out_mean_level = sum_level / 65536.0 / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / 65535.0 / 65535.0 / nsamples;
    }
}

static void convert_uc8_generic(void *iq_data,
                                uint16_t *mag_data,
                                unsigned nsamples,
                                struct converter_state *state,
                                double *out_mean_level,
                                double *out_mean_power)
{
    uint8_t *in = iq_data;
    float z1_I = state->z1_I;
    float z1_Q = state->z1_Q;
    const float dc_a = state->dc_a;
    const float dc_b = state->dc_b;

    unsigned i;
    uint8_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = *in++;
        Q = *in++;
        fI = (I - 127.5f) / 127.5f;
        fQ = (Q - 127.5f) / 127.5f;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t)(mag * 65535.0f + 0.5f);
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_sc16_generic(void *iq_data,
                                 uint16_t *mag_data,
                                 unsigned nsamples,
                                 struct converter_state *state,
                                double *out_mean_level,
                                double *out_mean_power)
{
    uint16_t *in = iq_data;
    float z1_I = state->z1_I;
    float z1_Q = state->z1_Q;
    const float dc_a = state->dc_a;
    const float dc_b = state->dc_b;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t)le16toh(*in++);
        Q = (int16_t)le16toh(*in++);
        fI = I / 32768.0f;
        fQ = Q / 32768.0f;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t)(mag * 65535.0f + 0.5f);
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static void convert_sc16_nodc(void *iq_data,
                              uint16_t *mag_data,
                              unsigned nsamples,
                              struct converter_state *state,
                              double *out_mean_level,
                              double *out_mean_power)
{
    MODES_NOTUSED(state);

    uint16_t *in = iq_data;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t)le16toh(*in++);
        Q = (int16_t)le16toh(*in++);
        fI = I / 32768.0f;
        fQ = Q / 32768.0f;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t)(mag * 65535.0f + 0.5f);
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

/*
 * Used by convert_uint16_offset12 to both shift the
 * sample value down by 2048 and get its absolute value.
 */
static __attribute__((aligned)) uint16_t shifts4096[4096];
/*
 * Contains the 0.0 -> 1.0 float magnitudes corresponding
 * to the 0 -> 2047 sample values.
 */
static __attribute__((aligned)) float magnitudes2048[2048];
/*
 * Contains the squares of the float magnitudes for
 * determining signal power.
 */
static __attribute__((aligned)) float powers2048[2048];
/*
 * Converts 0 -> 2047 scaled magnitudes to 0 -> 65535
 * scaled magnitudes for use by the demodulators.
 */
static __attribute__((aligned)) uint16_t scaled65k2048[2048];

static bool init_uint16()
{
    int i;

    for(i = 0; i < (int)ARRAY_SIZE(shifts4096); i++) {
        shifts4096[i] = ABS((i - 2048));
    }
    for(i = 0; i < (int)ARRAY_SIZE(magnitudes2048); i++) {
        magnitudes2048[i] = i / 2047.0f;
        powers2048[i] = magnitudes2048[i] * magnitudes2048[i];
        scaled65k2048[i] = magnitudes2048[i] * 65535.0f + 0.5f;
    }

    return 1;
}

/*
 * This format expects full scale -32767 -> +32767 signed values
 * and converts them to full scale 0 -> 65535 unsigned values.
 * Being "full-scale" does NOT imply full-scale precision.
 * We're still limited by the precision of the ADC.  In most
 * cases, the ADC 8 or 12 bit output was simply scaled up to
 * fit in 16 bits.
 */
static void convert_int16(void *int_data,
                              uint16_t *mag_data,
                              unsigned nsamples,
                              struct converter_state *state,
                              double *out_mean_level,
                              double *out_mean_power)
{
    MODES_NOTUSED(state);
    int16_t *in = int_data;
    size_t i;
    float sum_level = 0;
    float sum_power = 0;

    for (i = 0; i < nsamples; i++) {
        /*
         * Get the absolute value and rescale it to 0-2047
         * so we can use the LUTs.
         */
        in[i] = ABS(in[i]) >> 4;

        sum_level += magnitudes2048[in[i]];
        sum_power += powers2048[in[i]];
        mag_data[i] = scaled65k2048[in[i]];
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

/*
 * This format expects 12 bit values where the virtual
 * -2047 -> +2047 values have been shifted upwards by 2048.
 * This makes the the negative samples occupy 1-2047 and the
 * positive values occupy 2048 -> 4095.  We use the shifts4096
 * table to shift the values back again and take their
 * absolute values.
 */
static void convert_uint16_offset12(void *int_data,
                              uint16_t *mag_data,
                              unsigned nsamples,
                              struct converter_state *state,
                              double *out_mean_level,
                              double *out_mean_power)
{
    MODES_NOTUSED(state);
    uint16_t *in = int_data;
    size_t i;
    float sum_level = 0;
    float sum_power = 0;

    for (i = 0; i < nsamples; i++) {
        in[i] = shifts4096[in[i]];
        sum_level += magnitudes2048[in[i]];
        sum_power += powers2048[in[i]];
        mag_data[i] = scaled65k2048[in[i]];
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

// SC16Q11_TABLE_BITS controls the size of the lookup table
// for SC16Q11 data. The size of the table is 2 * (1 << (2*BITS))
// bytes. Reducing the number of bits reduces precision but
// can run substantially faster by staying in cache.
// See convert_benchmark.c for some numbers.

// Leaving SC16QQ_TABLE_BITS undefined will disable the table lookup and always use
// the floating-point path, which may be faster on some systems

#if defined(SC16Q11_TABLE_BITS)

#define USE_BITS SC16Q11_TABLE_BITS
#define LOSE_BITS (11 - SC16Q11_TABLE_BITS)

static uint16_t *sc16q11_lookup;
static bool init_sc16q11_lookup()
{
    if (sc16q11_lookup)
        return true;

    sc16q11_lookup = malloc(sizeof(uint16_t) * (1 << (USE_BITS * 2)));
    if (!sc16q11_lookup) {
        fprintf(stderr, "can't allocate SC16Q11 conversion lookup table\n");
        return false;
    }

    for (int i = 0; i < 2048; i += (1 << LOSE_BITS)) {
        for (int q = 0; q < 2048; q += (1 << LOSE_BITS)) {
            float fI = i / 2048.0, fQ = q / 2048.0;
            float magsq = fI * fI + fQ * fQ;
            if (magsq > 1)
                magsq = 1;
            float mag = sqrtf(magsq);

            unsigned index = ((i >> LOSE_BITS) << USE_BITS) | (q >> LOSE_BITS);
            sc16q11_lookup[index] = (uint16_t)(mag * 65535.0f + 0.5f);
        }
    }

    return true;
}

static void convert_sc16q11_table(void *iq_data,
                                  uint16_t *mag_data,
                                  unsigned nsamples,
                                  struct converter_state *state,
                                  double *out_mean_level,
                                  double *out_mean_power)
{
    uint16_t *in = iq_data;
    unsigned i;
    uint16_t I, Q;
    uint64_t sum_level = 0;
    uint64_t sum_power = 0;
    uint16_t mag;

    MODES_NOTUSED(state);

    for (i = 0; i < nsamples; ++i) {
        I = abs((int16_t)le16toh(*in++)) & 2047;
        Q = abs((int16_t)le16toh(*in++)) & 2047;
        mag = sc16q11_lookup[((I >> LOSE_BITS) << USE_BITS) | (Q >> LOSE_BITS)];
        *mag_data++ = mag;
        sum_level += mag;
        sum_power += (uint32_t)mag * (uint32_t)mag;
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / 65536.0 / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / 65535.0 / 65535.0 / nsamples;
    }
}

#else /* ! defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_nodc(void *iq_data,
                                 uint16_t *mag_data,
                                 unsigned nsamples,
                                 struct converter_state *state,
                                 double *out_mean_level,
                                 double *out_mean_power)
{
    MODES_NOTUSED(state);

    uint16_t *in = iq_data;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t)le16toh(*in++);
        Q = (int16_t)le16toh(*in++);
        fI = I / 2048.0f;
        fQ = Q / 2048.0f;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t)(mag * 65535.0f + 0.5f);
    }

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

#endif /* defined(SC16Q11_TABLE_BITS) */

static void convert_sc16q11_generic(void *iq_data,
                                    uint16_t *mag_data,
                                    unsigned nsamples,
                                    struct converter_state *state,
                                    double *out_mean_level,
                                    double *out_mean_power)
{
    uint16_t *in = iq_data;
    float z1_I = state->z1_I;
    float z1_Q = state->z1_Q;
    const float dc_a = state->dc_a;
    const float dc_b = state->dc_b;

    unsigned i;
    int16_t I, Q;
    float fI, fQ, magsq;
    float sum_level = 0, sum_power = 0;

    for (i = 0; i < nsamples; ++i) {
        I = (int16_t)le16toh(*in++);
        Q = (int16_t)le16toh(*in++);
        fI = I / 2048.0f;
        fQ = Q / 2048.0f;

        // DC block
        z1_I = fI * dc_a + z1_I * dc_b;
        z1_Q = fQ * dc_a + z1_Q * dc_b;
        fI -= z1_I;
        fQ -= z1_Q;

        magsq = fI * fI + fQ * fQ;
        if (magsq > 1)
            magsq = 1;

        float mag = sqrtf(magsq);
        sum_power += magsq;
        sum_level += mag;
        *mag_data++ = (uint16_t)(mag * 65535.0f + 0.5f);
    }

    state->z1_I = z1_I;
    state->z1_Q = z1_Q;

    if (out_mean_level) {
        *out_mean_level = sum_level / nsamples;
    }

    if (out_mean_power) {
        *out_mean_power = sum_power / nsamples;
    }
}

static struct {
    input_format_t format;
    int can_filter_dc;
    iq_convert_fn fn;
    const char *description;
    bool (*init)();
    const char *name;
} converters_table[] = {
    // In order of preference
    { INPUT_UC8,          0, convert_uc8_nodc,         "UC8, integer/table path", init_uc8_lookup, "uc8" },
    { INPUT_UC8,          1, convert_uc8_generic,      "UC8, float path", NULL, "uc8" },
    { INPUT_SC16,         0, convert_sc16_nodc,        "SC16, float path, no DC", NULL, "sc16" },
    { INPUT_SC16,         1, convert_sc16_generic,     "SC16, float path", NULL, "sc16" },
#if defined(SC16Q11_TABLE_BITS)
    { INPUT_SC16Q11,      0, convert_sc16q11_table,    "SC16Q11, integer/table path", init_sc16q11_lookup, "sc16q11" },
#else
    { INPUT_SC16Q11,      0, convert_sc16q11_nodc,     "SC16Q11, float path, no DC", NULL, "sc16q11" },
#endif
    { INPUT_SC16Q11,      1, convert_sc16q11_generic,  "SC16Q11, float path", NULL, "sc16q11" },
    { INPUT_INT16,        0, convert_int16,       "INT16, table path, no DC", init_uint16, "s16" },
    { INPUT_UINT16_OFFSET12,  0, convert_uint16_offset12,   "UINT16 offset 12, table path, no dc", init_uint16, "u16o12" },
    { 0, 0, NULL, NULL, NULL, NULL }
};

const char *formatGetName(input_format_t format_type)
{
    for (int i = 0; converters_table[i].name; i++) {
        if (format_type == converters_table[i].format) {
            return converters_table[i].name;
        }
    }
    return NULL;
}

input_format_t formatGetByName(const char *name)
{
    for (int i = 0; converters_table[i].name; i++) {
        if (strcasecmp(name, converters_table[i].name) == 0) {
            return converters_table[i].format;
        }
    }
    return INPUT_NONE;
}

iq_convert_fn init_converter(input_format_t format,
                             double sample_rate,
                             int filter_dc,
                             struct converter_state **out_state)
{
    int i;

    for (i = 0; converters_table[i].fn; ++i) {
        if (converters_table[i].format != format)
            continue;
        if (filter_dc && !converters_table[i].can_filter_dc)
            continue;
        break;
    }

    if (!converters_table[i].fn) {
        fprintf(stderr, "no suitable converter for format=%d dc=%d\n",
                format, filter_dc);
        return NULL;
    }

    if (converters_table[i].init) {
        if (!converters_table[i].init())
            return NULL;
    }

    *out_state = malloc(sizeof(struct converter_state));
    if (! *out_state) {
        fprintf(stderr, "can't allocate converter state\n");
        return NULL;
    }

    (*out_state)->z1_I = 0;
    (*out_state)->z1_Q = 0;

    if (filter_dc) {
        // init DC block @ 1Hz
        (*out_state)->dc_b = exp(-2.0 * M_PI * 1.0 / sample_rate);
        (*out_state)->dc_a = 1.0 - (*out_state)->dc_b;
    } else {
        // if the converter does filtering, make sure it has no effect
        (*out_state)->dc_b = 1.0;
        (*out_state)->dc_a = 0.0;
    }

    return converters_table[i].fn;
}

void cleanup_converter(struct converter_state *state)
{
    free(state);
}
