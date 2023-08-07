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
#include <SoapySDR/Device.h>
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
  bool is_stream_opened;
  bool is_stop;
  float gain;
  float lpfbw;
  float sr;
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
//SoapySDR.dev = NULL;
  SoapySDR.sdrhw = "lime";
  SoapySDR.dev = NULL;
  SoapySDR.rxStream = NULL;
  SoapySDR.serial = NULL;
  SoapySDR.antenna = "LNAW";
  SoapySDR.length = 1024;
  SoapySDR.sr = 2e6;
  SoapySDR.lpfbw = 2400000.0;

  SoapySDR.is_stream_opened = false;
  SoapySDR.is_stop = true;
  SoapySDR.gain = 30.0;
  SoapySDR.bytes_in_sample = 2 * sizeof(int16_t);
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
    printf("--soapysdr-sr <float>\n");
    printf("--soapysdr-lpfbw <float>\n");
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
  //} else if (!strcmp(argv[j], "--limesdr-lpfbw") && more) {
  //  LimeSDR.lpfbw = atof(argv[++j]);
  } else if (!strcmp(argv[j], "--soapysdr-sr") && more) {
    SoapySDR.sr = atof(argv[++j]);
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
      SoapySDR.bytes_in_sample = 2 * sizeof(int16_t);
    }
    
    SoapySDR.dev = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    if(SoapySDR.dev == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }else{
      //apply settings
      if (SoapySDRDevice_setSampleRate(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.sr) != 0)
      {
          printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
      }
      if (SoapySDRDevice_setAntenna(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.antenna) != 0)
      {
          printf("setAntenna fail: %s\n", SoapySDRDevice_lastError());
      }
      if (SoapySDRDevice_setGain(SoapySDR.dev, SOAPY_SDR_RX, 0, SoapySDR.gain) != 0)
      {
          printf("setAntenna fail: %s\n", SoapySDRDevice_lastError());
      }
      if (SoapySDRDevice_setFrequency(SoapySDR.dev, SOAPY_SDR_RX, 0, 1090e6, NULL) != 0)
      {
          printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
      }

      //setup a stream (complex floats)
      //SoapySDRStream *rxStream;
      // according to https://github.com/myriadrf/LimeSuite/blob/master/SoapyLMS7/Streaming.cpp line 155 only CF32, CS16 and CF12 are supported
      /*
      if (SoapySDRDevice_setupStream(SoapySDR.dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, &SoapySDR.rxStream,  NULL, NULL) != 0)
      {
          printf("setupStream fail: %s\n", SoapySDRDevice_lastError());
      }
      */
    }
    

    

    return true;
}

static void soapysdrCallback(void *buf, uint32_t len, void *ctx)
{
  
  //printf("%f", buf[x]);
}

void soapysdrRun()
{
  if (!SoapySDR.dev){
    return;
  }

// --------------------------------

  //int16_t *buffer = malloc(MODES_MAG_BUF_SAMPLES * SoapySDR.bytes_in_sample);
  //complex float buffn[MODES_MAG_BUF_SAMPLES * SoapySDR.bytes_in_sample];
  //int16_t buffer_size = MODES_MAG_BUF_SAMPLES * SoapySDR.bytes_in_sample;
  
  SoapySDR.rxStream = malloc(MODES_MAG_BUF_SAMPLES * SoapySDR.bytes_in_sample);
  
  //void *buffs[] = {buffer}; //array of buffers
  //if (!buffs){
    if(!SoapySDR.rxStream){
  //if (!buffer_size){
    printf("Error allocating sample buffer !\n");
    return;
  }
  SoapySDRDevice_activateStream(SoapySDR.dev, &SoapySDR.rxStream, 0, 0, 0); //start streaming

  
  
  int flags; //flags set by receive operation
  long long timeNs; //timestamp for receive buffer

  while (!Modes.exit){
    //int sampleCnt = SoapySDRDevice_readStream(SoapySDR.dev, SoapySDR.rxStream, buffs, MODES_MAG_BUF_SAMPLES, &flags, &timeNs, 100000);

    int sampleCnt = 0;
    if (sampleCnt < 0){
      printf("Soapy stream failed !");
      break;
    }

    if (sampleCnt){
      //soapysdrCallback(buffer,  sampleCnt * SoapySDR.bytes_in_sample, NULL);
      //printf("%i\n", sampleCnt);
      for (int x = 0; x < sampleCnt; x++)
        {
            printf("nop");
        }

    }
  }
  //free (buffs);
}

void soapysdrStop()
{
}

void soapysdrClose()
{
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

