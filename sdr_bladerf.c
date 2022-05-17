// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_bladerf.c: bladeRF support
//
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
#include "sdr_bladerf.h"

#include <libbladeRF.h>
#include <inttypes.h>

#if !defined(LIBBLADERF_API_VERSION) || (LIBBLADERF_API_VERSION < 0x01060100)
#error This libbladeRF is too old, upgrade or disable bladerf support
#endif

// Polyfill for the older bladerf API
#if LIBBLADERF_API_VERSION < 0x02000000
typedef unsigned int bladerf_frequency;
#endif

static struct {
    const char *device_str;
    const char *fpga_path;
    unsigned decimation;
    bladerf_lpf_mode lpf_mode;
    unsigned lpf_bandwidth;

    struct bladerf *device;

    iq_convert_fn converter;
    struct converter_state *converter_state;

    unsigned block_size;
} BladeRF;

void bladeRFInitConfig()
{
    BladeRF.device_str = NULL;
    BladeRF.fpga_path = NULL;
    BladeRF.decimation = 1;
    BladeRF.lpf_mode = BLADERF_LPF_NORMAL;
    BladeRF.lpf_bandwidth = 1750000;
    BladeRF.device = NULL;
}

bool bladeRFHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j+1 < argc);
    if (!strcmp(argv[j], "--bladerf-fpga") && more) {
        BladeRF.fpga_path = strdup(argv[++j]);
    } else if (!strcmp(argv[j], "--bladerf-decimation") && more) {
        BladeRF.decimation = atoi(argv[++j]);
    } else if (!strcmp(argv[j], "--bladerf-bandwidth") && more) {
        ++j;
        if (!strcasecmp(argv[j], "bypass")) {
            BladeRF.lpf_mode = BLADERF_LPF_BYPASSED;
        } else {
            BladeRF.lpf_mode = BLADERF_LPF_NORMAL;
            BladeRF.lpf_bandwidth = atoi(argv[j]);
        }
    } else {
        return false;
    }

    *jptr = j;
    return true;
}

void bladeRFShowHelp()
{
    printf("      bladeRF-specific options (use with --device-type bladerf)\n");
    printf("\n");
    printf("--device <ident>         select device by bladeRF 'device identifier'\n");
    printf("--bladerf-fpga <path>    use alternative FPGA bitstream ('' to disable FPGA load)\n");
    printf("--bladerf-decimation <N> assume FPGA decimates by a factor of N\n");
    printf("--bladerf-bandwidth <hz> set LPF bandwidth ('bypass' to bypass the LPF)\n");
    printf("\n");
}

static int lna_gain_db(bladerf_lna_gain gain)
{
    switch (gain) {
    case BLADERF_LNA_GAIN_BYPASS:
        return 0;
    case BLADERF_LNA_GAIN_MID:
        return BLADERF_LNA_GAIN_MID_DB;
    case BLADERF_LNA_GAIN_MAX:
        return BLADERF_LNA_GAIN_MAX_DB;
    default:
        return -1;
    }
}

