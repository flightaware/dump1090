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
        if (STARCH_IS_ALIGNED(in) && STARCH_IS_ALIGNED(mag_data))
            starch_magnitude_power_uc8_aligned(in, mag_data, nsamples, out_mean_level, out_mean_power);
        else
            starch_magnitude_power_uc8(in, mag_data, nsamples, out_mean_level, out_mean_power);
    } else {
        if (STARCH_IS_ALIGNED(in) && STARCH_IS_ALIGNED(mag_data))
            starch_magnitude_uc8_aligned(in, mag_data, nsamples);
        else
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

    if (STARCH_IS_ALIGNED(in) && STARCH_IS_ALIGNED(mag_data))
        starch_magnitude_sc16_aligned(in, mag_data, nsamples);
    else
        starch_magnitude_sc16(in, mag_data, nsamples);

    if (out_mean_level && out_mean_power) {
        if (STARCH_IS_ALIGNED(mag_data))
            starch_mean_power_u16_aligned(mag_data, nsamples, out_mean_level, out_mean_power);
        else
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

    if (STARCH_IS_ALIGNED(in) && STARCH_IS_ALIGNED(mag_data))
        starch_magnitude_sc16q11_aligned(in, mag_data, nsamples);
    else
        starch_magnitude_sc16q11(in, mag_data, nsamples);

    if (out_mean_level && out_mean_power) {
        if (STARCH_IS_ALIGNED(mag_data))
            starch_mean_power_u16_aligned(mag_data, nsamples, out_mean_level, out_mean_power);
        else
            starch_mean_power_u16(mag_data, nsamples, out_mean_level, out_mean_power);
    }
}

iq_convert_fn init_converter(input_format_t format,
                             double sample_rate,
                             int filter_dc,
                             struct converter_state **out_state)
{
    MODES_NOTUSED(sample_rate);
    MODES_NOTUSED(out_state);

    if (filter_dc) {
        fprintf(stderr, "DC filtering not supported (yet)\n");
        return NULL;
    }

    switch (format) {
    case INPUT_UC8:
        return convert_uc8;
    case INPUT_SC16:
        return convert_sc16;
    case INPUT_SC16Q11:
        return convert_sc16q11;
    default:
        fprintf(stderr, "no suitable converter for format=%d\n", format);
        return NULL;
    }
}

void cleanup_converter(struct converter_state *state)
{
    MODES_NOTUSED(state);
}
