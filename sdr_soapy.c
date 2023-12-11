// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_soapy.c: SoapySDR support
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

#include "dump1090.h"
#include "sdr_soapy.h"

#include <SoapySDR/Version.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

static struct {
    SoapySDRDevice *dev;
    SoapySDRStream *stream;

    iq_convert_fn converter;
    struct converter_state *converter_state;

    size_t channel;
    const char* antenna;
    double bandwidth;
    bool enable_agc;

    int num_gain_elements;
    char **gain_elements;

    SoapySDRRange gain_range;
    int current_gain_step;
} SOAPY;

// Polyfill some differences between SoapySDR 0.7 and 0.8
#if !defined(SOAPY_SDR_API_VERSION) || (SOAPY_SDR_API_VERSION < 0x00080000)

static void polyfill_SoapySDR_free(void *ignored)
{
    (void) ignored;
}

static SoapySDRStream *polyfill_SoapySDRDevice_setupStream(SoapySDRDevice *device,
                                                           const int direction,
                                                           const char *format,
                                                           const size_t *channels,
                                                           const size_t numChans,
                                                           const SoapySDRKwargs *args)
{
    SoapySDRStream *result;

    if (SoapySDRDevice_setupStream(device, &result, direction, format, channels, numChans, args) == 0)
        return result;
    else
        return NULL;
}

#define SoapySDR_free polyfill_SoapySDR_free
#define SoapySDRDevice_setupStream polyfill_SoapySDRDevice_setupStream

#endif /* pre-0.8 API */

//
// =============================== SoapySDR handling ==========================
//

void soapyInitConfig()
{
    SOAPY.dev = NULL;
    SOAPY.stream = NULL;
    SOAPY.converter = NULL;
    SOAPY.converter_state = NULL;
    SOAPY.channel = 0;
    SOAPY.antenna = NULL;
    SOAPY.bandwidth = 0;
    SOAPY.enable_agc = false;
    SOAPY.num_gain_elements = 0;
    SOAPY.gain_elements = NULL;
}

void soapyShowHelp()
{
    printf("      SoapySDR-specific options (use with --device-type soapy)\n");
    printf("\n");
    printf("--device <string>          select/configure device\n");
    printf("--channel <num>            select channel if device supports multiple channels (default: 0)\n");
    printf("--antenna <string>         select antenna (default depends on device)\n");
    printf("--bandwidth <hz>           set the baseband filter width (default: 3MHz, SDRPlay: 5MHz)\n");
    printf("--enable-agc               enable Automatic Gain Control if supported by device\n");
    printf("--gain-element <name>:<db> set gain in dB for a named gain element\n");
    printf("\n");
}

bool soapyHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j +1  < argc);

    if (!strcmp(argv[j], "--channel")) {
        SOAPY.channel = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--antenna") && more) {
        SOAPY.antenna = strdup(argv[++j]);
    } else if (!strcmp(argv[j], "--bandwidth") && more) {
        SOAPY.bandwidth = atof(argv[++j]);
    } else if (!strcmp(argv[j], "--enable-agc") && more) {
        SOAPY.enable_agc = true;
    } else if (!strcmp(argv[j], "--gain-element") && more) {
        ++SOAPY.num_gain_elements;
        if (! (SOAPY.gain_elements = realloc(SOAPY.gain_elements, SOAPY.num_gain_elements * sizeof(*SOAPY.gain_elements))) ) {
            perror("realloc");
            abort();
        }
        if (! (SOAPY.gain_elements[SOAPY.num_gain_elements-1] = strdup(argv[++j])) ) {
            perror("strdup");
            abort();
        }
    } else {
        return false;
    }

    *jptr = j;
    return true;
}

static void soapyShowDevices(SoapySDRKwargs *results, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        fprintf(stderr, "  #%zu: ", i);
        for (size_t j = 0; j < results[i].size; ++j) {
            if (j) fprintf(stderr, ", ");
            fprintf(stderr, "%s=%s", results[i].keys[j], results[i].vals[j]);
        }
        fprintf(stderr, "\n");
    }
}

