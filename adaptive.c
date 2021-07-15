// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// adaptive.c: adaptive gain control
//
// Copyright (c) 2021 FlightAware, LLC
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
#include "adaptive.h"

//
// gain limits
//
static int adaptive_gain_min;
static int adaptive_gain_max;

// gain steps relative to current gain
static float adaptive_gain_up_db;
static float adaptive_gain_down_db;

//
// block handling
//

static unsigned adaptive_block_remaining;              // samples in each block
static unsigned adaptive_block_size;                   // samples remaining in the current block

void adaptive_init();
void adaptive_update(uint16_t *buf, unsigned length, struct modesMessage *decoded);
static void adaptive_update_single(uint16_t *buf, unsigned length, struct modesMessage *decoded);
static void adaptive_end_of_block();
static void adaptive_control_update();

//
// burst handling
//

static unsigned adaptive_burst_window_size;            // samples in each burst window
static unsigned adaptive_burst_window_remaining;       // samples remaining in the current burst window
static unsigned adaptive_burst_window_counter;         // loud samples seen in current burst window
static unsigned adaptive_burst_runlength;              // consecutive loud burst windows seen
static unsigned adaptive_burst_block_loud_undecoded;   // loud undecoded bursts seen in this block so far
static unsigned adaptive_burst_block_loud_decoded;     // loud decoded messages seen in this block so far
static double adaptive_burst_loud_undecoded_smoothed;  // smoothed rate of loud misdecodes per block
static double adaptive_burst_loud_decoded_smoothed;    // smoothed rate of loud successful decodes per block
static unsigned adaptive_burst_change_timer;           // countdown inhibiting control after changing gain
static double adaptive_burst_loud_threshold;           // current signal level threshold for a "loud decode"
static unsigned adaptive_burst_loud_blocks = 0;        // consecutive blocks with loud rate
static unsigned adaptive_burst_quiet_blocks = 0;       // consecutive blocks with quiet rate

static void adaptive_burst_update(uint16_t *buf, unsigned length);
static void adaptive_burst_skip(unsigned length);
static unsigned adaptive_burst_count_samples(uint16_t *buf, unsigned n);
static void adaptive_burst_scan_windows(uint16_t *buf, unsigned windows);
static void adaptive_burst_end_of_window(unsigned counter);
static void adaptive_burst_end_of_block();

//
// noise floor measurement (adaptive dynamic range)
//

static unsigned *adaptive_range_radix;                 // radix-sort buckets for current block
static unsigned adaptive_range_radix_counter;          // sum of all radix-sort buckets (= number of samples sorted)
static double adaptive_range_smoothed;                 // smoothed noise floor estimate, dBFS
static enum { RANGE_SCAN_IDLE, RANGE_SCAN_UP, RANGE_SCAN_DOWN } adaptive_range_state = RANGE_SCAN_UP;
static unsigned adaptive_range_change_timer;           // countdown inhibiting control after changing gain
static unsigned adaptive_range_rescan_timer;           // countdown to next upwards gain reprobe
static int adaptive_range_gain_limit;                  // probed maximum gain step with acceptable dynamic range

static void adaptive_range_update(uint16_t *buf, unsigned length);
static void adaptive_range_end_of_block();

// Try to change the SDR gain to 'step' and tell the user about it,
// with 'why' as the reason to show. Return true if the gain actually changed.
static bool adaptive_set_gain(int step, const char *why)
{
    if (step < adaptive_gain_min)
        step = adaptive_gain_min;
    if (step > adaptive_gain_max)
        step = adaptive_gain_max;

    int current_gain = sdrGetGain();
    if (current_gain == step)
        return false;

    fprintf(stderr, "adaptive: changing gain from %.1fdB (step %d) to %.1fdB (step %d) because: %s\n",
            sdrGetGainDb(current_gain), current_gain, sdrGetGainDb(step), step, why);

    int new_gain = sdrSetGain(step);
    bool changed = (current_gain != new_gain);
    if (changed)
        ++Modes.stats_current.adaptive_gain_changes;
    return changed;
}

