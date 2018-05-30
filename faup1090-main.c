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
"--help                   Show this help\n"
"\n",
MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION
    );
}

static char *bo_connect_ipaddr = "127.0.0.1";
static int bo_connect_port = 30005;

static void faupParseArgs(int argc, char **argv) {
    int j;
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
        } else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }
}

int main(int argc, char **argv) {
    // Set sane defaults
    faupInitConfig();

    faupParseArgs(argc, argv);

    // Initialization
    faupInit();
    modesInitNet();

    return faupMainLoop(bo_connect_ipaddr, bo_connect_port);
}
