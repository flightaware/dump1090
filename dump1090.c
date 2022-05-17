// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// dump1090.c: main program & miscellany
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
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

#include <stdarg.h>

//
// ============================= Utility functions ==========================
//

static void log_with_timestamp(const char *format, ...) __attribute__((format (printf, 1, 2) ));

static void log_with_timestamp(const char *format, ...)
{
    char timebuf[128];
    char msg[1024];
    time_t now;
    struct tm local;
    va_list ap;

    now = time(NULL);
    localtime_r(&now, &local);
    strftime(timebuf, 128, "%c %Z", &local);
    timebuf[127] = 0;

    va_start(ap, format);
    vsnprintf(msg, 1024, format, ap);
    va_end(ap);
    msg[1023] = 0;

    fprintf(stderr, "%s  %s\n", timebuf, msg);
}

static void sigintHandler(int dummy) {
    MODES_NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
    log_with_timestamp("Caught SIGINT, shutting down..\n");
}

static void sigtermHandler(int dummy) {
    MODES_NOTUSED(dummy);
    signal(SIGTERM, SIG_DFL); // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
    log_with_timestamp("Caught SIGTERM, shutting down..\n");
}

void receiverPositionChanged(float lat, float lon, float alt)
{
    log_with_timestamp("Autodetected receiver location: %.5f, %.5f at %.0fm AMSL", lat, lon, alt);
    writeJsonToFile("receiver.json", generateReceiverJson); // location changed
}


//
// =============================== Initialization ===========================
//
void modesInitConfig(void) {
    // Default everything to zero/NULL
    memset(&Modes, 0, sizeof(Modes));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.gain                    = MODES_MAX_GAIN;
    Modes.freq                    = MODES_DEFAULT_FREQ;
    Modes.check_crc               = 1;
    Modes.net_heartbeat_interval  = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.net_input_raw_ports     = strdup("30001");
    Modes.net_output_raw_ports    = strdup("30002");
    Modes.net_output_sbs_ports    = strdup("30003");
    Modes.net_input_beast_ports   = strdup("30004,30104");
    Modes.net_output_beast_ports  = strdup("30005");
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.json_interval           = 1000;
    Modes.json_location_accuracy  = 1;
    Modes.maxRange                = 1852 * 300; // 300NM default max range
    Modes.mode_ac_auto            = 1;

    sdrInitConfig();
}
//
//=========================================================================
//
void modesInit(void) {
    int i;

    pthread_mutex_init(&Modes.data_mutex,NULL);
    pthread_cond_init(&Modes.data_cond,NULL);

    Modes.sample_rate = 2400000.0;

    // Allocate the various buffers used by Modes
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;

    if ( ((Modes.log10lut   = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256)                                 ) == NULL) )
    {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }

    for (i = 0; i < MODES_MAG_BUFFERS; ++i) {
        if ( (Modes.mag_buffers[i].data = calloc(MODES_MAG_BUF_SAMPLES+Modes.trailing_samples, sizeof(uint16_t))) == NULL ) {
            fprintf(stderr, "Out of memory allocating magnitude buffer.\n");
            exit(1);
        }

        Modes.mag_buffers[i].length = 0;
        Modes.mag_buffers[i].dropped = 0;
        Modes.mag_buffers[i].sampleTimestamp = 0;
    }

    // Validate the users Lat/Lon home location inputs
    if ( (Modes.fUserLat >   90.0)  // Latitude must be -90 to +90
      || (Modes.fUserLat <  -90.0)  // and
      || (Modes.fUserLon >  360.0)  // Longitude must be -180 to +360
      || (Modes.fUserLon < -180.0) ) {
        Modes.fUserLat = Modes.fUserLon = 0.0;
    } else if (Modes.fUserLon > 180.0) { // If Longitude is +180 to +360, make it -180 to 0
        Modes.fUserLon -= 360.0;
    }
    // If both Lat and Lon are 0.0 then the users location is either invalid/not-set, or (s)he's in the
    // Atlantic ocean off the west coast of Africa. This is unlikely to be correct.
    // Set the user LatLon valid flag only if either Lat or Lon are non zero. Note the Greenwich meridian
    // is at 0.0 Lon,so we must check for either fLat or fLon being non zero not both.
    // Testing the flag at runtime will be much quicker than ((fLon != 0.0) || (fLat != 0.0))
    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) {
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    }

    // Limit the maximum requested raw output size to less than one Ethernet Block
    if (Modes.net_output_flush_size > (MODES_OUT_FLUSH_SIZE))
      {Modes.net_output_flush_size = MODES_OUT_FLUSH_SIZE;}
    if (Modes.net_output_flush_interval > (MODES_OUT_FLUSH_INTERVAL))
      {Modes.net_output_flush_interval = MODES_OUT_FLUSH_INTERVAL;}
    if (Modes.net_sndbuf_size > (MODES_NET_SNDBUF_MAX))
      {Modes.net_sndbuf_size = MODES_NET_SNDBUF_MAX;}

    // Prepare the log10 lookup table: 100log10(x)
    Modes.log10lut[0] = 0; // poorly defined..
    for (i = 1; i <= 65535; i++) {
        Modes.log10lut[i] = (uint16_t) round(100.0 * log10(i));
    }

    // Prepare error correction tables
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
    modeACInit();

    if (Modes.show_only)
        icaoFilterAdd(Modes.show_only);
}

