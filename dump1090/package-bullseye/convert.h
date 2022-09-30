// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// convert.h: support for various IQ -> magnitude conversions
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

#ifndef DUMP1090_CONVERT_H
#define DUMP1090_CONVERT_H

struct converter_state;
typedef enum { INPUT_UC8=0, INPUT_SC16, INPUT_SC16Q11 } input_format_t;

typedef void (*iq_convert_fn)(void *iq_data,
                              uint16_t *mag_data,
                              unsigned nsamples,
                              struct converter_state *state,
                              double *out_mean_level,
                              double *out_mean_power);

iq_convert_fn init_converter(input_format_t format,
                             double sample_rate,
                             int filter_dc,
                             struct converter_state **out_state);

void cleanup_converter(struct converter_state *state);

#endif
