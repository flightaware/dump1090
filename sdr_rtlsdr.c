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

static struct {
    rtlsdr_dev_t *dev;
    bool digital_agc;
    int ppm_error;
    int direct_sampling;

    iq_convert_fn converter;
    struct converter_state *converter_state;
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
    RTLSDR.converter = NULL;
    RTLSDR.converter_state = NULL;
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

bool rtlsdrOpen(void) {
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
    } else {
        if (Modes.gain == MODES_AUTO_GAIN) {
            fprintf(stderr, "rtlsdr: enabling tuner AGC\n");
            rtlsdr_set_tuner_gain_mode(RTLSDR.dev, 0);
        } else {
            int *gains;
            int numgains;

            numgains = rtlsdr_get_tuner_gains(RTLSDR.dev, NULL);
            if (numgains <= 0) {
                fprintf(stderr, "rtlsdr: error getting tuner gains\n");
                return false;
            }

            gains = malloc(numgains * sizeof(int));
            if (rtlsdr_get_tuner_gains(RTLSDR.dev, gains) != numgains) {
                fprintf(stderr, "rtlsdr: error getting tuner gains\n");
                free(gains);
                return false;
            }

            int target = (Modes.gain == MODES_MAX_GAIN ? 9999 : Modes.gain);
            int closest = -1;

            for (int i = 0; i < numgains; ++i) {
                if (closest == -1 || abs(gains[i] - target) < abs(gains[closest] - target))
                    closest = i;
            }

            rtlsdr_set_tuner_gain(RTLSDR.dev, gains[closest]);
            free(gains);

            fprintf(stderr, "rtlsdr: tuner gain set to %.1f dB\n",
                    rtlsdr_get_tuner_gain(RTLSDR.dev)/10.0);
        }
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

    return true;
}

static struct timespec rtlsdr_thread_cpu;

void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx) {
    struct mag_buf *outbuf;
    struct mag_buf *lastbuf;
    uint32_t slen;
    unsigned next_free_buffer;
    unsigned free_bufs;
    unsigned block_duration;

    static int dropping = 0;
    static uint64_t sampleCounter = 0;

    MODES_NOTUSED(ctx);

    // Lock the data buffer variables before accessing them
    pthread_mutex_lock(&Modes.data_mutex);
    if (Modes.exit) {
        rtlsdr_cancel_async(RTLSDR.dev); // ask our caller to exit
    }

    next_free_buffer = (Modes.first_free_buffer + 1) % MODES_MAG_BUFFERS;
    outbuf = &Modes.mag_buffers[Modes.first_free_buffer];
    lastbuf = &Modes.mag_buffers[(Modes.first_free_buffer + MODES_MAG_BUFFERS - 1) % MODES_MAG_BUFFERS];
    free_bufs = (Modes.first_filled_buffer - next_free_buffer + MODES_MAG_BUFFERS) % MODES_MAG_BUFFERS;

    // Paranoia! Unlikely, but let's go for belt and suspenders here

    if (len != MODES_RTL_BUF_SIZE) {
        fprintf(stderr, "weirdness: rtlsdr gave us a block with an unusual size (got %u bytes, expected %u bytes)\n",
                (unsigned)len, (unsigned)MODES_RTL_BUF_SIZE);

        if (len > MODES_RTL_BUF_SIZE) {
            // wat?! Discard the start.
            unsigned discard = (len - MODES_RTL_BUF_SIZE + 1) / 2;
            outbuf->dropped += discard;
            buf += discard*2;
            len -= discard*2;
        }
    }

    slen = len/2; // Drops any trailing odd sample, that's OK

    if (free_bufs == 0 || (dropping && free_bufs < MODES_MAG_BUFFERS/2)) {
        // FIFO is full. Drop this block.
        dropping = 1;
        outbuf->dropped += slen;
        sampleCounter += slen;
        pthread_mutex_unlock(&Modes.data_mutex);
        return;
    }

    dropping = 0;
    pthread_mutex_unlock(&Modes.data_mutex);

    // Compute the sample timestamp and system timestamp for the start of the block
    outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
    sampleCounter += slen;

    // Get the approx system time for the start of this block
    block_duration = 1e3 * slen / Modes.sample_rate;
    outbuf->sysTimestamp = mstime() - block_duration;

    // Copy trailing data from last block (or reset if not valid)
    if (outbuf->dropped == 0) {
        memcpy(outbuf->data, lastbuf->data + lastbuf->length, Modes.trailing_samples * sizeof(uint16_t));
    } else {
        memset(outbuf->data, 0, Modes.trailing_samples * sizeof(uint16_t));
    }

    // Convert the new data
    outbuf->length = slen;
    RTLSDR.converter(buf, &outbuf->data[Modes.trailing_samples], slen, RTLSDR.converter_state, &outbuf->mean_level, &outbuf->mean_power);

    // Push the new data to the demodulation thread
    pthread_mutex_lock(&Modes.data_mutex);

    Modes.mag_buffers[next_free_buffer].dropped = 0;
    Modes.mag_buffers[next_free_buffer].length = 0;  // just in case
    Modes.first_free_buffer = next_free_buffer;

    // accumulate CPU while holding the mutex, and restart measurement
    end_cpu_timing(&rtlsdr_thread_cpu, &Modes.reader_cpu_accumulator);
    start_cpu_timing(&rtlsdr_thread_cpu);

    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
}

void rtlsdrRun()
{
    if (!RTLSDR.dev) {
        return;
    }

    start_cpu_timing(&rtlsdr_thread_cpu);

    rtlsdr_read_async(RTLSDR.dev, rtlsdrCallback, NULL,
                      /* MODES_RTL_BUFFERS */ 4,
                      MODES_RTL_BUF_SIZE);
    if (!Modes.exit) {
        fprintf(stderr, "rtlsdr: rtlsdr_read_async returned unexpectedly, probably lost the USB device, bailing out\n");
    }
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
}