// Update internal state to reflect a gain change
// (usually after adaptive_set_gain returns true, but also called during init)
static void adaptive_gain_changed()
{
    int new_gain = sdrGetGain();
    adaptive_gain_up_db = sdrGetGainDb(new_gain + 1) - sdrGetGainDb(new_gain);
    adaptive_gain_down_db = sdrGetGainDb(new_gain) - sdrGetGainDb(new_gain - 1);
    
    double loud_threshold_dbfs = 0 - adaptive_gain_up_db - 3.0;
    adaptive_burst_loud_threshold = pow(10, loud_threshold_dbfs / 10.0);

    adaptive_range_change_timer = Modes.adaptive_range_change_delay;
    adaptive_burst_change_timer = Modes.adaptive_burst_change_delay;
    adaptive_burst_loud_blocks = 0;
    adaptive_burst_quiet_blocks = 0;
}

// External init entry point
void adaptive_init()
{
    int maxgain = sdrGetMaxGain();

    // If the SDR doesn't support gain control, disable ourselves
    if (maxgain < 0) {
        if (Modes.adaptive_burst_control || Modes.adaptive_range_control) {
            fprintf(stderr, "warning: adaptive gain control requested, but SDR gain control not available, ignored.\n");
        }
        Modes.adaptive_burst_control = false;
        Modes.adaptive_range_control = false;
    }        

    // If we're disabled, do nothing
    if (!Modes.adaptive_burst_control && !Modes.adaptive_range_control)
        return;

    // Look for 40us bursts
    adaptive_burst_window_size = Modes.sample_rate / 25000;
    adaptive_burst_window_remaining = adaptive_burst_window_size;
    adaptive_burst_window_counter = 0;

    // Use an overall block size that is an exact multiple of the burst window, close to 1 second long
    adaptive_block_size = adaptive_burst_window_size * 25000;
    adaptive_block_remaining = adaptive_block_size;

    adaptive_range_radix = calloc(sizeof(unsigned), 65536);
    adaptive_range_state = RANGE_SCAN_UP;

    // select and enforce gain limits
    for (adaptive_gain_min = 0; adaptive_gain_min < maxgain; ++adaptive_gain_min) {
        if (sdrGetGainDb(adaptive_gain_min) >= Modes.adaptive_min_gain_db)
            break;
    }

    for (adaptive_gain_max = maxgain; adaptive_gain_max > adaptive_gain_min; --adaptive_gain_max) {
        if (sdrGetGainDb(adaptive_gain_max) <= Modes.adaptive_max_gain_db)
            break;
    }

    fprintf(stderr, "adaptive: enabled adaptive gain control with gain limits %.1fdB (step %d) .. %.1fdB (step %d)\n",
            sdrGetGainDb(adaptive_gain_min), adaptive_gain_min, sdrGetGainDb(adaptive_gain_max), adaptive_gain_max);
    if (Modes.adaptive_range_control)
        fprintf(stderr, "adaptive: enabled dynamic range control, target dynamic range %.1fdB\n", Modes.adaptive_range_target);
    if (Modes.adaptive_burst_control)
        fprintf(stderr, "adaptive: enabled burst control\n");
    adaptive_set_gain(sdrGetGain(), "constraining gain to adaptive gain limits");
    adaptive_gain_changed();

    adaptive_range_gain_limit = sdrGetGain();
}

// Feed some samples into the adaptive system. Any number of samples might be passed in.
void adaptive_update(uint16_t *buf, unsigned length, struct modesMessage *decoded)
{
    if (!Modes.adaptive_burst_control && !Modes.adaptive_range_control)
        return;
    
    // process samples up to a block boundary, then process the completed block
    while (length >= adaptive_block_remaining) {
        adaptive_update_single(buf, adaptive_block_remaining, decoded);
        buf += adaptive_block_remaining;
        length -= adaptive_block_remaining;

        adaptive_end_of_block();
        adaptive_block_remaining = adaptive_block_size;
    }

    // process final samples that don't complete a block
    if (length > 0) {
        adaptive_update_single(buf, length, decoded);
        adaptive_block_remaining -= length;
    }
}

