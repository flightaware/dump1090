// Part of dump1090, a Mode S message decoder
//
// sdr_ad9361.c: AD9361 One support
//
// Copyright (c) 2019 FlightAware LLC
// Copyright (c) 2025 Deepak Shandilya (deepakshekar@gmail.com)
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
#include "sdr_ad9361.h"

#include <iio.h>
#include <ad9361.h>
#include <inttypes.h>

#define AD9361_DEFAULT_RATE         2400000
#define AD9361_DEFAULT_FREQ         1090000000
#define AD9361_DEFAULT_WIDTH        1000
#define AD9361_DEFAULT_HEIGHT       700
#define AD9361_ASYNC_BUF_NUMBER     12
#define AD9361_DATA_LEN             (32*16384)  /* 512k */
#define AD9361_AUTO_GAIN            -100    /* Use automatic gain. */
#define AD9361_MAX_GAIN             70  /* Use max available gain. */


static struct
{
    struct iio_device *device;
    struct iio_context *ctx;
    int dev_index;
    uint64_t freq;
    int gain;
    int enable_agc;
    uint32_t samplerate;
    struct iio_channel *rx0_i;
    struct iio_channel *rx0_q;
    struct iio_buffer *rxbuf;

    iq_convert_fn converter;
    struct converter_state *converter_state;
} AD9361;

void ad9361InitConfig()
{
    AD9361.device = NULL;
    AD9361.ctx = NULL;
    AD9361.freq = Modes.freq;
    AD9361.enable_agc = 1;
    AD9361.gain = AD9361_AUTO_GAIN;
    AD9361.samplerate = AD9361_DEFAULT_RATE;
    AD9361.converter = NULL;
    AD9361.converter_state = NULL;
}

bool ad9361HandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    bool more = (j + 1 < argc);

    if (!strcmp (argv[j], "--host") && more) {
        Modes.dev_name = strdup (argv[++j]);
    }
    else if (!strcmp (argv[j], "--samplerate") && more) {
        AD9361.samplerate = atoi (argv[++j]);
    }
    else if (!strcmp (argv[j], "--set-gain") && more) {
        AD9361.gain = atoi (argv[++j]);
    }
    else if (!strcmp (argv[j], "--9361freq") && more) {
        AD9361.freq = atoi (argv[++j]);
    }
    else {
        return false;
    }

    *jptr = j;
    return true;
}

void ad9361ShowHelp()
{
    printf("      AD9361-specific options (use with --device-type ad9361)\n");
    printf("\n");
    printf("--host                   device string : e.g ip:pluto.local or ip:<addr>\n");
    printf("--samplerate             set sample rate (default: 2400000Hz)\n");
    printf("--set-gain               set gain : -100 for auto or upto 70 for manual gain\n");
    printf("--9361freq               set frequency (default : 1090000000 Hz)\n");
    printf("\n");
}

static void show_config()
{
    fprintf (stderr, "* Frequency : %" PRIu64 "\n", AD9361.freq);
    fprintf (stderr, "* Sample Rate : %u \n", AD9361.samplerate);
    fprintf (stderr, "* Gain (-100 = AGC) : %d \n", AD9361.gain);
    if (NULL != Modes.dev_name) {
        fprintf (stderr, "* Device : %s\n", Modes.dev_name);
    }
    else
      fprintf (stderr, "* Device : Local\n");
}

