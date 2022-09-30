// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_rtlsdr.c: rtlsdr dongle support
//
// Copyright (c) 2014-2017 Oliver Jowett <oliver@mutability.co.uk>
// Copyright (c) 2017 FlightAware LLC
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
#include "sdr_rtlsdr.h"

#include <rtl-sdr.h>

#if defined(__arm__) || defined(__aarch64__)
// Assume we need to use a bounce buffer to avoid performance problems on Pis running kernel 5.x and using zerocopy
#  define USE_BOUNCE_BUFFER
#endif

static struct {
    rtlsdr_dev_t *dev;
    bool digital_agc;
    int ppm_error;
    int direct_sampling;
    uint8_t *bounce_buffer;
    iq_convert_fn converter;
    struct converter_state *converter_state;
    int *gains;
    int gain_steps;
    int current_gain;
} RTLSDR;

//
// =============================== RTLSDR handling ==========================
//

void rtlsdrInitConfig()
{
    RTLSDR.dev = NULL;
    RTLSDR.digital_agc = false;
    RTLSDR.ppm_error = 0;
    RTLSDR.direct_sampling = 0;
    RTLSDR.bounce_buffer = NULL;
    RTLSDR.converter = NULL;
    RTLSDR.converter_state = NULL;
    RTLSDR.gains = NULL;
    RTLSDR.gain_steps = 0;
    RTLSDR.current_gain = 0;
}

static void show_rtlsdr_devices()
{
    int device_count = rtlsdr_get_device_count();
    fprintf(stderr, "rtlsdr: found %d device(s):\n", device_count);
    for (int i = 0; i < device_count; i++) {
        char vendor[256], product[256], serial[256];

        if (rtlsdr_get_device_usb_strings(i, vendor, product, serial) != 0) {
            fprintf(stderr, "  %d:  unable to read device details\n", i);
        } else {
            fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
        }
    }
}

static int find_device_index(char *s)
{
    int device_count = rtlsdr_get_device_count();
    if (!device_count) {
        return -1;
    }

    /* does string look like raw id number */
    if (!strcmp(s, "0")) {
        return 0;
    } else if (s[0] != '0') {
        char *s2;
        int device = (int)strtol(s, &s2, 10);
        if (s2[0] == '\0' && device >= 0 && device < device_count) {
            return device;
        }
    }

    /* does string exact match a serial */
    for (int i = 0; i < device_count; i++) {
        char serial[256];
        if (rtlsdr_get_device_usb_strings(i, NULL, NULL, serial) == 0 && !strcmp(s, serial)) {
            return i;
        }
    }

    /* does string prefix match a serial */
    for (int i = 0; i < device_count; i++) {
        char serial[256];
        if (rtlsdr_get_device_usb_strings(i, NULL, NULL, serial) == 0 && !strncmp(s, serial, strlen(s))) {
            return i;
        }
    }

    /* does string suffix match a serial */
    for (int i = 0; i < device_count; i++) {
        char serial[256];
        if (rtlsdr_get_device_usb_strings(i, NULL, NULL, serial) == 0 && strlen(s) < strlen(serial) && !strcmp(serial + strlen(serial) - strlen(s), s)) {
            return i;
        }
    }

    return -1;
}

void rtlsdrShowHelp()
{
    printf("      rtlsdr-specific options (use with --device-type rtlsdr)\n");
    printf("\n");
    printf("--device <index|serial>  select device by index or serial number\n");
    printf("--enable-agc             enable digital AGC (not tuner AGC!)\n");
    printf("--ppm <correction>       set oscillator frequency correction in PPM\n");
    printf("--direct <0|1|2>         set direct sampling mode\n");
    printf("\n");
}

bool rtlsdrHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j +1  < argc);

    if (!strcmp(argv[j], "--enable-agc")) {
        RTLSDR.digital_agc = true;
    } else if (!strcmp(argv[j], "--ppm") && more) {
        RTLSDR.ppm_error = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--direct") && more) {
        RTLSDR.direct_sampling = atoi(argv[++j]);
    } else {
        return false;
    }

    *jptr = j;
    return true;
}

// sort function to sort by increasing gain
static int sort_gains(const void *left, const void *right)
{
    const int *left_int = (const int *)left;
    const int *right_int = (const int *)right;
    return *left_int - *right_int;
}