static void show_config()
{
    int status;

    unsigned rate;
    bladerf_frequency freq;
    bladerf_lpf_mode lpf_mode;
    unsigned lpf_bw;
    bladerf_lna_gain lna_gain;
    int rxvga1_gain;
    int rxvga2_gain;
    int16_t lms_dc_i, lms_dc_q;
    int16_t fpga_phase, fpga_gain;
    struct bladerf_lms_dc_cals dc_cals;

    if ((status = bladerf_get_sample_rate(BladeRF.device, BLADERF_MODULE_RX, &rate)) < 0 ||
        (status = bladerf_get_frequency(BladeRF.device, BLADERF_MODULE_RX, &freq)) < 0 ||
        (status = bladerf_get_lpf_mode(BladeRF.device, BLADERF_MODULE_RX, &lpf_mode)) < 0 ||
        (status = bladerf_get_bandwidth(BladeRF.device, BLADERF_MODULE_RX, &lpf_bw)) < 0 ||
        (status = bladerf_get_lna_gain(BladeRF.device, &lna_gain)) < 0 ||
        (status = bladerf_get_rxvga1(BladeRF.device, &rxvga1_gain)) < 0 ||
        (status = bladerf_get_rxvga2(BladeRF.device, &rxvga2_gain)) < 0 ||
        (status = bladerf_get_correction(BladeRF.device, BLADERF_MODULE_RX, BLADERF_CORR_LMS_DCOFF_I, &lms_dc_i)) < 0 ||
        (status = bladerf_get_correction(BladeRF.device, BLADERF_MODULE_RX, BLADERF_CORR_LMS_DCOFF_Q, &lms_dc_q)) < 0 ||
        (status = bladerf_get_correction(BladeRF.device, BLADERF_MODULE_RX, BLADERF_CORR_FPGA_PHASE, &fpga_phase)) < 0 ||
        (status = bladerf_get_correction(BladeRF.device, BLADERF_MODULE_RX, BLADERF_CORR_FPGA_GAIN, &fpga_gain)) < 0 ||
        (status = bladerf_lms_get_dc_cals(BladeRF.device, &dc_cals)) < 0) {
        fprintf(stderr, "bladeRF: couldn't read back device configuration\n");
        return;
    }

    fprintf(stderr, "bladeRF: sampling rate: %.1f MHz\n", rate/1e6);
    fprintf(stderr, "bladeRF: frequency:     %.1f MHz\n", freq/1e6);
    fprintf(stderr, "bladeRF: LNA gain:      %ddB\n", lna_gain_db(lna_gain));
    fprintf(stderr, "bladeRF: RXVGA1 gain:   %ddB\n", rxvga1_gain);
    fprintf(stderr, "bladeRF: RXVGA2 gain:   %ddB\n", rxvga2_gain);

    switch (lpf_mode) {
    case BLADERF_LPF_NORMAL:
        fprintf(stderr, "bladeRF: LPF bandwidth: %.2f MHz\n", lpf_bw/1e6);
        break;
    case BLADERF_LPF_BYPASSED:
        fprintf(stderr, "bladeRF: LPF bypassed\n");
        break;
    case BLADERF_LPF_DISABLED:
        fprintf(stderr, "bladeRF: LPF disabled\n");
        break;
    default:
        fprintf(stderr, "bladeRF: LPF in unknown state\n");
        break;
    }

    fprintf(stderr, "bladeRF: calibration settings:\n");
    fprintf(stderr, "  LMS DC adjust:     I=%d Q=%d\n", lms_dc_i, lms_dc_q);
    fprintf(stderr, "  FPGA phase adjust: %+.3f degrees\n", fpga_phase * 10.0 / 4096);
    fprintf(stderr, "  FPGA gain adjust:  %+.3f\n", fpga_gain * 1.0 / 4096);
    fprintf(stderr, "  LMS LPF tuning:    %d\n", dc_cals.lpf_tuning);
    fprintf(stderr, "  LMS RX LPF filter: I=%d Q=%d\n", dc_cals.rx_lpf_i, dc_cals.rx_lpf_q);
    fprintf(stderr, "  LMS RXVGA2 DC ref: %d\n", dc_cals.dc_ref);
    fprintf(stderr, "  LMS RXVGA2A:       I=%d Q=%d\n", dc_cals.rxvga2a_i, dc_cals.rxvga2a_q);
    fprintf(stderr, "  LMS RXVGA2B:       I=%d Q=%d\n", dc_cals.rxvga2b_i, dc_cals.rxvga2b_q);

}

