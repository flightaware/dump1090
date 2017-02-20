// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_airspy.c: airspy dongle support
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
#include "sdr_airspy.h"

#include <airspy.h>
#include <soxr.h>
#include <inttypes.h>

static struct {
    struct airspy_device *dev;
    uint64_t serial_number;
    bool enable_lna_agc;
    bool enable_mixer_agc;
    bool enable_linearity;
    bool enable_sensitivity;
    bool enable_airspy_biast;
    uint8_t linearity_gain;
    uint8_t sensitivity_gain;
    uint8_t mixer_gain;
    uint8_t lna_gain;
    uint8_t vga_gain;
    soxr_t resampler;
    char *airspy_bytes;
    char *airspy_scratch;
    iq_convert_fn converter;
    struct converter_state *converter_state;
} AIRSPY;

//
// =============================== AIRSPY handling ==========================
//

void airspyInitConfig()
{
    AIRSPY.dev = NULL;
    AIRSPY.serial_number = 0;
    AIRSPY.enable_lna_agc = false;
    AIRSPY.enable_mixer_agc = false;
    AIRSPY.enable_linearity = false;
    AIRSPY.enable_sensitivity = false;
    AIRSPY.enable_airspy_biast = false;
    AIRSPY.linearity_gain = 0;
    AIRSPY.sensitivity_gain = 0; 
    AIRSPY.mixer_gain = 8;
    AIRSPY.lna_gain = 13;
    AIRSPY.vga_gain = 5;
    AIRSPY.resampler = NULL; 
    AIRSPY.converter = NULL;
    AIRSPY.converter_state = NULL;
}

static bool parse_uintt(char *s, uint64_t *const value, int utype)
{
    uint_fast8_t base = 10;
    char *s_end;
    uint64_t u64_val;

    if (s[0] == '-')  // likely forgot a parameter
        return false;

    if (!isdigit(s[0]))  // we expect at least one number
        return false;

    if( strlen(s) > 2 ) {  // check for hexadecimal
        if(s[0] == '0') {
            if( tolower(s[1]) == 'x' ) {
                base = 16;
                s += 2;
            }
        }
    }

    s_end = s;

    u64_val = strtoull(s, &s_end, base);
    if( (s != s_end) && (*s_end == 0) ) {
        switch(utype) {
            case 8:
            {
                uint8_t *u8_ptr = (uint8_t *)value;
                *u8_ptr = (uint8_t)u64_val;
                break;
            }
            case 16:
            {
                uint16_t *u16_ptr = (uint16_t *)value;
                *u16_ptr = (uint16_t)u64_val;
                break;
            }
            case 32:
            {
                uint32_t *u32_ptr = (uint32_t *)value;
                *u32_ptr = (uint32_t)u64_val;
                break;
            }
            case 64:
                *value = u64_val;
                break;
            default:
                return false;
                break;
        }
            return true;
    } else {
        return false;
    }
}

void airspyShowHelp()
{
    printf("      airspy-specific options (use with --device-type airspy)\n");
    printf("\n");
    printf("--serial-number <0x...>  Serial number (in hex) of Airspy to open (optional)\n");
    printf("--enable-lna-agc         Enable LNA AGC (default off)\n");
    printf("--enable-mixer-agc       Enable MIXER AGC (default off)\n");
    printf("--linearity-gain <n>     Set linearity simplified gain (0-21)\n");
    printf("--sensitivity-gain <n>   Set sensitivity simplified gain (0-21)\n");
    printf("--mixer-gain <n>         Set MIXER gain (0-15, default 8)\n");
    printf("--lna-gain <n>           Set LNA gain (0-14, default 13)\n");
    printf("--vga-gain <n>           Set VGA gain (0-15, default 5)\n");
    printf("--enable-airspy-biast    Set bias tee supply on (default off)\n");
    printf("\n");
}

