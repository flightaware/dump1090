// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_limesdr.c: LimeSDR dongle support
//
// Copyright (c) 2020 Gluttton <gluttton@ukr.net>
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
#include "sdr_limesdr.h"

#include <lime/LimeSuite.h>

static struct {
    lms_device_t *dev;
    lms_stream_t stream;
    bool is_stream_opened;
    bool is_stop;
    char verbosity;
    size_t oversample;
    float gain;
    float lpfbw;
    float bw;
    lms_info_str_t serial;
    int bytes_in_sample;
    iq_convert_fn converter;
    struct converter_state *converter_state;
} LimeSDR;

static void limesdrLogHandler(int lvl, const char *msg)
{
    if (lvl > LimeSDR.verbosity) {
        return;
    }

    FILE *out = NULL;
    switch (lvl) {
        default:
        case LMS_LOG_DEBUG:
        case LMS_LOG_INFO:
            out = stdout;
            break;
        case LMS_LOG_WARNING:
        case LMS_LOG_ERROR:
#ifdef LMS_LOG_CRITICAL
        case LMS_LOG_CRITICAL:
#endif
            out = stderr;
            break;
    }

    fprintf(out, "limesdr: %s\n", msg);
}

void limesdrInitConfig()
{
    LimeSDR.dev = NULL;
    LimeSDR.stream.channel = 0;
    LimeSDR.stream.fifoSize = 1024 * 1024;
    LimeSDR.stream.throughputVsLatency = 1.0; // best throughput
    LimeSDR.stream.isTx = false;
    LimeSDR.stream.dataFmt = LMS_FMT_I16; // should be matched with conveter
    LimeSDR.is_stream_opened = false;
    LimeSDR.is_stop = false;
    LimeSDR.verbosity = LMS_LOG_INFO;
    LimeSDR.oversample = 0; // default oversample
    LimeSDR.gain = -1;
    LimeSDR.lpfbw = 2400000.0;
    LimeSDR.bw = 2.5e6; // the minimal supported value
    LimeSDR.serial[0] = '\0';
    LimeSDR.bytes_in_sample = 2 * sizeof(int16_t); // hardcoded for LMS_FMT_I16

    LMS_RegisterLogHandler(limesdrLogHandler);
}

void limesdrShowHelp()
{
    printf("      limesdr-specific options (use with --device-type limesdr)\n");
    printf("\n");
    printf("--limesdr-verbosity      set verbosity level for LimeSDR messages\n");
    printf("--limesdr-serial         serial number of desired device\n");
    printf("--limesdr-channel        set number of an RX channel\n");
    printf("--limesdr-oversample     set RF oversampling ratio\n");
    printf("--limesdr-gain           set normalized gain (range: 0.0 to 1.0)\n");
    printf("--limesdr-lpfbw          set LPF bandwidth\n");
    printf("--limesdr-bw             set bandwidth\n");
    printf("\n");
}

bool limesdrHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j + 1 < argc);

    if (!strcmp(argv[j], "--limesdr-verbosity") && more) {
        LimeSDR.verbosity = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-serial") && more) {
        strcpy(LimeSDR.serial, argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-channel") && more) {
        LimeSDR.stream.channel = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-oversample") && more) {
        LimeSDR.oversample = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-gain") && more) {
        LimeSDR.gain = atof(argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-lpfbw") && more) {
        LimeSDR.lpfbw = atof(argv[++j]);
    } else if (!strcmp(argv[j], "--limesdr-bw") && more) {
        LimeSDR.bw = atof(argv[++j]);
    } else {
        return false;
    }
    *jptr = j;

    return true;
}

static size_t selectAntenna()
{
    ssize_t result = -1;
    lms_name_t *names = NULL;

    int numAntennas = LMS_GetAntennaList(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, NULL);
    if (numAntennas <= 0) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to get antenna list");
        goto done;
    }

    names = calloc(numAntennas, sizeof(lms_name_t));
    if (!names) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to get antenna list");
        goto done;
    }

    numAntennas = LMS_GetAntennaList(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, names);
    if (numAntennas < 0) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to get antenna list");
        goto done;
    }

    for (int i = 0; i < numAntennas; ++i) {
        lms_range_t range;
        if (LMS_GetAntennaBW(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, i, &range) < 0) {
            fprintf(stderr, "limesdr: unable to get antenna bandwidth for antenna %d (%s)\n", i, names[i]);
            continue;
        }

        if (range.min <= Modes.freq && range.max >= Modes.freq) {
            fprintf(stderr, "limesdr: selected rx antenna %d (%s) with bandwidth %.1f .. %.1fMHz\n", i, names[i], range.min / 1e6, range.max / 1e6);
            result = i;
            goto done;
        }
    }

 done:
    if (result < 0) {
        limesdrLogHandler(LMS_LOG_WARNING, "no suitable rx antenna range found, using LNAW");
        result = LMS_PATH_LNAW;
    }

    if (names)
        free(names);
    return result;
}

