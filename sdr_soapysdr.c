// Part of dump1090-soapysdr, a Mode S message decoder for SOAPYSDR devices.
//
// sdr_soapysdr.c: soapysdr supported SDR device support (header)
//
// Copyright (c) 2022 Ralf Heckhausen <ralf@avionix.eu>
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
// ./dump1090 --device-type soapysdr --soapysdr-serial 1D58983A574370 --soapysdr-rx-antenna LNAW --soapysdr-gain 2^[[H0.0 --soapysdr-sr 2.4e6
#include "dump1090.h"
#include "sdr_soapysdr.h"
//#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Logger.h>
#include <complex.h>
#include <math.h>

#if defined(__arm__) || defined(__aarch64__)
// Assume we need to use a bounce buffer to avoid performance problems on Pis running kernel 5.x and using zerocopy
#  define USE_BOUNCE_BUFFER
#endif

static struct {
  char* sdrhw;
  SoapySDRDevice *dev;
  SoapySDRStream *rxStream;
  char* serial;
  char* antenna;

  float gain;
  float lpfbw;
  float bw;
  iq_convert_fn converter;
  struct converter_state *converter_state;
  size_t length;
  int bytes_in_sample;
} SoapySDR;


//
// =============================== RTLSDR handling ==========================
//

void soapysdrInitConfig()
{
  SoapySDR.dev = NULL;
  SoapySDR.sdrhw = "lime";
  SoapySDR.dev = NULL;
  SoapySDR.rxStream = NULL;
  SoapySDR.serial = NULL;
  SoapySDR.antenna = "LNAW";
  SoapySDR.length = 1024;
  SoapySDR.bw = 2400000.0;
  SoapySDR.lpfbw = 2400000.0;

  SoapySDR.gain = 30.0;
  SoapySDR.bytes_in_sample = 2 * sizeof(int16_t);
  SoapySDR.converter = NULL;
  SoapySDR.converter_state = NULL;

  SoapySDR_setLogLevel(SOAPY_SDR_INFO);
  //SoapySDR_setLogLevel(SOAPY_SDR_NOTICE);
  //SoapySDR_setLogLevel(SOAPY_SDR_SSI);
  //SoapySDR_setLogLevel(SOAPY_SDR_DEBUG);
}

void soapysdrShowHelp()
{
    printf("      soapysdrShowHelp() in sdr_soapysdr.c\n");
    printf("      soapysdr-specific options (use with --device-type soapysdr)\n");
    printf("\n");
    printf("--soapysdr-driver <string>\n");
    printf("--soapysdr-serial <string>\n");
    printf("--soapysdr-rx-antenna <string>\n");
    printf("--soapysdr-gain <float>\n");
    printf("--soapysdr-bw <float>\n");
    printf("--soapysdr-lpfbw <float>\n");
    printf("--soapysdr-dev-info\n");
    printf("\n");
}