bool ad9361Open()
{
    if (AD9361.device) {
        return true;
    }

    int device_count;

    AD9361.ctx = NULL;
    fprintf (stderr, "* Acquiring IIO context\n");
    if (NULL != Modes.dev_name) {
        AD9361.ctx = iio_create_network_context (Modes.dev_name);
    }
    if (AD9361.ctx == NULL) {
        AD9361.ctx = iio_create_default_context ();
    }
    if (AD9361.ctx == NULL) {
      AD9361.ctx = iio_create_network_context ("pluto.local");
    }
    if (NULL == AD9361.ctx) return false;
    
    device_count = iio_context_get_devices_count (AD9361.ctx);
    
    if (!device_count) {
        fprintf (stderr, "No supported AD9361 devices found.\n");
        return false;
    }

    fprintf (stderr, "Found %d device(s):\n", device_count);

    fprintf (stderr, "* Acquiring AD9361 streaming devices\n");

    AD9361.device = iio_context_find_device (AD9361.ctx, "cf-ad9361-lpc");

    if (AD9361.device == NULL) {
        fprintf (stderr, "Error opening the PLUTOSDR device: %s\n", strerror (errno));
        return false;
    }

    fprintf (stderr, "* Acquiring AD9361 phy channel 0\n");

    struct iio_channel *phy_chn =
    iio_device_find_channel (iio_context_find_device
                 (AD9361.ctx, "ad9361-phy"), "voltage0", false);

    iio_channel_attr_write (phy_chn, "rf_port_select", "A_BALANCED");
    iio_channel_attr_write_longlong (phy_chn, "rf_bandwidth",
                   AD9361.samplerate);
    iio_channel_attr_write_longlong (phy_chn, "sampling_frequency",
                   AD9361.samplerate);

    struct iio_channel *lo_chn =
      iio_device_find_channel (iio_context_find_device
                 (AD9361.ctx, "ad9361-phy"), "altvoltage0", true);
    
    iio_channel_attr_write_longlong (lo_chn, "frequency", AD9361.freq);

    fprintf (stderr, "* Initializing AD9361 IIO streaming channels\n");

    AD9361.rx0_i = iio_device_find_channel (AD9361.device, "voltage0", false);

    if (!AD9361.rx0_i) AD9361.rx0_i = iio_device_find_channel (AD9361.device, "altvoltage0", false);

    AD9361.rx0_q = iio_device_find_channel (AD9361.device, "voltage1", false);
    
    if (!AD9361.rx0_q) AD9361.rx0_q = iio_device_find_channel (AD9361.device, "altvoltage1", false);

    ad9361_set_bb_rate (iio_context_find_device (AD9361.ctx, "ad9361-phy"), AD9361.samplerate);

    fprintf (stderr, "* Enabling IIO streaming channels\n");

    iio_channel_enable (AD9361.rx0_i);
    iio_channel_enable (AD9361.rx0_q);

    fprintf (stderr, "* Creating non-cyclic IIO buffers \n");

    AD9361.rxbuf =
      iio_device_create_buffer (AD9361.device, AD9361_DATA_LEN / 4, false);

    if (!AD9361.rxbuf) {
      fprintf (stderr, "Could not create RX buffer");
      return false;
    }

    if (AD9361.gain == AD9361_AUTO_GAIN) {
      iio_channel_attr_write (phy_chn, "gain_control_mode", "slow_attack");
      fprintf (stderr, "* Using AGC\r\n");
    }
    else {
      if (AD9361.gain > AD9361_MAX_GAIN) AD9361.gain = AD9361_MAX_GAIN;
      iio_channel_attr_write (phy_chn, "gain_control_mode", "manual");
      iio_channel_attr_write_longlong (phy_chn, "hardwaregain", AD9361.gain);
    }

    show_config();

    AD9361.converter = init_converter (INPUT_SC16,
                       Modes.sample_rate,
                       Modes.dc_filter,
                       &AD9361.converter_state);

    if (!AD9361.converter) {
      fprintf (stderr, "AD9361: can't initialize sample converter\n");
      return false;
    }

    return true;
}

void ad9361Run()
{
    void *p_dat;
    static unsigned dropped = 0;
    static unsigned sampleCounter = 0;
    size_t sample_size = iio_device_get_sample_size (AD9361.device);

    if (!AD9361.device) {
      return;
    }

    while (!Modes.exit) {
      ssize_t bytes_read = iio_buffer_refill (AD9361.rxbuf);
      p_dat = iio_buffer_first (AD9361.rxbuf, AD9361.rx0_i);
      int samples_read = bytes_read / sample_size;
      struct mag_buf *outbuf = NULL;
      outbuf = fifo_acquire ( /* don't wait */ 0);

      if (!outbuf) {
        // FIFO is full. Drop this block.
        fprintf (stderr, "FIFO is full \r\n");
        dropped += samples_read;
        sampleCounter += samples_read;
        return;
      }

      outbuf->flags = 0;
      if (dropped) {
        // We previously dropped some samples due to no buffers being available
        fprintf (stderr, "Was previously dropped \r\n");
        outbuf->flags |= MAGBUF_DISCONTINUOUS;
        outbuf->dropped = dropped;
      }

      dropped = 0;
      
      // Compute the sample timestamp and system timestamp for the start of the block
      outbuf->sampleTimestamp = sampleCounter * 12e6 / Modes.sample_rate;
      sampleCounter += samples_read;

      // Get the approx system time for the start of this block
      uint64_t block_duration = 1e3 * samples_read / Modes.sample_rate;
      outbuf->sysTimestamp = mstime () - block_duration;

      // Convert the new data
      unsigned int to_convert = samples_read;
      if (to_convert + outbuf->overlap > outbuf->totalLength) {
        // how did that happen?
        fprintf (stderr, "Overlap overflow \r\n");
        to_convert = outbuf->totalLength - outbuf->overlap;
        dropped = samples_read - to_convert;
      }

      AD9361.converter (p_dat, &outbuf->data[outbuf->overlap], to_convert,
            AD9361.converter_state, &outbuf->mean_level,
            &outbuf->mean_power);
      
      outbuf->validLength = outbuf->overlap + to_convert;

      // Push to the demodulation thread
      fifo_enqueue (outbuf);
    }
}

void ad9361Close()
{
    if (AD9361.device) {
        fprintf (stderr, "* Destroying buffers\n");
      
        if (AD9361.rxbuf) {
           iio_buffer_destroy (AD9361.rxbuf);
        }

        fprintf (stderr, "* Disabling streaming channels\n");
        
        if (AD9361.rx0_i) {
           iio_channel_disable (AD9361.rx0_i);
        }
      
        if (AD9361.rx0_q) {
           iio_channel_disable (AD9361.rx0_q);
        }
        
        fprintf (stderr, "* Destroying context\n");
      
        if (AD9361.ctx) {
           iio_context_destroy (AD9361.ctx);
        }

        AD9361.device = NULL;
    }
}