bool limesdrOpen(void)
{
    const size_t devCountMax = 8;
    lms_info_str_t list[devCountMax];
    const int devCount = LMS_GetDeviceList(list);
    if (devCount < 0) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to get a number of connected devices");
        goto error;
    }

    if (devCount < 1) {
        limesdrLogHandler(LMS_LOG_ERROR, "no connected devices");
        goto error;
    }

    limesdrLogHandler(LMS_LOG_INFO, "connected devices:");
    for (int i = 0; i < devCount; ++i) {
        limesdrLogHandler(LMS_LOG_INFO, list[i]);
    }

    bool isDevMatched = !strlen(LimeSDR.serial);
    int devIndex = 0;
    const char *serialTag = "serial=";
    for (int i = 0; i < devCount && !isDevMatched; ++i) {
        const char *serial = strstr(list[i], serialTag);
        if (serial) {
            if (strstr(serial + strlen(serialTag), LimeSDR.serial)) {
                isDevMatched = true;
                devIndex = i;
            }
        }
    }
    if (isDevMatched) {
        limesdrLogHandler(LMS_LOG_INFO, "selected device:");
        limesdrLogHandler(LMS_LOG_INFO, list[devIndex]);
    } else {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to find desired device");
        goto error;
    }

    if (LMS_Open(&LimeSDR.dev, list[devIndex], NULL) ) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to open device");
        goto error;
    }

    if (LMS_Init(LimeSDR.dev)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to initialize device");
        goto error;
    }

    if (LMS_EnableChannel(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, true)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to enable RX channel");
        goto error;
    }

    if (LMS_SetAntenna(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, selectAntenna())) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to set RF port");
        goto error;
    }

    if (LMS_SetLOFrequency(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, Modes.freq)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to set frequency");
        goto error;
    }

    if (LMS_SetSampleRate(LimeSDR.dev, Modes.sample_rate, LimeSDR.oversample)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to set sampling rate");
        goto error;
    }

    if (LimeSDR.gain >= 0) {
        if (LMS_SetNormalizedGain(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, LimeSDR.gain)) {
            limesdrLogHandler(LMS_LOG_ERROR, "unable to set gain");
            goto error;
        }
    } else {
        if (Modes.gain == MODES_DEFAULT_GAIN) {
            if (LMS_SetNormalizedGain(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, 1.0)) {
                limesdrLogHandler(LMS_LOG_ERROR, "unable to set gain");
                goto error;
            }
        } else {
            if (LMS_SetGaindB(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, Modes.gain)) {
                limesdrLogHandler(LMS_LOG_ERROR, "unable to set gain");
                goto error;
            }
        }
    }

    if (LMS_SetLPFBW(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, LimeSDR.lpfbw)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to set LP filter");
        goto error;
    }

    LimeSDR.is_stream_opened = true;
    if (LMS_SetupStream(LimeSDR.dev, &LimeSDR.stream)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to setup stream");
        LimeSDR.is_stream_opened = false;
        goto error;
    }

    if (LMS_Calibrate(LimeSDR.dev, LMS_CH_RX, LimeSDR.stream.channel, LimeSDR.bw, 0)) {
        limesdrLogHandler(LMS_LOG_ERROR, "unable to calibrate device");
        goto error;
    }

    LimeSDR.converter = init_converter(INPUT_SC16,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &LimeSDR.converter_state);
    if (!LimeSDR.converter) {
        limesdrLogHandler(LMS_LOG_ERROR, "can't initialize sample converter");
        goto error;
    }

    return true;

  error:
    if (LimeSDR.is_stream_opened) {
        LMS_DestroyStream(LimeSDR.dev, &LimeSDR.stream);
        LimeSDR.is_stream_opened = false;
    }

    if (LimeSDR.dev) {
        LMS_Close(LimeSDR.dev);
        LimeSDR.dev = NULL;
    }

    return false;
}

static void limesdrCallback(unsigned char *buf, uint32_t len, void *ctx)
{
    static int dropped = 0;
    static uint64_t sampleCounter = 0;

    MODES_NOTUSED(ctx);

    sdrMonitor();

    unsigned samples_read = len / LimeSDR.bytes_in_sample; // Drops any trailing odd sample, not much else we can do there

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
    unsigned block_duration = 1e3 * samples_read / Modes.sample_rate;
    outbuf->sysTimestamp = mstime() - block_duration;

    // Convert the new data
    unsigned to_convert = samples_read;
    if (to_convert + outbuf->overlap > outbuf->totalLength) {
        // how did that happen?
        to_convert = outbuf->totalLength - outbuf->overlap;
        dropped = samples_read - to_convert;
    }

    LimeSDR.converter(buf, &outbuf->data[outbuf->overlap], to_convert, LimeSDR.converter_state, &outbuf->mean_level, &outbuf->mean_power);
    outbuf->validLength = outbuf->overlap + to_convert;

    // Push to the demodulation thread
    fifo_enqueue(outbuf);
}

void limesdrRun()
{
    if (!LimeSDR.dev) {
        return;
    }

    int16_t *buffer = malloc(MODES_MAG_BUF_SAMPLES * LimeSDR.bytes_in_sample);
    if (!buffer) {
        limesdrLogHandler(LMS_LOG_ERROR, "out of memory allocating sample buffer");
        return;
    }

    LMS_StartStream(&LimeSDR.stream);

    while (!Modes.exit) {
        int sampleCnt = LMS_RecvStream(&LimeSDR.stream, buffer, MODES_MAG_BUF_SAMPLES, NULL, 1000);
        if (sampleCnt < 0) {
            limesdrLogHandler(LMS_LOG_ERROR, "LMS_RecvStream failed");
            break;
        }

        if (sampleCnt) {
            limesdrCallback((unsigned char *)buffer, sampleCnt * LimeSDR.bytes_in_sample, NULL);
        }
    }

    free(buffer);
    LMS_StopStream(&LimeSDR.stream);
}

void limesdrClose()
{
    if (LimeSDR.converter) {
        cleanup_converter(LimeSDR.converter_state);
        LimeSDR.converter = NULL;
        LimeSDR.converter_state = NULL;
    }

    LMS_StopStream(&LimeSDR.stream);

    if (LimeSDR.is_stream_opened) {
        LMS_DestroyStream(LimeSDR.dev, &LimeSDR.stream);
        LimeSDR.is_stream_opened = false;
    }

    if (LimeSDR.dev) {
        LMS_Close(LimeSDR.dev);
        LimeSDR.dev = NULL;
    }
}