bool rtlsdrOpen(void)
{
    if (!rtlsdr_get_device_count()) {
        fprintf(stderr, "rtlsdr: no supported devices found.\n");
        return false;
    }

    int dev_index = 0;
    if (Modes.dev_name) {
        if ((dev_index = find_device_index(Modes.dev_name)) < 0) {
            fprintf(stderr, "rtlsdr: no device matching '%s' found.\n", Modes.dev_name);
            show_rtlsdr_devices();
            return false;
        }
    }

    char manufacturer[256];
    char product[256];
    char serial[256];
    if (rtlsdr_get_device_usb_strings(dev_index, manufacturer, product, serial) < 0) {
        fprintf(stderr, "rtlsdr: error querying device #%d: %s\n", dev_index, strerror(errno));
        return false;
    }

    fprintf(stderr, "rtlsdr: using device #%d: %s (%s, %s, SN %s)\n",
            dev_index, rtlsdr_get_device_name(dev_index),
            manufacturer, product, serial);

    if (rtlsdr_open(&RTLSDR.dev, dev_index) < 0) {
        fprintf(stderr, "rtlsdr: error opening the RTLSDR device: %s\n",
            strerror(errno));
        return false;
    }

    // Set gain, frequency, sample rate, and reset the device
    if (RTLSDR.direct_sampling) {
        fprintf(stderr, "rtlsdr: direct sampling from input %d\n", RTLSDR.direct_sampling);
        rtlsdr_set_direct_sampling(RTLSDR.dev, RTLSDR.direct_sampling);
        RTLSDR.gain_steps = 0;
    } else {
        int *gains;
        int numgains;

        numgains = rtlsdr_get_tuner_gains(RTLSDR.dev, NULL);
        if (numgains <= 0) {
            fprintf(stderr, "rtlsdr: error getting tuner gains\n");
            return false;
            }

        gains = malloc((numgains + 1) * sizeof(int));
        if (rtlsdr_get_tuner_gains(RTLSDR.dev, gains) != numgains) {
            fprintf(stderr, "rtlsdr: error getting tuner gains\n");
            free(gains);
            return false;
        }

        qsort(gains, numgains, sizeof(gains[0]), sort_gains);

        // Fake an entry at slightly higher than max manual gain;
        // we will use this for the "tuner AGC enabled" settings
        // which due to librtlsdr quirks behaves like a "more than
        // max" gain. :/
        gains[numgains] = gains[numgains-1] + 90; // +9.0dB

        RTLSDR.gain_steps = numgains + 1;
        RTLSDR.gains = gains;

        int selected = -1;
        if (Modes.gain == MODES_LEGACY_AUTO_GAIN) {
            selected = numgains;
        } else if (Modes.gain == MODES_DEFAULT_GAIN) {
            selected = numgains - 1;
        } else {
            for (int i = 0; i <= numgains; ++i) {
                if (selected == -1 || fabs(gains[i]/10.0 - Modes.gain) < fabs(gains[selected]/10.0 - Modes.gain))
                    selected = i;
            }
        }

        rtlsdrSetGain(selected);
    }

    if (RTLSDR.digital_agc) {
        fprintf(stderr, "rtlsdr: enabling digital AGC\n");
        rtlsdr_set_agc_mode(RTLSDR.dev, 1);
    }

    rtlsdr_set_freq_correction(RTLSDR.dev, RTLSDR.ppm_error);
    rtlsdr_set_center_freq(RTLSDR.dev, Modes.freq);
    rtlsdr_set_sample_rate(RTLSDR.dev, (unsigned)Modes.sample_rate);

    rtlsdr_reset_buffer(RTLSDR.dev);

    RTLSDR.converter = init_converter(INPUT_UC8,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &RTLSDR.converter_state);
    if (!RTLSDR.converter) {
        fprintf(stderr, "rtlsdr: can't initialize sample converter\n");
        rtlsdrClose();
        return false;
    }

#ifdef USE_BOUNCE_BUFFER
    if (!(RTLSDR.bounce_buffer = malloc(MODES_RTL_BUF_SIZE))) {
        fprintf(stderr, "rtlsdr: can't allocate bounce buffer\n");
        rtlsdrClose();
        return false;
    }
#endif

    if (Modes.adaptive_range_target == 0)
        Modes.adaptive_range_target = 30.0;
    
    return true;
}

