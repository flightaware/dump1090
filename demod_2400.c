// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// demod_2400.c: 2.4MHz Mode S demodulator.
//
// Copyright (c) 2014,2015 Oliver Jowett <oliver@mutability.co.uk>
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

#include <assert.h>

#ifdef MODEAC_DEBUG
#include <gd.h>
#endif

// 2.4MHz sampling rate version
//
// When sampling at 2.4MHz we have exactly 6 samples per 5 symbols.
// Each symbol is 500ns wide, each sample is 416.7ns wide
//
// We maintain a phase offset that is expressed in units of 1/5 of a sample i.e. 1/6 of a symbol, 83.333ns
// Each symbol we process advances the phase offset by 6 i.e. 6/5 of a sample, 500ns
//
// The correlation functions below correlate a 1-0 pair of symbols (i.e. manchester encoded 1 bit)
// starting at the given sample, and assuming that the symbol starts at a fixed 0-5 phase offset within
// m[0]. They return a correlation value, generally interpreted as >0 = 1 bit, <0 = 0 bit

// TODO check if there are better (or more balanced) correlation functions to use here

// nb: the correlation functions sum to zero, so we do not need to adjust for the DC offset in the input signal
// (adding any constant value to all of m[0..3] does not change the result)

static inline int slice_phase0(uint16_t *m) {
    return 5 * m[0] - 3 * m[1] - 2 * m[2];
}
static inline int slice_phase1(uint16_t *m) {
    return 4 * m[0] - m[1] - 3 * m[2];
}
static inline int slice_phase2(uint16_t *m) {
    return 3 * m[0] + m[1] - 4 * m[2];
}
static inline int slice_phase3(uint16_t *m) {
    return 2 * m[0] + 3 * m[1] - 5 * m[2];
}
static inline int slice_phase4(uint16_t *m) {
    return m[0] + 5 * m[1] - 5 * m[2] - m[3];
}

static uint32_t valid_df_short_bitset;        // set of acceptable DF values for short messages
static uint32_t valid_df_long_bitset;         // set of acceptable DF values for long messages

static uint32_t generate_damage_set(uint8_t df, unsigned damage_bits)
{
    uint32_t result = (1 << df);
    if (!damage_bits)
        return result;

    for (unsigned bit = 0; bit < 5; ++bit) {
        unsigned damaged_df = df ^ (1 << bit);
        result |= generate_damage_set(damaged_df, damage_bits - 1);
    }

    return result;
}

static void init_bitsets()
{
    // DFs that we directly understand without correction
    valid_df_short_bitset = (1 << 0) | (1 << 4) | (1 << 5) | (1 << 11);
    valid_df_long_bitset = (1 << 16) | (1 << 17) | (1 << 18) | (1 << 20) | (1 << 21);

    if (Modes.enable_df24)
        valid_df_long_bitset |= (1 << 24) | (1 << 25) | (1 << 26) | (1 << 27) | (1 << 28) | (1 << 29) | (1 << 30) | (1 << 31);

    // if we can also repair DF damage, include those corrections
    if (Modes.fix_df && Modes.nfix_crc) {
        valid_df_short_bitset |= generate_damage_set(11, 1);
        valid_df_long_bitset |= generate_damage_set(17, Modes.nfix_crc);
        valid_df_long_bitset |= generate_damage_set(18, Modes.nfix_crc);
    }
}