static void soapyShowAllDevices()
{
    size_t length = 0;
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);
    soapyShowDevices(results, length);
    SoapySDRKwargsList_clear(results, length);
}

bool soapyOpen(void)
{
    size_t length = 0;
    SoapySDRKwargs *results = SoapySDRDevice_enumerateStrArgs(Modes.dev_name ? Modes.dev_name : "", &length);

    if (length == 0) {
        SoapySDRKwargsList_clear(results, length);
        fprintf(stderr, "soapy: no matching devices found; available devices:\n");
        soapyShowAllDevices();
        return false;
    }
    if (length > 1) {
        fprintf(stderr, "soapy: more than one matching device found; matching devices:\n");
        soapyShowDevices(results, length);
        SoapySDRKwargsList_clear(results, length);
        fprintf(stderr, "soapy: please select a single device with --device\n");
        return false;
    }

    fprintf(stderr, "soapy: selected device:  ");
    for (size_t j = 0; j < results[0].size; j++) {
        if (j) fprintf(stderr, ", ");
        fprintf(stderr, "%s=%s", results[0].keys[j], results[0].vals[j]);
    }
    fprintf(stderr, "\n");
    SoapySDRKwargsList_clear(results, length);

    SOAPY.dev = SoapySDRDevice_makeStrArgs(Modes.dev_name ? Modes.dev_name : "");
    if (!SOAPY.dev) {
        fprintf(stderr, "soapy: failed to create device: %s\n", SoapySDRDevice_lastError());
        return false;
    }

    SoapySDRKwargs result = SoapySDRDevice_getHardwareInfo(SOAPY.dev);
    if (result.size > 0) {
        fprintf(stderr, "soapy: hardware info:  ");
        for (size_t i = 0; i < result.size; ++i) {
            if (i) fprintf(stderr, ", ");
            fprintf(stderr, "%s=%s", result.keys[i], result.vals[i]);
        }
        fprintf(stderr, "\n");
    }
    SoapySDRKwargs_clear(&result);

    char* driver_key = SoapySDRDevice_getDriverKey(SOAPY.dev);
    if (driver_key) {
        fprintf(stderr, "soapy: driver key:      %s\n", driver_key);

        // Apply driver-specific defaults
        if (!strcmp(driver_key, "SDRplay")) {
            // Default to 5MHz bandwidth
            if (SOAPY.bandwidth == 0)
                SOAPY.bandwidth = 5.0e6;
        }

        SoapySDR_free(driver_key);
    }

    char* hw_key = SoapySDRDevice_getHardwareKey(SOAPY.dev);
    if (hw_key) {
        fprintf(stderr, "soapy: hardware key:    %s\n", hw_key);
        SoapySDR_free(hw_key);
    }

    //
    // Apply generic defaults
    //

    if (SOAPY.bandwidth == 0) {
        SOAPY.bandwidth = 3.0e6;
    }

    //
    // Configure everything
    //

    if (SOAPY.channel) {
        size_t supported_channels = SoapySDRDevice_getNumChannels(SOAPY.dev, SOAPY_SDR_RX);
        if (SOAPY.channel >= supported_channels) {
            fprintf(stderr, "soapy: device only supports %zu channels, not %zu\n", supported_channels, SOAPY.channel + 1);
            goto error;
        }
    }

    if (SoapySDRDevice_setSampleRate(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, Modes.sample_rate) != 0) {
        fprintf(stderr, "soapy: setSampleRate failed: %s\n", SoapySDRDevice_lastError());
        goto error;
    }

    if (SOAPY.antenna && SoapySDRDevice_setAntenna(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, SOAPY.antenna) != 0) {
        fprintf(stderr, "soapy: setAntenna(%s) failed: %s\n", SOAPY.antenna, SoapySDRDevice_lastError());

        length = 0;
        char** available_antennas = SoapySDRDevice_listAntennas(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, &length);
        fprintf(stderr, "soapy: available antennas: ");
        for (size_t i = 0; i < length; i++) {
            fprintf(stderr, "%s", available_antennas[i]);
            if (i+1 < length) fprintf(stderr, ", ");
        }
        fprintf(stderr, "\n");
        if (available_antennas)
            SoapySDRStrings_clear(&available_antennas, length);

        goto error;
    }

    if (SoapySDRDevice_setFrequency(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, Modes.freq, NULL) != 0) {
        fprintf(stderr, "soapy: setFrequency failed: %s\n", SoapySDRDevice_lastError());
        goto error;
    }

    SOAPY.gain_range = SoapySDRDevice_getGainRange(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel);
    if (SOAPY.gain_range.step <= 0)
        SOAPY.gain_range.step = 1.0;
    else if (SOAPY.gain_range.step <= 0.1)
        SOAPY.gain_range.step = 0.1;

    bool has_agc = SoapySDRDevice_hasGainMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel);
    if (SOAPY.enable_agc) {
        if (has_agc) {
            if (SoapySDRDevice_setGainMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, 1) != 0) {
                fprintf(stderr, "soapy: setGainMode failed: %s\n", SoapySDRDevice_lastError());
                goto error;
            }
        }
        else {
            fprintf(stderr, "soapy: device does not support enabling AGC\n");
            goto error;
        }
    } else {
        // Manual gain path

        if (has_agc) {
            if (SoapySDRDevice_setGainMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, 0) != 0) {
                fprintf(stderr, "soapy: setGainMode failed: %s\n", SoapySDRDevice_lastError());
                goto error;
            }
        }

        double gain = (Modes.gain == MODES_DEFAULT_GAIN ? SOAPY.gain_range.maximum : Modes.gain);
        if (SoapySDRDevice_setGain(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, gain) < 0) {
            fprintf(stderr, "soapy: setGain(%.1fdB) failed\n", gain);
            goto error;
        }

        for (int i = 0; i < SOAPY.num_gain_elements; ++i) {
            char *element = SOAPY.gain_elements[i];
            char *sep = strchr(element, ':');
            if (!sep || !sep[1]) {
                fprintf(stderr, "soapy: don't understand a gain element setting of '%s' (should be formatted as <element>:<db>)\n", element);
                goto error;
            }

            *sep = 0;
            char *endptr = NULL;
            double gain = strtod(sep + 1, &endptr);

            if ((gain == 0 && !endptr) || *endptr) {
                fprintf(stderr, "soapy: don't understand a gain value of '%s' for gain element %s (should be a floating-point gain in dB)\n", sep + 1, element);
                goto error;
            }

            if (SoapySDRDevice_setGainElement(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, element, gain) != 0) {
                fprintf(stderr, "soapy: setGainElement(%s,%.1fdB) failed: %s\n", element, gain, SoapySDRDevice_lastError());
                goto error;
            }
        }
    }

    SOAPY.current_gain_step = (int) round( (SoapySDRDevice_getGain(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) - SOAPY.gain_range.minimum) / SOAPY.gain_range.step );
    if (Modes.adaptive_range_target == 0)
        Modes.adaptive_range_target = (SOAPY.gain_range.maximum - SOAPY.gain_range.minimum) * 0.6; // just a wild guess

    if (SoapySDRDevice_setBandwidth(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, SOAPY.bandwidth) != 0) {
        fprintf(stderr, "soapy: setBandwidth(%.1f MHz) failed: %s\n", SOAPY.bandwidth / 1e6, SoapySDRDevice_lastError());
        goto error;
    }


    //
    // Done configuring, report the final device state
    //

    fprintf(stderr, "soapy: total gain:      %.1fdB",
            SoapySDRDevice_getGain(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel));
    char** gain_elements = SoapySDRDevice_listGains(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, &length);
    if (gain_elements) {
        for (size_t i = 0; i < length; i++) {
            fprintf(stderr, "; %s=%.1fdB", gain_elements[i],
                    SoapySDRDevice_getGainElement(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, gain_elements[i]));
        }
        SoapySDRStrings_clear(&gain_elements, length);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "soapy: frequency:       %.1f MHz\n",
            SoapySDRDevice_getFrequency(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) / 1e6);
    fprintf(stderr, "soapy: sample rate:     %.1f MHz\n",
            SoapySDRDevice_getSampleRate(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) / 1e6);
    fprintf(stderr, "soapy: bandwidth:       %.1f MHz\n",
            SoapySDRDevice_getBandwidth(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) / 1e6);
    if (has_agc) {
        fprintf(stderr, "soapy: AGC mode:        %s\n",
                SoapySDRDevice_getGainMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) ? "automatic" : "manual");
    }

    char* antenna = SoapySDRDevice_getAntenna(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel);
    if (antenna) {
        fprintf(stderr, "soapy: antenna:         %s\n", antenna);
        SoapySDR_free(antenna);
    }

    if (SoapySDRDevice_hasDCOffset(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel)) {
        fprintf(stderr, "soapy: DC offset mode:  %s\n",
                SoapySDRDevice_getDCOffsetMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) ? "automatic" : "manual");

        double offsetI;
        double offsetQ;
        if (!SoapySDRDevice_getDCOffset(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, &offsetI, &offsetQ)) {
            fprintf(stderr, "soapy: DC offset:  I=%.3f Q=%.3f\n", offsetI, offsetQ);
        }
    }

    if (SoapySDRDevice_hasIQBalance(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel)) {
#ifdef SOAPY_SDR_API_HAS_IQ_BALANCE_MODE
        fprintf(stderr, "soapy: IQ balance mode: %s\n",
                SoapySDRDevice_getIQBalanceMode(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel) ? "automatic" : "manual");
#endif

        double balanceI;
        double balanceQ;
        if (!SoapySDRDevice_getIQBalance(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, &balanceI, &balanceQ)) {
            fprintf(stderr, "soapy: IQ balance is I=%.1f, Q=%.1f\n", balanceI, balanceQ);
        }
    }

    if (SoapySDRDevice_hasFrequencyCorrection(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel)) {
        fprintf(stderr, "soapy: freq correction: %.1f ppm\n",
                SoapySDRDevice_getFrequencyCorrection(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel));
    }

    size_t channels[1] = { SOAPY.channel };
    SoapySDRKwargs stream_args = { 0, NULL, NULL };
    if ((SOAPY.stream = SoapySDRDevice_setupStream(SOAPY.dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, 1, &stream_args)) == NULL) {
        fprintf(stderr, "soapy: setupStream failed: %s\n", SoapySDRDevice_lastError());
        goto error;
    }

    SOAPY.converter = init_converter(INPUT_SC16,
                                     Modes.sample_rate,
                                     Modes.dc_filter,
                                     &SOAPY.converter_state);
    if (!SOAPY.converter) {
        fprintf(stderr, "soapy: can't initialize sample converter\n");
        goto error;
    }

    return true;

 error:
    if (SOAPY.dev != NULL) {
        SoapySDRDevice_unmake(SOAPY.dev);
        SOAPY.dev = NULL;
    }

    return false;
}