// Feed some samples into the adaptive system. The samples are guaranteed to not cross a block boundary.
static void adaptive_update_single(uint16_t *buf, unsigned length, struct modesMessage *decoded)
{
    if (decoded) {
        if (/* decoded->msgbits == 112 && */ decoded->signalLevel >= adaptive_burst_loud_threshold)
            ++adaptive_burst_block_loud_decoded;
        adaptive_burst_skip(length);
    } else {
        adaptive_burst_update(buf, length);
        adaptive_range_update(buf, length);
    }
}

// Burst measurement: ignore the next 'length' samples (they are a successfully decoded message)
static void adaptive_burst_skip(unsigned length)
{
    if (!Modes.adaptive_burst_control)
        return;

    // first window
    if (length < adaptive_burst_window_remaining) {
        // partial fill
        adaptive_burst_window_remaining -= length;
        return;
    }

    // skip remainder of first window, dispatch it
    adaptive_burst_end_of_window(adaptive_burst_window_counter);
    length -= adaptive_burst_window_remaining;

    // skip remaining windows, dispatch them
    unsigned windows = length / adaptive_burst_window_size;
    unsigned samples = windows * adaptive_burst_window_size;
    while (windows--)
        adaptive_burst_end_of_window(0);

    length -= samples;

    // final partial window
    adaptive_burst_window_counter = 0;
    adaptive_burst_window_remaining = adaptive_burst_window_size - length;
}

// Burst measurement: process 'length' samples from 'buf', look for loud bursts;
// the samples might cross burst window boundaries;
// the samples will not cross a block boundary.
static void adaptive_burst_update(uint16_t *buf, unsigned length)
{
    if (!Modes.adaptive_burst_control)
        return;

    // first window
    if (length < adaptive_burst_window_remaining) {
        // partial fill
        adaptive_burst_window_counter += adaptive_burst_count_samples(buf, length);
        adaptive_burst_window_remaining -= length;
        return;
    }

    // complete fill of first partial window
    unsigned n = adaptive_burst_window_remaining;
    unsigned counter = adaptive_burst_window_counter + adaptive_burst_count_samples(buf, n);
    adaptive_burst_end_of_window(counter);
    buf += n;
    length -= n;

    // remaining windows
    unsigned windows = length / adaptive_burst_window_size;
    unsigned samples = windows * adaptive_burst_window_size;
    adaptive_burst_scan_windows(buf, windows);
    buf += samples;
    length -= samples;

    // final partial window
    adaptive_burst_window_counter = adaptive_burst_count_samples(buf, length);
    adaptive_burst_window_remaining = adaptive_burst_window_size - length;
}

// Burst measurement: process 'windows' complete burst windows starting at 'buf';
// 'buf' is aligned to the start of a burst window
static void adaptive_burst_scan_windows(uint16_t *buf, unsigned windows)
{
    while (windows--) {
        unsigned counter = adaptive_burst_count_samples(buf, adaptive_burst_window_size);
        buf += adaptive_burst_window_size;
        adaptive_burst_end_of_window(counter);
    }
}

// Burst measurement: process 'n' samples from 'buf', look for loud samples;
// the samples are guaranteed not to cross window boundaries;
// return the number of loud samples seen
static inline unsigned adaptive_burst_count_samples(uint16_t *buf, unsigned n)
{
    unsigned counter;
    starch_count_above_u16(buf, n, 46395 /* -3dBFS */, &counter);
    return counter;
}

