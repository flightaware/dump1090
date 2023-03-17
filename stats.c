// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// stats.c: statistics helpers.
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

#include "dump1090.h"

void add_timespecs(const struct timespec *x, const struct timespec *y, struct timespec *z)
{
    z->tv_sec = x->tv_sec + y->tv_sec;
    z->tv_nsec = x->tv_nsec + y->tv_nsec;
    z->tv_sec += z->tv_nsec / 1000000000L;
    z->tv_nsec = z->tv_nsec % 1000000000L;
}

static void display_range_histogram(struct stats *st);

void display_stats(struct stats *st) {
    int j;
    time_t tt_start, tt_end;
    struct tm tm_start, tm_end;
    char tb_start[30], tb_end[30];

    printf("\n\n");

    tt_start = st->start/1000;
    localtime_r(&tt_start, &tm_start);
    strftime(tb_start, sizeof(tb_start), "%c %Z", &tm_start);
    tt_end = st->end/1000;
    localtime_r(&tt_end, &tm_end);
    strftime(tb_end, sizeof(tb_end), "%c %Z", &tm_end);

    printf("Statistics: %s - %s\n", tb_start, tb_end);

    if (!Modes.net_only) {
        printf("Local receiver:\n");
        printf("  %12llu samples processed\n",                        (unsigned long long)st->samples_processed);
        printf("  %12llu samples dropped\n",                          (unsigned long long)st->samples_dropped);

        printf("  %12u Mode A/C messages received\n",                 st->demod_modeac);
        printf("  %12u Mode-S message preambles received\n",          st->demod_preambles);
        printf("    %12u with bad message format or invalid CRC\n",   st->demod_rejected_bad);
        printf("    %12u with unrecognized ICAO address\n",           st->demod_rejected_unknown_icao);
        printf("    %12u accepted with correct CRC\n",                st->demod_accepted[0]);
        for (j = 1; j <= Modes.nfix_crc; ++j)
            printf("    %12u accepted with %d-bit error repaired\n", st->demod_accepted[j], j);

        if (st->noise_power_sum > 0 && st->noise_power_count > 0) {
            printf("  %5.1f dBFS noise power\n",
                   10 * log10(st->noise_power_sum / st->noise_power_count));
        } else {
            printf("  ----- dBFS noise power\n");
        }

        if (st->signal_power_sum > 0 && st->signal_power_count > 0) {
            printf("  %5.1f dBFS mean signal power\n",
                   10 * log10(st->signal_power_sum / st->signal_power_count));
        } else {
            printf("  ----- dBFS mean signal power\n");
        }

        if (st->peak_signal_power > 0) {
            printf("  %5.1f dBFS peak signal power\n",
                   10 * log10(st->peak_signal_power));
        } else {
            printf("  ----- dBFS peak signal power\n");
        }

        printf("  %5u messages with signal power above -3dBFS\n",
               st->strong_signal_count);

        if (st->sdr_gain >= 0)
            printf("  %4.1fdB current SDR gain\n",
                   sdrGetGainDb(st->sdr_gain));
    }

    if (st->adaptive_valid) {
        printf("Adaptive gain:\n"
               "  %5u loud undecoded bursts\n"
               "  %5u loud decoded messages\n"
               "  %5.1f dBFS latest noise floor\n"
               "  %5.1f dB latest dynamic range gain upper limit\n"
               "  %5u gain changes caused by adaptive gain control\n",
               st->adaptive_loud_undecoded,
               st->adaptive_loud_decoded,
               st->adaptive_noise_dbfs,
               sdrGetGainDb(st->adaptive_range_gain_limit),
               st->adaptive_gain_changes);

        uint32_t total_seconds = 0;
        for (unsigned i = 0; i < STATS_GAIN_COUNT; ++i)
            total_seconds += st->adaptive_gain_seconds[i];

        if (total_seconds) {
            unsigned count = 0;
            for (unsigned i = 0; i < STATS_GAIN_COUNT; ++i) {
                count += st->adaptive_gain_seconds[i];
                if (count >= total_seconds/2) {
                    printf("  %5.1f dB median gain\n", sdrGetGainDb(i));
                    break;
                }
            }

            printf("  Gain histogram:\n");
            for (unsigned i = 0; i < STATS_GAIN_COUNT; ++i) {
                unsigned seconds = st->adaptive_gain_seconds[i];
                if (seconds) {
                    printf("    %5.1f dB: %5u seconds (%5.1f%%)\n",
                           sdrGetGainDb(i), seconds, 100.0 * seconds / total_seconds);
                }
            }

        }
    }

    if (Modes.net) {
        printf("Messages from network clients:\n");
        printf("  %8u Mode A/C messages received\n",               st->remote_received_modeac);
        printf("  %8u Mode S messages received\n",                 st->remote_received_modes);
        printf("    %8u with bad message format or invalid CRC\n", st->remote_rejected_bad);
        printf("    %8u with unrecognized ICAO address\n",         st->remote_rejected_unknown_icao);
        printf("    %8u accepted with correct CRC\n",              st->remote_accepted[0]);
        for (j = 1; j <= Modes.nfix_crc; ++j)
            printf("    %8u accepted with %d-bit error repaired\n", st->remote_accepted[j], j);
    }

    printf("Decoder:\n"
           "  %8u total usable messages\n",
           st->messages_total);

    for (unsigned i = 0; i < 32; ++i) {
        if (st->messages_by_df[i])
            printf("    %8u DF%u messages\n", st->messages_by_df[i], i);
    }

    printf("  %8u surface position messages received\n"
           "  %8u airborne position messages received\n"
           "  %8u global CPR attempts with valid positions\n"
           "  %8u global CPR attempts with bad data\n"
           "    %8u global CPR attempts that failed the range check\n"
           "    %8u global CPR attempts that failed the speed check\n"
           "  %8u global CPR attempts with insufficient data\n"
           "  %8u local CPR attempts with valid positions\n"
           "    %8u aircraft-relative positions\n"
           "    %8u receiver-relative positions\n"
           "  %8u local CPR attempts that did not produce useful positions\n"
           "    %8u local CPR attempts that failed the range check\n"
           "    %8u local CPR attempts that failed the speed check\n"
           "  %8u CPR messages that look like transponder failures filtered\n",
           st->cpr_surface,
           st->cpr_airborne,
           st->cpr_global_ok,
           st->cpr_global_bad,
           st->cpr_global_range_checks,
           st->cpr_global_speed_checks,
           st->cpr_global_skipped,
           st->cpr_local_ok,
           st->cpr_local_aircraft_relative,
           st->cpr_local_receiver_relative,
           st->cpr_local_skipped,
           st->cpr_local_range_checks,
           st->cpr_local_speed_checks,
           st->cpr_filtered);

    printf("  %8u non-ES altitude messages from ES-equipped aircraft ignored\n", st->suppressed_altitude_messages);
    printf("  %8u unique aircraft tracks\n", st->unique_aircraft);
    printf("  %8u aircraft tracks where only one message was seen\n", st->single_message_aircraft);
    printf("  %8u aircraft tracks which were not marked reliable\n", st->unreliable_aircraft);

    {
        uint64_t demod_cpu_millis = (uint64_t)st->demod_cpu.tv_sec*1000UL + st->demod_cpu.tv_nsec/1000000UL;
        uint64_t reader_cpu_millis = (uint64_t)st->reader_cpu.tv_sec*1000UL + st->reader_cpu.tv_nsec/1000000UL;
        uint64_t background_cpu_millis = (uint64_t)st->background_cpu.tv_sec*1000UL + st->background_cpu.tv_nsec/1000000UL;

        printf("CPU load: %5.1f%%\n"
               "  %5llu ms for demodulation\n"
               "  %5llu ms for reading from USB\n"
               "  %5llu ms for network input and background tasks\n",
               100.0 * (demod_cpu_millis + reader_cpu_millis + background_cpu_millis) / (st->end - st->start + 1),
               (unsigned long long) demod_cpu_millis,
               (unsigned long long) reader_cpu_millis,
               (unsigned long long) background_cpu_millis);
    }

    if (Modes.stats_range_histo)
        display_range_histogram(st);

    fflush(stdout);
}