bool airspyHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool result = true;
    bool more = (j + 1 < argc);

    if (!strcmp(argv[j], "--serial-number") && more) {
        result = parse_uintt(argv[++j], &AIRSPY.serial_number, 64);
    } else if (!strcmp(argv[j], "--enable-lna-agc")) {
        AIRSPY.enable_lna_agc = true;
    } else if (!strcmp(argv[j], "--enable-mixer-agc")) {
        AIRSPY.enable_mixer_agc = true;
    } else if (!strcmp(argv[j], "--enable-airspy-biast")) {
        AIRSPY.enable_airspy_biast = true;
    } else if (!strcmp(argv[j], "--linearity-gain") && more) {
        result = parse_uintt(argv[++j], (uint64_t *)&AIRSPY.linearity_gain, 8);
        AIRSPY.enable_linearity = true;
    } else if (!strcmp(argv[j], "--sensitivity-gain") && more) {
        result = parse_uintt(argv[++j], (uint64_t *)&AIRSPY.sensitivity_gain, 8);
        AIRSPY.enable_sensitivity = true;
    } else if (!strcmp(argv[j], "--mixer-gain") && more) {
        result = parse_uintt(argv[++j], (uint64_t *)&AIRSPY.mixer_gain, 8);
    } else if (!strcmp(argv[j], "--lna-gain") && more) {
        result = parse_uintt(argv[++j], (uint64_t *)&AIRSPY.lna_gain, 8);
    } else if (!strcmp(argv[j], "--vga-gain") && more) {
        result = parse_uintt(argv[++j], (uint64_t *)&AIRSPY.vga_gain, 8);
    } else {
        return false;
    }

    if (!result)
        return false;

    if (AIRSPY.enable_linearity && AIRSPY.enable_sensitivity) {
        fprintf(stderr,
            "Airspy config: --linearity-gain and --sensitivity-gain cannot be used together.\n");
        return false;
    }

    if ((AIRSPY.enable_linearity || AIRSPY.enable_sensitivity) && (AIRSPY.enable_lna_agc || AIRSPY.enable_mixer_agc)) {
        fprintf(stderr,
            "Airspy config: --linearity-gain/--sensitivity-gain cannot be used with --enable-lna-agc/--enable-mixer-agc.\n");
        return false;
    }

    *jptr = j;
    return true;
}

