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

#ifdef DEMOD_2400X

//
// DEMOD_2400X demodulator (the default in this build; --no-demod2400x selects the
// stock one): these converters output interleaved signed I,Q at 12-bit scale
// (-2048..+2047) into the FIFO buffer (stored as uint16_t), instead of magnitudes.
//
// The iqdata_conv_* kernels also remove DC; the filter state is kept in
// converter_state between calls. They accept NULL for the level/power outputs.
//

struct converter_state {
    int32_t dc_offset[2];       // DC filter state (I, Q): running mean << 12
};

static void convert_iq_uc8(void *iq_data,
                           uint16_t *mag_data,
                           unsigned nsamples,
                           struct converter_state *state,
                           double *out_mean_level,
                           double *out_mean_power)
{
    const uc8_t *in = (const uc8_t *) iq_data;

    starch_iqdata_conv_uc8(in, mag_data, nsamples, state->dc_offset, state->dc_offset, out_mean_level, out_mean_power);
}

static void convert_iq_sc16(void *iq_data,
                            uint16_t *mag_data,
                            unsigned nsamples,
                            struct converter_state *state,
                            double *out_mean_level,
                            double *out_mean_power)
{
    const sc16_t *in = (const sc16_t *) iq_data;

    starch_iqdata_conv_sc16(in, mag_data, nsamples, state->dc_offset, state->dc_offset, out_mean_level, out_mean_power);
}

static void convert_iq_sc16q11(void *iq_data,
                               uint16_t *mag_data,
                               unsigned nsamples,
                               struct converter_state *state,
                               double *out_mean_level,
                               double *out_mean_power)
{
    const sc16_t *in = (const sc16_t *) iq_data;

    starch_iqdata_conv_sc16q11(in, mag_data, nsamples, state->dc_offset, state->dc_offset, out_mean_level, out_mean_power);
}

#endif /* DEMOD_2400X */

// Stock converters: magnitudes


static void convert_uc8(void *iq_data,
                        uint16_t *mag_data,
                        unsigned nsamples,
                        struct converter_state *state,
                        double *out_mean_level,
                        double *out_mean_power)
{
    MODES_NOTUSED(state);

    const uc8_t *in = (const uc8_t *) iq_data;

    if (out_mean_level && out_mean_power) {
        starch_magnitude_power_uc8(in, mag_data, nsamples, out_mean_level, out_mean_power);
    } else {
        starch_magnitude_uc8(in, mag_data, nsamples);
    }
}

static void convert_sc16(void *iq_data,
                         uint16_t *mag_data,
                         unsigned nsamples,
                         struct converter_state *state,
                         double *out_mean_level,
                         double *out_mean_power)
{
    MODES_NOTUSED(state);

    const sc16_t *in = (const sc16_t *) iq_data;

    starch_magnitude_sc16(in, mag_data, nsamples);

    if (out_mean_level && out_mean_power) {
        starch_mean_power_u16(mag_data, nsamples, out_mean_level, out_mean_power);
    }
}

static void convert_sc16q11(void *iq_data,
                            uint16_t *mag_data,
                            unsigned nsamples,
                            struct converter_state *state,
                            double *out_mean_level,
                            double *out_mean_power)
{
    MODES_NOTUSED(state);

    const sc16_t *in = (const sc16_t *) iq_data;

    starch_magnitude_sc16q11(in, mag_data, nsamples);

    if (out_mean_level && out_mean_power) {
        starch_mean_power_u16(mag_data, nsamples, out_mean_level, out_mean_power);
    }
}

iq_convert_fn init_converter(input_format_t format,
                             double sample_rate,
                             int filter_dc,
                             struct converter_state **out_state)
{
    MODES_NOTUSED(sample_rate);

    if (filter_dc) {
        fprintf(stderr, "DC filtering not supported (yet)\n");
        return NULL;
    }

#ifdef DEMOD_2400X
    if (Modes.demod_2400x) {
        if (!(*out_state = calloc(1, sizeof(struct converter_state)))) {
            fprintf(stderr, "can't allocate converter state\n");
            return NULL;
        }

        switch (format) {
        case INPUT_UC8:
            return convert_iq_uc8;
        case INPUT_SC16:
            return convert_iq_sc16;
        case INPUT_SC16Q11:
            return convert_iq_sc16q11;
        default:
            fprintf(stderr, "no suitable converter for format=%u\n", (unsigned) format);
            free(*out_state);
            *out_state = NULL;
            return NULL;
        }
    }
#else
    MODES_NOTUSED(out_state);
#endif

    switch (format) {
    case INPUT_UC8:
        return convert_uc8;
    case INPUT_SC16:
        return convert_sc16;
    case INPUT_SC16Q11:
        return convert_sc16q11;
    default:
        fprintf(stderr, "no suitable converter for format=%u\n", (unsigned) format);
        return NULL;
    }
}

void cleanup_converter(struct converter_state *state)
{
#ifdef DEMOD_2400X
    free(state);        // NULL with the stock converters
#else
    MODES_NOTUSED(state);
#endif
}

