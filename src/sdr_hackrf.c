// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_hackrf.h: HackRF One support (header)
//
// Copyright (c) 2019 FlightAware LLC
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
#include "sdr_hackrf.h"

#include <libhackrf/hackrf.h>
#include <inttypes.h>

static struct {
    hackrf_device *device;
    uint64_t freq;
    int enable_amp;
    int enable_ant_pwr;
    int lna_gain;
    int vga_gain;
    int rate;
    int ppm;

    iq_convert_fn converter;
    struct converter_state *converter_state;
} HackRF;

void hackRFInitConfig()
{
    HackRF.device = NULL;
    HackRF.freq = 1090000000;
    HackRF.enable_amp = 0;
    HackRF.enable_ant_pwr = 0;
    HackRF.lna_gain = 32;
    HackRF.vga_gain = 50;
    HackRF.rate = 2400000;
    HackRF.ppm = 0;
    HackRF.converter = NULL;
    HackRF.converter_state = NULL;
}

bool hackRFHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j+1 < argc);
    if (!strcmp(argv[j], "--lna-gain") && more) {
        HackRF.lna_gain = atoi(argv[++j]);

        if (HackRF.lna_gain % 8 != 0) {
            fprintf(stderr, "Error: --lna-gain must be multiple of 8\n");
            return false;
        }

        if (HackRF.lna_gain > 40 || HackRF.lna_gain < 0) {
            fprintf(stderr, "Error: --lna-gain range is 0 - 42\n");
            return false;
        }
    } else if (!strcmp(argv[j], "--vga-gain") && more) {
        HackRF.vga_gain = atoi(argv[++j]);

        if (HackRF.vga_gain % 2 != 0) {
            fprintf(stderr, "Error: --vga-gain must be multiple of 2\n");
            return false;
        }

        if (HackRF.vga_gain > 62 || HackRF.vga_gain < 0) {
            fprintf(stderr, "Error: --vga-gain range is 0 - 62\n");
            return false;
        }

    } else if (!strcmp(argv[j], "--ppm") && more) {
        HackRF.ppm = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--samplerate") && more) {
        HackRF.rate = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--enable-amp")) {
        HackRF.enable_amp = 1;
    } else if (!strcmp(argv[j], "--enable-antenna-power")) {
        HackRF.enable_ant_pwr = 1;
    }
     else {
        return false;
    }

    *jptr = j;

    return true;
}

void hackRFShowHelp()
{
    printf("      HackRF-specific options (use with --device-type hackrf)\n");
    printf("\n");
    printf("--enable-amp             enable amplifier (disabled unless specified)\n");
    printf("--enable-antenna-power   enable DC power to the antenna connector (disabled unless specified)\n");
    printf("--lna-gain               set LNA gain (default: 32dB. Range 0-40 in 8dB steps)\n");
    printf("--vga-gain               set VGA gain (default: 50dB. Range 0-62 in 2dB steps)\n");
    printf("--samplerate             set sample rate (default: 2400000Hz)\n");
    printf("--ppm                    ppm correction (default: 0)\n");
    printf("\n");
}

static void show_config()
{
    fprintf(stderr, "freq : %" PRIu64 "\n", HackRF.freq);
    fprintf(stderr, "lna_gain : %d\n", HackRF.lna_gain);
    fprintf(stderr, "vga_gain : %d\n", HackRF.vga_gain);
    fprintf(stderr, "samplerate : %d\n", HackRF.rate);
    fprintf(stderr, "ppm : %d\n", HackRF.ppm);
}

bool hackRFOpen()
{
    if (HackRF.device) {
        return true;
    }

    // Calculate sample rate and frequency deviation if ppm is specified
    if (HackRF.ppm != 0) {
        HackRF.rate = (uint32_t)((double)HackRF.rate * (1000000 - HackRF.ppm)/1000000+0.5);
        HackRF.freq = HackRF.freq * (1000000 - HackRF.ppm)/1000000;
    }

    int status;

    status = hackrf_init();
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_init failed with code %d\n", status);
        return false;
    }

    status = hackrf_open(&HackRF.device);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_open failed with code %d\n", status);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_freq(HackRF.device, HackRF.freq);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_freq failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_sample_rate(HackRF.device, HackRF.rate);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_sample_rate failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_amp_enable(HackRF.device, HackRF.enable_amp);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_amp_enable failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_lna_gain(HackRF.device, HackRF.lna_gain);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_lna_gain failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_vga_gain(HackRF.device, HackRF.vga_gain);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_vga_gain failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }

    status = hackrf_set_antenna_enable(HackRF.device, HackRF.enable_ant_pwr);
    if (status != 0) {
        fprintf(stderr, "HackRF: hackrf_set_antenna_enable failed with code %d\n", status);
        hackrf_close(HackRF.device);
        hackrf_exit();
        return false;
    }


    show_config();

    HackRF.converter = init_converter(INPUT_UC8,
                                      Modes.sample_rate,
                                      Modes.dc_filter,
                                      &HackRF.converter_state);
    if (!HackRF.converter) {
        fprintf(stderr, "HackRF: can't initialize sample converter\n");
        return false;
    }

    return true;
}

static int handle_hackrf_samples(hackrf_transfer *transfer)
{
    static unsigned dropped = 0;
    static uint64_t sampleCounter = 0;

    sdrMonitor();

    if (Modes.exit || transfer->valid_length < 0)
        return -1;

    uint8_t *buf = transfer->buffer;
    unsigned len = transfer->valid_length;

    // Values returned by HackRF need conversion from signed to unsigned
    for (unsigned i = 0; i < len; i++) {
        buf[i] ^= (uint8_t)0x80;
    }

    unsigned samples_read = len / 2; // Drops any trailing odd sample, that's OK

    struct mag_buf *outbuf = fifo_acquire(0 /* don't wait */);
    if (!outbuf) {
        // FIFO is full. Drop this block.
        dropped += samples_read;
        sampleCounter += samples_read;
        return 0;
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

    HackRF.converter(buf, &outbuf->data[outbuf->overlap], to_convert, HackRF.converter_state, &outbuf->mean_level, &outbuf->mean_power);
    outbuf->validLength = outbuf->overlap + to_convert;

    // Push to the demodulation thread
    fifo_enqueue(outbuf);

    return 0;
}


void hackRFRun()
{
    if (!HackRF.device) {
        fprintf(stderr, "hackRFRun: HackRF.device = NULL\n");
        return;
    }

    int status = hackrf_start_rx(HackRF.device, &handle_hackrf_samples, NULL);

    if (status != 0) { 
        fprintf(stderr, "hackrf_start_rx failed\n");
        hackrf_close(HackRF.device); 
        hackrf_exit(); 
        exit (1); 
    }

    // hackrf_start_rx does not block so we need to wait until the streaming is finished
    // before returning from the hackRFRun function
    while (hackrf_is_streaming(HackRF.device) == HACKRF_TRUE) {
        struct timespec slp = { 0, 100 * 1000 * 1000};
        nanosleep(&slp, NULL);
    }

    fprintf(stderr, "HackRF stopped streaming %d\n", hackrf_is_streaming(HackRF.device));
}

void hackRFClose()
{
    if (HackRF.device) {
        hackrf_close(HackRF.device);
        hackrf_exit();
        HackRF.device = NULL;
    }
}