bool airspyOpen(void) {
    #define AIRSPY_STATUS(status, message) \
        if (status != 0) { \
        fprintf(stderr, "%s\n", message); \
        airspyClose(); \
        return false; \
        } \

    int status;
    uint32_t count;
    uint32_t *samplerates;
    soxr_error_t sox_err = NULL;
    soxr_io_spec_t ios = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    soxr_quality_spec_t qts = soxr_quality_spec(SOXR_MQ, 0);
    soxr_runtime_spec_t rts = soxr_runtime_spec(2);

    AIRSPY.airspy_scratch = calloc(2 * MODES_RTL_BUF_SIZE, sizeof(int16_t));
    AIRSPY.airspy_bytes = malloc(2 * MODES_RTL_BUF_SIZE);
    if ((AIRSPY.airspy_bytes == NULL) || (AIRSPY.airspy_scratch == NULL)) {
        fprintf(stderr, "airspy buffer allocation failed\n");
        return false;
    }

    if (AIRSPY.serial_number) {
        status = airspy_open_sn(&AIRSPY.dev, AIRSPY.serial_number);
        AIRSPY_STATUS(status, "airspy_open_sn failed");
        fprintf(stderr, "Airspy with serial number 0x%" PRIX64 " found, ", AIRSPY.serial_number);
    } else {
        status = airspy_open(&AIRSPY.dev);
        AIRSPY_STATUS(status, "No Airspy compatible devices found");
        fprintf(stderr, "Airspy found, ");
    }

    fprintf(stderr, "configuring...\n");

    status = airspy_get_samplerates(AIRSPY.dev, &count, 0);
    AIRSPY_STATUS(status, "airspy_get_samplerates failed");
    samplerates = (uint32_t *)malloc(count * sizeof(uint32_t));
    airspy_get_samplerates(AIRSPY.dev, samplerates, count);
    // Set the samplerate to the highest value available
    status = airspy_set_samplerate(AIRSPY.dev, samplerates[0]);
    AIRSPY_STATUS(status, "airspy_set_samplerate failed");
    // Downsample from the airspy sample rate to the demod sample rate
    AIRSPY.resampler = soxr_create(samplerates[0], Modes.sample_rate, 2, &sox_err, &ios, &qts, &rts);
    if (sox_err) {
        fprintf(stderr, "soxr_create error: %s; %s\n", soxr_strerror(sox_err), strerror(errno));
        return false;
    }
    free(samplerates);

    status = airspy_set_sample_type(AIRSPY.dev, AIRSPY_SAMPLE_INT16_IQ);
    AIRSPY_STATUS(status, "airspy_set_sample_type failed");

    status = airspy_set_freq(AIRSPY.dev, Modes.freq);
    AIRSPY_STATUS(status, "airspy_set_freq failed");

    if (AIRSPY.enable_linearity) {
        status = airspy_set_linearity_gain(AIRSPY.dev, AIRSPY.linearity_gain);
        AIRSPY_STATUS(status, "airspy_set_linearity_gain failed");
        fprintf(stderr, "Linearity Mode Gain: %i, ", AIRSPY.linearity_gain);
    }
    else if (AIRSPY.enable_sensitivity) {
        status = airspy_set_sensitivity_gain(AIRSPY.dev, AIRSPY.sensitivity_gain);
        AIRSPY_STATUS(status, "airspy_set_sensitivity_gain failed");
        fprintf(stderr, "Sensitivity Mode Gain: %i, ", AIRSPY.sensitivity_gain);
    }
    else {
        if (AIRSPY.enable_mixer_agc) {
            status = airspy_set_mixer_agc(AIRSPY.dev, 1);
            AIRSPY_STATUS(status, "airspy_set_mixer_agc failed");
            fprintf(stderr, "MIXER Gain: AGC, ");
        } 
        else {
            status = airspy_set_mixer_gain(AIRSPY.dev, AIRSPY.mixer_gain);
            AIRSPY_STATUS(status, "airspy_set_mixer_gain failed");
            fprintf(stderr, "MIXER Gain: %i, ", AIRSPY.mixer_gain);
        }

        if (AIRSPY.enable_lna_agc) {
            status = airspy_set_lna_agc(AIRSPY.dev, 1);
            AIRSPY_STATUS(status, "airspy_set_lna_agc failed");
            fprintf(stderr, "LNA Gain: AGC, ");
        } 
        else {
            status = airspy_set_lna_gain(AIRSPY.dev, AIRSPY.lna_gain);
            AIRSPY_STATUS(status, "airspy_set_lna_gain failed");
            fprintf(stderr, "LNA Gain: %i, ", AIRSPY.lna_gain);
        }

        status = airspy_set_vga_gain(AIRSPY.dev, AIRSPY.vga_gain);
        AIRSPY_STATUS(status, "airspy_set_vga_gain failed");
        fprintf(stderr, "VGA Gain: %i, ", AIRSPY.vga_gain);
    }

    if (AIRSPY.enable_airspy_biast) {
        status = airspy_set_rf_bias(AIRSPY.dev, 1);
        AIRSPY_STATUS(status, "airspy_set_rf_bias (on) failed");
        fprintf(stderr, "Bias-t: On\n");
    }
    else {
        status = airspy_set_rf_bias(AIRSPY.dev, 0);
        AIRSPY_STATUS(status, "airspy_set_rf_bias (off) failed");
        fprintf(stderr, "Bias-t: Off\n");
    }

    AIRSPY.converter = init_converter(INPUT_UC8,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &AIRSPY.converter_state);

    if (!AIRSPY.converter) {
        fprintf(stderr, "airspy: can't initialize sample converter\n");
        airspyClose();
        return false;
    }

    fprintf(stderr, "Airspy successfully initialized\n");

    return true;
}

static struct timespec airspy_thread_cpu;

