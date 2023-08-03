// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr_bladerf.h: UHD support (header)
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
#include "sdr_uhd.h"

#include <inttypes.h>

// complex int16
#define SAMPLE_SIZE 4

static struct {
    uhd_usrp_handle *usrp;
    uhd_rx_metadata_handle *md;
    uhd_rx_streamer_handle *rx_streamer;
    char *device_args;
    size_t channel;
    char *sample_data;

    iq_convert_fn converter;
    struct converter_state *converter_state;
} uhd;

void uhdClose() {
    if (uhd.rx_streamer) {
	uhd_rx_streamer_free(uhd.rx_streamer);
	free(uhd.rx_streamer);
	uhd.rx_streamer = NULL;
    }
    if (uhd.md) {
	uhd_rx_metadata_free(uhd.md);
	free(uhd.md);
	uhd.md = NULL;
    }
    if (uhd.usrp) {
	uhd_usrp_free(uhd.usrp);
	free(uhd.usrp);
	uhd.usrp = NULL;
    }
    if (uhd.sample_data) {
	free(uhd.sample_data);
	uhd.sample_data = NULL;
    }
    if (uhd.device_args) {
	free(uhd.device_args);
	uhd.device_args = NULL;
    }
}

void uhdInitConfig() {
    uhd.usrp = NULL;
    uhd.md = NULL;
    uhd.rx_streamer = NULL;
    uhd.device_args = NULL;
    uhd.sample_data = NULL;
    uhd.channel = 0;
}

void uhdShowHelp() {
    printf("      uhd-specific options (use with --device-type uhd)\n");
    printf("\n");
    printf("--device-args <args>         UHD device args\n");
    printf("\n");
}

bool uhdHandleOption(int argc, char **argv, int *jptr) {
    int j = *jptr;
    bool more = (j+1 < argc);
    if (!strcmp(argv[j], "--uhd-device-args") && more) {
	uhd.device_args = strdup(argv[++j]);
    } else {
	return false;
    }

    *jptr = j;
    return true;
}

bool uhdOpen() {
    uhd_tune_request_t tune_request = {
	.target_freq = Modes.freq,
	.rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
	.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
    };
    uhd_tune_result_t tune_result;

    if (!uhd.device_args) {
	uhd.device_args = strdup("");
    }

    int uhd_error;

    uhd.usrp = malloc(sizeof(uhd_usrp_handle));
    uhd_error = uhd_usrp_make(uhd.usrp, uhd.device_args);
    if (uhd_error) {
	fprintf(stderr, "uhd_usrp_make() failed: %x", uhd_error);
	goto failed;
    }

    uhd.md = malloc(sizeof(uhd_rx_metadata_handle));
    uhd_error = uhd_rx_metadata_make(uhd.md);
    if (uhd_error) {
	fprintf(stderr, "uhd_rx_metadata_make() failed: %x", uhd_error);
	goto failed;
    }

    uhd.rx_streamer = malloc(sizeof(uhd_rx_streamer_handle));
    uhd_error = uhd_rx_streamer_make(uhd.rx_streamer);
    if (uhd_error) {
	fprintf(stderr, "uhd_rx_streamer_make() failed: %x", uhd_error);
	goto failed;
    }

    fprintf(stderr, "setting bw, rx rate to %fMsps\n", Modes.sample_rate / 1e6);
    uhd_error = uhd_usrp_set_rx_rate(*uhd.usrp, Modes.sample_rate, uhd.channel);
    if (uhd_error) {
	fprintf(stderr, "uhd_usrp_set_rx_rate() failed: %x", uhd_error);
	goto failed;
    }

    uhd_error = uhd_usrp_set_rx_bandwidth(*uhd.usrp, Modes.sample_rate, uhd.channel);
    if (uhd_error) {
	fprintf(stderr, "uhd_usrp_set_rx_bandwidth() failed: %x", uhd_error);
	goto failed;
    }

    if (Modes.gain != MODES_DEFAULT_GAIN) {
	fprintf(stderr, "setting rx gain to %f\n", Modes.gain);
	uhd_error = uhd_usrp_set_rx_gain(*uhd.usrp, Modes.gain, uhd.channel, "");
	if (!uhd_error) {
	    fprintf(stderr, "uhd_usrp_set_rx_gain() failed: %x", uhd_error);
	    goto failed;
	}
    }

    fprintf(stderr, "setting rx freq to %fMHz\n", Modes.freq / 1e6);
    uhd_error = uhd_usrp_set_rx_freq(*uhd.usrp, &tune_request, uhd.channel, &tune_result);
    if (uhd_error) {
	fprintf(stderr, "uhd_usrp_set_rx_freq() failed: %x", uhd_error);
	goto failed;
    }

    uhd.converter = init_converter(
	INPUT_SC16Q11, Modes.sample_rate, Modes.dc_filter, &uhd.converter_state);

    return true;

    failed:
	uhdClose();
	return false;
}

