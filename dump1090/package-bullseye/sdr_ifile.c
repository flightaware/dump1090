// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_ifile.c: "file" SDR support
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
#include "sdr_ifile.h"

static struct {
    const char *filename;
    input_format_t input_format;
    bool throttle;

    int fd;
    unsigned bytes_per_sample;
    unsigned bufsize;
    char *readbuf;
    iq_convert_fn converter;
    struct converter_state *converter_state;
} ifile;

void ifileInitConfig(void)
{
    ifile.filename = NULL;
    ifile.input_format = INPUT_UC8;
    ifile.throttle = false;
    ifile.fd = -1;
    ifile.bytes_per_sample = 0;
    ifile.bufsize = 0;
    ifile.readbuf = NULL;
    ifile.converter = NULL;
    ifile.converter_state = NULL;
}

void ifileShowHelp()
{
    printf("      ifile-specific options (use with --ifile)\n");
    printf("\n");
    printf("--ifile <path>           read samples from given file ('-' for stdin)\n");
    printf("--iformat <type>         set sample format (UC8, SC16, SC16Q11)\n");
    printf("--throttle               process samples at the original capture speed\n");
    printf("\n");
}

bool ifileHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j +1  < argc);

    if (!strcmp(argv[j], "--ifile") && more) {
        // implies --device-type ifile
        ifile.filename = strdup(argv[++j]);
        Modes.sdr_type = SDR_IFILE;
    } else if (!strcmp(argv[j],"--iformat") && more) {
        ++j;
        if (!strcasecmp(argv[j], "uc8")) {
            ifile.input_format = INPUT_UC8;
        } else if (!strcasecmp(argv[j], "sc16")) {
            ifile.input_format = INPUT_SC16;
        } else if (!strcasecmp(argv[j], "sc16q11")) {
            ifile.input_format = INPUT_SC16Q11;
        } else {
            fprintf(stderr, "Input format '%s' not understood (supported values: UC8, SC16, SC16Q11)\n",
                    argv[j]);
            return false;
        }
    } else if (!strcmp(argv[j],"--throttle")) {
        ifile.throttle = true;
    } else {
        return false;
    }

    *jptr = j;
    return true;
}

//
//=========================================================================
//
// This is used when --ifile is specified in order to read data from file
// instead of using an RTLSDR device
//
bool ifileOpen(void)
{
    if (!ifile.filename) {
        fprintf(stderr, "SDR type 'ifile' requires an --ifile argument\n");
        return false;
    }

    if (!strcmp(ifile.filename, "-")) {
        ifile.fd = STDIN_FILENO;
    } else if ((ifile.fd = open(ifile.filename, O_RDONLY)) < 0) {
        fprintf(stderr, "ifile: could not open %s: %s\n",
                ifile.filename, strerror(errno));
        return false;
    }

    switch (ifile.input_format) {
    case INPUT_UC8:
        ifile.bytes_per_sample = 2;
        break;
    case INPUT_SC16:
    case INPUT_SC16Q11:
        ifile.bytes_per_sample = 4;
        break;
    default:
        fprintf(stderr, "ifile: unhandled input format\n");
        ifileClose();
        return false;
    }

    ifile.bufsize = ifile.bytes_per_sample * MODES_MAG_BUF_SAMPLES; /* ~1M samples, about half a second's worth */

    if (!(ifile.readbuf = malloc(ifile.bufsize))) {
        fprintf(stderr, "ifile: failed to allocate read buffer\n");
        ifileClose();
        return false;
    }

    ifile.converter = init_converter(ifile.input_format,
                                     Modes.sample_rate,
                                     Modes.dc_filter,
                                     &ifile.converter_state);
    if (!ifile.converter) {
        fprintf(stderr, "ifile: can't initialize sample converter\n");
        ifileClose();
        return false;
    }

    return true;
}

void ifileRun()
{
    if (ifile.fd < 0)
        return;

    struct timespec next_buffer_delivery;
    clock_gettime(CLOCK_MONOTONIC, &next_buffer_delivery);

    bool eof = false;
    uint64_t sampleCounter = 0;

    while (!Modes.exit && !eof) {
        sdrMonitor();

        /* wait for up to 1000ms for a buffer */
        struct mag_buf *outbuf = fifo_acquire(100 /* milliseconds */);
        if (!outbuf) {
            // maybe we're slow, maybe we halted
            continue;
        }

        // Compute the sample timestamp and system time for the start of the block
        outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
        outbuf->sysTimestamp = mstime();

        unsigned bytes_wanted = (outbuf->totalLength - outbuf->overlap) * ifile.bytes_per_sample;
        if (bytes_wanted > ifile.bufsize)
            bytes_wanted = ifile.bufsize;

        unsigned bytes_read = 0;
        while (bytes_read < bytes_wanted) {
            ssize_t nread = read(ifile.fd, ifile.readbuf + bytes_read, bytes_wanted - bytes_read);
            if (nread <= 0) {
                if (nread < 0) {
                    fprintf(stderr, "ifile: error reading input file: %s\n", strerror(errno));
                }
                // Done.
                eof = true;
                break;
            }
            bytes_read += nread;
        }

        unsigned samples_read = bytes_read / ifile.bytes_per_sample;

        // Convert the new data
        ifile.converter(ifile.readbuf, &outbuf->data[outbuf->overlap], samples_read, ifile.converter_state, &outbuf->mean_level, &outbuf->mean_power);
        outbuf->validLength = outbuf->overlap + samples_read;
        outbuf->flags = 0;

        if (ifile.throttle || Modes.interactive) {
            // Wait until we are allowed to release this buffer to the FIFO
            while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_buffer_delivery, NULL) == EINTR)
                ;

            // compute the time we can deliver the next buffer.
            next_buffer_delivery.tv_nsec += samples_read * 1e9 / Modes.sample_rate;
            normalize_timespec(&next_buffer_delivery);
        }

        // Push the new data to the FIFO
        fifo_enqueue(outbuf);
        sampleCounter += samples_read;
    }

    // Wait for the FIFO to drain so we don't throw away trailing data
    fifo_drain();
}

void ifileClose()
{
    if (ifile.converter) {
        cleanup_converter(ifile.converter_state);
        ifile.converter = NULL;
        ifile.converter_state = NULL;
    }

    if (ifile.readbuf) {
        free(ifile.readbuf);
        ifile.readbuf = NULL;
    }

    if (ifile.fd >= 0 && ifile.fd != STDIN_FILENO) {
        close(ifile.fd);
        ifile.fd = -1;
    }
}
