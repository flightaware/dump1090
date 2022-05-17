// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// sdr.c: generic SDR infrastructure
//
// Copyright (c) 2016-2017 Oliver Jowett <oliver@mutability.co.uk>
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

#include "sdr_ifile.h"
#ifdef ENABLE_RTLSDR
#  include "sdr_rtlsdr.h"
#endif
#ifdef ENABLE_BLADERF
#  include "sdr_bladerf.h"
#endif
#ifdef ENABLE_HACKRF
#  include "sdr_hackrf.h"
#endif
#ifdef ENABLE_LIMESDR
#  include "sdr_limesdr.h"
#endif

typedef struct {
    const char *name;
    sdr_type_t sdr_type;
    void (*initConfig)();
    void (*showHelp)();
    bool (*handleOption)(int, char**, int*);
    bool (*open)();
    void (*run)();
    void (*stop)();
    void (*close)();
    int (*getgain)();
    int (*getmaxgain)();
    double (*getgaindb)(int);
    int (*setgain)(int);
} sdr_handler;

static void noInitConfig()
{
}

static void noShowHelp()
{
}

static bool noHandleOption(int argc, char **argv, int *jptr)
{
    MODES_NOTUSED(argc);
    MODES_NOTUSED(argv);
    MODES_NOTUSED(jptr);

    return false;
}

static bool noOpen()
{
    fprintf(stderr, "Net-only mode, no SDR device or file open.\n");
    return true;
}

static void noRun()
{
}

static void noStop()
{
}

static void noClose()
{
}

static int noGetGain()
{
    return -1;
}

static int noGetMaxGain()
{
    return -1;
}

static double noGetGainDb(int step)
{
    MODES_NOTUSED(step);
    return 0.0;
}

static int noSetGain(int step)
{
    MODES_NOTUSED(step);
    return 0;
}

static bool unsupportedOpen()
{
    fprintf(stderr, "Support for this SDR type was not enabled in this build.\n");
    return false;
}

static sdr_handler sdr_handlers[] = {
#ifdef ENABLE_RTLSDR
    { "rtlsdr", SDR_RTLSDR, rtlsdrInitConfig, rtlsdrShowHelp, rtlsdrHandleOption, rtlsdrOpen, rtlsdrRun, rtlsdrStop, rtlsdrClose, rtlsdrGetGain, rtlsdrGetMaxGain, rtlsdrGetGainDb, rtlsdrSetGain },
#endif

#ifdef ENABLE_BLADERF
    { "bladerf", SDR_BLADERF, bladeRFInitConfig, bladeRFShowHelp, bladeRFHandleOption, bladeRFOpen, bladeRFRun, noStop, bladeRFClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain },
#endif

#ifdef ENABLE_HACKRF
    { "hackrf", SDR_HACKRF, hackRFInitConfig, hackRFShowHelp, hackRFHandleOption, hackRFOpen, hackRFRun, noStop, hackRFClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain },
#endif
#ifdef ENABLE_LIMESDR
    { "limesdr", SDR_LIMESDR, limesdrInitConfig, limesdrShowHelp, limesdrHandleOption, limesdrOpen, limesdrRun, noStop, limesdrClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain },
#endif

    { "none", SDR_NONE, noInitConfig, noShowHelp, noHandleOption, noOpen, noRun, noStop, noClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain },
    { "ifile", SDR_IFILE, ifileInitConfig, ifileShowHelp, ifileHandleOption, ifileOpen, ifileRun, noStop, ifileClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain },

    { NULL, SDR_NONE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } /* must come last */
};

void sdrInitConfig()
{
    // Default SDR is the first type available in the handlers array.
    Modes.sdr_type = sdr_handlers[0].sdr_type;

    for (int i = 0; sdr_handlers[i].name; ++i) {
        sdr_handlers[i].initConfig();
    }
}

void sdrShowHelp()
{
    printf("--device-type <type>     Select SDR type (default: %s)\n", sdr_handlers[0].name);
    printf("\n");

    for (int i = 0; sdr_handlers[i].name; ++i) {
        sdr_handlers[i].showHelp();
    }
}

bool sdrHandleOption(int argc, char **argv, int *jptr)
{
    int j = *jptr;
    if (!strcmp(argv[j], "--device-type")) {
        if ((j+1) < argc) {
            ++j;
            for (int i = 0; sdr_handlers[i].name; ++i) {
                if (!strcasecmp(sdr_handlers[i].name, argv[j])) {
                    Modes.sdr_type = sdr_handlers[i].sdr_type;
                    *jptr = j;
                    return true;
                }
            }
            fprintf(stderr, "SDR type '%s' not recognized. ", argv[j]);
        }

        fprintf(stderr, "Supported SDR types:\n");
        for (int i = 0; sdr_handlers[i].name; ++i) {
            fprintf(stderr, "  %s\n", sdr_handlers[i].name);
        }

        return false;
    }

    for (int i = 0; sdr_handlers[i].name; ++i) {
        if (sdr_handlers[i].handleOption(argc, argv, jptr))
            return true;
    }

    return false;
}

static sdr_handler *current_handler()
{
    static sdr_handler unsupported_handler = { "unsupported", SDR_NONE, noInitConfig, noShowHelp, noHandleOption, unsupportedOpen, noRun, noStop, noClose, noGetGain, noGetMaxGain, noGetGainDb, noSetGain };

    for (int i = 0; sdr_handlers[i].name; ++i) {
        if (Modes.sdr_type == sdr_handlers[i].sdr_type) {
            return &sdr_handlers[i];
        }
    }

    return &unsupported_handler;
}

bool sdrOpen()
{
    pthread_mutex_init(&Modes.reader_cpu_mutex, NULL);
    return current_handler()->open();
}

void sdrRun()
{
    set_thread_name("dump1090-sdr");

    pthread_mutex_lock(&Modes.reader_cpu_mutex);
    Modes.reader_cpu_accumulator.tv_sec = 0;
    Modes.reader_cpu_accumulator.tv_nsec = 0;
    start_cpu_timing(&Modes.reader_cpu_start);
    pthread_mutex_unlock(&Modes.reader_cpu_mutex);

    current_handler()->run();

    pthread_mutex_lock(&Modes.reader_cpu_mutex);
    end_cpu_timing(&Modes.reader_cpu_start, &Modes.reader_cpu_accumulator);
    pthread_mutex_unlock(&Modes.reader_cpu_mutex);
}

void sdrStop()
{
    current_handler()->stop();
}

void sdrClose()
{
    pthread_mutex_destroy(&Modes.reader_cpu_mutex);
    current_handler()->close();
}

void sdrMonitor()
{
    pthread_mutex_lock(&Modes.reader_cpu_mutex);
    update_cpu_timing(&Modes.reader_cpu_start, &Modes.reader_cpu_accumulator);
    pthread_mutex_unlock(&Modes.reader_cpu_mutex);
}

void sdrUpdateCPUTime(struct timespec *addTo)
{
    pthread_mutex_lock(&Modes.reader_cpu_mutex);
    add_timespecs(&Modes.reader_cpu_accumulator, addTo, addTo);
    Modes.reader_cpu_accumulator.tv_sec = 0;
    Modes.reader_cpu_accumulator.tv_nsec = 0;
    pthread_mutex_unlock(&Modes.reader_cpu_mutex);
}

int sdrGetGain()
{
    return current_handler()->getgain();
}

int sdrGetMaxGain()
{
    return current_handler()->getmaxgain();
}

double sdrGetGainDb(int step)
{
    return current_handler()->getgaindb(step);
}

int sdrSetGain(int step)
{
    return current_handler()->setgain(step);
}