//
//=========================================================================
//
// We use a thread reading data in background, while the main thread
// handles decoding and visualization of data to the user.
//
// The reading thread calls the RTLSDR API to read data asynchronously, and
// uses a callback to populate the data buffer.
//
// A Mutex is used to avoid races with the decoding thread.
//

//
//=========================================================================
//
// We read data using a thread, so the main thread only handles decoding
// without caring about data acquisition
//

void *readerThreadEntryPoint(void *arg)
{
    MODES_NOTUSED(arg);

    sdrRun();

    // Wake the main thread (if it's still waiting)
    pthread_mutex_lock(&Modes.data_mutex);
    if (!Modes.exit)
        Modes.exit = 2; // unexpected exit
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);

    return NULL;
}
//
// ============================== Snip mode =================================
//
// Get raw IQ samples and filter everything is < than the specified level
// for more than 256 samples in order to reduce example file size
//
void snipMode(int level) {
    int i, q;
    uint64_t c = 0;

    while ((i = getchar()) != EOF && (q = getchar()) != EOF) {
        if (abs(i-127) < level && abs(q-127) < level) {
            c++;
            if (c > MODES_PREAMBLE_SIZE) continue;
        } else {
            c = 0;
        }
        putchar(i);
        putchar(q);
    }
}
//
// ================================ Main ====================================
//
void showHelp(void) {

    printf("-----------------------------------------------------------------------------\n");
    printf("| dump1090 ModeS Receiver     %45s |\n", MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION);
    printf("| build options: %-58s |\n",
           ""
#ifdef ENABLE_RTLSDR
           "ENABLE_RTLSDR "
#endif
#ifdef ENABLE_BLADERF
           "ENABLE_BLADERF "
#endif
#ifdef ENABLE_MONGOC
           "ENABLE_MONGOC "
#endif
#ifdef SC16Q11_TABLE_BITS
    // This is a little silly, but that's how the preprocessor works..
#define _stringize(x) #x
#define stringize(x) _stringize(x)
           "SC16Q11_TABLE_BITS=" stringize(SC16Q11_TABLE_BITS)
#undef stringize
#undef _stringize
#endif
           );
    printf("-----------------------------------------------------------------------------\n");
    printf("\n");

    sdrShowHelp();

    printf(
"      Common options\n"
"\n"
"--gain <db>              Set gain (default: max gain. Use -10 for auto-gain)\n"
"--freq <hz>              Set frequency (default: 1090 Mhz)\n"
"--interactive            Interactive mode refreshing data on screen. Implies --throttle\n"
"--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60)\n"
"--raw                    Show only messages hex values\n"
"--net                    Enable networking\n"
"--modeac                 Enable decoding of SSR Modes 3/A & 3/C\n"
"--no-modeac-auto         Don't enable Mode A/C if requested by a Beast connection\n"
"--net-only               Enable just networking, no RTL device or file used\n"
"--net-bind-address <ip>  IP address to bind to (default: Any; Use 127.0.0.1 for private)\n"
"--net-ri-port <ports>    TCP raw input listen ports  (default: 30001)\n"
"--net-ro-port <ports>    TCP raw output listen ports (default: 30002)\n"
"--net-sbs-port <ports>   TCP BaseStation output listen ports (default: 30003)\n"
"--net-bi-port <ports>    TCP Beast input listen ports  (default: 30004,30104)\n"
"--net-bo-port <ports>    TCP Beast output listen ports (default: 30005)\n"
"--net-ro-size <size>     TCP output minimum size (default: 0)\n"
"--net-ro-interval <rate> TCP output memory flush rate in seconds (default: 0)\n"
"--net-heartbeat <rate>   TCP heartbeat rate in seconds (default: 60 sec; 0 to disable)\n"
"--net-buffer <n>         TCP buffer size 64Kb * (2^n) (default: n=0, 64Kb)\n"
"--net-verbatim           Do not apply CRC corrections to messages we forward; send unchanged\n"
"--forward-mlat           Allow forwarding of received mlat results to output ports\n"
"--lat <latitude>         Reference/receiver latitude for surface posn (opt)\n"
"--lon <longitude>        Reference/receiver longitude for surface posn (opt)\n"
"--max-range <distance>   Absolute maximum range for position decoding (in nm, default: 300)\n"
"--fix                    Enable single-bits error correction using CRC\n"
"--no-fix                 Disable single-bits error correction using CRC\n"
"--no-crc-check           Disable messages with broken CRC (discouraged)\n"
#ifdef ALLOW_AGGRESSIVE
"--aggressive             More CPU for more messages (two bits fixes, ...)\n"
#endif
"--mlat                   display raw messages in Beast ascii mode\n"
"--stats                  With --ifile print stats at exit. No other output\n"
"--stats-range            Collect/show range histogram\n"
"--stats-every <seconds>  Show and reset stats every <seconds> seconds\n"
"--onlyaddr               Show only ICAO addresses (testing purposes)\n"
"--metric                 Use metric units (meters, km/h, ...)\n"
"--gnss                   Show altitudes as HAE/GNSS (with H suffix) when available\n"
"--snip <level>           Strip IQ file removing samples < level\n"
"--debug <flags>          Debug mode (verbose), see README for details\n"
"--quiet                  Disable output to stdout. Use for daemon applications\n"
"--show-only <addr>       Show only messages from the given ICAO on stdout\n"
"--write-json <dir>       Periodically write json output to <dir> (for serving by a separate webserver)\n"
"--write-json-every <t>   Write json output every t seconds (default 1)\n"
"--json-location-accuracy <n>  Accuracy of receiver location in json metadata: 0=no location, 1=approximate, 2=exact\n"
"--dcfilter               Apply a 1Hz DC filter to input data (requires more CPU)\n"
"--help                   Show this help\n"
"\n"
"Debug mode flags: d = Log frames decoded with errors\n"
"                  D = Log frames decoded with zero errors\n"
"                  c = Log frames with bad CRC\n"
"                  C = Log frames with good CRC\n"
"                  p = Log frames with bad preamble\n"
"                  n = Log network debugging info\n"
"                  j = Log frames to frames.js, loadable by debug.html\n"
    );
}