static void display_range_histogram(struct stats *st)
{
    uint32_t peak;
    int i, j;
    int heights[RANGE_BUCKET_COUNT];

#if 0
#define NPIXELS 4
    char *pixels[NPIXELS] = { ".", "o", "O", "|" };
#else
    // UTF-8 bar symbols
#define NPIXELS 8
    char *pixels[NPIXELS] = {
        "\xE2\x96\x81",
        "\xE2\x96\x82",
        "\xE2\x96\x83",
        "\xE2\x96\x84",
        "\xE2\x96\x85",
        "\xE2\x96\x86",
        "\xE2\x96\x87",
        "\xE2\x96\x88"
    };
#endif

    printf ("Range histogram:\n\n");

    for (i = 0, peak = 0; i < RANGE_BUCKET_COUNT; ++i) {
        if (st->range_histogram[i] > peak)
            peak = st->range_histogram[i];
    }

    for (i = 0; i < RANGE_BUCKET_COUNT; ++i) {
        heights[i] = st->range_histogram[i] * 20.0 * NPIXELS / peak;
        if (st->range_histogram[i] > 0 && heights[i] == 0)
            heights[i] = 1;
    }

    for (j = 0; j < 20; ++j) {
        for (i = 0; i < RANGE_BUCKET_COUNT; ++i) {
            int pheight = heights[i] - ((19 - j) * NPIXELS);
            if (pheight <= 0)
                printf(" ");
            else if (pheight >= NPIXELS)
                printf("%s", pixels[NPIXELS-1]);
            else
                printf("%s", pixels[pheight]);
        }
        printf("\n");
    }

    for (i = 0; i < RANGE_BUCKET_COUNT/4; ++i) {
        printf("----");
    }
    printf("\n");

    for (i = 0; i < RANGE_BUCKET_COUNT/4; ++i) {
        printf(" '  ");
    }
    printf("\n");

    for (i = 0; i < RANGE_BUCKET_COUNT/4; ++i) {
        int midpoint = round((i*4+1.5) * Modes.maxRange / RANGE_BUCKET_COUNT / 1000);
        printf("%03d ", midpoint);
    }
    printf("km\n");
}

