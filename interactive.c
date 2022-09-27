// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// interactive.c: aircraft tracking and interactive display
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

#include <curses.h>
#include <regex.h>
#include <sys/types.h>

//
//========================= Interactive mode ===============================


static int convert_altitude(int ft)
{
    if (Modes.metric)
        return (ft / 3.2828);
    else
        return ft;
}

static int convert_speed(int kts)
{
    if (Modes.metric)
        return (kts * 1.852);
    else
        return kts;
}

//
//=========================================================================
//
// Show the currently captured interactive data on screen.
//

double distance_units_conversion;
char  *distance_units_suffix;
regex_t callsign_filter_regex;

void interactiveInit() {
    if (!Modes.interactive) {
        return;
    }

    switch(Modes.interactive_distance_units) {
    case UNIT_NAUTICAL_MILES:
        distance_units_conversion = 0.53996;
        distance_units_suffix = "nm";
        break;
    case UNIT_STATUTE_MILES:
        distance_units_conversion = 0.621371;
        distance_units_suffix = "sm";
        break;
    case UNIT_KILOMETERS:
        distance_units_conversion = 1.0;
        distance_units_suffix = "km";
        break;
    }

    if (Modes.interactive_callsign_filter) {
        int rc = regcomp(&callsign_filter_regex, Modes.interactive_callsign_filter,
            REG_EXTENDED | REG_NOSUB | REG_ICASE);
        if (rc != 0) {
            char msg[256];
            regerror(rc, &callsign_filter_regex, msg, sizeof(msg));
            fprintf(stderr, "Unable to parse filter '%s': %s\n", Modes.interactive_callsign_filter, msg);
            exit(1);
        }
    }

    initscr();
    clear();
    refresh();
}

void interactiveCleanup(void) {
    if (Modes.interactive) {
        regfree(&callsign_filter_regex);
        endwin();
    }
}

void interactiveNoConnection(void) {
    if (!Modes.interactive)
        return;

    mvprintw(0, 0, "  /!\\ input connection lost /!\\ ");
    refresh();
}