// Burst measurement: we reached the end of a burst window with 'counter'
// loud samples seen, handle that window.
static void adaptive_burst_end_of_window(unsigned counter)
{
    if (counter > adaptive_burst_window_size / 4) {
        // This window is loud, extend any existing run of loud windows
        ++adaptive_burst_runlength;
    } else {
        // Quiet window. If we saw a run of loud windows >= 80us long, count
        // that as a candidate for an over-amplified message that was
        // not decoded.
        if (adaptive_burst_runlength >= 2 && adaptive_burst_runlength <= 5)
            ++adaptive_burst_block_loud_undecoded;
        adaptive_burst_runlength = 0;
    }
}

// Noise measurement: process 'length' samples from 'buf'.
// The samples will not cross a block boundary.
static void adaptive_range_update(uint16_t *buf, unsigned length)
{
    if (!Modes.adaptive_range_control)
        return;

    adaptive_range_radix_counter += length;
    while (length--) {
        // do a very simple radix sort of sample magnitudes
        // so we can later find the Nth percentile value
        ++adaptive_range_radix[buf[0]];
        ++buf;
    }
}

// Noise measurement: we reached the end of a block, update
// our noise estimate
static void adaptive_range_end_of_block()
{
    if (!Modes.adaptive_range_control)
        return;

    unsigned n = 0, i = 0;

    // measure Nth percentile magnitude
    unsigned count_n = adaptive_range_radix_counter * Modes.adaptive_range_percentile / 100;
    while (i < 65536 && n <= count_n)
        n += adaptive_range_radix[i++];
    uint16_t percentile_n = i - 1;

    // maintain an EMA of the Nth percentile
    adaptive_range_smoothed = adaptive_range_smoothed * (1 - Modes.adaptive_range_alpha) + percentile_n * Modes.adaptive_range_alpha;
    // .. report to stats in dBFS
    if (adaptive_range_smoothed > 0) {
        Modes.stats_current.adaptive_noise_dbfs = 20 * log10(adaptive_range_smoothed / 65536.0);
    } else {
        Modes.stats_current.adaptive_noise_dbfs = 0;
    }

    // reset radix sort for the next block
    memset(adaptive_range_radix, 0, 65536 * sizeof(unsigned));
    adaptive_range_radix_counter = 0;
}

// Burst measurement: we reached the end of a block, update our burst rate estimate
static void adaptive_burst_end_of_block()
{
    if (!Modes.adaptive_burst_control)
        return;

    // maintain an EMA of the number of undecoded loud bursts seen per block
    Modes.stats_current.adaptive_loud_undecoded += adaptive_burst_block_loud_undecoded;
    adaptive_burst_loud_undecoded_smoothed = adaptive_burst_loud_undecoded_smoothed * (1 - Modes.adaptive_burst_alpha) + adaptive_burst_block_loud_undecoded * Modes.adaptive_burst_alpha;
    adaptive_burst_block_loud_undecoded = 0;

    // maintain an EMA of the number of decoded, but loud, messages seen per block
    Modes.stats_current.adaptive_loud_decoded += adaptive_burst_block_loud_decoded;
    adaptive_burst_loud_decoded_smoothed = adaptive_burst_loud_decoded_smoothed * (1 - Modes.adaptive_burst_alpha) + adaptive_burst_block_loud_decoded * Modes.adaptive_burst_alpha;
    adaptive_burst_block_loud_decoded = 0;
}


void flush_stats(uint64_t now);

static void adaptive_increase_gain(const char *why)
{
    if (adaptive_set_gain(sdrGetGain() + 1, why))
        adaptive_gain_changed();
}

static void adaptive_decrease_gain(const char *why)
{
    if (adaptive_set_gain(sdrGetGain() - 1, why))
        adaptive_gain_changed();
}

// Adaptive gain: we reached a block boundary. Update measurements and act on them.
static void adaptive_end_of_block()
{
    adaptive_range_end_of_block();
    adaptive_burst_end_of_block();

    adaptive_control_update();

    Modes.stats_current.adaptive_valid = true;
    unsigned current = Modes.stats_current.adaptive_gain = sdrGetGain();
    Modes.stats_current.adaptive_range_gain_limit = adaptive_range_gain_limit;
    ++Modes.stats_current.adaptive_gain_seconds[current < STATS_GAIN_COUNT ? current : STATS_GAIN_COUNT-1];
}