static void display_total_stats(void)
{
    struct stats added;
    add_stats(&Modes.stats_alltime, &Modes.stats_current, &added);
    display_stats(&added);
}

//
//=========================================================================
//
// This function is called a few times every second by main in order to
// perform tasks we need to do continuously, like accepting new clients
// from the net, refreshing the screen in interactive mode, and so forth
//
void backgroundTasks(void) {
    static uint64_t next_stats_display;
    static uint64_t next_stats_update;
    static uint64_t next_json, next_history;

    uint64_t now = mstime();

    icaoFilterExpire();
    trackPeriodicUpdate();

    if (Modes.net) {
        modesNetPeriodicWork();
    }


    // Refresh screen when in interactive mode
    if (Modes.interactive) {
        interactiveShowData();
    }

    // always update end time so it is current when requests arrive
    Modes.stats_current.end = mstime();

    if (now >= next_stats_update) {
        int i;

        if (next_stats_update == 0) {
            next_stats_update = now + 60000;
        } else {
            Modes.stats_latest_1min = (Modes.stats_latest_1min + 1) % 15;
            Modes.stats_1min[Modes.stats_latest_1min] = Modes.stats_current;

            add_stats(&Modes.stats_current, &Modes.stats_alltime, &Modes.stats_alltime);
            add_stats(&Modes.stats_current, &Modes.stats_periodic, &Modes.stats_periodic);

            reset_stats(&Modes.stats_5min);
            for (i = 0; i < 5; ++i)
                add_stats(&Modes.stats_1min[(Modes.stats_latest_1min - i + 15) % 15], &Modes.stats_5min, &Modes.stats_5min);

            reset_stats(&Modes.stats_15min);
            for (i = 0; i < 15; ++i)
                add_stats(&Modes.stats_1min[i], &Modes.stats_15min, &Modes.stats_15min);

            reset_stats(&Modes.stats_current);
            Modes.stats_current.start = Modes.stats_current.end = now;

            if (Modes.json_dir)
                writeJsonToFile("stats.json", generateStatsJson);

            next_stats_update += 60000;
        }
    }

    if (Modes.stats && now >= next_stats_display) {
        if (next_stats_display == 0) {
            next_stats_display = now + Modes.stats;
        } else {
            add_stats(&Modes.stats_periodic, &Modes.stats_current, &Modes.stats_periodic);
            display_stats(&Modes.stats_periodic);
            reset_stats(&Modes.stats_periodic);

            next_stats_display += Modes.stats;
            if (next_stats_display <= now) {
                /* something has gone wrong, perhaps the system clock jumped */
                next_stats_display = now + Modes.stats;
            }
        }
    }

    if (Modes.json_dir && now >= next_json) {
        writeJsonToFile("aircraft.json", generateAircraftJson);
        next_json = now + Modes.json_interval;
    }

#ifdef ENABLE_MONGOC
    if (Modes.mongo_uri) {
        int len = 0;
        int code = writeJsonToMongo(generateAircraftJson(NULL, &len));

        if(len == 0) {
            fprintf(stderr, "Tried to write empty JSON to Mongo\n");
        }

        if(code != 0) {
            fprintf(stderr, "Error while writing JSON to Mongo\n");
        }
    }
#endif

    if (now >= next_history) {
        int rewrite_receiver_json = (Modes.json_dir && Modes.json_aircraft_history[HISTORY_SIZE-1].content == NULL);

        free(Modes.json_aircraft_history[Modes.json_aircraft_history_next].content); // might be NULL, that's OK.
        Modes.json_aircraft_history[Modes.json_aircraft_history_next].content =
            generateAircraftJson("/data/aircraft.json", &Modes.json_aircraft_history[Modes.json_aircraft_history_next].clen);

        if (Modes.json_dir) {
            char filebuf[PATH_MAX];
            snprintf(filebuf, PATH_MAX, "history_%d.json", Modes.json_aircraft_history_next);
            writeJsonToFile(filebuf, generateHistoryJson);
        }

        Modes.json_aircraft_history_next = (Modes.json_aircraft_history_next+1) % HISTORY_SIZE;

        if (rewrite_receiver_json)
            writeJsonToFile("receiver.json", generateReceiverJson); // number of history entries changed

        next_history = now + HISTORY_INTERVAL;
    }
}