//
// Given 'mlen' magnitude samples in 'm', sampled at 2.4MHz,
// try to demodulate some Mode S messages.
//
void demodulate2400(struct mag_buf *mag)
{
    static struct modesMessage zeroMessage;
    struct modesMessage mm;
    unsigned char msg1[MODES_LONG_MSG_BYTES], msg2[MODES_LONG_MSG_BYTES], *msg;
    uint32_t j;

    static unsigned last_message_end = 0;

    // initialize bitsets on first call
    if (!valid_df_short_bitset)
        init_bitsets();

    if (mag->flags & MAGBUF_DISCONTINUOUS) {
        // gap, start from the very beginning
        last_message_end = 0;
    }

    unsigned char *bestmsg;
    int bestscore, bestphase;

    // maximum lookahead we use
    assert(mag->overlap >= 19 + 1 + 269);

    uint16_t *m = mag->data;
    uint32_t mlen = mag->validLength - mag->overlap;

    uint64_t sum_scaled_signal_power = 0;

    msg = msg1;

    // sanity check
    if (last_message_end > mlen)
        last_message_end = mlen;

    for (j = last_message_end; j < mlen; j++) {
        uint16_t *preamble = &m[j];
        int high;
        uint32_t base_signal, base_noise;
        int try_phase;
        int msglen;

        // Look for a message starting at around sample 0 with phase offset 3..7

        // Ideal sample values for preambles with different phase
        // Xn is the first data symbol with phase offset N
        //
        // sample#: 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
        // phase 3: 2/4\0/5\1 0 0 0 0/5\1/3 3\0 0 0 0 0 0 X4
        // phase 4: 1/5\0/4\2 0 0 0 0/4\2 2/4\0 0 0 0 0 0 0 X0
        // phase 5: 0/5\1/3 3\0 0 0 0/3 3\1/5\0 0 0 0 0 0 0 X1
        // phase 6: 0/4\2 2/4\0 0 0 0 2/4\0/5\1 0 0 0 0 0 0 X2
        // phase 7: 0/3 3\1/5\0 0 0 0 1/5\0/4\2 0 0 0 0 0 0 X3
        //

        // quick check: we must have a rising edge 0->1 and a falling edge 12->13
        if (! (preamble[0] < preamble[1] && preamble[12] > preamble[13]) )
           continue;

        if (preamble[1] > preamble[2] &&                                       // 1
            preamble[2] < preamble[3] && preamble[3] > preamble[4] &&          // 3
            preamble[8] < preamble[9] && preamble[9] > preamble[10] &&         // 9
            preamble[10] < preamble[11]) {                                     // 11-12
            // peaks at 1,3,9,11-12: phase 3
            high = (preamble[1] + preamble[3] + preamble[9] + preamble[11] + preamble[12]) / 4;
            base_signal = preamble[1] + preamble[3] + preamble[9];
            base_noise = preamble[5] + preamble[6] + preamble[7];
        } else if (preamble[1] > preamble[2] &&                                // 1
                   preamble[2] < preamble[3] && preamble[3] > preamble[4] &&   // 3
                   preamble[8] < preamble[9] && preamble[9] > preamble[10] &&  // 9
                   preamble[11] < preamble[12]) {                              // 12
            // peaks at 1,3,9,12: phase 4
            high = (preamble[1] + preamble[3] + preamble[9] + preamble[12]) / 4;
            base_signal = preamble[1] + preamble[3] + preamble[9] + preamble[12];
            base_noise = preamble[5] + preamble[6] + preamble[7] + preamble[8];
        } else if (preamble[1] > preamble[2] &&                                // 1
                   preamble[2] < preamble[3] && preamble[4] > preamble[5] &&   // 3-4
                   preamble[8] < preamble[9] && preamble[10] > preamble[11] && // 9-10
                   preamble[11] < preamble[12]) {                              // 12
            // peaks at 1,3-4,9-10,12: phase 5
            high = (preamble[1] + preamble[3] + preamble[4] + preamble[9] + preamble[10] + preamble[12]) / 4;
            base_signal = preamble[1] + preamble[12];
            base_noise = preamble[6] + preamble[7];
        } else if (preamble[1] > preamble[2] &&                                 // 1
                   preamble[3] < preamble[4] && preamble[4] > preamble[5] &&    // 4
                   preamble[9] < preamble[10] && preamble[10] > preamble[11] && // 10
                   preamble[11] < preamble[12]) {                               // 12
            // peaks at 1,4,10,12: phase 6
            high = (preamble[1] + preamble[4] + preamble[10] + preamble[12]) / 4;
            base_signal = preamble[1] + preamble[4] + preamble[10] + preamble[12];
            base_noise = preamble[5] + preamble[6] + preamble[7] + preamble[8];
        } else if (preamble[2] > preamble[3] &&                                 // 1-2
                   preamble[3] < preamble[4] && preamble[4] > preamble[5] &&    // 4
                   preamble[9] < preamble[10] && preamble[10] > preamble[11] && // 10
                   preamble[11] < preamble[12]) {                               // 12
            // peaks at 1-2,4,10,12: phase 7
            high = (preamble[1] + preamble[2] + preamble[4] + preamble[10] + preamble[12]) / 4;
            base_signal = preamble[4] + preamble[10] + preamble[12];
            base_noise = preamble[6] + preamble[7] + preamble[8];
        } else {
            // no suitable peaks
            continue;
        }

        // Check for enough signal
        if (base_signal * 2 < 3 * base_noise) // about 3.5dB SNR
            continue;

        // Check that the "quiet" bits 6,7,15,16,17 are actually quiet
        if (preamble[5] >= high ||
            preamble[6] >= high ||
            preamble[7] >= high ||
            preamble[8] >= high ||
            preamble[14] >= high ||
            preamble[15] >= high ||
            preamble[16] >= high ||
            preamble[17] >= high ||
            preamble[18] >= high) {
            continue;
        }

        // try all phases
        Modes.stats_current.demod_preambles++;
        bestmsg = NULL; bestscore = SR_NOT_SET; bestphase = -1;
        for (try_phase = 4; try_phase <= 8; ++try_phase) {
            uint16_t *pPtr;
            int phase, score;

            // Decode all the next 112 bits, regardless of the actual message
            // size. We'll check the actual message type later

            pPtr = &m[j+19] + (try_phase/5);
            phase = try_phase % 5;

            unsigned bytelen = 1;
            for (unsigned i = 0; i < bytelen; ++i) {
                uint8_t theByte = 0;

                switch (phase) {
                case 0:
                    theByte =
                        (slice_phase0(pPtr) > 0 ? 0x80 : 0) |
                        (slice_phase2(pPtr+2) > 0 ? 0x40 : 0) |
                        (slice_phase4(pPtr+4) > 0 ? 0x20 : 0) |
                        (slice_phase1(pPtr+7) > 0 ? 0x10 : 0) |
                        (slice_phase3(pPtr+9) > 0 ? 0x08 : 0) |
                        (slice_phase0(pPtr+12) > 0 ? 0x04 : 0) |
                        (slice_phase2(pPtr+14) > 0 ? 0x02 : 0) |
                        (slice_phase4(pPtr+16) > 0 ? 0x01 : 0);


                    phase = 1;
                    pPtr += 19;
                    break;

                case 1:
                    theByte =
                        (slice_phase1(pPtr) > 0 ? 0x80 : 0) |
                        (slice_phase3(pPtr+2) > 0 ? 0x40 : 0) |
                        (slice_phase0(pPtr+5) > 0 ? 0x20 : 0) |
                        (slice_phase2(pPtr+7) > 0 ? 0x10 : 0) |
                        (slice_phase4(pPtr+9) > 0 ? 0x08 : 0) |
                        (slice_phase1(pPtr+12) > 0 ? 0x04 : 0) |
                        (slice_phase3(pPtr+14) > 0 ? 0x02 : 0) |
                        (slice_phase0(pPtr+17) > 0 ? 0x01 : 0);

                    phase = 2;
                    pPtr += 19;
                    break;

                case 2:
                    theByte =
                        (slice_phase2(pPtr) > 0 ? 0x80 : 0) |
                        (slice_phase4(pPtr+2) > 0 ? 0x40 : 0) |
                        (slice_phase1(pPtr+5) > 0 ? 0x20 : 0) |
                        (slice_phase3(pPtr+7) > 0 ? 0x10 : 0) |
                        (slice_phase0(pPtr+10) > 0 ? 0x08 : 0) |
                        (slice_phase2(pPtr+12) > 0 ? 0x04 : 0) |
                        (slice_phase4(pPtr+14) > 0 ? 0x02 : 0) |
                        (slice_phase1(pPtr+17) > 0 ? 0x01 : 0);

                    phase = 3;
                    pPtr += 19;
                    break;

                case 3:
                    theByte =
                        (slice_phase3(pPtr) > 0 ? 0x80 : 0) |
                        (slice_phase0(pPtr+3) > 0 ? 0x40 : 0) |
                        (slice_phase2(pPtr+5) > 0 ? 0x20 : 0) |
                        (slice_phase4(pPtr+7) > 0 ? 0x10 : 0) |
                        (slice_phase1(pPtr+10) > 0 ? 0x08 : 0) |
                        (slice_phase3(pPtr+12) > 0 ? 0x04 : 0) |
                        (slice_phase0(pPtr+15) > 0 ? 0x02 : 0) |
                        (slice_phase2(pPtr+17) > 0 ? 0x01 : 0);

                    phase = 4;
                    pPtr += 19;
                    break;

                case 4:
                    theByte =
                        (slice_phase4(pPtr) > 0 ? 0x80 : 0) |
                        (slice_phase1(pPtr+3) > 0 ? 0x40 : 0) |
                        (slice_phase3(pPtr+5) > 0 ? 0x20 : 0) |
                        (slice_phase0(pPtr+8) > 0 ? 0x10 : 0) |
                        (slice_phase2(pPtr+10) > 0 ? 0x08 : 0) |
                        (slice_phase4(pPtr+12) > 0 ? 0x04 : 0) |
                        (slice_phase1(pPtr+15) > 0 ? 0x02 : 0) |
                        (slice_phase3(pPtr+17) > 0 ? 0x01 : 0);

                    phase = 0;
                    pPtr += 20;
                    break;
                }

                msg[i] = theByte;

                if (i == 0) {
                    // inspect DF field early, only continue processing
                    // messages where the DF appears valid
                    unsigned df = theByte >> 3;
                    if (valid_df_long_bitset & (1 << df))
                        bytelen = MODES_LONG_MSG_BYTES;
                    else if (valid_df_short_bitset & (1 << df))
                        bytelen = MODES_SHORT_MSG_BYTES;
                }
            }

            if (bytelen == 1) {
                // rejected early by the DF filter
                Modes.stats_current.demod_rejected_bad++;
                continue;
            }

            // Score the mode S message and see if it's any good.
            score = scoreModesMessage(msg);
            if (score > bestscore) {
                // new high score!
                bestmsg = msg;
                bestscore = score;
                bestphase = try_phase;

                // swap to using the other buffer so we don't clobber our demodulated data
                // (if we find a better result then we'll swap back, but that's OK because
                // we no longer need this copy if we found a better one)
                msg = (msg == msg1) ? msg2 : msg1;
            }
        }

        // Do we have a candidate?
        if (bestscore < SR_ACCEPT_THRESHOLD) {
            if (bestscore >= SR_UNKNOWN_THRESHOLD)
                Modes.stats_current.demod_rejected_unknown_icao++;
            else
                Modes.stats_current.demod_rejected_bad++;
            continue; // nope.
        }

        msglen = modesMessageLenByType(bestmsg[0] >> 3);

        // Set initial mm structure details
        mm = zeroMessage;

        // For consistency with how the Beast / Radarcape does it,
        // we report the timestamp at the end of bit 56 (even if
        // the frame is a 112-bit frame)
        mm.timestampMsg = mag->sampleTimestamp + j*5 + (8 + 56) * 12 + bestphase;

        // compute message receive time as block-start-time + difference in the 12MHz clock
        mm.sysTimestampMsg = mag->sysTimestamp + receiveclock_ms_elapsed(mag->sampleTimestamp, mm.timestampMsg);

        mm.score = bestscore;

        // Decode the received message
        if (decodeModesMessage(&mm, bestmsg) < 0) {
            Modes.stats_current.demod_rejected_bad++;
            continue;
        } else {
            Modes.stats_current.demod_accepted[mm.correctedbits]++;
        }

        // measure signal power
        {
            double signal_power;
            uint64_t scaled_signal_power = 0;
            int signal_len = msglen*12/5;
            int k;

            for (k = 0; k < signal_len; ++k) {
                uint32_t mag = m[j+19+k];
                scaled_signal_power += mag * mag;
            }

            signal_power = scaled_signal_power / 65535.0 / 65535.0;
            mm.signalLevel = signal_power / signal_len;
            Modes.stats_current.signal_power_sum += signal_power;
            Modes.stats_current.signal_power_count += signal_len;
            sum_scaled_signal_power += scaled_signal_power;

            if (mm.signalLevel > Modes.stats_current.peak_signal_power)
                Modes.stats_current.peak_signal_power = mm.signalLevel;
            if (mm.signalLevel > 0.50119)
                Modes.stats_current.strong_signal_count++; // signal power above -3dBFS
        }

        // Feed "empty" sample to adaptive gain logic
        if (j > last_message_end)
            adaptive_update(&m[last_message_end], j - last_message_end, NULL);

        // Feed message samples to adaptive gain logic, update end pointer
        last_message_end = j + (msglen + 8) * 12/5;
        adaptive_update(&m[j], last_message_end - j, &mm);

        // Skip over the message:
        // (we actually skip to 8 bits before the end of the message,
        //  because we can often decode two messages that *almost* collide,
        //  where the preamble of the second message clobbered the last
        //  few bits of the first message, but the message bits didn't
        //  overlap)
        j = last_message_end - 8*12/5;

        // Pass data to the next layer
        useModesMessage(&mm);
    }

    /* update noise power */
    {
        double sum_signal_power = sum_scaled_signal_power / 65535.0 / 65535.0;
        Modes.stats_current.noise_power_sum += (mag->mean_power * mlen - sum_signal_power);
        Modes.stats_current.noise_power_count += mlen;
    }

    // feed trailing empty samples to adaptive gain logic
    if (last_message_end < mlen) {
        // trailing data from end of last message to start of overlap;
        // on the next pass, start from the start of the overlap
        adaptive_update(&m[last_message_end], mlen - last_message_end, NULL);
        last_message_end = 0;
    } else {
        // last decoded message runs into the overlap region;
        // on the next pass, start at the right place in the overlap;
        // no trailing data to pass this time
        last_message_end -= mlen;
    }
}