int airspyCallback(airspy_transfer *transfer) {
    int16_t *inptr = (int16_t *)transfer->samples;
    int16_t *outptr = (int16_t *)AIRSPY.airspy_scratch;
    size_t i_len = transfer->sample_count;
    size_t i, i_done, o_done;
    struct mag_buf *outbuf;
    struct mag_buf *lastbuf;
    uint32_t slen;
    unsigned next_free_buffer;
    unsigned free_bufs;
    unsigned block_duration;
    static int dropping = 0;
    static uint64_t sampleCounter = 0;

    if (Modes.exit) {
        airspy_stop_rx(AIRSPY.dev);
        return 0;
    }

    // Lock the data buffer variables before accessing them
    pthread_mutex_lock(&Modes.data_mutex);

    next_free_buffer = (Modes.first_free_buffer + 1) % MODES_MAG_BUFFERS;
    outbuf = &Modes.mag_buffers[Modes.first_free_buffer];
    lastbuf = &Modes.mag_buffers[(Modes.first_free_buffer + MODES_MAG_BUFFERS - 1) % MODES_MAG_BUFFERS];
    free_bufs = (Modes.first_filled_buffer - next_free_buffer + MODES_MAG_BUFFERS) % MODES_MAG_BUFFERS;

    // Give the demodulater what it expects
    soxr_process(AIRSPY.resampler, inptr, i_len, &i_done, outptr, MODES_RTL_BUF_SIZE, &o_done);
    for (i = 0; i < o_done; i++)
        AIRSPY.airspy_bytes[i] = (int8_t)(outptr[i] >> 4) + 127;

    // The resampler often produces an odd sample count, no real value in tracking how often
    if (o_done & 1)
        o_done -= 1;

    slen = o_done/2;

    if (free_bufs == 0 || (dropping && free_bufs < MODES_MAG_BUFFERS/2)) {
        // FIFO is full. Drop this block.
        dropping = 1;
        outbuf->dropped += slen;
        sampleCounter += slen;
        pthread_mutex_unlock(&Modes.data_mutex);
        return 0;
    }

    dropping = 0;
    pthread_mutex_unlock(&Modes.data_mutex);

    // Compute the sample timestamp and system timestamp for the start of the block
    outbuf->sampleTimestamp = sampleCounter + 12e6 / Modes.sample_rate;
    sampleCounter += slen;
    block_duration = 1e9 * slen / Modes.sample_rate;

    // Get the approx system time for the start of this block
    clock_gettime(CLOCK_REALTIME, &outbuf->sysTimestamp);
    outbuf->sysTimestamp.tv_nsec -= block_duration;
    normalize_timespec(&outbuf->sysTimestamp);

    // Copy trailing data from last block (or reset if not valid)
    if (outbuf->dropped == 0) {
        memcpy(outbuf->data, lastbuf->data + lastbuf->length, Modes.trailing_samples * sizeof(uint16_t));
    } else {
        memset(outbuf->data, 0, Modes.trailing_samples * sizeof(uint16_t));
    }

    // Convert the new data
    outbuf->length = slen;
    AIRSPY.converter(AIRSPY.airspy_bytes, &outbuf->data[Modes.trailing_samples], slen, AIRSPY.converter_state, &outbuf->mean_level, &outbuf->mean_power); 

    // Push the new data to the demodulation thread
    pthread_mutex_lock(&Modes.data_mutex);

    Modes.mag_buffers[next_free_buffer].dropped = 0;
    Modes.mag_buffers[next_free_buffer].length = 0;  // just in case
    Modes.first_free_buffer = next_free_buffer;

    // Accumulate CPU while holding the mutex, and restart measurement
    end_cpu_timing(&airspy_thread_cpu, &Modes.reader_cpu_accumulator);
    start_cpu_timing(&airspy_thread_cpu);

    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);

    return (0);
}

void airspyRun()
{
    if (!AIRSPY.dev) {
        return;
    }

    start_cpu_timing(&airspy_thread_cpu);

    while (!Modes.exit) {
        airspy_start_rx(AIRSPY.dev, airspyCallback, NULL);

        while ((airspy_is_streaming(AIRSPY.dev) == AIRSPY_TRUE) && (!Modes.exit)) {
            usleep(500);
        }
    }
}

void airspyClose()
{
    if (AIRSPY.dev) {
        if (airspy_is_streaming(AIRSPY.dev) == AIRSPY_TRUE)
            airspy_stop_rx(AIRSPY.dev);
        airspy_close(AIRSPY.dev);
        AIRSPY.dev = NULL;
    }

    if (AIRSPY.converter) {
        cleanup_converter(AIRSPY.converter_state);
        AIRSPY.converter = NULL;
        AIRSPY.converter_state = NULL;
    }

    if (AIRSPY.resampler)
        soxr_delete(AIRSPY.resampler);
}