static void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx)
{
    static unsigned dropped = 0;
    static uint64_t sampleCounter = 0;

    MODES_NOTUSED(ctx);

    sdrMonitor();

    if (Modes.exit) {
        rtlsdr_cancel_async(RTLSDR.dev); // ask our caller to exit
        return;
    }

    unsigned samples_read = len/2; // Drops any trailing odd sample, not much else we can do there
    if (!samples_read)
        return; // that wasn't useful

    struct mag_buf *outbuf = fifo_acquire(0 /* don't wait */);
    if (!outbuf) {
        // FIFO is full. Drop this block.
        dropped += samples_read;
        sampleCounter += samples_read;
        return;
    }

    outbuf->flags = 0;

    if (dropped) {
        // We previously dropped some samples due to no buffers being available
        outbuf->flags |= MAGBUF_DISCONTINUOUS;
        outbuf->dropped = dropped;
    }

    dropped = 0;

    // Compute the sample timestamp and system timestamp for the start of the block
    outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
    sampleCounter += samples_read;

    // Get the approx system time for the start of this block
    uint64_t block_duration = 1e3 * samples_read / Modes.sample_rate;
    outbuf->sysTimestamp = mstime() - block_duration;

    // Convert the new data
    unsigned to_convert = samples_read;
    if (to_convert + outbuf->overlap > outbuf->totalLength) {
        // how did that happen?
        to_convert = outbuf->totalLength - outbuf->overlap;
        dropped = samples_read - to_convert;
    }

#ifdef USE_BOUNCE_BUFFER
    // Work around zero-copy slowness on Pis with 5.x kernels
    memcpy(RTLSDR.bounce_buffer, buf, to_convert * 2);
    buf = RTLSDR.bounce_buffer;
#endif

    RTLSDR.converter(buf, &outbuf->data[outbuf->overlap], to_convert, RTLSDR.converter_state, &outbuf->mean_level, &outbuf->mean_power);
    outbuf->validLength = outbuf->overlap + to_convert;

    // Push to the demodulation thread
    fifo_enqueue(outbuf);
}

void rtlsdrRun()
{
    if (!RTLSDR.dev) {
        return;
    }

    rtlsdr_read_async(RTLSDR.dev, rtlsdrCallback, NULL,
                      /* MODES_RTL_BUFFERS */ 4,
                      MODES_RTL_BUF_SIZE);
    if (!Modes.exit) {
        fprintf(stderr, "rtlsdr: rtlsdr_read_async returned unexpectedly, probably lost the USB device, bailing out\n");
    }
}

void rtlsdrStop()
{
    if (!RTLSDR.dev) {
        return;
    }

    rtlsdr_cancel_async(RTLSDR.dev);
}

void rtlsdrClose()
{
    if (RTLSDR.dev) {
        rtlsdr_close(RTLSDR.dev);
        RTLSDR.dev = NULL;
    }

    if (RTLSDR.converter) {
        cleanup_converter(RTLSDR.converter_state);
        RTLSDR.converter = NULL;
        RTLSDR.converter_state = NULL;
    }

    free(RTLSDR.bounce_buffer);
    RTLSDR.bounce_buffer = NULL;

    free(RTLSDR.gains);
    RTLSDR.gains = NULL;
}

int rtlsdrGetGain()
{
    return RTLSDR.current_gain;
}

int rtlsdrGetMaxGain()
{
    return RTLSDR.gain_steps - 1;
}

double rtlsdrGetGainDb(int step)
{
    if (!RTLSDR.gains)
        return 0.0;

    if (step < 0)
        step = 0;
    if (step >= RTLSDR.gain_steps)
        step = RTLSDR.gain_steps - 1;
    return RTLSDR.gains[step] / 10.0;
}

int rtlsdrSetGain(int step)
{
    if (!RTLSDR.gains)
        return -1;

    if (step < 0)
        step = 0;
    if (step >= RTLSDR.gain_steps)
        step = RTLSDR.gain_steps - 1;

    if (step == RTLSDR.gain_steps - 1) {
        if (rtlsdr_set_tuner_gain_mode(RTLSDR.dev, 0) < 0) {
            fprintf(stderr, "rtlsdr: failed to enable tuner AGC\n");
            return RTLSDR.current_gain;
        }            

        fprintf(stderr, "rtlsdr: tuner gain set to about %.1f dB (gain step %d) (tuner AGC enabled)\n", RTLSDR.gains[step] / 10.0, step);
    } else {
        if (rtlsdr_set_tuner_gain_mode(RTLSDR.dev, 1) < 0) {
            fprintf(stderr, "rtlsdr: failed to disable tuner AGC\n");
            return RTLSDR.current_gain;
        }

        if (rtlsdr_set_tuner_gain(RTLSDR.dev, RTLSDR.gains[step]) < 0) {
            fprintf(stderr, "rtlsdr: failed to set tuner gain to %.1fdB\n", RTLSDR.gains[step] / 10.0);
            return RTLSDR.current_gain;
        }

        fprintf(stderr, "rtlsdr: tuner gain set to %.1f dB (gain step %d)\n", RTLSDR.gains[step] / 10.0, step);
    }

    RTLSDR.current_gain = step;
    return step;
}