bool bladeRFOpen()
{
    if (BladeRF.device) {
        return true;
    }

    int status;

    bladerf_set_usb_reset_on_open(true);
    if ((status = bladerf_open(&BladeRF.device, Modes.dev_name)) < 0) {
        fprintf(stderr, "Failed to open bladeRF: %s\n", bladerf_strerror(status));
        goto error;
    }

    const char *fpga_path;
    if (BladeRF.fpga_path) {
        fpga_path = BladeRF.fpga_path;
    } else {
        bladerf_fpga_size size;
        if ((status = bladerf_get_fpga_size(BladeRF.device, &size)) < 0) {
            fprintf(stderr, "bladerf_get_fpga_size failed: %s\n", bladerf_strerror(status));
            goto error;
        }

        switch (size) {
        case BLADERF_FPGA_40KLE:
            fpga_path = "/usr/share/Nuand/bladeRF/hostedx40.rbf";
            break;
        case BLADERF_FPGA_115KLE:
            fpga_path = "/usr/share/Nuand/bladeRF/hostedx115.rbf";
            break;
        default:
            fprintf(stderr, "bladeRF: unknown FPGA size, skipping FPGA load");
            fpga_path = NULL;
            break;
        }
    }

    if (fpga_path && fpga_path[0]) {
        fprintf(stderr, "bladeRF: loading FPGA bitstream from %s\n", fpga_path);
        if ((status = bladerf_load_fpga(BladeRF.device, fpga_path)) < 0) {
            fprintf(stderr, "bladerf_load_fpga() failed: %s\n", bladerf_strerror(status));
            goto error;
        }
    }

    switch (bladerf_device_speed(BladeRF.device)) {
    case BLADERF_DEVICE_SPEED_HIGH:
        BladeRF.block_size = 1024;
        break;
    case BLADERF_DEVICE_SPEED_SUPER:
        BladeRF.block_size = 2048;
        break;
    default:
        fprintf(stderr, "couldn't determine bladerf device speed\n");
        goto error;
    }

    if ((status = bladerf_set_sample_rate(BladeRF.device, BLADERF_MODULE_RX, Modes.sample_rate * BladeRF.decimation, NULL)) < 0) {
        fprintf(stderr, "bladerf_set_sample_rate failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_set_frequency(BladeRF.device, BLADERF_MODULE_RX, Modes.freq)) < 0) {
        fprintf(stderr, "bladerf_set_frequency failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_set_lpf_mode(BladeRF.device, BLADERF_MODULE_RX, BladeRF.lpf_mode)) < 0) {
        fprintf(stderr, "bladerf_set_lpf_mode failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_set_bandwidth(BladeRF.device, BLADERF_MODULE_RX, BladeRF.lpf_bandwidth, NULL)) < 0) {
        fprintf(stderr, "bladerf_set_lpf_bandwidth failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* turn the tx gain right off, just in case */
    if ((status = bladerf_set_gain(BladeRF.device, BLADERF_MODULE_TX, -100)) < 0) {
        fprintf(stderr, "bladerf_set_gain(TX) failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_set_gain(BladeRF.device, BLADERF_MODULE_RX, Modes.gain)) < 0) {
        fprintf(stderr, "bladerf_set_gain(RX) failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_set_loopback(BladeRF.device, BLADERF_LB_NONE)) < 0) {
        fprintf(stderr, "bladerf_set_loopback() failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_calibrate_dc(BladeRF.device, BLADERF_DC_CAL_LPF_TUNING)) < 0) {
        fprintf(stderr, "bladerf_calibrate_dc(LPF_TUNING) failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_calibrate_dc(BladeRF.device, BLADERF_DC_CAL_RX_LPF)) < 0) {
        fprintf(stderr, "bladerf_calibrate_dc(RX_LPF) failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    if ((status = bladerf_calibrate_dc(BladeRF.device, BLADERF_DC_CAL_RXVGA2)) < 0) {
        fprintf(stderr, "bladerf_calibrate_dc(RXVGA2) failed: %s\n", bladerf_strerror(status));
        goto error;
    }

    show_config();

    BladeRF.converter = init_converter(INPUT_SC16Q11,
                                       Modes.sample_rate,
                                       Modes.dc_filter,
                                       &BladeRF.converter_state);
    if (!BladeRF.converter) {
        fprintf(stderr, "can't initialize sample converter\n");
        goto error;
    }

    return true;

 error:
    if (BladeRF.device) {
        bladerf_close(BladeRF.device);
        BladeRF.device = NULL;
    }
    return false;
}

static unsigned timeouts = 0;

static void *handle_bladerf_samples(struct bladerf *dev,
                                    struct bladerf_stream *stream,
                                    struct bladerf_metadata *meta,
                                    void *samples,
                                    size_t num_samples,
                                    void *user_data)
{
    static uint64_t nextTimestamp = 0;  // what's the next timestamp we expect to see?
    static bool overrun = false;        // do we have a pending overrun to report?
    static bool dropped = 0;            // do we have some dropped samples to report?
    static bool first_buffer = true;    // is this the very first callback?

    MODES_NOTUSED(dev);
    MODES_NOTUSED(stream);
    MODES_NOTUSED(meta);
    MODES_NOTUSED(user_data);

    // record initial time for later sys timestamp calculation
    uint64_t entryTimestamp = mstime();

    sdrMonitor();

    if (Modes.exit) {
        // ask our caller to return
        return BLADERF_STREAM_SHUTDOWN;
    }

    timeouts = 0;

    // start handling metadata blocks
    unsigned samples_per_block = (BladeRF.block_size - 16) / 4;
    struct mag_buf *outbuf = NULL;

    for (unsigned offset = 0; offset < num_samples * 4; offset += BladeRF.block_size) {
        // read the next metadata header
        uint8_t *header = ((uint8_t*)samples) + offset;
        uint64_t metadata_magic = le32toh(*(uint32_t*)(header));
        uint64_t metadata_timestamp = le64toh(*(uint64_t*)(header + 4));
        uint32_t metadata_flags = le32toh(*(uint32_t*)(header + 12));
        void *sample_data = header + 16;

        if (metadata_magic != 0x12344321) {
            // first buffer is often in the wrong mode
            if (!first_buffer) {
                fprintf(stderr, "bladeRF: wrong metadata header magic value, skipping rest of buffer\n");
            }
            break;
        }

        if (metadata_flags & BLADERF_META_STATUS_OVERRUN)
            overrun = true;

        if (nextTimestamp && nextTimestamp != metadata_timestamp) {
            // dropped data or lost sync
            overrun = true;
            if (metadata_timestamp > nextTimestamp)
                dropped = (metadata_timestamp - nextTimestamp);
            else
                dropped = 0;
        }

        if (outbuf && (overrun || (outbuf->validLength + samples_per_block > outbuf->totalLength))) {
            // discontinuity or buffer is full. Push the current buffer and get a new one
            fifo_enqueue(outbuf);
            outbuf = NULL;
        }

        if (!outbuf) {
            // need a new buffer
            outbuf = fifo_acquire(/* don't wait */ 0);
            if (!outbuf) {
                // we have nowhere to put this data, drop it. nb: don't update nextTimestamp
                overrun = true;
                continue;
            }

            // set metadata on the new buffer
            outbuf->flags = 0;
            if (overrun) {
                outbuf->flags |= MAGBUF_DISCONTINUOUS;
            }
            outbuf->dropped = dropped;
            outbuf->validLength = outbuf->overlap;
            outbuf->sampleTimestamp = metadata_timestamp * 12e6 / Modes.sample_rate / BladeRF.decimation;
            outbuf->sysTimestamp = entryTimestamp + (num_samples - offset / 4) * 1000 / Modes.sample_rate / BladeRF.decimation;
            outbuf->mean_level = 0;
            outbuf->mean_power = 0;

            overrun = false;
            dropped = 0;
        }

        // Convert one block of sample data
        double mean_level, mean_power;
        BladeRF.converter(sample_data, &outbuf->data[outbuf->validLength], samples_per_block, BladeRF.converter_state, &mean_level, &mean_power);
        outbuf->validLength += samples_per_block;
        outbuf->mean_level += mean_level;
        outbuf->mean_power += mean_power;
        nextTimestamp = metadata_timestamp + samples_per_block * BladeRF.decimation;
    }

    // push the final buffer, if any
    if (outbuf) {
        fifo_enqueue(outbuf);
    }

    first_buffer = false;
    return samples;
}


void bladeRFRun()
{
    if (!BladeRF.device) {
        return;
    }

    unsigned transfers = 7;

    int status;
    struct bladerf_stream *stream = NULL;
    void **buffers = NULL;

    if ((status = bladerf_init_stream(&stream,
                                      BladeRF.device,
                                      handle_bladerf_samples,
                                      &buffers,
                                      /* num_buffers */ transfers,
                                      BLADERF_FORMAT_SC16_Q11_META,
                                      /* samples_per_buffer */ MODES_MAG_BUF_SAMPLES,
                                      /* num_transfers */ transfers,
                                      /* user_data */ NULL)) < 0) {
        fprintf(stderr, "bladerf_init_stream() failed: %s\n", bladerf_strerror(status));
        goto out;
    }

    unsigned ms_per_transfer = 1000 * MODES_MAG_BUF_SAMPLES / Modes.sample_rate;
    if ((status = bladerf_set_stream_timeout(BladeRF.device, BLADERF_MODULE_RX, ms_per_transfer * (transfers + 2))) < 0) {
        fprintf(stderr, "bladerf_set_stream_timeout() failed: %s\n", bladerf_strerror(status));
        goto out;
    }

    if ((status = bladerf_enable_module(BladeRF.device, BLADERF_MODULE_RX, true) < 0)) {
        fprintf(stderr, "bladerf_enable_module(RX, true) failed: %s\n", bladerf_strerror(status));
        goto out;
    }

    timeouts = 0; // reset to zero when we get a callback with some data
 retry:
    if ((status = bladerf_stream(stream, BLADERF_MODULE_RX)) < 0) {
        fprintf(stderr, "bladerf_stream() failed: %s\n", bladerf_strerror(status));
        if (status == BLADERF_ERR_TIMEOUT) {
            if (++timeouts < 5)
                goto retry;
            fprintf(stderr, "bladerf is wedged, giving up.\n");
        }
        goto out;
    }

 out:
    if ((status = bladerf_enable_module(BladeRF.device, BLADERF_MODULE_RX, false) < 0)) {
        fprintf(stderr, "bladerf_enable_module(RX, false) failed: %s\n", bladerf_strerror(status));
    }

    if (stream) {
        bladerf_deinit_stream(stream);
    }
}

void bladeRFClose()
{
    if (BladeRF.converter) {
        cleanup_converter(BladeRF.converter_state);
        BladeRF.converter = NULL;
        BladeRF.converter_state = NULL;
    }

    if (BladeRF.device) {
        bladerf_close(BladeRF.device);
        BladeRF.device = NULL;
    }
}