bool soapysdrHandleOption(int argc, char **argv, int *jptr)
{
  int j = *jptr;
  bool more = (j + 1 < argc);

  if (!strcmp(argv[j], "--soapysdr-driver") && more) {
      SoapySDR.sdrhw = strdup(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-serial") && more) {
    SoapySDR.serial = strdup(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-rx-antenna") && more) {
    SoapySDR.antenna = strdup(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-gain") && more) {
    SoapySDR.gain = atoi(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-bw") && more) {
    SoapySDR.bw = atof(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-lpfbw") && more) {
    SoapySDR.lpfbw = atof(argv[++j]);
  } else {
    return false;
  }
  *jptr = j;

  return true;
}

bool soapysdrOpen(void)
{
    //enumerate devices
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &SoapySDR.length);
    for (size_t i = 0; i < SoapySDR.length; i++)
    {
        printf("Found device #%d: ", (int)i);
        for (size_t j = 0; j < results[i].size; j++)
        {
            printf("%s=%s, ", results[i].keys[j], results[i].vals[j]);
        }
        printf("\n");
    }
    
    SoapySDRKwargsList_clear(results, SoapySDR.length);

    //create device instance
    //args can be user defined or from the enumeration result
    
    SoapySDRKwargs args = {};
    if (!strcmp(SoapySDR.sdrhw, "lime"))
    {
      SoapySDRKwargs_set(&args, "driver", "lime");
      //SoapySDRKwargs_set(&args, "soapy", "2");
    }

    SoapySDR.dev = SoapySDRDevice_make(&args);
    
    SoapySDRKwargs_clear(&args);

    if(SoapySDR.dev == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    } else {

        //apply settings
        
        getDeviceInfo(SoapySDR.dev);
        if (SoapySDRDevice_setFrequency(SoapySDR.dev, SOAPY_SDR_RX, 0, 1090e6, NULL) != 0)
        {
            printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
        } else {
            printf("soapy: frequency is %.1f MHz\n", SoapySDRDevice_getFrequency(SoapySDR.dev, SOAPY_SDR_RX, 0) / 1e6);
        }

        if (SoapySDRDevice_setSampleRate(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.bw) != 0)
        {
            printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
        } else {
            printf("soapy: sample rate is %.1f MHz\n", SoapySDRDevice_getSampleRate(SoapySDR.dev, SOAPY_SDR_RX, 0) / 1e6);
        }

        if (SoapySDRDevice_setBandwidth(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.lpfbw) != 0)
        {
            printf("setBandwidth fail: %s\n", SoapySDRDevice_lastError());
        } else {
            printf("soapy: lp bandwidth is %.1f MHz\n", SoapySDRDevice_getBandwidth(SoapySDR.dev, SOAPY_SDR_RX, 0) / 1e6);
        }

        if (SoapySDRDevice_setGain(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.gain) != 0)
        {
            printf("setGain fail: %s\n", SoapySDRDevice_lastError());
        } else {
            printf("soapy: gain is %.1f dB\n", SoapySDRDevice_getGain(SoapySDR.dev, SOAPY_SDR_RX, 0));
        }

        if (SoapySDRDevice_setAntenna(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.antenna) != 0)
        {
            printf("setAntenna fail: %s\n", SoapySDRDevice_lastError());
        } else {
            printf("soapy: antenna is %s\n", SoapySDRDevice_getAntenna(SoapySDR.dev, SOAPY_SDR_RX, 0));
        }

        printf("--soapy: gain mode is %d\n", SoapySDRDevice_getGainMode(SoapySDR.dev, SOAPY_SDR_RX, 0));

        
        size_t channels[1] = { 0 };
        
        SoapySDR.rxStream = SoapySDRDevice_setupStream(SoapySDR.dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, 1, NULL);
        if (!SoapySDR.rxStream) {
            printf("soapy: setupStream failed: %s\n", SoapySDRDevice_lastError());
            goto error;
        } else {
            printf("soapy: streamPointer: %p\n", &SoapySDR.rxStream);
            printf("soapy: max stream MTU is %i\n", (uint16_t)SoapySDRDevice_getStreamMTU(SoapySDR.dev, SoapySDR.rxStream));
        }
        SoapySDR.converter = init_converter(INPUT_SC16, Modes.sample_rate, Modes.dc_filter, &SoapySDR.converter_state);

        if (!SoapySDR.converter) {
            printf("soapy: can't initialize sample converter\n");
            goto error;
        }
    return true;   
    }

    error:
        printf("ERROR!!!");
        if (SoapySDR.dev != NULL) {
            SoapySDRDevice_unmake(SoapySDR.dev);
            SoapySDR.dev = NULL;
        }

        return false;
    
   
}

static void soapysdrCallback(void *buf, uint32_t len, void *ctx)
{
    static int dropped = 0;
    static uint64_t sampleCounter = 0;

    MODES_NOTUSED(ctx);

    sdrMonitor();

    unsigned samples_read = len / SoapySDR.bytes_in_sample; // Drops any trailing odd sample, not much else we can do there

    if (!samples_read)
        return; // that wasn't useful

    struct mag_buf *outbuf = fifo_acquire(0 /* don't wait */);

    if (!outbuf) {
        // FIFO is full. Drop this block.
        dropped += samples_read;
        sampleCounter += samples_read;
        printf("FIFO is full. Drop this block.\n");
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

    SoapySDR.converter(buf, &outbuf->data[outbuf->overlap], to_convert, SoapySDR.converter_state, &outbuf->mean_level, &outbuf->mean_power);
    
    outbuf->validLength = outbuf->overlap + to_convert;
    // Push to the demodulation thread
    fifo_enqueue(outbuf);

    
}

void soapysdrRun()
{
    if (!SoapySDR.dev){
        return;
    }

    if (SoapySDRDevice_activateStream(SoapySDR.dev, SoapySDR.rxStream, 0, 0, 0) != 0) {
        printf("soapy: activateStream failed: %s\n", SoapySDRDevice_lastError());
        return;
    }

    int16_t* buffer = malloc(MODES_MAG_BUF_SAMPLES * SoapySDR.bytes_in_sample);  //131072
    void *buffers[] = {buffer};

    while (!Modes.exit) {
        int flags; //flags set by receive operation
        long long timeNs; //timestamp for receive buffer
        int sampleCnt = SoapySDRDevice_readStream(SoapySDR.dev, SoapySDR.rxStream, buffers, MODES_MAG_BUF_SAMPLES, &flags, &timeNs, 1000000);
        if (sampleCnt < 0) {
            printf("Stream receive error ....\n");
            break;
        }

        if (sampleCnt) {
            soapysdrCallback(buffer, sampleCnt * SoapySDR.bytes_in_sample, NULL);
        }

    }
    free(buffer);
}

void soapysdrStop()
{
    printf("Closing stream... ");
    if (SoapySDR.rxStream) {
        SoapySDRDevice_closeStream(SoapySDR.dev, SoapySDR.rxStream);
        SoapySDR.rxStream = NULL;
    }
    printf("closed !\n");

    printf("Closing converter... ");
    if (SoapySDR.converter) {
        cleanup_converter(SoapySDR.converter_state);
        SoapySDR.converter = NULL;
        SoapySDR.converter_state = NULL;
    }
    printf("closed !\n");
}

void soapysdrClose()
{
    printf("Closing device... ");
    if (SoapySDR.dev) {
        SoapySDRDevice_unmake(SoapySDR.dev);
        SoapySDR.dev = NULL;
    }
    printf("closed !\n");
}

int soapysdrGetGain()
{
  return 0;
}

int soapysdrGetMaxGain()
{
  return 0;
}

double soapysdrGetGainDb(int step)
{
  return 0.0;
}

int soapysdrSetGain(int step)
{
  return 0;
}

void getDeviceInfo(struct SoapySDRDevice *sdr)
{
    //query device info
    size_t length;
    printf("\n");
    char** names = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx antennas: ");
    for (size_t i = 0; i < length; i++) printf("%s, ", names[i]);
    printf("\n");
    SoapySDRStrings_clear(&names,length);
    
    names = SoapySDRDevice_listGains(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx gains: ");
    for (size_t i = 0; i < length; i++) printf("%s, ", names[i]);
    printf("\n");
    SoapySDRStrings_clear(&names, length);
    
    SoapySDRRange *ranges = SoapySDRDevice_getFrequencyRange(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx freq ranges: ");
    for(size_t i = 0; i < length; i++) printf("[%g Hz -> %g Hz], ", ranges[i].minimum, ranges[i].maximum);
    free(ranges);
    printf("\n");
    
    size_t num_channels = SoapySDRDevice_getNumChannels(sdr, SOAPY_SDR_RX);
    printf("Number of channels available: %i", (int)num_channels);
    printf("\n");

    return;
}