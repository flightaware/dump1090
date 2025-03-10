// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// faup1090.c: cut down version that just does 30005 -> stdout forwarding
//
// Copyright (c) 2014,2015 Oliver Jowett <oliver@mutability.co.uk>
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

struct _Modes Modes;

#include <stdarg.h>

void receiverPositionChanged(float lat, float lon, float alt)
{
    /* nothing */
    (void) lat;
    (void) lon;
    (void) alt;
}

//
// =============================== Initialization ===========================
//
static void faupInitConfig(void) {
    // Default everything to zero/NULL
    memset(&Modes, 0, sizeof(Modes));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.nfix_crc                = 1;
    Modes.fix_df                  = 1;
    Modes.net                     = 1;
    Modes.net_heartbeat_interval  = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.maxRange                = 1852 * 360; // 360NM default max range; this also disables receiver-relative positions
    Modes.quiet                   = 1;
    Modes.net_output_flush_size   = MODES_OUT_FLUSH_SIZE;
    Modes.net_output_flush_interval = 200; // milliseconds
    Modes.faup_rate_multiplier    = FAUP_DEFAULT_RATE_MULTIPLIER;
}

//
//=========================================================================
//
static void faupInit(void) {
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

    // Prepare error correction tables
    modesChecksumInit(1);
    icaoFilterInit();
    modeACInit();
}

//
// ================================ Main ====================================
//
static void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"| faup1090 ModeS conversion     %45s |\n"
"-----------------------------------------------------------------------------\n"
"--net-bo-ipaddr <addr>   IP address to connect to for Beast data (default: 127.0.0.1)\n"
"--net-bo-port <port>     Port to connect for Beast data (default: 30005)\n"
"--lat <latitude>         Reference/receiver latitude for surface posn (opt)\n"
"--lon <longitude>        Reference/receiver longitude for surface posn (opt)\n"
"--stdout                 REQUIRED. Write results to stdout.\n"
"--help                   Show this help\n"
"\n",
MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION
    );
}

//
//=========================================================================
//
// This function is called a few times every second by main in order to
// perform tasks we need to do continuously, like accepting new clients
// from the net, refreshing the screen in interactive mode, and so forth
//
static void backgroundTasks(void) {
    icaoFilterExpire();
    trackPeriodicUpdate();
    modesNetPeriodicWork();
}

//
//=========================================================================
//
int main(int argc, char **argv) {
    int j;
    int stdout_option = 0;
    char *bo_connect_ipaddr = "127.0.0.1";
    int bo_connect_port = 30005;
    struct client *c;
    struct net_service *beast_input, *fatsv_output, *fa_cmd_input;

    // Set sane defaults
    faupInitConfig();

    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = j+1 < argc; // There are more arguments

        if (!strcmp(argv[j],"--net-bo-port") && more) {
            bo_connect_port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-ipaddr") && more) {
            bo_connect_ipaddr = argv[++j];
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else if (!strcmp(argv[j],"--stdout")) {
            stdout_option = 1;
        } else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }

    if (!stdout_option) {
        fprintf(stderr,
                "--stdout is required, output always goes to stdout.\n");
            showHelp();
        exit(1);
    }

    // Initialization
    faupInit();
    modesInitNet();

    // Set up input connection
    beast_input = makeBeastInputService();
    c = serviceConnect(beast_input, bo_connect_ipaddr, bo_connect_port);
    if (!c) {
        fprintf (stderr,
                 "faup1090: failed to connect to %s:%d (is dump1090 running?): %s\n",
                 bo_connect_ipaddr, bo_connect_port, Modes.aneterr);
        exit (1);
    }

    sendBeastSettings(c, "CdfjV"); // Beast binary, no filters, CRC checks on, no mode A/C, verbatim mode on

    // Set up output connection on stdout
    fatsv_output = makeFatsvOutputService();
    createGenericClient(fatsv_output, STDOUT_FILENO);

    fa_cmd_input = makeFaCmdInputService();
    createGenericClient(fa_cmd_input, STDIN_FILENO);

    // Run it until we've lost either connection
    while (!Modes.exit && beast_input->connections && fatsv_output->connections) {
        struct timespec r = { 0, 100 * 1000 * 1000};
        backgroundTasks();
        nanosleep(&r, NULL);
    }

    modesDeInitNet();

    return 0;
}
//
//=========================================================================
//