void soapyRun()
{
    if (!SOAPY.dev) {
        return;
    }

    if (SoapySDRDevice_activateStream(SOAPY.dev, SOAPY.stream, 0, 0, 0) != 0) {
        fprintf(stderr, "soapy: activateStream failed: %s\n", SoapySDRDevice_lastError());
        return;
    }

    uint8_t* buf;
    const int buffer_elements = MODES_MAG_BUF_SAMPLES; // 131072
    buf = malloc(buffer_elements * 4);

    unsigned int dropped = 0;
    uint64_t sampleCounter = 0;

    while (!Modes.exit) {
        int flags;
        long long timeNs;
        int samples_read = SoapySDRDevice_readStream(SOAPY.dev, SOAPY.stream, (void *) &buf, buffer_elements, &flags, &timeNs, 5000000);
        if (samples_read <= 0) {
            fprintf(stderr, "soapy: readStream failed: %s\n", SoapySDRDevice_lastError());
            return;
        }

        struct mag_buf *outbuf = fifo_acquire(0 /* no wait */);
        if (!outbuf) {
            fprintf(stderr, "soapy: fifo is full, dropping samples\n");
            // FIFO is full. Drop this block.
            dropped += samples_read;
            sampleCounter += samples_read;
            continue;
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

        unsigned int to_convert = samples_read;
        if (to_convert + outbuf->overlap > outbuf->totalLength) {
            // how did that happen?
            to_convert = outbuf->totalLength - outbuf->overlap;
            dropped = samples_read - to_convert;
        }

        // Convert the new data
        SOAPY.converter(buf, &outbuf->data[outbuf->overlap], to_convert, SOAPY.converter_state, &outbuf->mean_level, &outbuf->mean_power);
        outbuf->validLength = outbuf->overlap + to_convert;

        // Push to the demodulation thread
        fifo_enqueue(outbuf);
    }

    free(buf);
}

void soapyClose()
{
    if (SOAPY.stream) {
        SoapySDRDevice_closeStream(SOAPY.dev, SOAPY.stream);
        SOAPY.stream = NULL;
    }

    if (SOAPY.dev) {
        SoapySDRDevice_unmake(SOAPY.dev);
        SOAPY.dev = NULL;
    }

    if (SOAPY.converter) {
        cleanup_converter(SOAPY.converter_state);
        SOAPY.converter = NULL;
        SOAPY.converter_state = NULL;
    }

    for (int i = 0; i < SOAPY.num_gain_elements; ++i) {
        free(SOAPY.gain_elements[i]);
    }
    free(SOAPY.gain_elements);
    SOAPY.gain_elements = NULL;
}

//
// This is a bit horrible; since soapy doesn't tell us the actual device steps,
// we track the last requested gain step ourselves and always report that as
// the current step. Otherwise adaptive gain will get confused if there are
// any step values that can't actually be set, i.e.:
//
//   current gain is at step 4
//   adaptive wants to increase gain, set gain = current + 1 = 5
//   request gain 5, hardware can't actually do that, nearest is step 4
//   adaptive wants to increase gain further, set gain = current + 1 = 5
//   we make no progress

int soapyGetGain() {
    return SOAPY.current_gain_step;
}

int soapyGetMaxGain() {
    return (int) ceil( (SOAPY.gain_range.maximum - SOAPY.gain_range.minimum) / SOAPY.gain_range.step );
}

double soapyGetGainDb(int step) {
    double gain = SOAPY.gain_range.minimum + step * SOAPY.gain_range.step;
    if (gain < SOAPY.gain_range.minimum)
        gain = SOAPY.gain_range.minimum;
    if (gain > SOAPY.gain_range.maximum)
        gain = SOAPY.gain_range.maximum;
    return gain;
}

int soapySetGain(int step) {
    double gainDb = soapyGetGainDb(step);

    if (SoapySDRDevice_setGain(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, gainDb) != 0) {
        fprintf(stderr, "soapy: setGain(%.1fdB) failed: %s\n", gainDb, SoapySDRDevice_lastError());
        return -1;
    }

    fprintf(stderr, "soapy: total gain set to %.1fdB", SoapySDRDevice_getGain(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel));
    size_t length = 0;
    char** gain_elements = SoapySDRDevice_listGains(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, &length);
    for (size_t i = 0; i < length; i++) {
        fprintf(stderr, "; %s=%.1fdB", gain_elements[i],
                SoapySDRDevice_getGainElement(SOAPY.dev, SOAPY_SDR_RX, SOAPY.channel, gain_elements[i]));
    }
    if (gain_elements)
        SoapySDRStrings_clear(&gain_elements, length);
    fprintf(stderr, "\n");

    SOAPY.current_gain_step = step;
    return step;
}