#ifdef MODEAC_DEBUG

static int yscale(unsigned signal)
{
    return (int) (299 - 299.0 * signal / 65536.0);
}

static void draw_modeac(uint16_t *m, unsigned modeac, unsigned f1_clock, unsigned noise_threshold, unsigned signal_threshold, unsigned bits, unsigned noisy_bits, unsigned uncertain_bits)
{
    // 25 bits at 87*60MHz
    // use 1 pixel = 30MHz = 1087 pixels

    gdImagePtr im = gdImageCreate(1088, 300);
    int red = gdImageColorAllocate(im, 255, 0, 0);
    int brightgreen = gdImageColorAllocate(im, 0, 255, 0);
    int darkgreen = gdImageColorAllocate(im, 0, 180, 0);
    int blue = gdImageColorAllocate(im, 0, 0, 255);
    int grey = gdImageColorAllocate(im, 200, 200, 200);
    int white = gdImageColorAllocate(im, 255, 255, 255);
    int black = gdImageColorAllocate(im, 0, 0, 0);

    gdImageFilledRectangle(im, 0, 0, 1087, 299, white);

    // draw samples
    for (unsigned pixel = 0; pixel < 1088; ++pixel) {
        int clock_offset = (pixel - 150) * 2;
        int bit = clock_offset / 87;
        int sample = (f1_clock + clock_offset) / 25;
        int bitoffset = clock_offset % 87;
        int color;

        if (sample < 0)
            continue;

        if (clock_offset < 0 || bit >= 20) {
            color = grey;
        } else if (bitoffset < 27 && (uncertain_bits & (1 << (19-bit)))) {
            color = red;
        } else if (bitoffset >= 27 && (noisy_bits & (1 << (19-bit)))) {
            color = red;
        } else if (bitoffset >= 27) {
            color = grey;
        } else if (bits & (1 << (19-bit))) {
            color = brightgreen;
        } else {
            color = darkgreen;
        }

        gdImageLine(im, pixel, 299, pixel, yscale(m[sample]), color);
    }

    // draw bit boundaries
    for (unsigned bit = 0; bit < 20; ++bit) {
        unsigned clock = 87 * bit;
        unsigned pixel0 = clock / 2 + 150;
        unsigned pixel1 = (clock + 27) / 2 + 150;

        gdImageLine(im, pixel0, 0, pixel0, 299, (bit == 0 || bit == 14) ? black : grey);
        gdImageLine(im, pixel1, 0, pixel1, 299, (bit == 0 || bit == 14) ? black : grey);
    }

    // draw thresholds
    gdImageLine(im, 0, yscale(noise_threshold), 1087, yscale(noise_threshold), blue);
    gdImageLine(im, 0, yscale(signal_threshold), 1087, yscale(signal_threshold), blue);

    // save it

    static int file_counter;
    char filename[PATH_MAX];
    sprintf(filename, "modeac_%04X_%04d.png", modeac, ++file_counter);
    fprintf(stderr, "writing %s\n", filename);

    FILE *pngout = fopen(filename, "wb");
    gdImagePng(im, pngout);
    fclose(pngout);
    gdImageDestroy(im);
}