void interactiveShowData(void) {
    struct aircraft *a = Modes.aircrafts;
    static uint64_t next_update;
    static bool need_clear = true;
    uint64_t now = mstime();
    char progress;
    char spinner[4] = "|/-\\";
    int valid = 0;
    double signalMax = -100.0;
    double signalMin = +100.0;
    double signalMean = 0.0;
    double distanceMax = 0.0;

    if (!Modes.interactive)
        return;

    if (need_clear) {
        clear();
        need_clear = false;
    }
    // Refresh screen every (MODES_INTERACTIVE_REFRESH_TIME) miliseconde
    if (now < next_update)
        return;

    next_update = now + MODES_INTERACTIVE_REFRESH_TIME;

    if (Modes.interactive_show_distance) {
        mvprintw(1, 0, " Hex    Mode  Sqwk  Flight   Alt    Spd  Hdg  Dist(%s) Bearing  RSSI  Msgs  Ti",
            distance_units_suffix);
    } else {
        mvprintw(1, 0, " Hex    Mode  Sqwk  Flight   Alt    Spd  Hdg    Lat      Long   RSSI  Msgs  Ti");
    }

    mvhline(2, 0, ACS_HLINE, 80);

    progress = spinner[(now/1000)%4];
    mvaddch(0, 79, progress);

    int rows = getmaxy(stdscr);
    int row = 3;
    int rowMaxd = 0;
    int rowMaxRSSI = 0;
    int rowMinRSSI = 0;

    // Ensure trackDataValid uses the current time
    _messageNow = now;

    while (a) {

        if (a->reliable && (now - a->seen) < Modes.interactive_display_ttl
            && (Modes.interactive_callsign_filter == NULL || a->callsign_matched
            || regexec(&callsign_filter_regex, a->callsign, 0, NULL, 0) == 0)) {
            char strSquawk[5] = " ";
            char strFl[7]     = " ";
            char strTt[5]     = " ";
            char strGs[5]     = " ";
            int msgs  = a->messages;

            a->callsign_matched = 1;
            valid++;

            if (trackDataValid(&a->squawk_valid)) {
                snprintf(strSquawk, sizeof(strSquawk), "%04x", a->squawk);
            }

            if (trackDataValid(&a->gs_valid)) {
                snprintf (strGs, sizeof(strGs), "%3d", convert_speed(a->gs));
            }

            if (trackDataValid(&a->track_valid)) {
                snprintf (strTt, sizeof(strTt), "%03.0f", a->track);
            }

            if (msgs > 99999) {
                msgs = 99999;
            }

            double distance               = 0.0;
            char strDistance[8]           = " ";
            double bearing                = 0.0;
            char strBearing[9]            = " ";
            char strMode[5]               = "    ";
            char strLat[8]                = " ";
            char strLon[9]                = " ";
            double * pSig                 = a->signalLevel;
            double signalAverage = (pSig[0] + pSig[1] + pSig[2] + pSig[3] +
                                    pSig[4] + pSig[5] + pSig[6] + pSig[7]) / 8.0;

            switch (a->addrtype) {
            case ADDR_ADSB_ICAO:
                if (a->adsb_version >= 0) {
                    strMode[0] = 'A';
                    strMode[1] = '0' + a->adsb_version;
                } else {
                    strMode[0] = 'S';
                }
                break;
            case ADDR_ADSB_ICAO_NT:
                strMode[0] = 'N';
                strMode[1] = 'T';
                break;
            case ADDR_ADSR_ICAO:
            case ADDR_ADSR_OTHER:
                strMode[0] = 'R';
                break;
            case ADDR_TISB_ICAO:
            case ADDR_TISB_TRACKFILE:
            case ADDR_TISB_OTHER:
                strMode[0] = 'T';
                break;
            default:
                strMode[0] = '?';
                break;
            }

            if (a->modeA_hit) {
                strMode[2] = 'a';
            }
            if (a->modeC_hit) {
                strMode[3] = 'c';
            }

            if (trackDataValid(&a->position_valid)) {
                snprintf(strLat, sizeof(strLat), "%7.03f", a->lat);
                snprintf(strLon, sizeof(strLon), "%8.03f", a->lon);
            }

            if (trackDataValid(&a->airground_valid) && a->airground == AG_GROUND) {
                snprintf(strFl, sizeof(strFl), "grnd ");
            } else if (Modes.use_gnss && trackDataValid(&a->altitude_geom_valid)) {
                snprintf(strFl, sizeof(strFl), "%5dH", convert_altitude(a->altitude_geom));
            } else if (trackDataValid(&a->altitude_baro_valid)) {
                snprintf(strFl, sizeof(strFl), "%5d ", convert_altitude(a->altitude_baro));
            }
            double signalDisplay = 10.0 * log10(signalAverage);


            if ((Modes.bUserFlags & MODES_USER_LATLON_VALID) && trackDataValid(&a->position_valid)) {
                distance = greatcircle(Modes.fUserLat, Modes.fUserLon,
                    a->lat, a->lon);

                distance /= 1000.0;

                distance *= distance_units_conversion;
                if (distance > distanceMax) {
                    distanceMax = distance;
                }
                snprintf(strDistance, sizeof(strDistance), "%5.1f ", distance);
                bearing = get_bearing(Modes.fUserLat, Modes.fUserLon, a->lat, a->lon);
                snprintf(strBearing, sizeof(strBearing), "%5.0f ", bearing);
            }

            if (signalDisplay > signalMax) {
                signalMax = signalDisplay;
            }
            if (signalDisplay < signalMin) {
                signalMin = signalDisplay;
            }
            signalMean += signalDisplay;

            if (row < rows) {
                mvprintw(row, 0, "%s%06X %-4s  %-4s  %-8s %6s %3s  %3s  %7s %8s %5.1f %5d %*.0f",
                     (a->addr & MODES_NON_ICAO_ADDRESS) ? "~" : " ", (a->addr & 0xffffff),
                     strMode, strSquawk, a->callsign, strFl, strGs, strTt,
                     Modes.interactive_show_distance ? strDistance : strLat,
                     Modes.interactive_show_distance ? strBearing : strLon,
                     signalDisplay, msgs, Modes.interactive_display_size, (now - a->seen)/1000.0);

                if (signalDisplay >= signalMax) {
                    rowMaxRSSI = row;
                }
                if (signalDisplay <= signalMin) {
                    rowMinRSSI = row;
                }
                if (distance >= distanceMax) {
                    rowMaxd = row;
                }

                ++row;
            }

        }
        a = a->next;
    }


    if (Modes.mode_ac && !Modes.interactive_callsign_filter) {
        for (unsigned i = 1; i < 4096 && row < rows; ++i) {
            if (modeAC_match[i] || modeAC_count[i] < 50 || modeAC_age[i] > 5)
                continue;

            char strMode[5] = "  A ";
            char strFl[7] = " ";
            unsigned modeA = indexToModeA(i);
            int modeC = modeAToModeC(modeA);
            if (modeC != INVALID_ALTITUDE) {
                strMode[3] = 'C';
                snprintf(strFl, sizeof(strFl), "%5d ", convert_altitude(modeC * 100));
            }
            valid++;

            mvprintw(row, 0,
                     "%7s %-4s  %04x  %-8s %6s %3s  %3s  %7s %8s %5s %5d %2d\n",
                     "",    /* address */
                     strMode, /* mode */
                     modeA, /* squawk */
                     "",    /* callsign */
                     strFl, /* altitude */
                     "",    /* gs */
                     "",    /* heading */
                     "",    /* lat */
                     "",    /* lon */
                     "",    /* signal */
                     modeAC_count[i], /* messages */
                     modeAC_age[i]);  /* age */
            ++row;
        }
    }

    if (rowMaxd > 3 && Modes.interactive_show_distance) {
        mvprintw(rowMaxd, 52, "+");
    }

    mvprintw(rowMaxRSSI, 68, "+");
    mvprintw(rowMinRSSI, 68, "-");

    mvprintw(0, 0, " Tot: %3d Vis: %3d RSSI: Max %5.1f+ Mean %5.1f Min %5.1f-  MaxD: %6.1f%s+ ",
		valid, (rows-3) < valid ? (rows-3) : valid, signalMax, signalMean / valid, signalMin, distanceMax,
			distance_units_suffix);

    if (row < rows) {
        move(row, 0);
        clrtobot();
    }
    move(0, 0);

    refresh();
}

//
//=========================================================================
//
