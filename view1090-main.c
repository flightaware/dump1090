//  dump1090 - ADS-B software radio decoder
// 
//  Copyright (C) 2018 Paul Ciarlo <paul.ciarlo@gmail.com>
// 
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "dump1090.h"

static void sigintHandler(int dummy) {
    MODES_NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
}

//
// ================================ Main ====================================
//
static void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"| view1090 ModeS Viewer       %45s |\n"
"-----------------------------------------------------------------------------\n"
  "--no-interactive         Disable interactive mode, print messages to stdout\n"
  "--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60)\n"
  "--modeac                 Enable decoding of SSR modes 3/A & 3/C\n"
  "--net-bo-ipaddr <IPv4>   TCP Beast output listen IPv4 (default: 127.0.0.1)\n"
  "--net-bo-port <port>     TCP Beast output listen port (default: 30005)\n"
  "--lat <latitude>         Reference/receiver latitide for surface posn (opt)\n"
  "--lon <longitude>        Reference/receiver longitude for surface posn (opt)\n"
  "--max-range <distance>   Absolute maximum range for position decoding (in nm, default: 300)\n"
  "--no-crc-check           Disable messages with broken CRC (discouraged)\n"
  "--no-fix                 Disable single-bits error correction using CRC\n"
  "--fix                    Enable single-bits error correction using CRC\n"
  "--aggressive             More CPU for more messages (two bits fixes, ...)\n"
  "--metric                 Use metric units (meters, km/h, ...)\n"
  "--show-only <addr>       Show only messages from the given ICAO on stdout\n"
  "--help                   Show this help\n",
  MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION
    );
}

//
//=========================================================================
//
int main(int argc, char **argv) {
    int j;
    struct client *c;
    struct net_service *s;
    char *bo_connect_ipaddr = "127.0.0.1";
    int bo_connect_port = 30005;

    // Set sane defaults

    view1090InitConfig();
    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)

    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments

        if        (!strcmp(argv[j],"--net-bo-port") && more) {
            bo_connect_port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-ipaddr") && more) {
            bo_connect_ipaddr = argv[++j];
        } else if (!strcmp(argv[j],"--modeac")) {
            Modes.mode_ac = 1;
        } else if (!strcmp(argv[j],"--no-interactive")) {
            Modes.interactive = 0;
        } else if (!strcmp(argv[j],"--show-only") && more) {
            Modes.show_only = (uint32_t) strtoul(argv[++j], NULL, 16);
            Modes.interactive = 0;
        } else if (!strcmp(argv[j],"--interactive-ttl") && more) {
            Modes.interactive_display_ttl = (uint64_t)(1000 * atof(argv[++j]));
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--fix")) {
            Modes.nfix_crc = MODES_MAX_BITERRORS;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.nfix_crc = 0;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.nfix_crc = MODES_MAX_BITERRORS;
        } else if (!strcmp(argv[j],"--max-range") && more) {
            Modes.maxRange = atof(argv[++j]) * 1852.0; // convert to metres
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            showHelp();
            exit(1);
        }
    }

#ifdef _WIN32
    // Try to comply with the Copyright license conditions for binary distribution
    if (!Modes.quiet) {showCopyright();}
#define MSG_DONTWAIT 0
#endif

    // Initialization
    view1090Init();
    modesInitNet();

    // Try to connect to the selected ip address and port. We only support *ONE* input connection which we initiate.here.
    s = makeBeastInputService();
    c = serviceConnect(s, bo_connect_ipaddr, bo_connect_port);
    if (!c) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n", bo_connect_ipaddr, bo_connect_port, Modes.aneterr);
        exit(1);
    }

    sendBeastSettings(c, "Cd"); // Beast binary format, no filters
    sendBeastSettings(c, Modes.mode_ac ? "J" : "j");  // Mode A/C on or off
    sendBeastSettings(c, Modes.check_crc ? "f" : "F");  // CRC checks on or off

    // Keep going till the user does something that stops us
    while (!Modes.exit) {
        icaoFilterExpire();
        trackPeriodicUpdate();
        modesNetPeriodicWork();

        if (Modes.interactive)
            interactiveShowData();

        if (s->connections == 0) {
            // lost input connection, try to reconnect
            usleep(1000000);
            c = serviceConnect(s, bo_connect_ipaddr, bo_connect_port);
            continue;
        }

        usleep(100000);
    }

    interactiveCleanup();
    return (0);
}
//
//=========================================================================
//
