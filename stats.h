// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// stats.h: statistics structures and prototypes.
//
// Copyright (c) 2015 Oliver Jowett <oliver@mutability.co.uk>
// Copyright (c) 2021 FlightAware LLC
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

// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef DUMP1090_STATS_H
#define DUMP1090_STATS_H

struct stats {
    uint64_t start;
    uint64_t end;

    // Mode S demodulator counts:
    uint32_t demod_preambles;
    uint32_t demod_rejected_bad;
    uint32_t demod_rejected_unknown_icao;
    uint32_t demod_accepted[MODES_MAX_BITERRORS+1];

    // Mode A/C demodulator counts:
    uint32_t demod_modeac;

    uint64_t samples_processed;
    uint64_t samples_dropped;

    // SDR settings:
    int sdr_gain;  // current gain step in use

    // timing:
    struct timespec demod_cpu;
    struct timespec reader_cpu;
    struct timespec background_cpu;

    // noise floor:
    double noise_power_sum;
    uint64_t noise_power_count;

    // mean signal power:
    double signal_power_sum;
    uint64_t signal_power_count;

    // peak signal power seen
    double peak_signal_power;

    // number of signals with power > -3dBFS
    uint32_t strong_signal_count;

    // remote messages:
    uint32_t remote_received_modeac;
    uint32_t remote_received_modes;
    uint32_t remote_rejected_bad;
    uint32_t remote_rejected_unknown_icao;
    uint32_t remote_accepted[MODES_MAX_BITERRORS+1];

    // total messages:
    uint32_t messages_total;
    // .. divided by DF
    uint32_t messages_by_df[32];

    // CPR decoding:
    unsigned int cpr_surface;
    unsigned int cpr_airborne;
    unsigned int cpr_global_ok;
    unsigned int cpr_global_bad;
    unsigned int cpr_global_skipped;
    unsigned int cpr_global_range_checks;
    unsigned int cpr_global_speed_checks;
    unsigned int cpr_local_ok;
    unsigned int cpr_local_skipped;
    unsigned int cpr_local_range_checks;
    unsigned int cpr_local_speed_checks;
    unsigned int cpr_local_aircraft_relative;
    unsigned int cpr_local_receiver_relative;
    unsigned int cpr_filtered;

    // number of altitude messages ignored because
    // we had a recent DF17/18 altitude
    unsigned int suppressed_altitude_messages;

    // aircraft:
    // total "new" aircraft (i.e. not seen in the last 30 or 300s)
    unsigned int unique_aircraft;
    // we saw only a single message
    unsigned int single_message_aircraft;
    // we never considered the track reliable
    unsigned int unreliable_aircraft;

    // range histogram
#define RANGE_BUCKET_COUNT 76
    uint32_t range_histogram[RANGE_BUCKET_COUNT];

    // adaptive gain measurements
#define STATS_GAIN_COUNT 64
    bool adaptive_valid;                                // is the following data valid?
    uint32_t adaptive_gain_seconds[STATS_GAIN_COUNT];   // Seconds spent at each gain step
    uint32_t adaptive_loud_undecoded;                   // Total number of loud, undecoded bursts
    uint32_t adaptive_loud_decoded;                     // Total number of loud, decoded messages
    uint32_t adaptive_gain_changes;                     // Total number of gain changes caused by adaptive gain control
    double adaptive_noise_dbfs;                         // Current adaptive-dynamic-range smoothed noise measurement, dBFS
    int adaptive_range_gain_limit;                      // Current adaptive-dynamic-range gain step limit
};

void add_stats(const struct stats *st1, const struct stats *st2, struct stats *target);
void display_stats(struct stats *st);
void reset_stats(struct stats *st);

void add_timespecs(const struct timespec *x, const struct timespec *y, struct timespec *z);

#endif