//
//=========================================================================
//
//
//=========================================================================
//
int main(int argc, char **argv) {
    int j;

    // Set sane defaults
    modesInitConfig();

    // signal handlers:
    signal(SIGINT, sigintHandler);
    signal(SIGTERM, sigtermHandler);

    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = j+1 < argc; // There are more arguments

        if (!strcmp(argv[j],"--freq") && more) {
            Modes.freq = (int) strtoll(argv[++j],NULL,10);
        } else if ( (!strcmp(argv[j], "--device") || !strcmp(argv[j], "--device-index")) && more) {
            Modes.dev_name = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--gain") && more) {
            Modes.gain = (int) (atof(argv[++j])*10); // Gain is in tens of DBs
        } else if (!strcmp(argv[j],"--dcfilter")) {
            Modes.dc_filter = 1;
        } else if (!strcmp(argv[j],"--measure-noise")) {
            // Ignored
        } else if (!strcmp(argv[j],"--fix")) {
            Modes.nfix_crc = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--phase-enhance")) {
            // Ignored, always enabled
        } else if (!strcmp(argv[j],"--raw")) {
            Modes.raw = 1;
        } else if (!strcmp(argv[j],"--net")) {
            Modes.net = 1;
        } else if (!strcmp(argv[j],"--modeac")) {
            Modes.mode_ac = 1;
            Modes.mode_ac_auto = 0;
        } else if (!strcmp(argv[j],"--no-modeac-auto")) {
            Modes.mode_ac_auto = 0;
        } else if (!strcmp(argv[j],"--net-beast")) {
            fprintf(stderr, "--net-beast ignored, use --net-bo-port to control where Beast output is generated\n");
        } else if (!strcmp(argv[j],"--net-only")) {
            Modes.net = 1;
            Modes.sdr_type = SDR_NONE;
       } else if (!strcmp(argv[j],"--net-heartbeat") && more) {
            Modes.net_heartbeat_interval = (uint64_t)(1000 * atof(argv[++j]));
       } else if (!strcmp(argv[j],"--net-ro-size") && more) {
            Modes.net_output_flush_size = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ro-rate") && more) {
            Modes.net_output_flush_interval = 1000 * atoi(argv[++j]) / 15; // backwards compatibility
        } else if (!strcmp(argv[j],"--net-ro-interval") && more) {
            Modes.net_output_flush_interval = (uint64_t)(1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--net-ro-port") && more) {
            free(Modes.net_output_raw_ports);
            Modes.net_output_raw_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ri-port") && more) {
            free(Modes.net_input_raw_ports);
            Modes.net_input_raw_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-port") && more) {
            free(Modes.net_output_beast_ports);
            Modes.net_output_beast_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bi-port") && more) {
            free(Modes.net_input_beast_ports);
            Modes.net_input_beast_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bind-address") && more) {
            free(Modes.net_bind_address);
            Modes.net_bind_address = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-http-port") && more) {
            if (strcmp(argv[++j], "0")) {
                fprintf(stderr, "warning: --net-http-port not supported in this build, option ignored.\n");
            }
        } else if (!strcmp(argv[j],"--net-sbs-port") && more) {
            free(Modes.net_output_sbs_ports);
            Modes.net_output_sbs_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-buffer") && more) {
            Modes.net_sndbuf_size = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-verbatim")) {
            Modes.net_verbatim = 1;
        } else if (!strcmp(argv[j],"--forward-mlat")) {
            Modes.forward_mlat = 1;
        } else if (!strcmp(argv[j],"--onlyaddr")) {
            Modes.onlyaddr = 1;
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--hae") || !strcmp(argv[j],"--gnss")) {
            Modes.use_gnss = 1;
        } else if (!strcmp(argv[j],"--aggressive")) {
#ifdef ALLOW_AGGRESSIVE
            Modes.nfix_crc = MODES_MAX_BITERRORS;
#else
            fprintf(stderr, "warning: --aggressive not supported in this build, option ignored.\n");
#endif
        } else if (!strcmp(argv[j],"--interactive")) {
            Modes.interactive = 1;
        } else if (!strcmp(argv[j],"--interactive-ttl") && more) {
            Modes.interactive_display_ttl = (uint64_t)(1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--max-range") && more) {
            Modes.maxRange = atof(argv[++j]) * 1852.0; // convert to metres
        } else if (!strcmp(argv[j],"--debug") && more) {
            char *f = argv[++j];
            while(*f) {
                switch(*f) {
                case 'D': Modes.debug |= MODES_DEBUG_DEMOD; break;
                case 'd': Modes.debug |= MODES_DEBUG_DEMODERR; break;
                case 'C': Modes.debug |= MODES_DEBUG_GOODCRC; break;
                case 'c': Modes.debug |= MODES_DEBUG_BADCRC; break;
                case 'p': Modes.debug |= MODES_DEBUG_NOPREAMBLE; break;
                case 'n': Modes.debug |= MODES_DEBUG_NET; break;
                case 'j': Modes.debug |= MODES_DEBUG_JS; break;
                default:
                    fprintf(stderr, "Unknown debugging flag: %c\n", *f);
                    exit(1);
                    break;
                }
                f++;
            }
        } else if (!strcmp(argv[j],"--stats")) {
            if (!Modes.stats)
                Modes.stats = (uint64_t)1 << 60; // "never"
        } else if (!strcmp(argv[j],"--stats-range")) {
            Modes.stats_range_histo = 1;
        } else if (!strcmp(argv[j],"--stats-every") && more) {
            Modes.stats = (uint64_t) (1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--snip") && more) {
            snipMode(atoi(argv[++j]));
            exit(0);
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else if (!strcmp(argv[j],"--quiet")) {
            Modes.quiet = 1;
        } else if (!strcmp(argv[j],"--show-only") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
        } else if (!strcmp(argv[j],"--mlat")) {
            Modes.mlat = 1;
        } else if (!strcmp(argv[j],"--oversample")) {
            // Ignored
#ifndef _WIN32
        } else if (!strcmp(argv[j], "--write-json") && more) {
            Modes.json_dir = strdup(argv[++j]);
        } else if (!strcmp(argv[j], "--write-json-every") && more) {
            Modes.json_interval = (uint64_t)(1000 * atof(argv[++j]));
            if (Modes.json_interval < 100) // 0.1s
                Modes.json_interval = 100;
        } else if (!strcmp(argv[j], "--json-location-accuracy") && more) {
            Modes.json_location_accuracy = atoi(argv[++j]);

         // Mongo flags
        } else if (!strcmp(argv[j], "--write-mongo-uri") && more) {
            Modes.mongo_uri = strdup(argv[++j]);
        } else if (!strcmp(argv[j], "--write-mongo-db") && more) {
            Modes.mongo_database = strdup(argv[++j]);
        } else if (!strcmp(argv[j], "--write-mongo-col") && more) {
            Modes.mongo_collection = strdup(argv[++j]);
#endif
        } else if (sdrHandleOption(argc, argv, &j)) {
            /* handled */
        } else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }

#ifdef _WIN32
    // Try to comply with the Copyright license conditions for binary distribution
    if (!Modes.quiet) {showCopyright();}
#endif

    // Initialization
    log_with_timestamp("%s %s starting up.", MODES_DUMP1090_VARIANT, MODES_DUMP1090_VERSION);
    modesInit();

    if (!sdrOpen()) {
        exit(1);
    }

    if (Modes.net) {
        modesInitNet();
    }

    // init stats:
    Modes.stats_current.start = Modes.stats_current.end =
        Modes.stats_alltime.start = Modes.stats_alltime.end =
        Modes.stats_periodic.start = Modes.stats_periodic.end =
        Modes.stats_5min.start = Modes.stats_5min.end =
        Modes.stats_15min.start = Modes.stats_15min.end = mstime();

    for (j = 0; j < 15; ++j)
        Modes.stats_1min[j].start = Modes.stats_1min[j].end = Modes.stats_current.start;

    // write initial json files so they're not missing
    writeJsonToFile("receiver.json", generateReceiverJson);
    writeJsonToFile("stats.json", generateStatsJson);
    writeJsonToFile("aircraft.json", generateAircraftJson);

    interactiveInit();

    // If the user specifies --net-only, just run in order to serve network
    // clients without reading data from the RTL device
    if (Modes.sdr_type == SDR_NONE) {
        while (!Modes.exit) {
            struct timespec start_time;

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);

            usleep(100000);
        }
    } else {
        int watchdogCounter = 10; // about 1 second

        // Create the thread that will read the data from the device.
        pthread_mutex_lock(&Modes.data_mutex);
        pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

        while (!Modes.exit) {
            struct timespec start_time;

            if (Modes.first_free_buffer == Modes.first_filled_buffer) {
                /* wait for more data.
                 * we should be getting data every 50-60ms. wait for max 100ms before we give up and do some background work.
                 * this is fairly aggressive as all our network I/O runs out of the background work!
                 */

                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += 100000000;
                normalize_timespec(&ts);

                pthread_cond_timedwait(&Modes.data_cond, &Modes.data_mutex, &ts); // This unlocks Modes.data_mutex, and waits for Modes.data_cond
            }

            // Modes.data_mutex is locked, and possibly we have data.

            // copy out reader CPU time and reset it
            add_timespecs(&Modes.reader_cpu_accumulator, &Modes.stats_current.reader_cpu, &Modes.stats_current.reader_cpu);
            Modes.reader_cpu_accumulator.tv_sec = 0;
            Modes.reader_cpu_accumulator.tv_nsec = 0;

            if (Modes.first_free_buffer != Modes.first_filled_buffer) {
                // FIFO is not empty, process one buffer.

                struct mag_buf *buf;

                start_cpu_timing(&start_time);
                buf = &Modes.mag_buffers[Modes.first_filled_buffer];

                // Process data after releasing the lock, so that the capturing
                // thread can read data while we perform computationally expensive
                // stuff at the same time.
                pthread_mutex_unlock(&Modes.data_mutex);

                demodulate2400(buf);
                if (Modes.mode_ac) {
                    demodulate2400AC(buf);
                }

                Modes.stats_current.samples_processed += buf->length;
                Modes.stats_current.samples_dropped += buf->dropped;
                end_cpu_timing(&start_time, &Modes.stats_current.demod_cpu);

                // Mark the buffer we just processed as completed.
                pthread_mutex_lock(&Modes.data_mutex);
                Modes.first_filled_buffer = (Modes.first_filled_buffer + 1) % MODES_MAG_BUFFERS;
                pthread_cond_signal(&Modes.data_cond);
                pthread_mutex_unlock(&Modes.data_mutex);
                watchdogCounter = 10;
            } else {
                // Nothing to process this time around.
                pthread_mutex_unlock(&Modes.data_mutex);
                if (--watchdogCounter <= 0) {
                    log_with_timestamp("No data received from the SDR for a long time, it may have wedged");
                    watchdogCounter = 600;
                }
            }

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);

            pthread_mutex_lock(&Modes.data_mutex);
        }

        pthread_mutex_unlock(&Modes.data_mutex);

        log_with_timestamp("Waiting for receive thread termination");
        pthread_join(Modes.reader_thread,NULL);     // Wait on reader thread exit
        pthread_cond_destroy(&Modes.data_cond);     // Thread cleanup - only after the reader thread is dead!
        pthread_mutex_destroy(&Modes.data_mutex);
    }

    interactiveCleanup();

    // If --stats were given, print statistics
    if (Modes.stats) {
        display_total_stats();
    }

    sdrClose();

    if (Modes.exit == 1) {
        log_with_timestamp("Normal exit.");
        return 0;
    } else {
        log_with_timestamp("Abnormal exit.");
        return 1;
    }
}
//
//=========================================================================
//