void uhdRun() {
    uhd_stream_args_t stream_args = {
	.cpu_format = "sc16",
	.otw_format = "sc16",
	.args = "",
	.channel_list = &uhd.channel,
	.n_channels = 1
    };

    uhd_stream_cmd_t stream_cmd = {
	.stream_mode = UHD_STREAM_MODE_START_CONTINUOUS,
	.stream_now = true
    };

    uhd_stream_cmd_t stop_stream_cmd = {
	.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS,
    };

    size_t num_rx_samps = 0;
    struct mag_buf *outbuf = NULL;
    int uhd_error;

    uhd_error = uhd_usrp_get_rx_stream(*uhd.usrp, &stream_args, *uhd.rx_streamer);
    if (uhd_error) {
	fprintf(stderr, "uhd_usrp_get_rx_stream() failed: %x\n", uhd_error);
	return;
    }

    fprintf(stderr, "streaming...\n");
    uhd.sample_data = malloc(MODES_MAG_BUF_SAMPLES * SAMPLE_SIZE);
    int64_t full_secs;
    double frac_secs;
    uhd_rx_metadata_time_spec(*uhd.md, &full_secs, &frac_secs);
    double first_recv_time = full_secs + frac_secs;

    uhd_error = uhd_rx_streamer_issue_stream_cmd(*uhd.rx_streamer, &stream_cmd);
    if (uhd_error) {
	fprintf(stderr, "uhd_rx_streamer_issue_stream_cmd() failed: %x\n", uhd_error);
	return;
    }

    for (;;) {
	outbuf = fifo_acquire(0);
	if (!outbuf) {
	    if (Modes.exit) {
		break;
	    }
	    fprintf(stderr, "could not get outbuf\n");
	    continue;
	}

	outbuf->sysTimestamp = mstime();
	uhd_rx_streamer_recv(*uhd.rx_streamer, (void **) &uhd.sample_data, MODES_MAG_BUF_SAMPLES, uhd.md, 3.0, false, &num_rx_samps);
	uhd_rx_metadata_error_code_t error_code;
	uhd_rx_metadata_error_code(*uhd.md, &error_code);
	if (error_code != UHD_RX_METADATA_ERROR_CODE_NONE) {
	    fprintf(stderr, "error code while streaming: %x\n", error_code);
	    break;
	}
	uhd_rx_metadata_time_spec(*uhd.md, &full_secs, &frac_secs);
	double recv_time = full_secs + frac_secs;
	outbuf->sampleTimestamp = (recv_time - first_recv_time) * 12e6 / Modes.sample_rate;

	uhd.converter(uhd.sample_data, &outbuf->data[outbuf->overlap], num_rx_samps, uhd.converter_state, &outbuf->mean_level, &outbuf->mean_power);
	outbuf->validLength = outbuf->overlap + num_rx_samps;
	outbuf->flags = 0;

	fifo_enqueue(outbuf);
    }

    uhd_rx_streamer_issue_stream_cmd(*uhd.rx_streamer, &stop_stream_cmd);
}
