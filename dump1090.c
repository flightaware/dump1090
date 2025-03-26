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
#include "cpu.h"

#include <stdarg.h>

struct _Modes Modes;

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
static void modesInitConfig(void) {
    // Default everything to zero/NULL
    memset(&Modes, 0, sizeof(Modes));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.gain                    = MODES_DEFAULT_GAIN;
    Modes.freq                    = MODES_DEFAULT_FREQ;
    Modes.fix_df                  = 1;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.json_interval           = 1000;
    Modes.json_stats_interval     = 60000;
    Modes.json_location_accuracy  = 1;
    Modes.maxRange                = 1852 * 300; // 300NM default max range
    Modes.mode_ac_auto            = 1;

    Modes.net_heartbeat_interval = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.net_output_flush_size = 1300;
    Modes.net_output_flush_interval = 500;

    // adaptive
    Modes.adaptive_min_gain_db = 0;
    Modes.adaptive_max_gain_db = 99999;

    Modes.adaptive_duty_cycle = 0.5;

    Modes.adaptive_burst_control = false;
    Modes.adaptive_burst_alpha = 2.0 / (5 + 1);
    Modes.adaptive_burst_change_delay = 5;
    Modes.adaptive_burst_loud_runlength = 10;
    Modes.adaptive_burst_loud_rate = 5.0;
    Modes.adaptive_burst_quiet_runlength = 10;
    Modes.adaptive_burst_quiet_rate = 5.0;

    Modes.adaptive_range_control = false;
    Modes.adaptive_range_alpha = 2.0 / (5 + 1);
    Modes.adaptive_range_percentile = 40;
    Modes.adaptive_range_change_delay = 10;
    Modes.adaptive_range_scan_delay = 300;
    Modes.adaptive_range_rescan_delay = 3600;

    sdrInitConfig();
}
//
//=========================================================================
//
static void modesInit(void) {
    int i;

    Modes.sample_rate = 2400000.0;

    // Allocate the various buffers used by Modes
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;

    if ( ((Modes.log10lut   = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256)                                 ) == NULL) )
    {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }

    if (!fifo_create(MODES_MAG_BUFFERS, MODES_MAG_BUF_SAMPLES + Modes.trailing_samples, Modes.trailing_samples)) {
        fprintf(stderr, "Out of memory allocating FIFO\n");
        exit(1);
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

static void *readerThreadEntryPoint(void *arg)
{
    MODES_NOTUSED(arg);

    sdrRun();

    if (!Modes.exit)
        Modes.exit = 2; // unexpected exit

    fifo_halt(); // wakes the main thread, if it's still waiting
    return NULL;
}
//
// ============================== Snip mode =================================
//
// Get raw IQ samples and filter everything is < than the specified level
// for more than 256 samples in order to reduce example file size
//
static void snipMode(int level) {
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
static void showVersion()
{
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
#ifdef ENABLE_HACKRF
           "ENABLE_HACKRF "
#endif
#ifdef ENABLE_LIMESDR
           "ENABLE_LIMESDR "
#endif
           );
    printf("-----------------------------------------------------------------------------\n");
}

static void showDSP()
{
    printf("  detected runtime CPU features: ");
    if (cpu_supports_avx())
        printf("AVX ");
    if (cpu_supports_avx2())
        printf("AVX2 ");
    if (cpu_supports_armv7_neon_vfpv4())
        printf("ARMv7+NEON+VFPv4 ");
    printf("\n");

    printf("  selected DSP implementations: \n");
#define SHOW(x) do {                                                    \
        printf("    %-40s %s\n", #x , starch_ ## x ## _select()->name);  \
        printf("    %-40s %s\n", #x "_aligned", starch_ ## x ## _aligned_select()->name); \
    } while(0)

    SHOW(magnitude_uc8);
    SHOW(magnitude_power_uc8);
    SHOW(magnitude_sc16);
    SHOW(magnitude_sc16q11);
    SHOW(mean_power_u16);
    SHOW(count_above_u16);

#undef SHOW

    printf("\n");
}

static void showHelp(void)
{
    showVersion();

    sdrShowHelp();

    printf(
"      Output modes\n"
"\n"
// ------ 80 char limit ----------------------------------------------------------|
"--raw                    Show only messages hex values\n"
"--modeac                 Enable decoding of SSR Modes 3/A & 3/C\n"
"--mlat                   display raw messages in Beast ascii mode\n"
"--onlyaddr               Show only ICAO addresses (testing purposes)\n"
"--metric                 Use metric units (meters, km/h, ...)\n"
"--gnss                   Show altitudes as HAE/GNSS when available\n"
"--quiet                  Disable output to stdout. Use for daemon applications\n"
"--show-only <addr>       Show only messages from the given ICAO on stdout\n"
"--snip <level>           Strip IQ file removing samples < level\n"
"\n"
"      Decoder settings\n"
"\n"
// ------ 80 char limit ----------------------------------------------------------|
"--gain <db>              Set gain in dB (default: varies by SDR type)\n"
"--freq <hz>              Set frequency (default: 1090 Mhz)\n"
"--fix                    Enable single-bit error correction using CRC\n"
"--fix-2bit               Enable two-bit error correction using CRC\n"
"                          (use with caution!)\n"
"--no-fix                 Disable error correction using CRC\n"
"--no-fix-df              Disable error correction of the DF message field\n"
"                          (reduces CPU requirements)\n"
"--enable-df24            Enable decoding of DF24 Comm-D ELM messages\n"
"--lat <latitude>         Reference/receiver latitude for surface positions\n"
"--lon <longitude>        Reference/receiver longitude for surface positions\n"
"--max-range <distance>   Absolute maximum range for position decoding (in NM)\n"
"\n"
// ------ 80 char limit ----------------------------------------------------------|
"      Adaptive gain\n"
"\n"
"--adaptive-burst                     Adjust gain for too-loud message bursts\n"
"--adaptive-burst-change-delay <s>     Set delay after changing gain before\n"
"                                       resuming burst control (seconds)\n"
"--adaptive-burst-alpha <a>            Set burst rate smoothing factor\n"
"                                       (0..1, smaller=more smoothing)\n"
"--adaptive-burst-loud-rate <r>        Set burst rate for gain decrease\n"
"--adaptive-burst-loud-runlength <l>   Set burst runlength for gain decrease\n"
"--adaptive-burst-quiet-rate <r>       Set burst rate for gain increase\n"
"--adaptive-burst-quiet-runlength <l>  Set burst runlength for gain increase\n"
"--adaptive-range                     Adjust gain for target dynamic range\n"
"--adaptive-range-target <db>          Set target dynamic range in dB\n"
"--adaptive-range-alpha <a>            Set dynamic range noise smoothing factor\n"
"                                       (0..1, smaller=more smoothing)\n"
"--adaptive-range-percentile <p>       Set dynamic range noise percentile\n"
"--adaptive-range-change-delay <s>     Set delay after changing gain before\n"
"                                       resuming dynamic range control (seconds)\n"
"--adaptive-range-scan-delay <s>       Set scan interval for dynamic range\n"
"                                       gain scanning following a gain decrease\n"
"                                       due to an increase in noise (seconds)\n"
"--adaptive-range-rescan-delay <s>     Set periodic rescan interval for dynamic\n"
"                                       range gain scanning (seconds)\n"
"--adaptive-min-gain <g>              Set gain adjustment range lower limit (dB)\n"
"--adaptive-max-gain <g>              Set gain adjustment range upper limit (dB)\n"
"--adaptive-duty-cycle <p>            Set adaptive gain duty cycle %% (1..100)\n"
"\n"
// ------ 80 char limit ----------------------------------------------------------|
"      Network connections\n"
"\n"
"--net                    Enable networking with default ports unless overridden\n"
"--no-modeac-auto         Don't enable Mode A/C if requested by a net connection\n"
"--net-only               Enable just networking, no RTL device or file used\n"
"--net-bind-address <ip>  IP address to bind to (use 127.0.0.1 for private)\n"
"--net-ri-port <ports>    TCP raw input listen ports  (default: 30001)\n"
"--net-ro-port <ports>    TCP raw output listen ports (default: 30002)\n"
"--net-sbs-port <ports>   TCP BaseStation output listen ports (default: 30003)\n"
"--net-bi-port <ports>    TCP Beast input listen ports  (default: 30004,30104)\n"
"--net-bo-port <ports>    TCP Beast output listen ports (default: 30005)\n"
"--net-stratux-port <ports>  TCP Stratux output listen ports (default: disabled)\n"
"--net-ro-size <size>     TCP output minimum size (default: 0)\n"
"--net-ro-interval <rate> TCP output memory flush rate in seconds (default: 0)\n"
"--net-heartbeat <rate>   TCP heartbeat rate in seconds\n"
"                          (default: 60 sec; 0 to disable)\n"
"--net-buffer <n>         TCP buffer size 64Kb * (2^n) (default: n=0, 64Kb)\n"
"--net-verbatim           Make output connections default to verbatim mode\n"
"                           (forward all messages without correction)\n"
"--forward-mlat           Allow forwarding of received mlat results\n"
"\n"
// ------ 80 char limit ----------------------------------------------------------|
"      Stats and json output\n"
"\n"
"--stats                  Show stats summary at exit.\n"
"--stats-every <seconds>  Show and reset stats every <seconds> seconds\n"
"--stats-range            Collect/show range histogram\n"
"--write-json <dir>       Periodically write json output to <dir>\n"
"                          (for serving by a separate webserver)\n"
"--write-json-every <t>   Write json aircraft output every t seconds (default 1)\n"
"--json-stats-every <t>   Write json stats output every t seconds (default 60)\n"
"--json-location-accuracy <n>  Accuracy of receiver location in json metadata\n"
"                          (0=no location, 1=approximate, 2=exact)\n"
"\n"
"      Interactive mode\n"
"\n"
"--interactive                       Interactive mode refreshing data on screen.\n"
"                                     Implies --throttle\n"
"--interactive-ttl <sec>             Remove from list if idle for <sec>\n"
"--interactive-show-distance         Show aircraft distance and bearing\n"
"                                     (requires --lat and --lon)\n"
"--interactive-distance-units <u>    Distance units ('km', 'sm', 'nm')\n"
"--interactive-callsign-filter <r>   Filter rows by callsign against regex\n"
"\n"
"      Misc\n"
"\n"
"--wisdom <path>          Read DSP wisdom from given path\n"
"--version                Show version, build and DSP options\n"
"--help                   Show this help\n"
    );
}

// Accumulate stats data from stats_current to stats_periodic, stats_alltime and stats_latest;
// reset stats_current
void flush_stats(uint64_t now);
void flush_stats(uint64_t now)
{
    if (Modes.sdr_type != SDR_NONE) {
        Modes.stats_current.sdr_gain = sdrGetGain();
    }

    add_stats(&Modes.stats_current, &Modes.stats_periodic, &Modes.stats_periodic);
    add_stats(&Modes.stats_current, &Modes.stats_alltime, &Modes.stats_alltime);
    add_stats(&Modes.stats_current, &Modes.stats_latest, &Modes.stats_latest);

    reset_stats(&Modes.stats_current);
    Modes.stats_current.start = Modes.stats_current.end = now;
}

//
//=========================================================================
//
// This function is called a few times every second by main in order to
// perform tasks we need to do continuously, like accepting new clients
// from the net, refreshing the screen in interactive mode, and so forth
//
static void backgroundTasks(void) {
    static uint64_t next_stats_display;
    static uint64_t next_stats_update;
    static uint64_t next_json_stats_update;
    static uint64_t next_json, next_history;

    uint64_t now = mstime();

    if (Modes.sdr_type != SDR_IFILE) {
        // don't run these if processing data from a file
        icaoFilterExpire();
        trackPeriodicUpdate();
    }

    if (Modes.net) {
        modesNetPeriodicWork();
    }


    // Refresh screen when in interactive mode
    if (Modes.interactive) {
        interactiveShowData();
    }

    // copy out reader CPU time and reset it
    sdrUpdateCPUTime(&Modes.stats_current.reader_cpu);

    // always update end time so it is current when requests arrive
    Modes.stats_current.end = mstime();

    // 1-minute stats update
    if (now >= next_stats_update) {
        int i;

        if (next_stats_update == 0) {
            next_stats_update = now + 60000;
        } else {
            flush_stats(now); // Ensure stats_latest is up to date

            // move stats_latest into 1-min ring buffer
            Modes.stats_newest_1min = (Modes.stats_newest_1min + 1) % 15;
            Modes.stats_1min[Modes.stats_newest_1min] = Modes.stats_latest;
            reset_stats(&Modes.stats_latest);

            // recalculate 5-min window
            reset_stats(&Modes.stats_5min);
            for (i = 0; i < 5; ++i)
                add_stats(&Modes.stats_1min[(Modes.stats_newest_1min - i + 15) % 15], &Modes.stats_5min, &Modes.stats_5min);

            // recalculate 15-min window
            reset_stats(&Modes.stats_15min);
            for (i = 0; i < 15; ++i)
                add_stats(&Modes.stats_1min[i], &Modes.stats_15min, &Modes.stats_15min);

            next_stats_update += 60000;
        }
    }

    // --stats-every display
    if (Modes.stats && now >= next_stats_display) {
        if (next_stats_display == 0) {
            next_stats_display = now + Modes.stats;
        } else {
            flush_stats(now); // Ensure stats_periodic is up to date

            display_stats(&Modes.stats_periodic);
            reset_stats(&Modes.stats_periodic);

            next_stats_display += Modes.stats;
            if (next_stats_display <= now) {
                /* something has gone wrong, perhaps the system clock jumped */
                next_stats_display = now + Modes.stats;
            }
        }
    }

    // json stats update
    if (Modes.json_dir && now >= next_json_stats_update) {
        if (next_json_stats_update == 0) {
            next_json_stats_update = now + Modes.json_stats_interval;
        } else {
            flush_stats(now); // Ensure everything we'll write is up to date
            writeJsonToFile("stats.json", generateStatsJson);
            next_json_stats_update += Modes.json_stats_interval;
        }
    }

    if (Modes.json_dir && now >= next_json) {
        writeJsonToFile("aircraft.json", generateAircraftJson);
        next_json = now + Modes.json_interval;
    }

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

static void applyNetDefaults()
{
    if (!Modes.net_input_raw_ports)
        Modes.net_input_raw_ports = strdup("30001");
    if (!Modes.net_output_raw_ports)
        Modes.net_output_raw_ports = strdup("30002");
    if (!Modes.net_output_sbs_ports)
        Modes.net_output_sbs_ports = strdup("30003");
    if (!Modes.net_input_beast_ports)
        Modes.net_input_beast_ports = strdup("30004,30104");
    if (!Modes.net_output_beast_ports)
        Modes.net_output_beast_ports = strdup("30005");
}

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
            Modes.gain = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--dcfilter")) {
#if 0
            Modes.dc_filter = 1;
#else
            fprintf(stderr, "--dcfilter option ignored (please raise an issue on github if you have a usecase that needs this)\n");
#endif
        } else if (!strcmp(argv[j],"--measure-noise")) {
            // Ignored
        } else if (!strcmp(argv[j],"--fix")) {
            if (Modes.nfix_crc < 1)
                Modes.nfix_crc = 1;
        } else if (!strcmp(argv[j],"--fix-2bit")) {
            Modes.nfix_crc = 2;
        } else if (!strcmp(argv[j],"--enable-df24")) {
            Modes.enable_df24 = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--no-fix-df")) {
            Modes.fix_df = 0;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            fprintf(stderr, "warning: --no-crc-check no longer supported, option ignored (please raise an issue on github if you have a usecase that needs this)\n");
        } else if (!strcmp(argv[j],"--phase-enhance")) {
            // Ignored, always enabled
        } else if (!strcmp(argv[j],"--raw")) {
            Modes.raw = 1;
        } else if (!strcmp(argv[j],"--net")) {
            Modes.net = 1;
            applyNetDefaults();
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
            applyNetDefaults();
       } else if (!strcmp(argv[j],"--net-heartbeat") && more) {
            Modes.net_heartbeat_interval = (uint64_t)(1000 * atof(argv[++j]));
       } else if (!strcmp(argv[j],"--net-ro-size") && more) {
            Modes.net_output_flush_size = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ro-rate") && more) {
            Modes.net_output_flush_interval = 1000 * atoi(argv[++j]) / 15; // backwards compatibility
        } else if (!strcmp(argv[j],"--net-ro-interval") && more) {
            Modes.net_output_flush_interval = (uint64_t)(1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--net-ro-port") && more) {
            Modes.net = 1;
            free(Modes.net_output_raw_ports);
            Modes.net_output_raw_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ri-port") && more) {
            Modes.net = 1;
            free(Modes.net_input_raw_ports);
            Modes.net_input_raw_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-port") && more) {
            Modes.net = 1;
            free(Modes.net_output_beast_ports);
            Modes.net_output_beast_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bi-port") && more) {
            Modes.net = 1;
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
            Modes.net = 1;
            free(Modes.net_output_sbs_ports);
            Modes.net_output_sbs_ports = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--net-stratux-port") && more) {
            Modes.net = 1;
            free(Modes.net_output_stratux_ports);
            Modes.net_output_stratux_ports = strdup(argv[++j]);
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
            fprintf(stderr, "warning: --aggressive not supported in this build, option ignored (consider '--fix-2bit' instead)\n");
        } else if (!strcmp(argv[j],"--interactive")) {
            Modes.interactive = 1;
        } else if (!strcmp(argv[j],"--interactive-ttl") && more) {
            Modes.interactive_display_ttl = (uint64_t)(1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--interactive-show-distance")) {
            Modes.interactive_show_distance = 1;
        } else if (!strcmp(argv[j], "--interactive-distance-units") && more) {
            char *units = argv[++j];
            if (!strcmp(units, "km")) {
                Modes.interactive_distance_units = UNIT_KILOMETERS;
            } else if (!strcmp(units, "sm")) {
                Modes.interactive_distance_units = UNIT_STATUTE_MILES;
            } else {
                Modes.interactive_distance_units = UNIT_NAUTICAL_MILES;
            }
        } else if (!strcmp(argv[j], "--interactive-callsign-filter") && more) {
            Modes.interactive_callsign_filter = strdup(argv[++j]);
        } else if (!strcmp(argv[j], "--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--max-range") && more) {
            Modes.maxRange = atof(argv[++j]) * 1852.0; // convert to metres
        } else if (!strcmp(argv[j],"--debug") && more) {
            fprintf(stderr, "warning: --debug is obsolete and ignored\n");
            ++j;
        } else if (!strcmp(argv[j],"--stats")) {
            if (!Modes.stats)
                Modes.stats = (uint64_t)1 << 60; // "never"
        } else if (!strcmp(argv[j],"--stats-range")) {
            Modes.stats_range_histo = 1;
        } else if (!strcmp(argv[j],"--stats-every") && more) {
            Modes.stats = (uint64_t) (1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--json-stats-every") && more) {
            Modes.json_stats_interval = (uint64_t) (1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--snip") && more) {
            snipMode(atoi(argv[++j]));
            exit(0);
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else if (!strcmp(argv[j],"--version")) {
            showVersion();
            showDSP();
            exit(0);
        } else if (!strcmp(argv[j],"--quiet")) {
            Modes.quiet = 1;
        } else if (!strcmp(argv[j],"--show-only") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
        } else if (!strcmp(argv[j],"--mlat")) {
            Modes.mlat = 1;
        } else if (!strcmp(argv[j],"--oversample")) {
            // Ignored
        } else if (!strcmp(argv[j], "--write-json") && more) {
            Modes.json_dir = strdup(argv[++j]);
        } else if (!strcmp(argv[j], "--write-json-every") && more) {
            Modes.json_interval = (uint64_t)(1000 * atof(argv[++j]));
            if (Modes.json_interval < 100) // 0.1s
                Modes.json_interval = 100;
        } else if (!strcmp(argv[j], "--json-location-accuracy") && more) {
            Modes.json_location_accuracy = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--wisdom") && more) {
            if (starch_read_wisdom (argv[++j]) < 0) {
                fprintf(stderr,
                        "Failed to read wisdom file %s: %s\n", argv[j], strerror(errno));
                exit(1);
            }
        } else if (!strcmp(argv[j], "--adaptive-min-gain") && more) {
            Modes.adaptive_min_gain_db = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-max-gain") && more) {
            Modes.adaptive_max_gain_db = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-duty-cycle") && more) {
            Modes.adaptive_duty_cycle = atof(argv[++j]) / 100.0;
        } else if (!strcmp(argv[j], "--adaptive-burst")) {
            Modes.adaptive_burst_control = true;
        } else if (!strcmp(argv[j], "--adaptive-burst-alpha") && more) {
            Modes.adaptive_burst_alpha = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-burst-change-delay") && more) {
            Modes.adaptive_burst_change_delay = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-burst-loud-rate") && more) {
            Modes.adaptive_burst_loud_rate = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-burst-loud-runlength") && more) {
            Modes.adaptive_burst_loud_runlength = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-burst-quiet-rate") && more) {
            Modes.adaptive_burst_quiet_rate = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-burst-quiet-runlength") && more) {
            Modes.adaptive_burst_quiet_runlength = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range")) {
            Modes.adaptive_range_control = true;
        } else if (!strcmp(argv[j], "--adaptive-range-alpha") && more) {
            Modes.adaptive_range_alpha = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range-percentile") && more) {
            Modes.adaptive_range_percentile = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range-target") && more) {
            Modes.adaptive_range_target = atof(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range-change-delay") && more) {
            Modes.adaptive_range_change_delay = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range-scan-delay") && more) {
            Modes.adaptive_range_scan_delay = atoi(argv[++j]);
        } else if (!strcmp(argv[j], "--adaptive-range-rescan-delay") && more) {
            Modes.adaptive_range_rescan_delay = atoi(argv[++j]);
        } else if (sdrHandleOption(argc, argv, &j)) {
            /* handled */
        } else {
            fprintf(stderr,
                    "Unknown or not enough arguments for option '%s'.\nTry %s --help for full option help.\n",
                    argv[j],
                    argv[0]);
            exit(1);
        }
    }

    if (Modes.sdr_type == SDR_NONE && !Modes.net) {
        fprintf(stderr,
                "No SDR available and network mode not enabled; nothing to do!\n"
                "Select a SDR type (--device-type or --ifile), or enable network mode (--net)\n"
                "Try %s --help for full option help.\n",
                argv[0]);
        exit(1);
    }

    if (Modes.nfix_crc > MODES_MAX_BITERRORS)
        Modes.nfix_crc = MODES_MAX_BITERRORS;

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
    reset_stats(&Modes.stats_current);
    reset_stats(&Modes.stats_alltime);
    reset_stats(&Modes.stats_periodic);
    reset_stats(&Modes.stats_latest);
    reset_stats(&Modes.stats_5min);
    reset_stats(&Modes.stats_15min);

    Modes.stats_current.start = Modes.stats_current.end =
        Modes.stats_alltime.start = Modes.stats_alltime.end =
        Modes.stats_periodic.start = Modes.stats_periodic.end =
        Modes.stats_latest.start = Modes.stats_latest.end =
        Modes.stats_5min.start = Modes.stats_5min.end =
        Modes.stats_15min.start = Modes.stats_15min.end = mstime();

    for (j = 0; j < 15; ++j) {
        reset_stats(&Modes.stats_1min[j]);
        Modes.stats_1min[j].start = Modes.stats_1min[j].end = Modes.stats_current.start;
    }

    adaptive_init();

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
            struct timespec slp = { 0, 100 * 1000 * 1000};

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);

            nanosleep(&slp, NULL);
        }
    } else {
        int watchdogCounter = 300; // about 30 seconds

        // Create the thread that will read the data from the device.
        pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

        while (!Modes.exit) {
            // get the next sample buffer off the FIFO; wait only up to 100ms
            // this is fairly aggressive as all our network I/O runs out of the background work!
            struct mag_buf *buf = fifo_dequeue(100 /* milliseconds */);
            struct timespec start_time;

            if (buf) {
                // Process one buffer

                start_cpu_timing(&start_time);
                demodulate2400(buf);
                if (Modes.mode_ac) {
                    demodulate2400AC(buf);
                }

                Modes.stats_current.samples_processed += buf->validLength - buf->overlap;
                Modes.stats_current.samples_dropped += buf->dropped;
                end_cpu_timing(&start_time, &Modes.stats_current.demod_cpu);

                // Return the buffer to the FIFO freelist for reuse
                fifo_release(buf);

                // We got something so reset the watchdog
                watchdogCounter = 300;
            } else {
                // Nothing to process this time around.
                if (--watchdogCounter <= 0) {
                    log_with_timestamp("No samples received from the SDR for a long time. Maybe the hardware is wedged? Giving up.");
                    Modes.exit = 2; // abnormal exit
                }
            }

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);
        }

        log_with_timestamp("Waiting for receive thread termination");
        sdrStop();   // tell reader thread to wake up and exit
        fifo_halt(); // Reader thread should do this anyway, but just in case..

        // Wait on reader thread exit
        if (join_thread(Modes.reader_thread, NULL, 30000) == ETIMEDOUT) {
            log_with_timestamp("Receive thread did not shut down cleanly in 30 seconds, aborting.");
            abort(); // Can't complete cleanup while the receive thread is active; bail out.
        }
    }

    modesDeInitNet();
    interactiveCleanup();

    // Write final stats
    flush_stats(0);
    writeJsonToFile("stats.json", generateStatsJson);
    if (Modes.stats) {
        display_stats(&Modes.stats_alltime);
    }

    sdrClose();
    fifo_destroy();

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