#endif

//////////
////////// MODE A/C
//////////

// Mode A/C bits are 1.45us wide, consisting of 0.45us on and 1.0us off
// We track this in terms of a (virtual) 60MHz clock, which is the lowest common multiple
// of the bit frequency and the 2.4MHz sampling frequency
//
//            0.45us = 27 cycles }
//            1.00us = 60 cycles } one bit period = 1.45us = 87 cycles
//
// one 2.4MHz sample = 25 cycles

void demodulate2400AC(struct mag_buf *mag)
{
    struct modesMessage mm;
    uint16_t *m = mag->data;
    uint32_t mlen = mag->validLength - mag->overlap;
    unsigned f1_sample;

    memset(&mm, 0, sizeof(mm));

    double noise_stddev = sqrt(mag->mean_power - mag->mean_level * mag->mean_level); // Var(X) = E[(X-E[X])^2] = E[X^2] - (E[X])^2
    unsigned noise_level = (unsigned) ((mag->mean_power + noise_stddev) * 65535 + 0.5);

    for (f1_sample = 1; f1_sample < mlen; ++f1_sample) {
        // Mode A/C messages should match this bit sequence:

        // bit #     value
        //   -1       0    quiet zone
        //    0       1    framing pulse (F1)
        //    1      C1
        //    2      A1
        //    3      C2
        //    4      A2
        //    5      C4
        //    6      A4
        //    7       0    quiet zone (X1)
        //    8      B1
        //    9      D1
        //   10      B2
        //   11      D2
        //   12      B4
        //   13      D4
        //   14       1    framing pulse (F2)
        //   15       0    quiet zone (X2)
        //   16       0    quiet zone (X3)
        //   17     SPI
        //   18       0    quiet zone (X4)
        //   19       0    quiet zone (X5)

        // Look for a F1 and F2 pair,
        // with F1 starting at offset f1_sample.

        // the first framing pulse covers 3.5 samples:
        //
        // |----|        |----|
        // | F1 |________| C1 |_
        //
        // | 0 | 1 | 2 | 3 | 4 |
        //
        // and there is some unknown phase offset of the
        // leading edge e.g.:
        //
        //   |----|        |----|
        // __| F1 |________| C1 |_
        //
        // | 0 | 1 | 2 | 3 | 4 |
        //
        // in theory the "on" period can straddle 3 samples
        // but it's not a big deal as at most 4% of the power
        // is in the third sample.

        if (!(m[f1_sample-1] < m[f1_sample+0]))
            continue;      // not a rising edge

        if (m[f1_sample+2] > m[f1_sample+0] || m[f1_sample+2] > m[f1_sample+1])
            continue;      // quiet part of bit wasn't sufficiently quiet

        unsigned f1_level = (m[f1_sample+0] + m[f1_sample+1]) / 2;

        if (noise_level * 2 > f1_level) {
            // require 6dB above noise
            continue;
        }

        // estimate initial clock phase based on the amount of power
        // that ended up in the second sample

        float f1a_power = (float)m[f1_sample] * m[f1_sample];
        float f1b_power = (float)m[f1_sample+1] * m[f1_sample+1];
        float fraction = f1b_power / (f1a_power + f1b_power);
        unsigned f1_clock = (unsigned) (25 * (f1_sample + fraction * fraction) + 0.5);

        // same again for F2
        // F2 is 20.3us / 14 bit periods after F1
        unsigned f2_clock = f1_clock + (87 * 14);
        unsigned f2_sample = f2_clock / 25;
        assert(f2_sample < mlen + mag->overlap);

        if (!(m[f2_sample-1] < m[f2_sample+0]))
            continue;

        if (m[f2_sample+2] > m[f2_sample+0] || m[f2_sample+2] > m[f2_sample+1])
            continue;      // quiet part of bit wasn't sufficiently quiet

        unsigned f2_level = (m[f2_sample+0] + m[f2_sample+1]) / 2;

        if (noise_level * 2 > f2_level) {
            // require 6dB above noise
            continue;
        }

        unsigned f1f2_level = (f1_level > f2_level ? f1_level : f2_level);

        float midpoint = sqrtf(noise_level * f1f2_level); // geometric mean of the two levels
        unsigned signal_threshold = (unsigned) (midpoint * M_SQRT2 + 0.5); // +3dB
        unsigned noise_threshold = (unsigned) (midpoint / M_SQRT2 + 0.5);  // -3dB

        // Looks like a real signal. Demodulate all the bits.
        unsigned uncertain_bits = 0;
        unsigned noisy_bits = 0;
        unsigned bits = 0;
        unsigned bit;
        unsigned clock;
        for (bit = 0, clock = f1_clock; bit < 20; ++bit, clock += 87) {
            unsigned sample = clock / 25;

            bits <<= 1;
            noisy_bits <<= 1;
            uncertain_bits <<= 1;

            // check for excessive noise in the quiet period
            if (m[sample+2] >= signal_threshold) {
                noisy_bits |= 1;
            }

            // decide if this bit is on or off
            if (m[sample+0] >= signal_threshold || m[sample+1] >= signal_threshold) {
                bits |= 1;
            } else if (m[sample+0] > noise_threshold && m[sample+1] > noise_threshold) {
                /* not certain about this bit */
                uncertain_bits |= 1;
            } else {
                /* this bit is off */
            }
        }

        // framing bits must be on
        if ((bits & 0x80020) != 0x80020) {
            continue;
        }

        // quiet bits must be off
        if ((bits & 0x0101B) != 0) {
            continue;
        }

        if (noisy_bits || uncertain_bits) {
            continue;
        }

        // Convert to the form that we use elsewhere:
        //  00 A4 A2 A1  00 B4 B2 B1  SPI C4 C2 C1  00 D4 D2 D1
        unsigned modeac =
            ((bits & 0x40000) ? 0x0010 : 0) |  // C1
            ((bits & 0x20000) ? 0x1000 : 0) |  // A1
            ((bits & 0x10000) ? 0x0020 : 0) |  // C2
            ((bits & 0x08000) ? 0x2000 : 0) |  // A2
            ((bits & 0x04000) ? 0x0040 : 0) |  // C4
            ((bits & 0x02000) ? 0x4000 : 0) |  // A4
            ((bits & 0x00800) ? 0x0100 : 0) |  // B1
            ((bits & 0x00400) ? 0x0001 : 0) |  // D1
            ((bits & 0x00200) ? 0x0200 : 0) |  // B2
            ((bits & 0x00100) ? 0x0002 : 0) |  // D2
            ((bits & 0x00080) ? 0x0400 : 0) |  // B4
            ((bits & 0x00040) ? 0x0004 : 0) |  // D4
            ((bits & 0x00004) ? 0x0080 : 0);   // SPI

#ifdef MODEAC_DEBUG
        draw_modeac(m, modeac, f1_clock, noise_threshold, signal_threshold, bits, noisy_bits, uncertain_bits);
#endif

        // This message looks good, submit it

        // For consistency with how the Beast / Radarcape does it,
        // we report the timestamp at the second framing pulse (F2)
        mm.timestampMsg = mag->sampleTimestamp + f2_clock / 5;  // 60MHz -> 12MHz

        // compute message receive time as block-start-time + difference in the 12MHz clock
        mm.sysTimestampMsg = mag->sysTimestamp + receiveclock_ms_elapsed(mag->sampleTimestamp, mm.timestampMsg);

        decodeModeAMessage(&mm, modeac);

        // Pass data to the next layer
        useModesMessage(&mm);

        f1_sample += (20*87 / 25);
        Modes.stats_current.demod_modeac++;
    }
}