static void adaptive_control_update()
{
    // votes for what to do with the gain
    // "gain_not_up" overlaps somewhat with "gain_down", but they are not identical;
    // burst control may want to prevent gain from increasing, but not necessarily
    // decrease gain.

    bool gain_up = false;
    const char *gain_up_reason = NULL;
    bool gain_down = false;
    const char *gain_down_reason = NULL;
    bool gain_not_up = false;

    int current_gain = sdrGetGain();

    if (adaptive_burst_change_timer)
        --adaptive_burst_change_timer;
    if (adaptive_range_change_timer > 0)
        --adaptive_range_change_timer;
    if (adaptive_range_rescan_timer > 0)
        --adaptive_range_rescan_timer;

    if (Modes.adaptive_burst_control && !adaptive_burst_change_timer) {
        if (adaptive_burst_loud_undecoded_smoothed > Modes.adaptive_burst_loud_rate) {
            adaptive_burst_quiet_blocks = 0;
            ++adaptive_burst_loud_blocks;
        } else if (adaptive_burst_loud_decoded_smoothed < Modes.adaptive_burst_quiet_rate) {
            adaptive_burst_loud_blocks = 0;
            ++adaptive_burst_quiet_blocks;
        } else {
            adaptive_burst_loud_blocks = 0;
            adaptive_burst_quiet_blocks = 0;
        }

        if (adaptive_burst_loud_blocks >= Modes.adaptive_burst_loud_runlength) {
            // we need to reduce gain (further)
            gain_down = gain_not_up = true;
            gain_down_reason = "high rate of loud undecoded messages";

            // if we're currently doing a downward scan, reducing gain further may confuse it;
            // stop that scan and restart it once we are no longer in a reduced-gain state
            if (adaptive_range_state == RANGE_SCAN_DOWN) {
                adaptive_range_state = RANGE_SCAN_IDLE;
                adaptive_range_rescan_timer = 0;
            }
        } else if (adaptive_burst_quiet_blocks < Modes.adaptive_burst_quiet_runlength) {
            // we're OK at the current gain, but should not increase it
            gain_not_up = true;
        } else if (current_gain < adaptive_range_gain_limit) {
            // we're OK at the current gain, and can increase gain to the previously discovered
            // dynamic range limit
            gain_up = true;
            gain_up_reason = "low loud message rate and gain below dynamic range limit";
        }
    }

    if (Modes.adaptive_range_control && !adaptive_range_change_timer) {
        float available_range = -20 * log10(adaptive_range_smoothed / 65536.0);
        // allow the gain limit to increase if this gain setting is acceptable
        // (decreasing the limit is done separately in SCAN_UP / IDLE states when we decide to reduce gain)
        if (available_range >= Modes.adaptive_range_target && current_gain > adaptive_range_gain_limit)
            adaptive_range_gain_limit = current_gain;

        switch (adaptive_range_state) {
        case RANGE_SCAN_UP:
            if (available_range < Modes.adaptive_range_target) {
                // Current gain fails to meet our target. Switch to downward scanning.
                fprintf(stderr, "adaptive: available dynamic range (%.1fdB) < required dynamic range (%.1fdB), switching to downward scan\n", available_range, Modes.adaptive_range_target);
                gain_down = gain_not_up = true;
                gain_down_reason = "probing dynamic range gain lower bound";
                adaptive_range_state = RANGE_SCAN_DOWN;
                if (adaptive_range_gain_limit >= current_gain)
                    adaptive_range_gain_limit = current_gain - 1;
                break;
            }

            if (sdrGetGain() >= adaptive_gain_max) {
                // We have reached our upper gain limit
                fprintf(stderr, "adaptive: reached upper gain limit, halting dynamic range scan here\n");
                adaptive_range_state = RANGE_SCAN_IDLE;
                adaptive_range_rescan_timer = Modes.adaptive_range_rescan_delay;
                break;
            }

            // This gain step is OK and we have more to try, try the next gain step up.
            // (But if burst detection has inhibited increasing gain, don't do anything yet, just try again next block)
            if (!gain_not_up) {
                fprintf(stderr, "adaptive: available dynamic range (%.1fdB) >= required dynamic range (%.1fdB), continuing upward scan\n", available_range, Modes.adaptive_range_target);
                gain_up = true;
                gain_up_reason = "probing dynamic range gain upper bound";
            }
            break;

        case RANGE_SCAN_DOWN:
            if (available_range >= Modes.adaptive_range_target) {
                // Current gain meets our target; we are done with the scan.
                fprintf(stderr, "adaptive: available dynamic range (%.1fdB) >= required dynamic range (%.1fdB), stopping downwards scan here\n", available_range, Modes.adaptive_range_target);
                adaptive_range_state = RANGE_SCAN_IDLE;
                adaptive_range_rescan_timer = Modes.adaptive_range_rescan_delay;
                break;
            }

            if (sdrGetGain() <= adaptive_gain_min) {
                fprintf(stderr, "adaptive: reached lower gain limit, halting dynamic range scan here\n");
                adaptive_range_state = RANGE_SCAN_IDLE;
                adaptive_range_rescan_timer = Modes.adaptive_range_rescan_delay;
                break;
            }

            // This gain step is too loud and we have more to try, try the next gain step down
            fprintf(stderr, "adaptive: available dynamic range (%.1fdB) < required dynamic range (%.1fdB), continuing downwards scan\n", available_range, Modes.adaptive_range_target);
            gain_down = gain_not_up = true;
            gain_down_reason = "probing dynamic range gain lower bound";
            break;

        case RANGE_SCAN_IDLE:
            // Look for increased noise that could be compensated for by decreasing gain.
            // Do this even if we're waiting to rescan or if burst control is also active
            if (available_range + adaptive_gain_down_db / 2 < Modes.adaptive_range_target && sdrGetGain() > adaptive_gain_min) {
                fprintf(stderr, "adaptive: available dynamic range (%.1fdB) + half gain step down (%.1fdB) < required dynamic range (%.1fdB), starting downward scan\n",
                        available_range, Modes.adaptive_range_target, adaptive_gain_down_db);
                if (current_gain >= adaptive_range_gain_limit)
                    adaptive_range_gain_limit = current_gain - 1;
                adaptive_range_state = RANGE_SCAN_DOWN;
                gain_down = gain_not_up = true;
                gain_down_reason = "dynamic range fell below target value";
                break;
            }

            // Infrequently consider increasing gain to handle the case where we've selected a too-low gain where the noise floor is dominated by noise unrelated to the gain setting.
            // But don't do this while burst control is preventing gain increases.
            if (!adaptive_range_rescan_timer && !gain_not_up) {
                if (available_range >= Modes.adaptive_range_target && sdrGetGain() < adaptive_gain_max) {
                    fprintf(stderr, "adaptive: start periodic scan for acceptable dynamic range at increased gain\n");
                    gain_up = true;
                    gain_up_reason = "periodic re-probing of dynamic range gain upper bound";
                    adaptive_range_state = RANGE_SCAN_UP;
                    break;
                }

                // Nothing to do for a while.
                adaptive_range_rescan_timer = Modes.adaptive_range_rescan_delay;
            }

            break;

        default:
            fprintf(stderr, "adaptive: in a weird state (%d), trying to fix it\n", adaptive_range_state);
            adaptive_range_state = RANGE_SCAN_IDLE;
            adaptive_range_rescan_timer = Modes.adaptive_range_rescan_delay;
            break;
        }
    }

    // now actually perform any gain changes

    if (gain_down)
        adaptive_decrease_gain(gain_down_reason);
    else if (gain_up && !gain_not_up)
        adaptive_increase_gain(gain_up_reason);
}