void reset_stats(struct stats *st) {
    static struct stats st_zero;
    *st = st_zero;
    st->sdr_gain = -1;
}

void add_stats(const struct stats *st1, const struct stats *st2, struct stats *target) {
    int i;

    if (st1->start == 0)
        target->start = st2->start;
    else if (st2->start == 0)
        target->start = st1->start;
    else if (st1->start < st2->start)
        target->start = st1->start;
    else
        target->start = st2->start;

    const struct stats *newer;
    if (st1->end > st2->end || (st1->end == st2->end && st1->start > st2->start)) {
        newer = st1;
    } else {
        newer = st2;
    }

    target->end = newer->end;

    target->demod_preambles = st1->demod_preambles + st2->demod_preambles;
    target->demod_rejected_bad = st1->demod_rejected_bad + st2->demod_rejected_bad;
    target->demod_rejected_unknown_icao = st1->demod_rejected_unknown_icao + st2->demod_rejected_unknown_icao;
    for (i = 0; i < MODES_MAX_BITERRORS+1; ++i)
        target->demod_accepted[i]  = st1->demod_accepted[i] + st2->demod_accepted[i];
    target->demod_modeac = st1->demod_modeac + st2->demod_modeac;

    target->samples_processed = st1->samples_processed + st2->samples_processed;
    target->samples_dropped = st1->samples_dropped + st2->samples_dropped;

    target->sdr_gain = newer->sdr_gain;

    add_timespecs(&st1->demod_cpu, &st2->demod_cpu, &target->demod_cpu);
    add_timespecs(&st1->reader_cpu, &st2->reader_cpu, &target->reader_cpu);
    add_timespecs(&st1->background_cpu, &st2->background_cpu, &target->background_cpu);

    // noise power:
    target->noise_power_sum = st1->noise_power_sum + st2->noise_power_sum;
    target->noise_power_count = st1->noise_power_count + st2->noise_power_count;

    // mean signal power:
    target->signal_power_sum = st1->signal_power_sum + st2->signal_power_sum;
    target->signal_power_count = st1->signal_power_count + st2->signal_power_count;

    // peak signal power seen
    if (st1->peak_signal_power > st2->peak_signal_power)
        target->peak_signal_power = st1->peak_signal_power;
    else
        target->peak_signal_power = st2->peak_signal_power;

    // strong signals
    target->strong_signal_count = st1->strong_signal_count + st2->strong_signal_count;

    // remote messages:
    target->remote_received_modeac = st1->remote_received_modeac + st2->remote_received_modeac;
    target->remote_received_modes = st1->remote_received_modes + st2->remote_received_modes;
    target->remote_rejected_bad = st1->remote_rejected_bad + st2->remote_rejected_bad;
    target->remote_rejected_unknown_icao = st1->remote_rejected_unknown_icao + st2->remote_rejected_unknown_icao;
    for (i = 0; i < MODES_MAX_BITERRORS+1; ++i)
        target->remote_accepted[i]  = st1->remote_accepted[i] + st2->remote_accepted[i];

    // total messages:
    target->messages_total = st1->messages_total + st2->messages_total;
    for (i = 0; i < 32; ++i)
        target->messages_by_df[i] = st1->messages_by_df[i] + st2->messages_by_df[i];

    // CPR decoding:
    target->cpr_surface = st1->cpr_surface + st2->cpr_surface;
    target->cpr_airborne = st1->cpr_airborne + st2->cpr_airborne;
    target->cpr_global_ok = st1->cpr_global_ok + st2->cpr_global_ok;
    target->cpr_global_bad = st1->cpr_global_bad + st2->cpr_global_bad;
    target->cpr_global_skipped = st1->cpr_global_skipped + st2->cpr_global_skipped;
    target->cpr_global_range_checks = st1->cpr_global_range_checks + st2->cpr_global_range_checks;
    target->cpr_global_speed_checks = st1->cpr_global_speed_checks + st2->cpr_global_speed_checks;
    target->cpr_local_ok = st1->cpr_local_ok + st2->cpr_local_ok;
    target->cpr_local_aircraft_relative = st1->cpr_local_aircraft_relative + st2->cpr_local_aircraft_relative;
    target->cpr_local_receiver_relative = st1->cpr_local_receiver_relative + st2->cpr_local_receiver_relative;
    target->cpr_local_skipped = st1->cpr_local_skipped + st2->cpr_local_skipped;
    target->cpr_local_range_checks = st1->cpr_local_range_checks + st2->cpr_local_range_checks;
    target->cpr_local_speed_checks = st1->cpr_local_speed_checks + st2->cpr_local_speed_checks;
    target->cpr_filtered = st1->cpr_filtered + st2->cpr_filtered;

    target->suppressed_altitude_messages = st1->suppressed_altitude_messages + st2->suppressed_altitude_messages;

    // aircraft
    target->unique_aircraft = st1->unique_aircraft + st2->unique_aircraft;
    target->single_message_aircraft = st1->single_message_aircraft + st2->single_message_aircraft;
    target->unreliable_aircraft = st1->unreliable_aircraft + st2->unreliable_aircraft;

    // range histogram
    for (i = 0; i < RANGE_BUCKET_COUNT; ++i)
        target->range_histogram[i] = st1->range_histogram[i] + st2->range_histogram[i];

    // adaptive gain measurements

    const struct stats *adaptive_best;
    if (st1->adaptive_valid && st2->adaptive_valid)
        adaptive_best = newer;
    else if (st1->adaptive_valid)
        adaptive_best = st1;
    else
        adaptive_best = st2;

    target->adaptive_valid = adaptive_best->adaptive_valid;
    for (unsigned i = 0; i < STATS_GAIN_COUNT; ++i)
        target->adaptive_gain_seconds[i] = st1->adaptive_gain_seconds[i] + st2->adaptive_gain_seconds[i];
    target->adaptive_loud_undecoded = st1->adaptive_loud_undecoded + st2->adaptive_loud_undecoded;
    target->adaptive_loud_decoded = st1->adaptive_loud_decoded + st2->adaptive_loud_decoded;
    target->adaptive_gain_changes = st1->adaptive_gain_changes + st2->adaptive_gain_changes;
    target->adaptive_noise_dbfs = adaptive_best->adaptive_noise_dbfs;
    target->adaptive_range_gain_limit = adaptive_best->adaptive_range_gain_limit;
}
